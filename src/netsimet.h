/**
 * @file   netsimet.h
 * @brief  Responsible for creating the connections with the simet servers.
 * @author  Rafael de O. Lopes Goncalves
 * @date 26 de outubro de 2011
 */

//#include <sys/socket.h>

#ifndef NETSIMET_H
#define NETSIMET_H
#include <openssl/ssl.h>
#include <net/if.h>

#define MAX_BUFFER BUFSIZ
#define RTT_TEST_PACK_NUMBERS_SIZE 3
#define RTT_TEST_PACKET_BYTES 24
#define TIMEOUT 30
#define SHORT_TIMEOUT 10
#define ATE_DESCONEXAO 0
#define PARA_EM_BARRA_R_BARRA_N 1



typedef struct {
	int8_t rcode;
	int64_t tempo_transferencia;
	int64_t tempo_conexao;
	int64_t tempo_total;
	int64_t size;
	uint8_t redirs;
	uint16_t last_rc;
} site_download;



#define BUFSIZE_ROTAS 8192

struct iface_stdgw_st {
	char if_ipv4[IFNAMSIZ];
	char ipv4_std_gw[INET_ADDRSTRLEN + 1];
	char if_ipv6[IFNAMSIZ];
	char ipv6_std_gw[INET6_ADDRSTRLEN + 1];
};

struct route_info
{
	unsigned char dst[sizeof(struct in6_addr)];
	unsigned char src[sizeof(struct in6_addr)];
	unsigned char prefsrc[sizeof(struct in6_addr)];
	unsigned char ip_gw[sizeof(struct in6_addr)];
	char *ifname;
};
#define NUM_BOGONS 38
#define NUM_BOGONS_DENY 4

typedef enum {
	ALLOCATED,
	RESERVED,
	UNIQUE_LOCAL
} status_bogon;

typedef enum {
	IANA,
	APNIC,
	ARIN,
	LACNIC,
	RIPE_NCC,
	AFRINIC,
	SIX_TO_FOUR,
	IETF
} rir_ou_tec;

typedef struct {
	char *ip;
	uint8_t prefix;
	rir_ou_tec designation;
	status_bogon status;
} ipv6_bogon;


/* ######################################################################## */
/* ######################################################################## */
/**
 * @brief Send a message to dest via tcp.
 *
 * @param dest Destination host address to be resolved by getaddrinfo_a
 * @param port destination port
 * @param message the message to be sended
 * @param result[out] response string, will be dynamically allocated.
 *
 * Please free(result) after its usage.
 *
 * @return  negative on error, size of result otherwise.
 */
int send_rec_tcp_then_close(const char *dest, const char *port, int family, const char *message, char **result);

/**
 * Opens the socket as specified by addr.
 *
 * @param addr address to connect.
 * @param sock  result socket.
 *
 * @return <0 if has had an error.
*/
int connect_socket(struct addrinfo *addr, int *sock);

/**
 * @brief Send to the socket the message with lenght len
 *
 * @param message buffer to be sended.
 * @param len lenght.
 */
int send_tcp(int socket_fd, char *message, int64_t len);

/**
 * Get the local ip for the socket
 * @param socketfd
 * @return a malloc'd string
 */
char *get_local_ip(int socketfd);


/**
 * Get the remote  ip for the connected socket
 * @param socketfd
 * @return a malloc'd string
 */
char * get_remote_ip(int socketfd);

/**
 * For the given socket gets its local tcp or udp port
 * @param socket a connected socket.local: Av Prof Alfonso Bovero, 918/952
 */
int get_local_port(int socket);

int connect_tcp(const char *host, const char *port, int family, int *sock);

int close_tcp(int sock);

/*Udp*/
int connect_udp(const char *host, const char *port, int family, int *sock);

int send_udp(int socket_udp, int64_t counter, int64_t timestamp, int64_t extra);

/**
 * Sends a rtp package through udp.
 */
int send_rtp(int socket_udp, uint16_t counter, int32_t timestamp);

