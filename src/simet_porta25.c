/********************************************
* @file  simet_porta25.c                    *
* @brief  teste da porta 25                 *
* @author  Michel Vale Ferreira             *
* @date 19 de outubro de 2012               *
********************************************/


#include "config.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <string.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>



#include "unp.h"
#include "debug.h"
#include "byteconversion.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "utils.h"
#include "simet_porta25.h"
#include "simet_tools.h"

				


int chama_smtp_porta25 (CONFIG_SIMET *config, char *chamada, int family) {
	char *msg_tcp, *result = NULL, *result2 = NULL;
	int32_t status, sock, i;
	int64_t tam;

	char **mensagens;
/*		"HELO mail.simet.nic.br\r\n",
		"MAIL FROM: simetbox@ceptro.br\r\n", 
		"RCPT TO: porta25@ceptro.br\r\n",
		"DATA\r\n",
		"quit\r\n"
*/
	mensagens = (char**) malloc (5 * sizeof (char**));
	if (asprintf(&mensagens[0], "HELO %s\r\n", config->porta25_server) < 0) {
		ERROR_PRINT("%s", NAO_ALOCA_MEM);
		saida (1);
	}
	if (asprintf(&mensagens[1], "MAIL FROM: %s\r\n", config->porta25_from) < 0) {
		ERROR_PRINT("%s", NAO_ALOCA_MEM);
		saida (1);
	}
	if (asprintf(&mensagens[2], "RCPT TO: %s\r\n", config->porta25_to) < 0) {
		ERROR_PRINT("%s", NAO_ALOCA_MEM);
		saida (1);
	}
	if (asprintf(&mensagens[3], "DATA\r\n") < 0) {
		ERROR_PRINT("%s", NAO_ALOCA_MEM);
		saida (1);
	}
	if (asprintf(&mensagens[4], "quit\r\n") < 0) {
		ERROR_PRINT("%s", NAO_ALOCA_MEM);
		saida (1);
	}
	
	
	msg_tcp = malloc (strlen (chamada) + 5);
	if (!msg_tcp) {
		INFO_PRINT("Nao conseguiu alocar buffer envio de mensagem tcp do host %s:25 para a chamada %s", config->porta25_server, chamada);
		return (1);
	}

	
	sprintf(msg_tcp,"%s\r\n", chamada);
	status = connect_tcp(config->porta25_server, "25", family, &sock);
	if (status < 0) {
		INFO_PRINT("Nao conectou com %s:25", config->porta25_server);
		return (status);
	}
	else INFO_PRINT("conectou com %s:25. sock %d", config->porta25_server, sock);

	// lê mensagem de boas-vindas do servidor
	result2 = recebe_resposta (sock, &tam, PARA_EM_BARRA_R_BARRA_N);
	if (!result2) {
		INFO_PRINT ("sem mensagem de boas-vindas. Abortando...");
		saida (1);
	}	
	result = remove_barra_r_barra_n(result2);
	if (result) {
		INFO_PRINT ("boas-vindas: [%s]", result);
		free (result);
	}


	for (i = 0; i < 4; i++) {
		result = remove_barra_r_barra_n(chama_troca_msg_tcp_barra_r_barra_n(sock, config->porta25_server, "25", mensagens[i], &tam));
		if (result) {
			INFO_PRINT ("resposta da chamada: %s: [%s]", mensagens[i], result);
			free (result);
		}
		else { 
			INFO_PRINT("Nao conseguiu enviar a mensagem [%s] ao host %s:25\n", mensagens[i], config->porta25_server);
			return (2);
		}
			
	}
	result = remove_barra_r_barra_n(chama_troca_msg_tcp_barra_r_barra_n(sock, config->porta25_server, "25", msg_tcp, &tam));
	free (msg_tcp);
	if (result) {
		INFO_PRINT ("resposta da chamada: %s: [%s]", chamada, result);
		free (result);
	}
	else { 
		INFO_PRINT("Nao conseguiu enviar a mensagem [%s] ao host %s:25\n", chamada, config->porta25_server);
		return (2);
	}
	
	for (i = 4; i < 5; i++) {
		result = remove_barra_r_barra_n(chama_troca_msg_tcp_barra_r_barra_n(sock, config->porta25_server, "25", mensagens[i], &tam));
		if (result) {
			INFO_PRINT ("resposta da chamada: %s: [%s]", mensagens[i], result);
			free (result);
		}
		else { 
			INFO_PRINT("Nao conseguiu enviar a mensagem [%s] ao host %s:25\n", mensagens[i], config->porta25_server);
			return (2);
		}
	}

	close (sock);
	
	return (0);
}


