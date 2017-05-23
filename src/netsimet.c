/** ----------------------------------------
* @file   netsimet.c
* @brief
* @author  Rafael de O. Lopes Gonçalves
* @date 26 de outubro de 2011
*------------------------------------------*/

#include "config.h"

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <string.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdarg.h>
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "unp.h"
#include "debug.h"
#include "simet_alexa.h"
#include "netsimet.h"
#include "base64.h"
#include "byteconversion.h"
#include "utils.h"

#define DEFAULT_INTERFACE "eth0"

#define REDIR_301 "301 Moved Permanently"
#define ERRO_403 "Forbidden"
#define ERRO_404 "404 Not Found"

#define myfree(mem) do {fprintf(stderr,"%s:%3.d\%d\n", __FILE__, __LINE__, &mem); free (mem);} while (0)



SSL_CTX * init_ssl_context(int exibe_erro_ssl) {
	SSL_CTX *ctx;

	INFO_PRINT ("SSL INIT\n");
	SSL_library_init ();
	if (exibe_erro_ssl) {
		INFO_PRINT ("carrega mensagens erro SSL\n");
		SSL_load_error_strings ();
		ERR_load_crypto_strings ();
	}
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
	ctx = SSL_CTX_new(TLS_client_method());
	INFO_PRINT("Novo ssl %lx\n", OPENSSL_VERSION_NUMBER);
#else
	ctx = SSL_CTX_new(SSLv23_client_method());
	INFO_PRINT("Deprecated ssl %lx\n", OPENSSL_VERSION_NUMBER);
#endif
	if (ctx == NULL){
		ERR_print_errors_fp(stderr);
		abort();
	}

	return ctx;
}


int send_tcp(int socket_fd, char *message, int64_t len) {
	int64_t total = 0;
	int64_t bytesleft = len;
	int n;

	while (total < len){
		n = send(socket_fd, message + total, bytesleft, 0);
		if (n == -1){
			break;
		} else{
			total = total + n;
			bytesleft = bytesleft - n;
		}
	}
	if (total < len){
		return -1;
	} else{
		return total;
	}
}

int close_tcp(int sock) {
	shutdown(sock, SHUT_RDWR);
	return close(sock);

}
/*
int connect_tcp(const char *host, const char *port, int *sock) {
	int sockfd, n;
	struct addrinfo local_hints, *res, *ressave;

	bzero(&local_hints, sizeof(struct addrinfo));
	local_hints.ai_family = AF_UNSPEC;
	local_hints.ai_socktype = SOCK_STREAM;
	local_hints.ai_flags = AI_CANONNAME;

	if ((n = getaddrinfo_a(host, port, &local_hints, &ressave)) != 0){
		perror("connect_tcp: ");
		ERROR_PRINT("connect_tcp error for %s, %s: %s\n", host, port, gai_strerror(n));
		return -1;
	}
	res = ressave;
	do{

		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0){
			continue;
		}
		if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0){
			break;
		}
		close(sockfd);
	} while ((res = res->ai_next) != NULL);

	if (res == NULL){
		perror("connect_tcp");
		ERROR_PRINT("connect_tcp error for %s, %s", host, port);
		freeaddrinfo(ressave);
		return -1;
	}

	freeaddrinfo(ressave);
	*sock = sockfd;
	return *sock;
}
*/

int convert_family_type(int family) {
	if (family == 4)
		return AF_INET;
	else if (family == 6)
		return AF_INET6;
	else
		return AF_UNSPEC;
}
int ip_type_family(char *ip) {
	struct addrinfo hint, *res = NULL;
	int ret;

	memset(&hint, '\0', sizeof (hint));

	hint.ai_family = PF_UNSPEC;
	hint.ai_flags = AI_NUMERICHOST;

	ret = getaddrinfo(ip, NULL, &hint, &res);
	if (ret) {
		ERRNO_PRINT("IP [%s] invalido, gai_strerror[%s]", ip, gai_strerror(ret));
		saida(1);
	}
	ret = res->ai_family;


	freeaddrinfo(res);

	return ret;
}

void fill_socket_addr(struct sockaddr_storage* addr, const char* host, int port, int family) {
	memset(addr, 0, sizeof(*addr));
	if (family == AF_INET) {
		((struct sockaddr_in*) addr)->sin_family = family;
		((struct sockaddr_in*) addr)->sin_port = htons(port);
		inet_pton(family, host, &((struct sockaddr_in*) addr)->sin_addr);
	}
	else if (family == AF_INET6) {
		((struct sockaddr_in6*) addr)->sin6_family = family;
		((struct sockaddr_in6*) addr)->sin6_port = htons(port);
		inet_pton(family, host, &((struct sockaddr_in6*) addr)->sin6_addr);
	}
}

char *get_ip_by_host (const char* host, int family) {
	int n;
	struct addrinfo local_hints, *res;
	char* temp, *ip = NULL;

	bzero(&local_hints, sizeof(struct addrinfo));
	local_hints.ai_flags = AI_CANONNAME;

	local_hints.ai_family = convert_family_type(family);


	if ((n = getaddrinfo(host, PORT_HTTP, &local_hints, &res)) != 0) {
		if (family)
			INFO_PRINT("impossivel obter IPv%d para %s: %s\n", family, host, gai_strerror(n));
		else
			INFO_PRINT("impossivel obter IP para %s: %s\n", host, gai_strerror(n));
		return NULL;
	}

	temp = get_ip (res->ai_addr);
	if (temp) {
		ip = strdup (temp);
		free (temp);
	}
	freeaddrinfo(res);
	return ip;
}

int connect_tcp(const char *host, const char *port, int family, int *sock) {
	int sockfd, rc, n, err, flags = 1, conectou = 0, num_reg = 1, i, num_shift;
	struct addrinfo local_hints, *res, *ressave, *temp, *temp2 = NULL;
	fd_set wset, eset;
	struct timeval tval;
	struct timespec ts_ini, ts_fim;
	socklen_t err_size;
	uint64_t intervalo = 0;
	char *ip;
	// devolve o socket ou um número negativo em caso de problema começando em -98 e subindo até -20 no máximo por causa dos códigos de erro do getaddrinfo

	bzero(&local_hints, sizeof(struct addrinfo));
	local_hints.ai_socktype = SOCK_STREAM;
	local_hints.ai_flags = AI_CANONNAME;

	local_hints.ai_family = convert_family_type(family);


	if ((n = getaddrinfo(host, port, &local_hints, &res)) != 0) {
		ERRNO_PRINT("connect_tcp erro #%d para %s, %s, %d: %s\n", n, host, port, family, gai_strerror(n));
		return n;
	}
	for (num_reg = 0, temp = res; temp != NULL; temp = temp->ai_next) {
		num_reg++;
//		ip = get_ip (temp->ai_addr);
//		INFO_PRINT ("%s", ip);
//		free (ip);
		if ((num_reg > 1) && (temp->ai_next == NULL)) {
			temp2 = temp;
			temp->ai_next = res;
			break;
		}
	}
	if (num_reg > 1) {
		num_shift = get_rand_i() % num_reg;
//		INFO_PRINT ("shift: %d\n", num_shift);
		for (i = 0, temp = res; i < num_shift; temp2 = temp, temp = temp->ai_next, i++);
		temp2->ai_next = NULL;
		ressave = res = temp;
	}
	else ressave = res;


//	for (temp = res; temp != NULL; temp = temp->ai_next) {
//		ip = get_ip (temp->ai_addr);
//		INFO_PRINT ("%s", ip);
//		free (ip);
//	}
	do {

		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0){
			continue;
		}
		if (fcntl (sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
			ERRNO_PRINT("nao conseguiu setar sock para nao-blocante");
			freeaddrinfo (ressave);
			shutdown (sockfd, SHUT_RDWR);
			close (sockfd);
			return -97;
		}

		FD_ZERO(&wset);
		FD_SET(sockfd, &wset);

		FD_ZERO(&eset);
		FD_SET(sockfd, &eset);

		tval.tv_sec = TIMEOUT;
		tval.tv_usec = 0;
		clock_gettime(CLOCK_MONOTONIC, &ts_ini);
		ip = get_ip (res->ai_addr);

		INFO_PRINT ("conectando a %s:%s... ", ip, port);
		free (ip);

		while ((TIMEVAL_MICROSECONDS(tval)) && (!conectou)) {
			rc = connect(sockfd, res->ai_addr, res->ai_addrlen);
			if (rc == 0) {
				conectou = 1;
				break;
			}
			else if (rc == -1) {
				if (errno != EINPROGRESS) {
					ERRNO_PRINT ("connect()");
					break;
				}
			}
			rc = select(sockfd + 1, NULL, &wset, &eset, &tval);
			clock_gettime(CLOCK_MONOTONIC, &ts_fim);
			intervalo = (TIMESPEC_NANOSECONDS(ts_fim) - TIMESPEC_NANOSECONDS(ts_ini))/1000;
			if (rc == -1) {
				INFO_PRINT ("select (connect) falhou em %"PRUI64" usec", intervalo);
				shutdown (sockfd, SHUT_RDWR);
				close(sockfd);
				continue;
			}
			else if (rc == 0) {
				shutdown (sockfd, SHUT_RDWR);
				close(sockfd);
				INFO_PRINT ("Timeout em connect em %"PRUI64" usec", intervalo);
				continue;
			}
		}
		if (!conectou) {
			err = -1;
			err_size = sizeof (err);
			getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err,  &err_size);
			if (err != 0)
			{
				ERRNO_PRINT ("em getsockopt apos %"PRUI64" usec no select", intervalo);
				continue;
			}
			else if (FD_ISSET(sockfd, &eset))
			{
				INFO_PRINT ("FD_ISSET exception");
				shutdown (sockfd, SHUT_RDWR);
				close(sockfd);
			}

			shutdown (sockfd, SHUT_RDWR);
			close(sockfd);
			continue;

		}
	} while ((!conectou) && ((res = res->ai_next) != NULL));

	if (!conectou) {
		ERROR_PRINT ("connect_tcp error for %s, %s, familia:%d", host, port, family);
		if (sockfd > 0) {
			shutdown (sockfd, SHUT_RDWR);
			close (sockfd);
		}
		freeaddrinfo(ressave);
		return -96;
	}
	else {
		INFO_PRINT ("conectado");
	}
	freeaddrinfo(ressave);

	*sock = sockfd;
	return *sock;
}
int converte_str_ip (const char *host, void *addr) {
	int n;
	struct addrinfo local_hints, *res;

	bzero(&local_hints, sizeof(struct addrinfo));
	local_hints.ai_socktype = SOCK_STREAM;
	local_hints.ai_flags = AI_CANONNAME;
	local_hints.ai_family = AF_UNSPEC;


	if ((n = getaddrinfo(host, NULL, &local_hints, &res)) != 0) {
		ERRNO_PRINT("converte_str_ip erro #%d para %s: %s\n", n, host, gai_strerror(n));
		return n;
	}

	memcpy (addr, res->ai_addr, res->ai_addrlen);


	freeaddrinfo(res);
	return 0;
}