/**
 * @brief Recieve a package from the socket_udp
 * The numbers resulting of the first 24 bytes are  decodeded to this client host bytes order (big endian to host)
 *
 *
 * @param socket_udp the socket to receive the data
 * @param counter the first 8 bytes
 * @param timestamp the 8-16th bytes
 * @param extra bytes.
 *
 * @return  The total number of bytes in this recv
 */
int recv_udp(int socket_udp, uint64_t * counter, uint64_t * timestamp, uint64_t * extra);
int recv_udp2(int socket_udp, uint64_t * counter, uint64_t * timestamp, uint64_t * extra);



/**
 * Connects to an already connected socket creating a ssl
 * connection.
 */
SSL * connect_ssl(int socket_fd, SSL_CTX* ctx);
SSL * connect_ssl_bio(int socket_fd, SSL_CTX* ctx);

struct rtp_packet {
    uint16_t dummy1;
    uint16_t sequence_number;
    int32_t timestamp;
    int32_t src;
    int32_t dummy2;
    int64_t dummy3[RTT_TEST_PACK_NUMBERS_SIZE - 2];
};


union packetudp_u {
    uint64_t numbers[RTT_TEST_PACK_NUMBERS_SIZE];
    uint8_t buffer[RTT_TEST_PACKET_BYTES];
    struct rtp_packet rtp;
};

typedef enum {
	PROTO_IPV4 = 0,
	PROTO_IPV6
} PROTO_DISPONIVEL;

typedef struct {
	uint8_t proto [2];
	uint8_t rede_local[2];
	struct in_addr addr_v4;
	struct in6_addr addr_v6;
	struct in_addr addr_v4_pub;
	struct in6_addr addr_v6_pub;
	struct in_addr netmask_v4;
	struct in6_addr netmask_v6;
	struct iface_stdgw_st iface_stdgw;
} proto_disponiveis;

int convert_family_type(int family);
int ip_type_family(char *ip);
char* get_ip_by_host(const char* host, int family);
void fill_socket_addr(struct sockaddr_storage*, const char* host, int port, int family);


void decode_pac(union packetudp_u pac, uint64_t * counter, uint64_t * timestamp, uint64_t * extra);


ssize_t recv_timeout(int sockfd, char *buffer, uint8_t buffer_size, uint8_t timeout);

char * get_local_macaddress();

char *recebe_resposta (int sock, int64_t *size_recv, uint8_t para_em_barra_r_barra_n);

char *chama_web_service (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t *tam, int redir, const char *format, ...);
char *chama_web_service_ssl_arg (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t *tam, int redir, const char *format, ...);
char *chama_troca_msg_tcp_barra_r_barra_n (int sock, char* host, char *port, char *chamada, int64_t* size_recv);
char *chama_troca_msg_tcp (int sock, char* host, char *port, char *chamada, int64_t* size_recv);
char *chama_web_service_put_ssl (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t *tam, char *string, const char *format, ...);
char *chama_web_service_put (char* host, char *port, int family, int64_t *tam, char *string, const char *format, ...);
site_download *tempo_download_pagina_ssl (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t *size, int retries, char *chamada);
site_download *tempo_download_pagina (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t *size, int retries, char *chamada);
char *prepara_url (char* host, char *port, int64_t *size, char *chamada);
char *chama_troca_msg_tcp_ssl (int sock, char* host, char *port, char *chamada, int64_t *size_recv, SSL_CTX *ssl_ctx);
#endif
int return_code (char *entrada, int size);
char *encontra_redir (char* entrada);
SSL_CTX *init_ssl_context(int exibe_erros_ssl);
void protocolos_disponiveis (proto_disponiveis *disp, int family, int pode_tunel, CONFIG_SIMET* config, SSL_CTX * ssl_ctx);
void chama_web_service_arq (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t* size, int redir, char *chamada, char *arquivo);
int trace_route (char *ip, char *nome, char *hash_measure, char *ip_origem, int family);
int is_rede_local (struct in_addr *meu_addr);
int converte_str_ip (const char *host, void *addr);
char *get_ip(void *sa);
int checa_ipv6_bogons (struct in6_addr *ipv6);