int main_porta25 (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options) {

	char ip_cliente[INET6_ADDRSTRLEN + 1], ip_iface[INET6_ADDRSTRLEN + 1], *provedor_cliente, *ret_ws, *hash_measure;

	int conseguiu_enviar, nome_resolvido, acesso_porta25, completou_envio_porta25, realizou_teste = 0;
	char* formata_parametro, *temp, *aux, *versao;
	proto_disponiveis disp;
	int family = 0, proto, rc;
	char doc[] =
		"uso:\n\tsimet_porta25\n\n\tTesta a gerencia da porta 25 pelo provedor\n";		

	if (options)
		parse_args_basico (options, doc, &family);
	
	versao = obtem_versao();
	hash_measure = get_hash_measure (config->host, config->port, 0, config->context_root, mac_address, ssl_ctx);
	protocolos_disponiveis (&disp, family, 1, config, ssl_ctx);
	if (asprintf (&temp, "{\"simetPorta25\":{\"version\":\"%s\",\"hashDevice\":\"%s\",\"hashMeasure\":\"%s\",", versao, mac_address, hash_measure) < 0) {
		ERROR_PRINT ("mem");
		saida (1);
	}
	free (versao);
	for (proto = PROTO_IPV4; proto <= PROTO_IPV6; proto++) {
		if (!disp.proto [proto])
			continue;
/*		family = (proto == PROTO_IPV4)? 4 : 6;
		ip_cliente = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, family, NULL, 0, "/%s/IpClienteServlet", config->context_root);
		if (!ip_cliente) {
			ERROR_PRINT ("Nao foi possivel obter o ip do cliente. Abortando\n");
			return (1);
		}
*/
		aux = temp;
		switch (proto) {
			case PROTO_IPV4:
				rc = asprintf (&temp, "%s\"resultadosIPv4\":", aux);
				inet_ntop(AF_INET, &disp.addr_v4_pub, ip_cliente, INET_ADDRSTRLEN);
				inet_ntop(AF_INET, &disp.addr_v4, ip_iface, INET_ADDRSTRLEN);
				break;
			case PROTO_IPV6:
				rc = asprintf (&temp, "%s\"resultadosIPv6\":", aux);
				inet_ntop(AF_INET6, &disp.addr_v6_pub, ip_cliente, INET6_ADDRSTRLEN);
				inet_ntop(AF_INET6, &disp.addr_v6, ip_iface, INET6_ADDRSTRLEN);
				break;
			default:
				ERROR_PRINT ("proto invalido");
				saida (1);
		}
		if (rc < 0) {
			ERROR_PRINT ("mem");
			saida (1);
		}
		free (aux);

		provedor_cliente = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, (proto == PROTO_IPV4)? 4 : 6, NULL, 0, "/%s/ProviderServlet", config->context_root);
		
		

		if (asprintf (&formata_parametro, 
				"Subject: teste de porta 25\r\n"
				"From: %s\r\n"
				"To: %s\r\n"
				"o simetbox com o mac_address: %s e ip: %s do provedor %s conseguiu enviar este email.\r\n.", config->porta25_from, config->porta25_to, mac_address, ip_cliente, provedor_cliente) == -1) {
			ERROR_PRINT ("Nao foi possivel alocar memoria");
			saida (1);
		}
		conseguiu_enviar = chama_smtp_porta25 (config, formata_parametro, (proto == PROTO_IPV4)? 4 : 6);
		switch (conseguiu_enviar) {
			case -96:
				acesso_porta25 = 0;
				completou_envio_porta25 = 0;
				nome_resolvido = 1;
				break;
			case -2:
				acesso_porta25 = 0;
				completou_envio_porta25 = 0;
				nome_resolvido = 0;
				break;
			case 1:
				acesso_porta25 = 0;
				completou_envio_porta25 = 0;
				nome_resolvido = 1;
				break;
			case 2:
				acesso_porta25 = 1;
				completou_envio_porta25 = 0;
				nome_resolvido = 1;
				break;
			default:
				acesso_porta25 = 1;
				completou_envio_porta25 = 1;
				nome_resolvido = 1;
				break;
		}
		free (formata_parametro);
		
		aux = temp;
//"ipPublico": "IP", "acessaPorta25": "True", "completouEnvio": "False"
		rc = asprintf (&temp, "%s{\"ipPublico\":\"%s\",\"ipInterface\":\"%s\",\"nomeResolvido\":\"%s\",\"acessaPorta25\":\"%s\",\"completouEnvio\":\"%s\"},", aux, ip_cliente, ip_iface,  nome_resolvido?"True":"False", acesso_porta25?"True":"False", completou_envio_porta25?"True":"False");
		if (rc < 0) {
			ERROR_PRINT ("mem");
			saida (1);
		}
		free (aux);
		realizou_teste = 1;
	}
	if (realizou_teste) {
		temp [rc - 1] = '\0';
		aux = temp;
		rc = asprintf (&temp, "%s}}", aux);
		if (rc < 0) {
			ERROR_PRINT ("mem");
			saida (1);
		}
		free (aux);
/*	ret_ws = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, 0, NULL, 0, "/%s/TestePorta25?hashMeasure=%s&macAddress=%s&acessaPorta25=%d&completouEnvio=%d",
		config->context_optional, hash_measure, mac_address, acesso_porta25, completou_envio_porta25);
	if (ret_ws)
		free (ret_ws);
	*/
		INFO_PRINT ("chamar webservice PUT: [%s]\n", temp);
		ret_ws = chama_web_service_put_ssl (ssl_ctx, config->host, config->port, 0, NULL, temp, "/%s/TestePorta25", config->context_optional);
		if (ret_ws) {
			INFO_PRINT ("resultado put: [%s]\n", ret_ws);
			free (ret_ws);
		}
		else INFO_PRINT ("PUT sem resposta");
	}
	free (hash_measure);
	return (0);
}