// SSL * connect_ssl(int socket_fd, SSL_CTX* ctx) {
// 	int flags;
//
// 	if (!ctx) {
// 	ERR_print_errors_fp (stderr);
// 		saida (1);
// 	}
// 	SSL* ssl = SSL_new(ctx);
// 	if (!ssl) {
// 		ERR_print_errors_fp (stderr);
// 		saida (1);
// 	}
//
// 	flags = fcntl (socket_fd, F_GETFL, 0);
// 	flags &= ~O_NONBLOCK;
//
// 	fcntl (socket_fd, F_SETFL, flags);
//
//
// 	SSL_set_fd(ssl, socket_fd);
// 	/* perform the connection */
// 	if (SSL_connect(ssl) == -1){
// 		ERR_print_errors_fp(stderr);
// 		return NULL;
// 	} else{
// 		INFO_PRINT("Connected with %s encryption\n", SSL_get_cipher(ssl));
// 		return ssl;
// 	}
// }

SSL * connect_ssl_bio(int socket_fd, SSL_CTX* ctx) {
	int rc,  desconecte = 0, segunda_tentativa = 0;
	BIO *sbio = NULL;
	struct timeval tv_timeo;
	fd_set rwset;


	if (!ctx) {
		ERR_print_errors_fp (stderr);
		saida (1);
	}
	SSL* ssl = SSL_new(ctx);
	if (!ssl) {
		ERR_print_errors_fp (stderr);
		saida (1);
	}
	sbio = BIO_new_socket (socket_fd, BIO_NOCLOSE);
	SSL_set_bio (ssl, sbio, sbio);
	SSL_set_fd (ssl, socket_fd);
	/* perform the connection */
	tv_timeo.tv_sec = TIMEOUT/2;
	tv_timeo.tv_usec = 0;
	while (!desconecte) {
		rc = SSL_connect(ssl);
		if (rc == 1)
			break;
		switch (SSL_get_error(ssl, rc)) {
			case SSL_ERROR_NONE:
				/* Everything worked :-) */
				break;
			case SSL_ERROR_SSL:
				INFO_PRINT ("ssl handshake failure\n" );
				ERR_print_errors_fp(stderr);
				desconecte = 1;
				break;

				/* These are for NON-BLOCKING I/O only! */
			case SSL_ERROR_WANT_READ:
				if (segunda_tentativa) {
						desconecte = 1;
						INFO_PRINT ("connect want read." );
					}
					else {
					FD_ZERO (&rwset);
					FD_SET (socket_fd, &rwset);
					if (select(socket_fd + 1, &rwset, NULL, NULL, &tv_timeo) <= 0) {
						INFO_PRINT ("tentando segunda vez" );
						segunda_tentativa = 1;
					}
				}
				break;
			case SSL_ERROR_WANT_WRITE:
				FD_ZERO (&rwset);
				FD_SET (socket_fd, &rwset);
				if (select(socket_fd + 1, NULL, &rwset, NULL, &tv_timeo) <= 0) {
					desconecte = 1;
					INFO_PRINT ("connect want write." );
				}
				break;
			case SSL_ERROR_WANT_CONNECT:
				INFO_PRINT ("want connect. sleep a while, try again." );
				desconecte = 1;
				break;
			case SSL_ERROR_SYSCALL:
				ERROR_PRINT("SSL_connect");
				desconecte = 1;
				break;
			case SSL_ERROR_WANT_X509_LOOKUP:
				/* not used! */
				INFO_PRINT ("shouldn't be getting this.\n" );
				break;
			case SSL_ERROR_ZERO_RETURN:
				INFO_PRINT ("connection closed.\n" );
				desconecte = 1;
		}
	}
	ERR_clear_error();

	if (desconecte) {
		/* close everything down */
		SSL_shutdown(ssl);
		close(socket_fd);

		SSL_free( ssl );
		ssl = NULL;
		return NULL;
	}
	INFO_PRINT("Connected with %s encryption\n", SSL_get_cipher(ssl));
	return ssl;
}
int connect_socket(struct addrinfo *addr, int *sock) {
	*sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (*sock == -1){
		perror("Socket error");
		return -1;
	}
	if (connect(*sock, addr->ai_addr, addr->ai_addrlen) == -1){
		perror("Connection Error");
		return -1;
	}
	return 1;
}

int connect_udp(const char *host, const char *port, int family, int *socketfd) {
	int sockfd, n;
	struct addrinfo hints, *res, *ressave;


//    TRACE_PRINT("%p ----> %s",host, host );

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_DGRAM;

	hints.ai_family = convert_family_type(family);

	if ((n = getaddrinfo (host, port, &hints, &res)) != 0) {
		ERRNO_PRINT("connect_udp error for %s, %s, %d: %s\n", host, port, family, gai_strerror(n));
		return -1;
	}
	ressave = res;

	do{
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0)
			continue; /* ignore this one */
		if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
			break; /* success */
		close(sockfd); /* ignore this one */
	} while ((res = res->ai_next) != NULL);
	if (res == NULL){ /* errno set from final connect() */
		ERROR_PRINT("udp_connect error for %s, %s", host, port);
		return -1;
	}

	freeaddrinfo(ressave);
	*socketfd = sockfd;
//    TRACE_PRINT("%p ----> %s",host, host );
	return (sockfd);
}

int send_rec_tcp_then_close(const char *dest, const char *port, int family, const char *message, char **result) {
	int sock;
	int status;
	int i;
	int bytes_received;
	char recv_buffer[MAX_BUFFER + 1];
	int using = 0;
	int total = MAX_BUFFER;

	status = connect_tcp(dest, port, family, &sock);
	if (status < 0){
		perror("send_rec_tcp_message_then_close connect");
		return status;
	}
	int64_t len = strlen(message);
	status = send(sock, message, len, 0);
	if (status < 0){
		return status;
	}
	*result = calloc(total, sizeof(char));
	bytes_received = 1;
	while (bytes_received > 0){
		bytes_received = recv(sock, recv_buffer, MAX_BUFFER, 0);
		if (using + bytes_received > total){
			total = total + MAX_BUFFER;
			*result = realloc(*result, sizeof(char) * total);
		}
		for (i = 0; i < bytes_received; i++){
			(*result)[using + i] = recv_buffer[i];
		}
		using = using + bytes_received;
	}
	if (bytes_received < 0){
		return bytes_received;
	}
	(*result)[using + 1] = '\0';
	close_tcp(sock);
	return 1;
}

char * get_ip(void *sa_void) {
	// void, mas deve ser uma das seguintes structs:
	// sockaddr, sockaddr_in, sockaddr_in6 ou sockaddr_storage
	char * str = NULL;
	struct sockaddr *sa = sa_void;
//	INFO_PRINT("(get_ip) sa->sa_family=0x%x \t AF_INET=0x%x \t AF_INET6=0x%x", sa->sa_family, AF_INET, AF_INET6);
	if (sa->sa_family == AF_INET) {
		str = calloc(INET_ADDRSTRLEN + 1, sizeof(char));
		inet_ntop(AF_INET, &(((struct sockaddr_in *) sa)->sin_addr), str, INET_ADDRSTRLEN);
	} else if (sa->sa_family == AF_INET6) {
		str = calloc(INET6_ADDRSTRLEN + 1, sizeof(char));
		inet_ntop(AF_INET6, &(((struct sockaddr_in6 *) sa)->sin6_addr), str, INET6_ADDRSTRLEN);
	}
	return str;
}

char *get_local_ip(int socketfd) {
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof(addr);
	if (getsockname(socketfd, (struct sockaddr *) &addr, (socklen_t *) &addr_len) < 0){
		ERROR_PRINT("Error getting local address");
		return NULL;
	}
	return get_ip(&addr);

}


char * get_remote_ip(int socketfd) {
	struct sockaddr_storage addr;
	char *result;
	socklen_t addr_len = sizeof(addr);
	if (getpeername(socketfd, (struct sockaddr *) &addr, (socklen_t *) &addr_len) < 0){
		ERROR_PRINT("Error getting remote ip address");
		saida (1);
//        return NULL;
	}
	result = get_ip(&addr);
	INFO_PRINT ("remote IP: %s", result);
	return result;

}



static union packetudp_u encode_packet(int64_t counter, int64_t timestamp, int64_t extra) {
	union packetudp_u pac;
	pac.numbers[0] = htobe64(counter);
	pac.numbers[1] = htobe64(timestamp);
	pac.numbers[2] = htobe64(extra);
	return pac;
}

void decode_pac(union packetudp_u pac, uint64_t * counter, uint64_t * timestamp, uint64_t * extra) {
	if (pac.buffer[0] == 0x80 && pac.buffer[1] == 0x14){
		/*RTT package*/
		TRACE_PRINT(
				"bytes recv = {x%x x%x x%x x%x x%x x%x"
				" x%x x%x x%x x%x x%x x%x"
				" x%x x%x x%x x%x x%x x%x"
				" x%x x%x x%x x%x x%x x%x}",
				pac.buffer[0], pac.buffer[1], pac.buffer[2],
				pac.buffer[3], pac.buffer[4], pac.buffer[5],
				pac.buffer[6], pac.buffer[7], pac.buffer[8],
				pac.buffer[9], pac.buffer[10], pac.buffer[11],
				pac.buffer[12], pac.buffer[13], pac.buffer[14],
				pac.buffer[15], pac.buffer[16], pac.buffer[17],
				pac.buffer[18], pac.buffer[19], pac.buffer[20],
				pac.buffer[21], pac.buffer[22], pac.buffer[23]);

		*counter = be16toh(pac.rtp.sequence_number);
		*timestamp = be32toh(pac.rtp.timestamp);
		*extra = 0;

	} else{
		*counter = be64toh(pac.numbers[0]);
		*timestamp = be64toh(pac.numbers[1]);
		*extra = be64toh(pac.numbers[2]);
	}
//        INFO_PRINT("udp_pkg: sequence_number = %"PRUI64" - timestamp = %"PRUI64" - extra = %"PRUI64" ||| pac0 = %"PRUI64", - pac1 = %"PRUI64", - pac2 = %"PRUI64,*counter,*timestamp, *extra, pac.numbers[0], pac.numbers[1], pac.numbers[2]);

}


int send_rtp(int socket_udp, uint16_t counter, int32_t timestamp){
	union packetudp_u pac;
	int status;

	pac.buffer[0] = 0x80;
	pac.buffer[1] = 0x14;
	pac.rtp.sequence_number = htons((uint16_t) counter);
	pac.rtp.timestamp = htobe32((int32_t) timestamp);
	pac.rtp.src =  htobe32(1l);

	TRACE_PRINT(
			"bytes send = {x%x x%x x%x x%x x%x x%x"
			" x%x x%x x%x x%x x%x x%x"
			" x%x x%x x%x x%x x%x x%x"
			" x%x x%x x%x x%x x%x x%x}",
			pac.buffer[0], pac.buffer[1], pac.buffer[2], pac.buffer[3], pac.buffer[4],
			pac.buffer[5], pac.buffer[6], pac.buffer[7], pac.buffer[8], pac.buffer[9],
			pac.buffer[10], pac.buffer[11], pac.buffer[12], pac.buffer[13], pac.buffer[14],
			pac.buffer[15], pac.buffer[16], pac.buffer[17], pac.buffer[18], pac.buffer[19],
			pac.buffer[20], pac.buffer[21], pac.buffer[22], pac.buffer[23]);

	TRACE_PRINT("udp_pkg: sequence_number = %d\ttimestamp = %d", counter, timestamp);
	status = send(socket_udp, pac.buffer, 12, 0);
	if (status < 0){
		ERRNO_PRINT("fail send udp: status = %d, socket %d\n", status, socket_udp);
		saida (1);
	}
	return status;
}



