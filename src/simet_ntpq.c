#define _GNU_SOURCE
#include "config.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "utils.h"
#include "unp.h"
#include "debug.h"
#include "byteconversion.h"
#include "simet_ntpq.h"
#include "simet_tools.h"
#include "gopt.h"
#include "netsimet.h"

#define BUFLEN 12
#define BUFLEN_REC 512
#define NTPQ_PORT "123"


int select_recv_tam (int sockfd, char *buf, int tam) {
	int rc, num;
	struct timeval tv_timeo;
	fd_set rset;

	FD_ZERO (&rset);
	FD_SET (sockfd, &rset);

	tv_timeo.tv_sec = TIMEOUT;
	tv_timeo.tv_usec = 0; // select zera o timeout
	num = select (sockfd + 1, &rset, NULL, NULL, &tv_timeo);
	if (!num) {
		return -1;
	}
	rc = 0;
	if (FD_ISSET(sockfd, &rset))
		rc = recv(sockfd, buf, tam, 0);
//	hexdump (buf, rc);

	return rc;

}

int main_ntpq (const char *nome, char* mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX* ctx, void *options) {
	int sockfd;
	char buf[BUFLEN];
	char buf_resp[BUFLEN_REC];
	ntpq_response resp;
	int family = 4;
	int rc;
	int flags = 1, tam, tam_anterior = 0, tem_dados = 0;
	char* char_resp = NULL, *aux = NULL, *ws = NULL, *ret_ws = NULL, *dst;
	char *key, *value, *pointer;


	char doc[] = "uso:\n\tsimet_ntpq <NTP Server>\n\tDefault localhost";

	if (argc == 1) {
		INFO_PRINT("%s", doc);
		dst = strdup("localhost");
	}
	else dst = strdup(argv[1]);

	if (options)
		parse_args_basico(options, doc, &family);

	INFO_PRINT ("Cria Socket");
	if ((sockfd = connect_udp(dst, NTPQ_PORT, family, &sockfd)) ==-1)
		ERROR_PRINT("socket");

	free (dst);
	if (fcntl (sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
		ERRNO_PRINT("nao conseguiu setar sock para nao-blocante");
		close (sockfd);
		return - 1;
	}

	memset (&buf, 0, sizeof(buf));

	buf[0] = 22;
	buf[1] = 02;
	buf[3] = 01;


	INFO_PRINT ("Envia pacote");
	rc = send(sockfd, buf, BUFLEN, 0);
	if (rc != BUFLEN)
		ERRNO_PRINT("sendto()");

	INFO_PRINT ("Recebe pacote\n");

	tam = sizeof (buf_resp);
	memset (buf_resp, 0, tam);
	do {
		rc = select_recv_tam (sockfd, buf_resp, tam);
		if (rc < sizeof (resp)) {
			ERROR_PRINT ("falha recv ntpq");
			return 1;
		}
		memcpy (&resp, &buf_resp, sizeof (resp));
		resp.sequence = be16toh (resp.sequence);
		tam = resp.count = be16toh (resp.count);
		resp.offset = be16toh (resp.offset);
		resp.association_id = be16toh (resp.association_id);


		if (!(aux = (char*) realloc (char_resp, tam + tam_anterior +1))) {
			ERROR_PRINT ("falha malloc");
			break;
		}
		char_resp = aux;
		memcpy (char_resp + tam_anterior, &buf_resp [sizeof(resp)], tam);
		tam_anterior += tam;
		char_resp [tam_anterior] = '\0';
		INFO_PRINT ("resp.leap_indicator %d, resp.version_number %d, resp.mode %d, resp.response %d, resp.error %d, resp.more %d, resp.opcode %d, resp.sequence %d, resp.leap_indicator_2 %d, resp.clock_src %d, resp.system_evt_counter %d, resp.system_evt_code %d, resp.association_id %d, resp.offset %d, resp.count %d, char_resp %s\n", resp.leap_indicator, resp.version_number, resp.mode, resp.response, resp.error, resp.more, resp.opcode, resp.sequence, resp.leap_indicator_2, resp.clock_src, resp.system_evt_counter, resp.system_evt_code, resp.association_id, resp.offset, resp.count, char_resp);


	} while (resp.more);
	close(sockfd);
//	INFO_PRINT ("[%s]\n",char_resp);
	remove_comentarios_e_barra_n (char_resp);
	INFO_PRINT ("[%s]\n",char_resp);
	if (asprintf (&ws, "{\"ntp\":{\"mac_address\":\"%s\"", mac_address) < 0) {
		ERROR_PRINT ("mem");
		saida (1);
	}

	for (pointer = char_resp; (key = strtok(pointer,"=")) ; pointer = NULL) {
		value = strtok (NULL, ",");
		if ((key) && (value)) {
			INFO_PRINT ("key [%s], value [%s]", key, value);
			if ((strcmp (key, "refid") == 0) || (strcmp (key, "offset") == 0) || (strcmp (key, "rootdelay") == 0) || (strcmp (key, "sys_jitter") == 0)) {
				if (asprintf (&ws, "%s, \"%s\": \"%s\"", ws, key, value) < 0) {
					ERROR_PRINT ("mem");
					saida (1);
				}
				// remover este bloco após implantação em prod da variavel correta refid
				if (strcmp (key, "refid") == 0) {
					if (asprintf (&ws, "%s, \"rfid\": \"%s\"", ws, value) < 0) {
						ERROR_PRINT ("mem");
						saida (1);
					}
				// remover este bloco após implantação em prod da variavel correta refid
				}
				else tem_dados = 1;
			}
		}
	}

	free (char_resp);
	if (asprintf (&ws, "%s}}", ws) < 0) {
		ERROR_PRINT ("mem");
		saida (1);
	}
	if (tem_dados) {
		INFO_PRINT ("ws: [%s]\n", ws);
		ret_ws = chama_web_service_put_ssl (ctx, config->host, config->port, 0, NULL, ws, "/%s/ntp-measure", config->context_web_persistence_optional);
	}
	else INFO_PRINT ("sem dados ntpq\n");
	free (ws);
	if (ret_ws)
		free (ret_ws);

	return 0;
}
