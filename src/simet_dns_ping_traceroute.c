#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


#include "simet_dns_ping_traceroute.h"
#include "utils.h"
#include "unp.h"
#include "debug.h"
#include "byteconversion.h"
#include "netsimet.h"
#include "gopt.h"

#define FIRST_ROOT_SERVER 'a'
#define LAST_ROOT_SERVER 'm'


int gera_lista_parametros (char *hash_measure, int family, char* ip_origem) {
	char index, *host = NULL, *ip, *modelo_digitos;
	int pc = 0;

	INFO_PRINT("Iniciando parametros ping e trace route de raiz dns\n");
	modelo_digitos = obtem_modelo (&pc);
	free (modelo_digitos);


	for (index = FIRST_ROOT_SERVER; index <= LAST_ROOT_SERVER; index++) {
		if (asprintf (&host, "%c.root-servers.net", index) < 0) {
			ERROR_PRINT ("nao conseguiu alocar nome %c.root-server.net\n", index);
			return 1;
		}

		ip = get_ip_by_host(host, family);
		if (!ip)
			continue;

		INFO_PRINT("Host %s, Endereco IP : %s\n", host, ip);
		//trace_route(ip, host, hash_measure, ip_origem, family);
		if (pc)
			fprintf (stdout, "-m %s -n %s -o %s %s\n", hash_measure, host, ip_origem, ip);
		else
			fprintf (stdout, "-m %s -n %s -o %s -%d %s\n", hash_measure, host, ip_origem, family, ip);

		free (ip);
		free (host);
	}
	INFO_PRINT("Finalizando parametros ping e trace route de raiz dns\n");
	return 0;

}

int main_dns_ping_traceroute (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options)
{
	char *hash_measure_trace, *ret_ws, ip_cliente[INET6_ADDRSTRLEN + 1];
	const char versao [] = VERSAO_DNS_TRACEROUTE;
	char doc[] =
		"uso:\n\tsimet_dns_traceroute\n\n\tTesta traceroute para servidores raiz de dns";
	int proto, family = 0;

	proto_disponiveis disp;
	if (options)
		parse_args_basico (options, doc, &family);



//	if(geteuid() != 0) {
	// Tell user to run app as root, then exit.

//		printf ("user must be root to use this traceroute method. try again with superuser privileges\n");
//		saida (1);
//	}

	protocolos_disponiveis (&disp, family, 1, config, ssl_ctx);

	INFO_PRINT("%s versão %s", nome, versao);

	hash_measure_trace = get_hash_measure (config->host, config->port, family, config->context_root, mac_address, ssl_ctx);
	//inicia testes de traceroute

	for (proto = PROTO_IPV4; proto <= PROTO_IPV6; proto++) {
		if (!disp.proto [proto])
			continue;
		INFO_PRINT ("testando IPv%d\n", (proto == PROTO_IPV4)? 4 : 6);
		switch (proto) {
			case PROTO_IPV4:
				inet_ntop(AF_INET, &disp.addr_v4_pub, ip_cliente, INET_ADDRSTRLEN);
				break;
			case PROTO_IPV6:
				inet_ntop(AF_INET6, &disp.addr_v6_pub, ip_cliente, INET6_ADDRSTRLEN);
				break;
		}
		gera_lista_parametros (hash_measure_trace, (proto == PROTO_IPV4)? 4 : 6, ip_cliente);
	}
//	ret_ws = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, family, NULL, 0, "/%s/%s%s", config->context_optional, "DNSFinished?hashMeasure=", hash_measure_trace);
	free (hash_measure_trace);
	if (ret_ws)
		free (ret_ws);
	return 0;
}