int send_udp(int socket_udp, int64_t counter, int64_t timestamp, int64_t extra) {
	union packetudp_u pac;
	int status;
	pac = encode_packet(counter, timestamp, extra);
	status = send(socket_udp, pac.buffer, sizeof(pac.buffer), 0);
	if (status < 0){
		ERROR_PRINT("fail send udp: status = %d no socket %d\n", status, socket_udp);
	}
	return status;
}



int recv_udp2(int socket_udp, uint64_t * counter, uint64_t * timestamp, uint64_t * extra) {
	int status;
//    int i;
	uint8_t buffer[BUFSIZ];
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;

	union packetudp_u pac;
	peer_addr_len = sizeof(struct sockaddr_storage);

	status = recvfrom(socket_udp, buffer, sizeof(buffer), 0, (struct sockaddr *) &peer_addr, &peer_addr_len);
//    INFO_PRINT("udp recv %d bytes", status);
	if (status <= 0){
		return status;
	}
	memcpy(pac.buffer, buffer, sizeof(pac.buffer));
//    for (i = 0; i < sizeof(pac.buffer); i++){
//        pac.buffer[i] = buffer[i];
//    }

	decode_pac(pac, counter, timestamp, extra);
	return status;
}
int recv_udp(int socket_udp, uint64_t * counter, uint64_t * timestamp, uint64_t * extra) {
	int status;
//    int i;
	uint8_t buffer[BUFSIZ];
	union packetudp_u pac;

	status = recv(socket_udp, buffer, sizeof(buffer), 0);
//    INFO_PRINT("udp recv %d bytes", status);
	if (status <= 0){
		return status;
	}
	memcpy(pac.buffer, buffer, sizeof(pac.buffer));
//    for (i = 0; i < sizeof(pac.buffer); i++){
//        pac.buffer[i] = buffer[i];
//    }

	decode_pac(pac, counter, timestamp, extra);
	return status;
}
/*
int recv_udp(int socket_udp, uint64_t * counter, uint64_t * timestamp, uint64_t * extra) {
	int status, i;
//    uint8_t buffer[BUFSIZ];
	union packetudp_u pac;
	status = recv(socket_udp, pac.buffer, sizeof(pac.buffer), 0);
//    TRACE_PRINT("udp recv %d bytes", status);
	if (status <= 0){
		return status;
	}
//    memcpy(pac.buffer, buffer
//    for (i = 0; i < sizeof(pac.buffer); i++){
//        pac.buffer[i] = buffer[i];
//    }

	decode_pac(pac, counter, timestamp, extra);
	return status;
}
*/
int get_local_port(int socket) {
	struct sockaddr_storage sin;
	int port = -1;
	socklen_t addrlen = sizeof(sin);
	if (getsockname(socket, (struct sockaddr *) &sin, &addrlen) == 0) {
		if (sin.ss_family == AF_INET) {
			port = ntohs((*(struct sockaddr_in*) &sin).sin_port);
		}
		else if (sin.ss_family == AF_INET6) {
			port = ntohs((*(struct sockaddr_in6*) &sin).sin6_port);
		}
	}
	else {
		ERROR_PRINT("fail to get local ip");
		return -1;
	}
	return port;
}

char * get_local_macaddress() {
	int fd;
	struct ifreq ifr;
	char * buffer;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_ifru.ifru_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, DEFAULT_INTERFACE, IFNAMSIZ-1);
	ioctl(fd, SIOCGIFHWADDR, &ifr);
	buffer = malloc(13);
	snprintf(buffer, 13, "%.2x%.2x%.2x%.2x%.2x%.2x",
			(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
			(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
			(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
			(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
			(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
			(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]);
	/*TODO*/
	return buffer;
}


ssize_t recv_timeout(int sockfd, char *buffer, uint8_t buffer_size, uint8_t timeout) {
	struct timeval tv;

	tv.tv_sec = timeout;  /* Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange errors

	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
	return recv(sockfd, buffer, buffer_size, 0);
}

char *recebe_resposta (int sock, int64_t *size_recv, uint8_t para_em_barra_r_barra_n) {
	char *result = NULL, *aux = NULL, answer [8192], *vel;
	int num;
	int tmpres = 0, tam = 0;
	uint64_t tempo_total;
	struct timeval tv_timeo;
	struct timespec ts_ini, ts_fim;
	fd_set rset, rset_master;

	clock_gettime(CLOCK_MONOTONIC, &ts_ini);

	FD_ZERO (&rset_master);
	FD_SET (sock, &rset_master);

	do {
		tv_timeo.tv_sec = TIMEOUT;
		tv_timeo.tv_usec = 0; // select zera o timeout
		memcpy(&rset, &rset_master, sizeof(rset_master));
		num = select (sock + 1, &rset, NULL, NULL, &tv_timeo);
		if (num <= 0) {
			*size_recv = tam * -1;
			fprintf (stderr, "\rTOTAL recebido: %d bytes com TIMEOUT", tam);
			fflush(stderr);
			return result;
		}
		do {
			if (FD_ISSET(sock, &rset)) {
				memset (answer, 0, sizeof (answer));
				tmpres = recv(sock, answer, sizeof (answer) - 1, 0);
				if (tmpres > 0) {
					aux = (char *) realloc (result, tam + tmpres + 1);
					if (!aux) {
						INFO_PRINT ("Impossivel alocar memoria para saida do Web Service. Abortando\n");
						if (result) {
							free (result);
							saida (1);
						}
					}
					result = aux;
					memcpy (result + tam, answer, tmpres);
					tam += tmpres;
					result[tam] = '\0';
					fprintf (stderr, "\rRecebidos %d bytes", tam);
					fflush(stderr);
				}
				if (para_em_barra_r_barra_n) {
					if ((tam > 1) && (aux = memrchr (result, '\n', tam)) && (*(aux-1) == '\r')) { // procura por um "/r/n" de trás pra frente
						break;
					}
				}
			}
		} while(tmpres  > 0);
		if (tmpres < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			}
		}
		else {
			clock_gettime(CLOCK_MONOTONIC, &ts_fim);
			tempo_total = (TIMESPEC_NANOSECONDS(ts_fim) - TIMESPEC_NANOSECONDS(ts_ini))/1000;
			*size_recv = tam;
			vel = velocidade(tam, tempo_total);
			fprintf (stderr, "\rTOTAL recebido: %d bytes em %"PRUI64" us e %sbps", tam, tempo_total, vel);
			fflush(stderr);
			free (vel);
			return result;
		}
	} while (num > 0);

	*size_recv = tam;
	fprintf (stderr, "\rTOTAL recebido: %d bytes", tam);
	fflush(stderr);
	return result;
}

int posicao_inicio_conteudo_html (char *entrada, int64_t *tam, int64_t size_recv) {
	char *aux, *htmlsize, *htmlcontent, *num, contentlen[]="Content-Length:";
	int64_t size = 0;
	int j;

	if (!entrada)
		saida (1);
	htmlsize = strcasestr(entrada, contentlen);
	if (htmlsize) {
		htmlsize += strlen (contentlen);
		aux = strstr(htmlsize, "\r\n");
		if (aux) {
			num = malloc (aux - htmlsize + 1);
			if (!num) {
				ERROR_PRINT ("%s\n", NAO_ALOCA_MEM);
				saida (1);
			}
			strncpy (num, htmlsize, aux - htmlsize + 1);
			size = atol (num);
			free (num);
		}
	}
	else {
		htmlsize = entrada;
	}

	htmlcontent = strstr(htmlsize, "\r\n\r\n");
	j = htmlsize - entrada;
	if((htmlsize) && (j + 4 <= size_recv)){
		htmlcontent += 4;
		if (!size)
			size = size_recv - j;
		if (tam)
			*tam = size;
	}
	else return 0;

	return htmlcontent - entrada;
}


void recebe_resposta_baixa_arq (int sock, int64_t *size_recv, char *arquivo) {
	char *result = NULL, *aux = NULL, answer [8192];
	int num, rc, ini_conteudo = 0;
	int tmpres = 0, tam = 0;
	struct timeval tv_timeo;
	fd_set rset, rset_master;
	FILE *fp = NULL;

	FD_ZERO (&rset_master);
	FD_SET (sock, &rset_master);

	do {
		tv_timeo.tv_sec = TIMEOUT;
		tv_timeo.tv_usec = 0; // select zera o timeout
		memcpy(&rset, &rset_master, sizeof(rset_master));
		num = select (sock + 1, &rset, NULL, NULL, &tv_timeo);
		if (num <= 0) {
			*size_recv = tam * -1;
			fprintf (stderr, "\rTOTAL recebido: %d bytes com TIMEOUT", tam);
			fflush(stderr);
			return;
		}
		do {
			if (FD_ISSET(sock, &rset)) {
				memset (answer, 0, sizeof (answer));
				tmpres = recv(sock, answer, sizeof (answer) - 1, 0);
				if (tmpres > 0) {
					if (!ini_conteudo) {
						aux = (char *) realloc (result, tam + tmpres + 1);
						if (!aux) {
							INFO_PRINT ("Impossivel alocar memoria para saida do Web Service. Abortando\n");
							if (result) {
								free (result);
								saida (1);
							}
						}
						result = aux;
						memcpy (result + tam, answer, tmpres);
						tam += tmpres;
						result[tam] = '\0';
						fprintf (stderr, "\rRecebidos %d bytes", tam - ini_conteudo);
						fflush(stderr);
						ini_conteudo = posicao_inicio_conteudo_html (result, size_recv, tam);
						if (ini_conteudo) {
							if((fp = fopen(arquivo, "wb")) == NULL) {
								INFO_PRINT("nao pode abrir arquivo %s para escrita.\n", arquivo);
								saida (1);
							}
							rc = fwrite(&result[ini_conteudo], 1, tam - ini_conteudo, fp);
							if (rc != (tam - ini_conteudo))
								INFO_PRINT ("Escrita com problemas");
							free (result);
							tam -= ini_conteudo;
						}
					}
					else {
						rc = fwrite(answer, 1, tmpres, fp);
						if (rc != tmpres)
							INFO_PRINT ("Escrita com problemas");
						else {
							tam += tmpres;
							fprintf (stderr, "\rRecebidos %d bytes", tam);
							fflush(stderr);
						}
					}
				}
			}
		} while(tmpres  > 0);
		if (tmpres < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			}
		}
		else {
			*size_recv = tam;
			fprintf (stderr, "\rTOTAL recebido: %d bytes", tam);
			fclose (fp);
			fflush(stderr);
			return;
		}
	} while (num > 0);

	*size_recv = tam;
	fprintf (stderr, "\rTOTAL recebido: %d bytes", tam);
	fclose (fp);
	fflush(stderr);
}

char *chama_troca_msg_tcp (int sock, char* host, char *port, char *chamada, int64_t* size_recv) {

	if (send(sock, chamada, strlen(chamada), 0) < strlen (chamada)) {
		INFO_PRINT ("Erro ao enviar %s ao host %s:%s", chamada, host, port);
		return NULL;
	}

	return recebe_resposta (sock, size_recv, ATE_DESCONEXAO);
}
void chama_troca_msg_tcp_arq (int sock, char* host, char *port, char *chamada, int64_t* size_recv, char *arquivo) {

	if (send(sock, chamada, strlen(chamada), 0) < strlen (chamada)) {
		INFO_PRINT ("Erro ao enviar %s ao host %s:%s", chamada, host, port);
		return;
	}

	recebe_resposta_baixa_arq (sock, size_recv, arquivo);
}
char *chama_troca_msg_tcp_barra_r_barra_n (int sock, char* host, char *port, char *chamada, int64_t* size_recv) {

	if (send(sock, chamada, strlen(chamada), 0) < strlen (chamada)) {
		INFO_PRINT ("Erro ao enviar %s ao host %s:%s", chamada, host, port);
		return NULL;
	}

	return recebe_resposta (sock, size_recv, PARA_EM_BARRA_R_BARRA_N);
}



int return_code (char *entrada, int size) {
	int pos = 9; // posicao do código http na resposta
	int tam_cod = 3;
	int min_tam = pos + tam_cod;
	int cod;
	char *str_cod;

	if ((!entrada) || (size < min_tam))
		return -1;
	str_cod = strndup (entrada + pos, tam_cod);
	cod = atoi (str_cod);
	free (str_cod);
	return cod;
}

char *parse_html (char *entrada, int64_t *tam, int64_t size_recv) {
	char *aux, *htmlsize, *htmlcontent, *num, contentlen[]="Content-Length:";
	int64_t size = 0;
	int i, j;

	if (!entrada)
		saida (1);
	if (!strnlen (entrada, 1))
		return NULL;
	htmlsize = strcasestr(entrada, contentlen);
	if (htmlsize) {
		htmlsize += strlen (contentlen);
		aux = strstr(htmlsize, "\r\n");
		if (aux) {
			num = malloc (aux - htmlsize + 1);
			if (!num) {
				ERROR_PRINT ("%s\n", NAO_ALOCA_MEM);
				saida (1);
			}
			strncpy (num, htmlsize, aux - htmlsize + 1);
			size = atol (num);
			free (num);
		}
	}
	else {
		htmlsize = entrada;
	}

	htmlcontent = strstr(htmlsize, "\r\n\r\n");
	j = htmlsize - entrada;
	if((htmlsize) && (j + 4 <= size_recv)){
		htmlcontent += 4;
		if (!size)
			size = size_recv - j;
		else if (size > size_recv - (entrada - htmlcontent)) {
			// pegou size em content-lenght
			INFO_PRINT ("tamanho recebido no cabecalho %"PRI64" maior que bytes recebidos %"PRI64"\n", size, size_recv);
			saida (1);
		}

/*		aux = (char*) malloc (size + 1);
		if (!aux) {
			ERROR_PRINT ("Impossivel alocar memoria para conteudo da saida do Web Service. Abortando\n");
			return NULL;
		}
		else {
			memcpy (aux, htmlcontent, size);
			aux[size] = '\0';
			if (tam)
				*tam = size;
		}
*/
		for (i = 0, j = htmlcontent - entrada; i < size; i++, j++)
			entrada [i] = entrada [j];
		entrada [i] = '\0';
		if (tam)
			*tam = size;
	}
	else return NULL;

	return entrada;
}

char *parse_html_put (char *entrada, int64_t *tam, int64_t size_recv) {
	char *aux, *htmlcontent = NULL, *htmlsize, *num;
	int64_t size = 0;

	if (!entrada)
		saida (1);
	if (strcasestr(entrada, ERRO_404)) {
		INFO_PRINT ("%s\n", ERRO_404);
		return NULL;
	}
	else if (strcasestr(entrada, ERRO_403)) {
		INFO_PRINT ("%s\n", ERRO_403);
		return NULL;
	}
	htmlsize = strstr(entrada, "\r\n\r\n");
	if((htmlsize) && (htmlsize - entrada + 4 <= size_recv)){
		htmlsize += 4;
		aux = strstr(htmlsize, "\r\n");
		if (aux) {
			num = malloc (aux - htmlsize + 1);
			if (!num) {
				ERROR_PRINT ("%s\n", NAO_ALOCA_MEM);
				saida (1);
			}
			strncpy (num, htmlsize, aux - htmlsize + 1);
			size = atol (num);
			free (num);
			if (size) {
				htmlcontent = strstr(aux, "\r\n");
			}
			if ((htmlcontent) && (htmlcontent - entrada + 2 <= size_recv)) {
				htmlcontent += 2;
				if (size > size_recv - (entrada - htmlcontent)) {
					INFO_PRINT ("tamanho recebido no cabecalho maior que bytes recebidos\n");
					saida (1);
				}
			}
		}
		else {
			size = size_recv - (htmlsize - entrada);
			htmlcontent = htmlsize;
		}

	}
	if (htmlcontent) {
		aux = strdup (htmlcontent);
		if (!aux) {
			INFO_PRINT ("Impossivel alocar memoria para conteudo da saida do Web Service. Abortando\n");
			return NULL;
		}
		aux[size] = '\0';
		if (tam)
			*tam = size;
	}
	else return NULL;
	return aux;
}

char *prepara_url (char* host, char *port, int64_t *size, char *chamada) {
	char *url_ws;
	int rc;


	if (!size) {
		// arquivo pode ser cacheado
		rc = asprintf(&url_ws,"GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: SimetBox\r\nPragma: no-cache\r\n\r\n", chamada, host);
	}
	else {
		rc = asprintf(&url_ws,"GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: SimetBox\r\n\r\n", chamada, host);
	}
	if (rc <=0) {
		ERROR_PRINT ("erro alocacao memoria");
		saida (1);
	}

//	INFO_PRINT ("prepara_url [%s]\n", url_ws);
	return url_ws;
}

char *encontra_redir (char* entrada) {
	char *result = NULL, *aux, *site_redir, location [] = "location: ";
	site_redir = strcasestr(entrada, location);
	if (site_redir) {
		site_redir+= strlen (location);
		aux = strstr(site_redir, "\r\n");
		if (aux) {
			result = strndup (site_redir, aux - site_redir + 1);
			if (result)
				result [aux - site_redir] = '\0';
			else {
				ERROR_PRINT ("nao alocou memoria");
				saida (1);
			}
			INFO_PRINT ("redir para: %s\n", result);
		}
		else {
			result = entrada;
			return NULL;
		}

	}
	else {
		INFO_PRINT ("nao achei redir para: %s\n", entrada);
		result = entrada;
		return NULL;
	}

 	return result;
}

char *chama_web_service (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t* size, int redir, const char *format, ...) {
	char *url_ws, *result, *chamada, *site, *aux;
	int status, sock, i;
	int64_t size_recv;
	URL_PARTS *urlpartsp, urlparts;


	va_list arg;
	va_start (arg, format);
	if (vasprintf (&chamada, format, arg) < 0) {
		ERROR_PRINT ("sem memoria");
		saida (1);
	}
	va_end (arg);



	urlpartsp = &urlparts;

	if (redir == 5) {
		if (size)
			*size = 0;
		return strdup ("");
	}

	aux = converte_url2(chamada);
	chamada = aux;

	for (i = 0; i < 3; i++) {
		status = connect_tcp(host, port, family, &sock);
		if (status < 0){
			INFO_PRINT("Nao conectou com %s:%s para a chamada %s", host, port, chamada);
			sleep (5);
		}
		else break;
	}
	if (status < 0)
		return strdup ("");

	url_ws = prepara_url (host, port, size, chamada);

	result = chama_troca_msg_tcp(sock, host, port, url_ws, &size_recv);
	free (url_ws);

	status = return_code(result, size_recv);
	INFO_PRINT ("host: %s, chamada: %s, cod %d\n", host, chamada, status);

	if ((status >= 301) && (status <= 307)) {
		site = encontra_redir (result);
		if (!site) {
			free (result);
			return strdup ("");
			if (size)
				*size = 0;
			close_tcp (sock);
			return strdup ("");
		}
		if ((site) && (strncmp (site, "/", 1) == 0)) {
			if (asprintf(&aux, "http%s://%s%s",strncmp(port, "80", 2)?"s":"", host, site) <= 0) {
				ERROR_PRINT ("erro alocacao memoria");
				saida (1);
			}
			ERROR_PRINT ("erro redir para %s do host %s. Redirecionando para %s", site, host, aux);
			free (site);
			site = aux;
		}
		free (result);
		separa_protocolo_host_chamada (urlpartsp, site);
		if (urlpartsp->https) {
			if (!ssl_ctx)
				ssl_ctx = init_ssl_context(0);
			result = chama_web_service_ssl_arg (ssl_ctx, urlpartsp->host, PORT_HTTPS , family, size, redir + 1, urlpartsp->chamada);
		}
		else {
			result = chama_web_service (ssl_ctx, urlpartsp->host, PORT_HTTP, family, size, redir + 1, urlpartsp->chamada);
		}
		free (urlpartsp->host);
		free (urlpartsp->chamada);
		free (chamada);
		free (site);
		return result;
	}
	else if (status == 200) {
		parse_html (result, size, size_recv);
	}
	else {
		free (result);
		result = strdup ("");
	}

	close_tcp (sock);

	if (!result) {
		saida (1);
	}
	else {
		INFO_PRINT ("saida webservice %s:\n[%s]\n", chamada, result);
	}
	free (chamada);

	return (result);
}

void chama_web_service_arq (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t* size, int redir, char *chamada, char *arquivo) {
	char *url_ws, *aux;
	int status, sock, i;
	int64_t size_recv;



	if (redir == 5) {
		if (size)
			*size = 0;
		return ;
	}

	aux = converte_url2(chamada);
	chamada = aux;

	for (i = 0; i < 3; i++) {
		status = connect_tcp(host, port, family, &sock);
		if (status < 0){
			INFO_PRINT("Nao conectou com %s:%s para a chamada %s", host, port, chamada);
			sleep (5);
		}
		else break;
	}
	if (status < 0)
		return;

	url_ws = prepara_url (host, port, size, chamada);

	chama_troca_msg_tcp_arq (sock, host, port, url_ws, &size_recv, arquivo);
	free (url_ws);


	close_tcp (sock);

	free (chamada);
}


char *chama_web_service_put (char* host, char *port, int family, int64_t *size, char *string, const char *format, ...) {
	char *url_ws, *result, *content, *chamada;
	int status, sock, i;
	int64_t size_recv;

	va_list arg;
	va_start (arg, format);
	if (vasprintf (&chamada, format, arg) < 0) {
		ERROR_PRINT ("sem memoria");
		saida (1);
	}
	va_end (arg);

	for (i = 0; i < 3; i++) {
		status = connect_tcp(host, port, family, &sock);
		if (status < 0){
			INFO_PRINT("Nao conectou com %s:%s para a chamada %s", host, port, chamada);
			sleep (5);
		}
		else break;
	}
	if (status < 0)
		return strdup ("");
	if (asprintf(&url_ws,"PUT %s HTTP/1.1\r\nContent-Type: application/json\r\nAccept: application/json\r\ncharset: utf-8\r\nHost: %s\r\nUser-Agent: SimetBox\r\nConnection: close\r\nContent-Length: %zu\r\n\r\n%s", chamada, host, strlen(string), string) < 0) {
		INFO_PRINT("Nao conseguiu alocar buffer envio de webservice do host %s:%s para a chamada %s", host, port, chamada);
		saida (1);
	}
	result = chama_troca_msg_tcp (sock, host, port, url_ws, &size_recv);
	status = return_code (result, size_recv);
	content = parse_html_put (result, size, size_recv);
	free (result);
	result = content;
	close_tcp (sock);


	free (url_ws);
	if (!result) {
		free (chamada);
		return NULL;
	}
	else {
		INFO_PRINT ("saida webservice %s:\n[%s]\n", chamada, result);
	}
	free (chamada);

	return (result);
}

// char *chama_web_service_post (char* host, char *port, int family, int64_t *size, char *string, const char *format, ...) {
// 	char *url_ws, *result, *content, *chamada;
// 	int status, sock, i;
// 	int64_t size_recv;
//
// 	va_list arg;
// 	va_start (arg, format);
// 	if (vasprintf (&chamada, format, arg) < 0) {
// 		ERROR_PRINT ("sem memoria");
// 		saida (1);
// 	}
// 	va_end (arg);
//
// 	for (i = 0; i < 3; i++) {
// 		status = connect_tcp(host, port, family, &sock);
// 		if (status < 0){
// 			INFO_PRINT("Nao conectou com %s:%s para a chamada %s", host, port, chamada);
// 			sleep (5);
// 		}
// 		else break;
// 	}
// 	if (status < 0)
// 		return strdup ("");
// 	if (asprintf(&url_ws,"POST %s HTTP/1.1\r\nContent-Type: text/xml\r\nSOAPAction:%s%s\r\ncharset: utf-8\r\nHost: %s\r\nUser-Agent: SimetBox\r\nConnection: close\r\nContent-Length: %zu\r\n\r\n%s", chamada, host, host, metodo, strlen(string), string) < 0) {
// 		INFO_PRINT("Nao conseguiu alocar buffer envio de webservice do host %s:%s para a chamada %s", host, port, chamada);
// 		saida (1);
// 	}
// 	result = chama_troca_msg_tcp (sock, host, port, url_ws, &size_recv);
// 	status = return_code (result, size_recv);
// 	content = parse_html_put (result, size, size_recv);
// 	free (result);
// 	result = content;
// 	close_tcp (sock);
//
//
// 	free (url_ws);
// 	if (!result) {
// 		free (chamada);
// 		return NULL;
// 	}
// 	else {
// 		INFO_PRINT ("saida webservice %s:\n[%s]\n", chamada, result);
// 	}
// 	free (chamada);
//
// 	return (result);
// }

char *recebe_resposta_ssl (int sock, int64_t *size_recv, SSL *ssl) {
	char *result = NULL, *aux = NULL, answer [8192], *vel;
	int tmpres = 0, tam = 0;
	struct timeval tv_timeo;
	struct timespec ts_ini, ts_fim;
	fd_set readfds;
	int rc, rc_sel = 0, le_mais, timeo = 0;
	uint64_t tempo_total;



	clock_gettime(CLOCK_MONOTONIC, &ts_ini);

	do {
		le_mais = 0;
		memset (answer, 0, sizeof (answer));
//				if (FD_ISSET(sock, &readfds)) {
		rc = SSL_read(ssl, answer, sizeof (answer) - 1);
		//INFO_PRINT ("rc SSL_read: %d SSL_pending: %d, %"PRI64"\n", rc, SSL_pending(ssl), tam);

		if (rc <= 0) {
			switch (SSL_get_error (ssl, rc)) {
				case SSL_ERROR_NONE:
					tmpres = 0;
					break;
				case SSL_ERROR_WANT_WRITE:
					INFO_PRINT ("READ WANT_WRITE\n");
					break;
				case SSL_ERROR_WANT_READ:
					FD_ZERO(&readfds);
					FD_SET(sock, &readfds);
					tv_timeo.tv_sec = TIMEOUT;
					tv_timeo.tv_usec = 0;
					if ((rc_sel = select(sock + 1, &readfds, NULL, NULL, &tv_timeo)) > 0)
						le_mais = 1;
					if (rc_sel <= 0)
						timeo = 1;
//					INFO_PRINT ("WANT_READ rc sel %d, le_mais %d, tam %"PRI64"\n", rc_sel, le_mais, tam);
					break;
				case SSL_ERROR_ZERO_RETURN:
					/* End of data */
					tmpres = 0;
					break;
				case SSL_ERROR_SYSCALL:
					tmpres = 0;
					break;
				default:
					ERROR_PRINT ("SSL read problem");
					break;
			}
		}
		else tmpres = rc;
		if (le_mais) {
			continue;
		}

		if (tmpres > 0) {
			aux = (char *) realloc (result, tam + tmpres + 1);
			if (!aux) {
				INFO_PRINT ("Impossivel alocar memoria para saida do Web Service. Abortando\n");
				if (result) {
					free (result);
					saida (1);
				}
			}
			result = aux;
			memcpy (result + tam, answer, tmpres);
			tam += tmpres;
			tmpres = 0;
			result[tam] = '\0';
			fprintf (stderr, "\rRecebidos %d bytes", tam);
			fflush(stderr);

		}
		else if (tmpres == 0)
			break;
	} while ((le_mais) || rc > 0);

	clock_gettime(CLOCK_MONOTONIC, &ts_fim);
	tempo_total = (TIMESPEC_NANOSECONDS(ts_fim) - TIMESPEC_NANOSECONDS(ts_ini))/1000;
	if (timeo == 0) {
		*size_recv = tam;
		vel = velocidade(tam, tempo_total);
		fprintf (stderr, "\rTOTAL recebido: %d bytes em %"PRUI64" us e %sbps", tam, tempo_total, vel);
		fflush(stderr);
		free (vel);
	}
	else {
		*size_recv = tam * -1;
		fprintf (stderr, "\rTOTAL recebido: %d bytes com TIMEOUT", tam);
		fflush(stderr);
	}
	if (!result) {
		*size_recv = 0;
		result = strdup ("");
	}
//	INFO_PRINT ("size2 %"PRI64"\n", tam);
	return result;
}

char *chama_troca_msg_tcp_ssl (int sock, char* host, char *port, char *chamada, int64_t *size_recv, SSL_CTX *ssl_ctx) {
	SSL * ssl;
	int len, rc;
	char *resposta;
	struct timeval tv_timeo;
	fd_set writefds;
	int rc_sel, escreve_mais;

	ssl = connect_ssl_bio(sock, ssl_ctx);
	if (!ssl) {
		ERROR_PRINT("nao conectou ao host %s:%s", host, port);
		if (size_recv)
			*size_recv = 0;
		return strdup ("");
	}

	len = strlen(chamada);
	do {
		escreve_mais = 0;
		rc = SSL_write(ssl, chamada, len);
		if (rc <= 0) {
			switch(SSL_get_error(ssl,rc)){
				case SSL_ERROR_NONE:
					if(rc != len)
						ERROR_PRINT("escrita incompleta ao enviar %s ao host %s:%s", chamada, host, port);
					break;
				case SSL_ERROR_WANT_WRITE:
//					INFO_PRINT ("SEND WANT_WRITE\n");
					FD_ZERO(&writefds);
					FD_SET(sock, &writefds);
					tv_timeo.tv_sec = TIMEOUT;
					tv_timeo.tv_usec = 0;
					if ((rc_sel = select(sock + 1, NULL, &writefds, NULL, &tv_timeo)) > 0)
						escreve_mais = 1;
					break;
				case SSL_ERROR_WANT_READ:
					INFO_PRINT ("SEND WANT_READ\n");
					break;
				case SSL_ERROR_ZERO_RETURN:
					/* End of data */
					break;
				case SSL_ERROR_SYSCALL:
					break;
				default:
					ERROR_PRINT("problema em SSL write ao enviar %s [%d bytes enviados do total %d bytes] ao host %s:%s", chamada, rc, len, host, port);
			}
		}
		else if (rc != len)
			ERROR_PRINT("problema em SSL write ao enviar %s [%d bytes enviados do total %d bytes] ao host %s:%s", chamada, rc, len, host, port);
	} while ((rc != len) && (escreve_mais));

	resposta = recebe_resposta_ssl (sock, size_recv, ssl);
	SSL_shutdown (ssl);
	close_tcp(sock);
	SSL_free (ssl);

	return resposta;
}

/*char *get_rand(int32_t num)
{
	int fd, dstlen;
	char *randchars, *dst, *rand_b64 = NULL;

	randchars = malloc ((sizeof (char) * num) + 1);
	if (!randchars) {
		INFO_PRINT ("nao conseguiu alocar caracteres aleatorios (1)\n");
		saida (1);
	}
	memset(randchars, 0, (sizeof (char) * num) + 1);
	dstlen = (sizeof (char) * num * 2) + 1;
	dst = malloc (dstlen);
	if (!dst) {
		INFO_PRINT ("nao conseguiu alocar caracteres aleatorios (2)\n");
		saida (1);
	}
	memset (dst, 0, dstlen);

	fd = open("/dev/urandom", O_RDONLY);
	if (fd != -1)
	{
		if (!read(fd, (void *)randchars, sizeof(char) * num)) {
			INFO_PRINT ("nao foi possivel ler /dev/urandom\n");
			saida (1);
		}
		close(fd);
	}
	base64_encode ((unsigned char*)dst, &dstlen, (unsigned char*)randchars, num);
	rand_b64 = converte_url (dst);
//    INFO_PRINT ("%d (%d) chars, %s\n", strlen (rand_b64), dstlen, rand_b64);
	free (randchars);
	free (dst);
	return rand_b64;
}
*/
/*void mysyslog (int priority, const char *format, ...)
{
	char *string_for_log;
	va_list arg;
	va_start (arg, format);
	vasprintf (&string_for_log, format, arg);
	va_end (arg);
	printf ("%s\n", string_for_log);
	syslog (priority, "pthread_self=%ld, %s", pthread_self(), string_for_log);
	free (string_for_log);
}
*/



char *chama_web_service_ssl_arg (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t *size, int redir, const char *format, ...) {
	char *url_ws, *result, *chamada, *site, *aux;
	int status, sock, i;
	int64_t size_recv;
	URL_PARTS *urlpartsp, urlparts;


	urlpartsp = &urlparts;

	if (redir == 5) {
		if (size)
			*size = 0;
		return strdup ("");
	}

	va_list arg;
	va_start (arg, format);
	if (vasprintf (&chamada, format, arg) < 0) {
		ERROR_PRINT ("sem memoria");
		saida (1);
	}
	va_end (arg);

	aux = converte_url2(chamada);
	free (chamada);
	chamada = aux;
#ifdef DEBUGMEM
	result = chama_web_service (ssl_ctx, host, PORT_HTTP, family, size, redir + 1, chamada);
	free (chamada);
	return result;
#endif

	for (i = 0; i < 3; i++) {
		status = connect_tcp(host, port, family, &sock);
		if (status < 0){
			INFO_PRINT("Nao conectou com %s:%s para a chamada %s", host, port, chamada);
			sleep (5);
		}
		else break;
	}
	if (status < 0)
		return strdup ("");
	url_ws = prepara_url (host, port, size, chamada);

	result = chama_troca_msg_tcp_ssl (sock, host, port, url_ws, &size_recv, ssl_ctx);
	free (url_ws);

	status = return_code(result, size_recv);
	if (status != 200) {
		INFO_PRINT ("host: %s, chamada: %s, cod %d, result :[%s]\n", host, chamada, status, result);
	}

	if ((status >= 301) && (status <= 307)) {
		site = encontra_redir (result);
		if (!site) {
			free (result);
			result = NULL;
			if( size)
				*size = 0;
			free (chamada);
			return strdup ("");
		}

		if ((site) && (strncmp (site, "/", 1) == 0)) {
			if (asprintf(&aux, "http%s://%s%s",strncmp(port, PORT_HTTP, 2)?"s":"", host, site) <= 0) {
				ERROR_PRINT ("erro alocacao memoria");
				saida (1);
			}
			ERROR_PRINT ("erro redir para %s do host %s. Redirecionando para %s", site, host, aux);
			free (site);
			site = aux;
		}
		free (result);
		separa_protocolo_host_chamada (urlpartsp, site);
		if (urlpartsp->https) {
			result = chama_web_service_ssl_arg (ssl_ctx, urlpartsp->host, PORT_HTTPS , family, size, redir + 1, urlpartsp->chamada);
		}
		else {
			result = chama_web_service (ssl_ctx, urlpartsp->host, PORT_HTTP, family, size, redir + 1, urlpartsp->chamada);
		}
		free (urlpartsp->host);
		free (site);
		free (chamada);
		return result;
	}

	if (status == 200) {
		parse_html (result, size, size_recv);
	}
	else {
		free (result);
		result = strdup ("");
	}
	INFO_PRINT ("saida webservice %s:\n[%s]\n", chamada, result);

	free (chamada);

	return (result);

}


char *chama_web_service_put_ssl (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t *size, char *string, const char *format, ...) {
	char *url_ws, *result, *content, *chamada;
	int status, sock, i;
	int64_t size_recv;



	va_list arg;
	va_start (arg, format);
	if (vasprintf (&chamada, format, arg) < 0) {
		ERROR_PRINT ("sem memoria");
		saida (1);
	}
	va_end (arg);
#ifdef DEBUGMEM
	result = chama_web_service_put (host, PORT_HTTP, family, size, string, "/%s/dns-measures", chamada);
	free (chamada);
	return result;
#endif

	for (i = 0; i < 3; i++) {
		status = connect_tcp(host, port, family, &sock);
		if (status < 0){
			INFO_PRINT("Nao conectou com %s:%s, %d para a chamada %s", host, port, family, chamada);
			sleep (5);
		}
		else break;
	}
	if (status < 0)
		return strdup ("");
	// mudar para Connection: keep-alive a fim de manter uma única conexão
	if (asprintf(&url_ws,"PUT %s HTTP/1.1\r\nContent-Type: application/json\r\nAccept: application/json\r\ncharset: utf-8\r\nHost: %s\r\nUser-Agent: SimetBox\r\nConnection: close\r\nContent-Length: %zu\r\n\r\n%s", chamada, host, strlen(string), string) < 0) {
		INFO_PRINT("Nao conseguiu alocar buffer envio de webservice do host %s:%s para a chamada %s", host, port, chamada);
		saida (1);
	}


	result = chama_troca_msg_tcp_ssl (sock, host, port, url_ws, &size_recv, ssl_ctx);
	content = parse_html_put (result, size, size_recv);
	free (result);
	result = content;
	free (url_ws);


	if (!result) {
		free (chamada);
		return NULL;
	}
	else {
		INFO_PRINT ("saida webservice %s:\n[%s]\n", chamada, result);
	}
	free (chamada);

	return (result);
}

////////////////// obtem interface gateway padrão
char* read_nl_sock (int sock, int seq_num, int pid, int *tam) {
	struct nlmsghdr *nl_header;
	int len = 0, msg_len = 0, res, flags;
	char temp_buffer [BUFSIZ], *buffer, *temp;
	struct timeval timeout_tv;
	fd_set fds;

	buffer = NULL;

	flags = fcntl (sock, F_GETFL, 0);
	fcntl (sock, F_SETFL,flags | O_NONBLOCK);
	do {
		timeout_tv.tv_sec = 1;
		timeout_tv.tv_usec = 0;

		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		res = select (FD_SETSIZE, &fds, NULL, NULL, &timeout_tv);
		if (res < 0)
	        return NULL;
		if (res == 0)
			break;
		if ((len = recv(sock, temp_buffer, BUFSIZ, 0)) < 0) {
			ERRNO_PRINT ("recv");
			return NULL;
		}

		temp = realloc (buffer, msg_len + len);
		if (!temp) {
			ERROR_PRINT ("mem");
			return NULL;
		}
		buffer = temp;
		memcpy (buffer + msg_len, temp_buffer, len);



		INFO_PRINT ("leitura de rotas: len %d\n", msg_len + len);

		nl_header = (struct nlmsghdr *)buffer;

		if ((NLMSG_OK(nl_header, msg_len + len) == 0) || (nl_header->nlmsg_type == NLMSG_ERROR)) {
			ERRNO_PRINT ("erro no pacote: %d\n", len);
			return NULL;
		}

		if (nl_header->nlmsg_type == NLMSG_DONE) {
			break;
		}
		else {
			msg_len += len;
		}

		if ((nl_header->nlmsg_flags & NLM_F_MULTI) == 0) {
			break;
		}
	}
	while ((nl_header->nlmsg_seq != seq_num) || (nl_header->nlmsg_pid != pid));
	*tam = msg_len;
	return buffer;
}

int parse_routes (struct nlmsghdr *nl_header, struct route_info *rt_info, struct iface_stdgw_st *iface_stdgw)
{
	struct rtmsg *rt_msg;
	struct rtattr *rt_attr;
	int rt_len, priority, metric;
	char buffer_ip [INET6_ADDRSTRLEN + 1] = {0};
	char ip_gw [INET6_ADDRSTRLEN + 1] = {0};
	char aux[200];

	rt_msg = (struct rtmsg *) NLMSG_DATA (nl_header);

	if (((rt_msg->rtm_family != AF_INET) && (rt_msg->rtm_family != AF_INET6)) || (rt_msg->rtm_table != RT_TABLE_MAIN))
		return -1;

	rt_attr = (struct rtattr *) RTM_RTA (rt_msg);
	rt_len = RTM_PAYLOAD(nl_header);

	for (; RTA_OK (rt_attr, rt_len); rt_attr = RTA_NEXT (rt_attr, rt_len))
	{
		switch (rt_attr->rta_type)
		{
			case RTA_UNSPEC:
				DEBUG_PRINT ("RTA_UNSPEC");
				break;
			case RTA_IIF:
				if_indextoname (*(int *) RTA_DATA (rt_attr), aux);
				DEBUG_PRINT ("RTA_IIF, %s", aux);
				break;
			case RTA_OIF:
				if_indextoname (*(int *) RTA_DATA (rt_attr), rt_info->ifname);
				DEBUG_PRINT ("RTA_OIF, %s", rt_info->ifname);
				break;
			case RTA_DST:
				memcpy (&rt_info->dst, RTA_DATA(rt_attr), sizeof(rt_info->dst));
				inet_ntop (rt_msg->rtm_family, &rt_info->dst, buffer_ip, sizeof (buffer_ip));
				DEBUG_PRINT ("RTA_DST %s", buffer_ip);
				break;
			case RTA_GATEWAY:
				memcpy (&rt_info->ip_gw, RTA_DATA(rt_attr), sizeof(rt_info->ip_gw));
				inet_ntop (rt_msg->rtm_family, &rt_info->ip_gw, ip_gw, sizeof (ip_gw));
				inet_ntop (rt_msg->rtm_family, &rt_info->dst, buffer_ip, sizeof (buffer_ip));
				DEBUG_PRINT ("RTA_GATEWAY ipgw %s, buffer_ip %s, RTN_UNICAST %d, RTA_GATEWAY %d", ip_gw, buffer_ip, rt_msg->rtm_type & RTN_UNICAST, rt_msg->rtm_type & RTA_GATEWAY);
				break;
			case RTA_PREFSRC:
				memcpy (&rt_info->prefsrc, RTA_DATA(rt_attr), sizeof(rt_info->prefsrc));
				inet_ntop (rt_msg->rtm_family, &rt_info->prefsrc, aux, sizeof (aux));
				DEBUG_PRINT("RTA_PREFSRC %s", aux);
				break;
			case RTA_SRC:
				DEBUG_PRINT("RTA_SRC");
				memcpy (&rt_info->src, RTA_DATA(rt_attr), sizeof(rt_info->src));
				inet_ntop (rt_msg->rtm_family, &rt_info->src, aux, sizeof (aux));
				DEBUG_PRINT("RTA_SRC %s", aux);
				break;
			case RTA_PRIORITY:
				memcpy (&priority, RTA_DATA(rt_attr), sizeof(rt_attr->rta_len));
				DEBUG_PRINT ("RTA_PRIORITY, %d", priority);
				break;
			case RTA_METRICS:
				memcpy (&metric, RTA_DATA(rt_attr), sizeof(rt_attr->rta_len));
				DEBUG_PRINT ("RTA_METRICS, %d", metric);
				break;
			case RTA_MULTIPATH:
				DEBUG_PRINT ("RTA_MULTIPATH\n");
				break;
			case RTA_PROTOINFO:
				DEBUG_PRINT ("RTA_PROTOINFO");
				break;
			case RTA_FLOW:
				DEBUG_PRINT ("RTA_FLOW");
				break;
			case RTA_CACHEINFO:
				DEBUG_PRINT ("RTA_CACHEINFO");
				break;
			case RTA_TABLE:
				DEBUG_PRINT ("RTA_TABLE");
				priority = 0;
				metric = 0;
				memset(buffer_ip, 0, sizeof(buffer_ip));
				memset(ip_gw, 0, sizeof(ip_gw));
				break;
#ifdef RTA_PREF
			case RTA_PREF:
				DEBUG_PRINT ("RTA_PREF");
				break;
#endif
			default:
				DEBUG_PRINT ("rt_attr->rta_type desconhecido: %d", rt_attr->rta_type);
		}
	}

//	inet_ntop (rt_msg->rtm_family, &rt_info->dst, buffer_ip, sizeof (buffer_ip));
//	inet_ntop (rt_msg->rtm_family, &rt_info->ip_gw, ip_gw, sizeof (ip_gw));
	switch (rt_msg->rtm_family) {
		case AF_INET:
			if ((strnlen(iface_stdgw->if_ipv4, 1) == 0) && (!strncmp (buffer_ip, "0.0.0.0", 7))) {
				strncpy(iface_stdgw->if_ipv4, rt_info->ifname, IF_NAMESIZE);
				strncpy(iface_stdgw->ipv4_std_gw, ip_gw, INET_ADDRSTRLEN);
				INFO_PRINT("gateway padrao ipv4 %s, ip %s, RTN_UNICAST %d RTA_GATEWAY %d\n", iface_stdgw->if_ipv4, ip_gw, rt_msg->rtm_type & RTN_UNICAST, rt_msg->rtm_type & RTA_GATEWAY);
			}
		break;
		case AF_INET6:
			if ((strnlen (iface_stdgw->if_ipv6, 1) == 0) && (!strncmp (buffer_ip, "::", 2)) && (strncmp (rt_info->ifname, "lo", IF_NAMESIZE))) {
				strncpy (iface_stdgw->if_ipv6, rt_info->ifname, IF_NAMESIZE);
				strncpy (iface_stdgw->ipv6_std_gw, ip_gw, INET6_ADDRSTRLEN);
				INFO_PRINT("gateway padrao ipv6 %s RTN_UNICAST %d RTA_GATEWAY %d, ip gw %s, buffer_ip %s\n", iface_stdgw->if_ipv6, rt_msg->rtm_type & RTN_UNICAST, rt_msg->rtm_type & RTA_GATEWAY, ip_gw, buffer_ip);
			}
		break;
	}
	return 0;
}







int get_iface_stdgw (struct iface_stdgw_st *iface_stdgw)
{
	struct nlmsghdr *nl_msg;
	struct route_info rt_info;
	char buffer [BUFSIZE_ROTAS] = {0}, *recv_buffer;
	char ifname [IF_NAMESIZE];
	int sock, len, msg_seq = 0;

	if ((sock = socket (PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) {
		ERRNO_PRINT ("socket");
		return -1;
	}

	nl_msg = (struct nlmsghdr *)buffer;

	nl_msg->nlmsg_len = NLMSG_LENGTH (sizeof (struct rtmsg));
	nl_msg->nlmsg_type = RTM_GETROUTE;

	nl_msg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	nl_msg->nlmsg_seq = msg_seq++;
	nl_msg->nlmsg_pid = getpid();

	if (send (sock, nl_msg, nl_msg->nlmsg_len, 0) < 0) {
		ERROR_PRINT ("send");
		close (sock);
		return -1;
	}

	if ((recv_buffer = read_nl_sock (sock, msg_seq, getpid(), &len)) < 0) {
		ERROR_PRINT ("read");
		close (sock);
		return -1;
	}
	close (sock);
	if (len == 0) {
		ERROR_PRINT ("tam tabela de rotas %d\n", len);
		return -1;
	}
	nl_msg = (struct nlmsghdr *)recv_buffer;
	for (; NLMSG_OK (nl_msg,len); nl_msg = NLMSG_NEXT (nl_msg,len)){
		memset (&rt_info, 0, sizeof (struct route_info));
		rt_info.ifname = ifname;
		parse_routes (nl_msg, &rt_info, iface_stdgw);
	}
	INFO_PRINT ("Interfaces com gateway padrao: IPv4: %s e IPv6: %s\n", (strnlen(iface_stdgw->if_ipv4, 1))?iface_stdgw->if_ipv4:"nenhuma", (strnlen(iface_stdgw->if_ipv6, 1))?iface_stdgw->if_ipv6:"nenhuma");
	free (recv_buffer);
	return 0;
}
////////////////// obtem interface gateway padrão


//obtem ips interface

void mascara_n_bits (struct sockaddr *mask, int num_bits) {
	int i;
	uint32_t temp[4];
	memset (&temp, 0, sizeof(struct sockaddr));
	for (i = 0; i <= (num_bits - 1)/32; i++)
		temp [i] = (~0) << (31 - MIN(31, num_bits - 1 - i * 32));
	memcpy (mask, &temp, sizeof(struct sockaddr));
}
void ip_and_mask (struct sockaddr *ip, struct sockaddr *mask, int size) {
	int i;
	uint32_t a[4], b[4];
	memcpy (&a, ip, size);
	memcpy (&b, mask, size);
	for (i = 0; i < size/4; i++)
		a[i] = a[i] & b[i];
	memcpy (ip, &a, size);
}

int checa_lista_bogons (struct in6_addr *ipv6, ipv6_bogon *ranges, int num) {
	struct in6_addr mask, masked_ip, ip_prefix;
	char buf [INET6_ADDRSTRLEN];
	int i, prefix = -1;

	prefix = -1;
	for (i = 0; i < num; i++) {
		if (prefix != ranges[i].prefix) {
			prefix = ranges[i].prefix;

			memcpy (&masked_ip, ipv6, sizeof(struct in6_addr));
			inet_ntop (AF_INET6, &masked_ip, buf, INET6_ADDRSTRLEN);
			mascara_n_bits ((struct sockaddr*) &mask, prefix);
			//for (j = 0; j < 4; j++)
			//	masked_ip.s6_addr32[j] = be32toh(masked_ip.s6_addr32[j]);
			be128toh ((uint32_t *)&(masked_ip.s6_addr32));
			ip_and_mask ((struct sockaddr *) &masked_ip, (struct sockaddr *) &mask, sizeof (struct in6_addr));
			//for (j = 0; j < 4; j++)
			//	masked_ip.s6_addr32[j] = htobe32(masked_ip.s6_addr32[j]);
			htobe128 ((uint32_t *)&(masked_ip.s6_addr32));
		}
		inet_pton(AF_INET6, ranges[i].ip, &ip_prefix);
		if (memcmp (&ip_prefix, &masked_ip, MIN (sizeof(struct in6_addr), prefix / 8 + 1)) == 0) {
			return i;
		}

	}
	return -1;
}
int checa_ipv6_bogons (struct in6_addr *ipv6) {
	int rc;
	// http://www.team-cymru.org/ipv6-router-reference.html
/*
	ipv6 prefix-list ipv6-global-route permit 2001:0000::/32
	ipv6 prefix-list ipv6-global-route permit 2001:0200::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:0400::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:0600::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:0800::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:0A00::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:0C00::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:0E00::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:1200::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:1400::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:1600::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:1800::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:1A00::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:1C00::/22 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:2000::/20 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:3000::/21 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:3800::/22 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:4000::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:4200::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:4400::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:4600::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:4800::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:4A00::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:4C00::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:5000::/20 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:8000::/19 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:A000::/20 le 64
	ipv6 prefix-list ipv6-global-route permit 2001:B000::/20 le 64
	ipv6 prefix-list ipv6-global-route permit 2002:0000::/16 le 64
	ipv6 prefix-list ipv6-global-route permit 2003:0000::/18 le 64
	ipv6 prefix-list ipv6-global-route permit 2400:0000::/12 le 64
	ipv6 prefix-list ipv6-global-route permit 2600:0000::/12 le 64
	ipv6 prefix-list ipv6-global-route permit 2610:0000::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2620:0000::/23 le 64
	ipv6 prefix-list ipv6-global-route permit 2800:0000::/12 le 64
	ipv6 prefix-list ipv6-global-route permit 2A00:0000::/12 le 64
	ipv6 prefix-list ipv6-global-route permit 2C00:0000::/12 le 64

	salvar esta lista em /tmp/bogon e rodar
		cat /tmp/bogon |awk '{print $5}'|sed 's/\//", /g'|awk '{print "\x7b\x22"$0"\x7d,"}'
	para obter a inicialização dos permitidos e não esquecer de incluir o "fe80::/10 antes dos demais.
*/
/*
	ipv6_bogon denied_ranges [NUM_BOGONS_DENY] = {
		{"2001:0db8::", 32} // bloco para documentação
	};
	ipv6_bogon accepted_ranges [NUM_BOGONS_ACCEPT] = {
		{"fe80::", 10}, // rede local
		{"2400:0000::", 12},
		{"2600:0000::", 12},
		{"2800:0000::", 12},
		{"2A00:0000::", 12},
		{"2C00:0000::", 12},
		{"2002:0000::", 16}, // 6to4
		{"2003:0000::", 18},
		{"2001:8000::", 19},
		{"2001:2000::", 20},
		{"2001:5000::", 20},
		{"2001:A000::", 20},
		{"2001:B000::", 20},
		{"2001:3000::", 21},
		{"2001:1C00::", 22},
		{"2001:3800::", 22},
		{"2001:0200::", 23},
		{"2001:0400::", 23},
		{"2001:0600::", 23},
		{"2001:0800::", 23},
		{"2001:0A00::", 23},
		{"2001:0C00::", 23},
		{"2001:0E00::", 23},
		{"2001:1200::", 23},
		{"2001:1400::", 23},
		{"2001:1600::", 23},
		{"2001:1800::", 23},
		{"2001:1A00::", 23},
		{"2001:4000::", 23},
		{"2001:4200::", 23},
		{"2001:4400::", 23},
		{"2001:4600::", 23},
		{"2001:4800::", 23},
		{"2001:4A00::", 23},
		{"2001:4C00::", 23},
		{"2610:0000::", 23},
		{"2620:0000::", 23},
		{"2001:0000::", 32} // teredo
	};
*/

	ipv6_bogon denied_ranges [NUM_BOGONS_DENY] = {
		{"fe80::", 10, IANA, RESERVED},
		{"fec0::", 10, IETF, RESERVED},
		{"3ffe::", 16, IANA, RESERVED},
		{"2001:0db8::", 32, IANA, RESERVED} // bloco para documentação
	};
	ipv6_bogon ranges [NUM_BOGONS] = {
		{"fc00::", 7, IANA, UNIQUE_LOCAL},
		{"2400:0000::", 12, APNIC, ALLOCATED},
		{"2600:0000::", 12, ARIN, ALLOCATED},
		{"2800:0000::", 12, LACNIC, ALLOCATED},
		{"2a00:0000::", 12, RIPE_NCC, ALLOCATED},
		{"2c00:0000::", 12, AFRINIC, ALLOCATED},
		{"2002:0000::", 16, SIX_TO_FOUR, ALLOCATED},
		{"2003:0000::", 18, RIPE_NCC, ALLOCATED},
		{"2001:8000::", 19, APNIC, ALLOCATED},
		{"2001:2000::", 20, RIPE_NCC, ALLOCATED},
		{"2001:5000::", 20, RIPE_NCC, ALLOCATED},
		{"2001:a000::", 20, APNIC, ALLOCATED},
		{"2001:b000::", 20, APNIC, ALLOCATED},
		{"2001:3000::", 21, RIPE_NCC, ALLOCATED},
		{"2001:1c00::", 22, RIPE_NCC, ALLOCATED},
		{"2001:3800::", 22, RIPE_NCC, ALLOCATED},
		{"2001:0000::", 23, IANA, ALLOCATED},
		{"2001:0200::", 23, APNIC, ALLOCATED},
		{"2001:0400::", 23, ARIN, ALLOCATED},
		{"2001:0600::", 23, RIPE_NCC, ALLOCATED},
		{"2001:0800::", 23, RIPE_NCC, ALLOCATED},
		{"2001:0a00::", 23, RIPE_NCC, ALLOCATED},
		{"2001:0c00::", 23, APNIC, ALLOCATED},
		{"2001:0e00::", 23, APNIC, ALLOCATED},
		{"2001:1200::", 23, LACNIC, ALLOCATED},
		{"2001:1400::", 23, RIPE_NCC, ALLOCATED},
		{"2001:1600::", 23, RIPE_NCC, ALLOCATED},
		{"2001:1800::", 23, ARIN, ALLOCATED},
		{"2001:1a00::", 23, RIPE_NCC, ALLOCATED},
		{"2001:4000::", 23, RIPE_NCC, ALLOCATED},
		{"2001:4200::", 23, AFRINIC, ALLOCATED},
		{"2001:4400::", 23, APNIC, ALLOCATED},
		{"2001:4600::", 23, RIPE_NCC, ALLOCATED},
		{"2001:4800::", 23, ARIN, ALLOCATED},
		{"2001:4a00::", 23, RIPE_NCC, ALLOCATED},
		{"2001:4c00::", 23, RIPE_NCC, ALLOCATED},
		{"2610:0000::", 23, ARIN, ALLOCATED},
		{"2620:0000::", 23, ARIN, ALLOCATED}
	};


	rc = checa_lista_bogons (ipv6, denied_ranges, NUM_BOGONS_DENY);
	if (rc != -1) {
		INFO_PRINT ("prefixo IPv6 %s/%d irregular\n", denied_ranges[rc].ip, denied_ranges[rc].prefix);
		return (rc * -1) - 1;
	}

	rc = checa_lista_bogons (ipv6, ranges, NUM_BOGONS);
	if (rc != -1) {
		INFO_PRINT ("prefixo IPv6 %s/%d regular\n", ranges[rc].ip, ranges[rc].prefix);
		return rc;
	}

	INFO_PRINT ("prefixo IPv6 desconhecido\n");

	return 1;
}

int get_iface_ip (void *addr, void *net, char *iface, int family, int pode_tunel) {
	struct ifaddrs *ifaddr, *ifa;


	if (getifaddrs(&ifaddr) == -1) {
		ERRNO_PRINT("getifaddrs");
		return 1;
	}

	/* Walk through linked list, maintaining head pointer so we
	 c*an free list later */

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if ((!ifa->ifa_addr) || (ifa->ifa_addr->sa_family != family) ||(strcmp (ifa->ifa_name, iface)) || !(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_RUNNING) || (!pode_tunel && ifa->ifa_flags & IFF_POINTOPOINT))
			continue;
		if (family == AF_INET) {
			memcpy (addr, &((struct sockaddr_in*) ifa->ifa_addr)->sin_addr, sizeof (struct in_addr));
			memcpy (net, &((struct sockaddr_in*) ifa->ifa_netmask)->sin_addr, sizeof (struct in_addr));
		}
		else if (family == AF_INET6) {
			memcpy (addr, &((struct sockaddr_in6*) ifa->ifa_addr)->sin6_addr, sizeof (struct in6_addr));
			memcpy (net, &((struct sockaddr_in6*) ifa->ifa_netmask)->sin6_addr, sizeof (struct in6_addr));

		}
		freeifaddrs(ifaddr);

		return 0;
	}

	freeifaddrs(ifaddr);

	return 1;
}
void bin(struct sockaddr *ip, size_t size) {
	// imprime bits de IPv4 ou IPv6 (informar em size)
	uint32_t bits, i, j, k;
	uint32_t n[4]; //ipv4 ou ipv6

	memcpy(&n, ip, size);

	bits = 31;
	k = (size == sizeof(struct in_addr)) ? 1 : 4; // ipv4 ou ipv6
	for (j = 0; j < k; j++)
		for (i = 1 << bits; i > 0; i = i >> 1)
			(n[j] & i) ? fprintf(stderr, "1"): fprintf(stderr, "0");
}

void protocolos_disponiveis (proto_disponiveis *disp, int family, int pode_tunel, CONFIG_SIMET* config, SSL_CTX * ssl_ctx) {
	struct in_addr addr_v4;
	memset (disp, 0, sizeof (proto_disponiveis));
	memset (&addr_v4, 0, sizeof (struct in_addr));
	int rc, sem_ipv4_ou_rede_local = 0;
	char *ip_cliente;

	if (get_iface_stdgw (&disp->iface_stdgw)) {
		// erro exit
		INFO_PRINT ("Impossivel obter gateways padrao");
		disp->proto [PROTO_IPV4] = 1;
	}
	else {
		// ipv4
		if ((strnlen(disp->iface_stdgw.if_ipv4, 1) != 0) && (get_iface_ip (&disp->addr_v4, &disp->netmask_v4, disp->iface_stdgw.if_ipv4, AF_INET, pode_tunel) == 0) && ((family == 4) || (family == 0)))
			disp->proto [PROTO_IPV4] = 1;
		else
			INFO_PRINT ("Impossivel realizar teste em IPv4");
		//ipv6
		if ((strnlen(disp->iface_stdgw.if_ipv6, 1) != 0) && (get_iface_ip (&disp->addr_v6, &disp->netmask_v6, disp->iface_stdgw.if_ipv6, AF_INET6, pode_tunel) == 0)) {
			rc = checa_ipv6_bogons (&disp->addr_v6);
			if (((family == 6) || (family == 0)) && (rc != -1)) {
				disp->proto [PROTO_IPV6] = 1;
				if (rc == 0) {
					// rede local
					disp->rede_local[PROTO_IPV6] = 1;
					ip_cliente = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, 6, NULL, 0, "/%s/IpClienteServlet", config->context_root);
					if ((!ip_cliente) || (strnlen(ip_cliente, 1) == 0)) {
						ERROR_PRINT ("Nao foi possivel obter o ip do cliente. Abortando\n");
						saida (1);
					}
					INFO_PRINT ("IPv6 publico: %s", ip_cliente);
					if (converte_str_ip (ip_cliente, (void*)&disp->addr_v6_pub) != 0)
						saida(1);
					free (ip_cliente);
				}
				else
					// ip público
					memcpy (&disp->addr_v6_pub, &disp->addr_v6, sizeof (struct in6_addr));
			}
		}
		if (!disp->proto [PROTO_IPV6])
			INFO_PRINT ("Impossivel realizar teste em IPv6");

	}
	if (disp->proto [PROTO_IPV4] == 1) {
		if (memcmp (&disp->addr_v4, &addr_v4, sizeof (disp->addr_v4)) == 0)
			sem_ipv4_ou_rede_local = 1;
		else if (is_rede_local (&disp->addr_v4)) {
				sem_ipv4_ou_rede_local = 1;
				disp->rede_local [PROTO_IPV4] = 1;
		}
		if (sem_ipv4_ou_rede_local) {
			// pegar webservice v4
			ip_cliente = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, 4, NULL, 0, "/%s/IpClienteServlet", config->context_root);
			INFO_PRINT ("IPv4 publico: %s", ip_cliente);
			if ((!ip_cliente) || (strnlen(ip_cliente, 1) == 0)) {
				ERROR_PRINT ("Nao foi possivel obter o ip do cliente. Abortando\n");
				saida (1);
			}
			// converter pra num_bits
			if (inet_pton(AF_INET, ip_cliente, &disp->addr_v4_pub) <= 0) {
				ERRNO_PRINT ("inet_pton");
				saida(1);
			}
			free (ip_cliente);
		}
		else {
			memcpy (&disp->addr_v4_pub, &disp->addr_v4, sizeof (struct in_addr));
		}
	}
	INFO_PRINT ("Protocolos disponiveis: IPv4 %s e faz teste: %s e IPv6 %s e faz teste: %s\n", disp->proto [PROTO_IPV4]?(disp->rede_local[PROTO_IPV4]?"local":"publico"):"Nao", disp->proto [PROTO_IPV4]?"Sim":"Nao", disp->proto [PROTO_IPV6]?(disp->rede_local[PROTO_IPV6]?"local":"publico"):"Nao", disp->proto [PROTO_IPV6]?"Sim":"Nao");



}

int trace_route (char *ip, char *nome, char *hash_measure, char *ip_origem, int family) {
	char *chamada, *ret_func;
	int rc;

	if (ip_origem)
		rc = asprintf (&chamada, "/usr/bin/simet_traceroute.sh -%d -m %s -n %s -o %s %s", family, hash_measure, nome, ip_origem, ip);
	else
		rc = asprintf (&chamada, "/usr/bin/simet_traceroute.sh -%d -m %s -n %s %s", family, hash_measure, nome, ip);
	if (rc < 0) {
		ERROR_PRINT ("nao alocou chamada para traceroute\n");
		return -1;
	}

	ret_func = chama_pipe(chamada, "falha ao executar simet_traceroute.sh\n");

	if (ret_func) {
		free (ret_func);
	}
	free (chamada);
	return 0;
}

int is_rede_local (struct in_addr *meu_addr) {
	struct in_addr ip_min_v4, ip_max_v4, meu_ip_v4;
	int i;
	const char *min_rede[] = {"10.0.0.0",
							"100.64.0.0",
							"172.16.0.0",
							"192.168.0.0"};
	const char *max_rede[] = {"10.255.255.255",
							"100.127.255.255",
							"172.31.255.255",
							"192.168.255.255"};

	memcpy (&meu_ip_v4, meu_addr, sizeof (meu_ip_v4));
	for (i = 0; i < 4; i++) {
		inet_pton(AF_INET, min_rede[i], &ip_min_v4);
		inet_pton(AF_INET, max_rede[i], &ip_max_v4);
		if ((be32toh(meu_ip_v4.s_addr) >= be32toh(ip_min_v4.s_addr)) && (be32toh(meu_ip_v4.s_addr) <= be32toh(ip_max_v4.s_addr)))
			return 1;
	}
	return 0;
}
/*
# Disable transit for some prefixes [RFC6890]
# "This host on this network" [RFC1122] : 0.0.0.0/8
# Private-Use                 [RFC1918] : 10.0.0.0/8
# Shared Address Space        [RFC6598] : 100.64.0.0/10
# Link Local                  [RFC3927] : 169.254.0.0/16
# Private-Use                 [RFC1918] : 172.16.0.0/12
# DS-Lite                     [RFC6333] : 192.0.0.0/29
# Documentation (TEST-NET-1)  [RFC5737] : 192.0.2.0/24
# 6to4 Relay Anycast          [RFC3068] : 192.88.99.0/24
# Private-Use                 [RFC1918] : 192.168.0.0/16
# Benchmarking                [RFC2544] : 198.18.0.0/15
# Documentation (TEST-NET-2)  [RFC5737] : 198.51.100.0/24
# Documentation (TEST-NET-3)  [RFC5737] : 203.0.113.0/24
# Reserved                    [RFC1112] : 240.0.0.0/4
*/
