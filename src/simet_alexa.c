#define _GNU_SOURCE
#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
//#include <dlfcn.h>
#include <json-c/json.h>


#include "unp.h"
#include "debug.h"
#include "byteconversion.h"
#include "utils.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "simet_alexa.h"
#include "simet_tools.h"



site_download *tempo_download_pagina_ssl (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t *size, int retries, char *chamada) {
	char *url_ws, *result = NULL, *site, *aux;
	int status, sock, i;
	int64_t size_recv = 0;
	struct timespec ts_start, ts_end;
	URL_PARTS *urlpartsp, urlparts;
	site_download* alexa_result, *alexa_result_aux = NULL;


	urlpartsp = &urlparts;

	INFO_PRINT ("tempo_download_pagina_ssl: host %s\n", host);
	alexa_result = (site_download*) malloc (sizeof (site_download));
	if (!alexa_result) {
		INFO_PRINT ("Nao foi possivel alocar variavel de tempo alexa\n");
		saida (1);
	}

	url_ws = prepara_url (host, port, size, chamada);

	for (i = 0, result = NULL; i < retries && !result; i++) {
		alexa_result->tempo_transferencia = alexa_result->tempo_conexao = alexa_result->size = alexa_result->redirs = alexa_result->last_rc = alexa_result->rcode = 0;

		clock_gettime(CLOCK_MONOTONIC, &ts_start);
		status = connect_tcp(host, port, family, &sock);
		clock_gettime(CLOCK_MONOTONIC, &ts_end);

		alexa_result->tempo_conexao = (TIMESPEC_NANOSECONDS(ts_end) - TIMESPEC_NANOSECONDS(ts_start)) / 1000;

		if (status < 0){
			alexa_result->rcode = status;
			free (url_ws);
			return alexa_result;
		}
		clock_gettime(CLOCK_MONOTONIC, &ts_start);
		result = chama_troca_msg_tcp_ssl (sock, host, port, url_ws, &size_recv, ssl_ctx);
		clock_gettime(CLOCK_MONOTONIC, &ts_end);
		if (size_recv <= 0) {
			alexa_result->rcode = -1;
			free (result);
//			free (urlpartsp->host);
//			free (urlpartsp->chamada);
			return alexa_result;
		}
		alexa_result->tempo_transferencia += (TIMESPEC_NANOSECONDS(ts_end) - TIMESPEC_NANOSECONDS(ts_start)) / 1000;

		status = return_code(result, size_recv);
		INFO_PRINT ("host: %s, chamada: %s, cod %d\n", host, chamada, status);
		alexa_result->last_rc = status;
		if ((status >= 301) && (status <= 307)) {
			alexa_result->redirs++;
			site = encontra_redir (result);
			if (!site) {
				free (url_ws);
				*size = 0;
				break;
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
			separa_protocolo_host_chamada (urlpartsp, site);
			if (urlpartsp->https) {
				if (!ssl_ctx) {
					INFO_PRINT ("reinicializa ssl_ctx\n");
					ssl_ctx = init_ssl_context(0);
				}
				alexa_result_aux = tempo_download_pagina_ssl (ssl_ctx, urlpartsp->host, PORT_HTTPS, family, size, retries, urlpartsp->chamada);
			}
			else {
				alexa_result_aux = tempo_download_pagina (ssl_ctx, urlpartsp->host, PORT_HTTP, family, size, retries, urlpartsp->chamada);
			}
			if ((!alexa_result_aux) || (alexa_result_aux->rcode == -1)) {
				alexa_result->tempo_transferencia = -1;
				alexa_result->tempo_conexao = -1;
			}
			else {
				alexa_result->tempo_transferencia += alexa_result_aux->tempo_transferencia;
				alexa_result->tempo_conexao += alexa_result_aux->tempo_conexao;
			}
			alexa_result->redirs += alexa_result_aux->redirs;
			alexa_result->last_rc = alexa_result_aux->last_rc;
			if (alexa_result_aux)
				free (alexa_result_aux);
			free (urlpartsp->host);
			free (urlpartsp->chamada);
			free (site);
		}
	}
	free (url_ws);
	free (result);



	if (size)
		*size += size_recv;



	INFO_PRINT ("alexa (https): %s %s tempo_transferencia %"PRI64" us, tempo_conexao %"PRI64", size %"PRI64"\n", host, chamada, alexa_result->tempo_transferencia, alexa_result->tempo_conexao, *size);
	return (alexa_result);

}

site_download* tempo_download_pagina (SSL_CTX *ssl_ctx, char* host, char *port, int family, int64_t *size, int retries, char *chamada) {
	char *url_ws, *result, *site, *aux;
	int status, sock, i;
	int64_t size_recv;
	struct timespec ts_start, ts_end;
	URL_PARTS *urlpartsp, urlparts;
	site_download *alexa_result, *alexa_result_aux = NULL;



	urlpartsp = &urlparts;

	alexa_result = (site_download*) malloc (sizeof (site_download));
	if (!alexa_result) {
		INFO_PRINT ("Nao foi possivel alocar variavel de tempo alexa\n");
		saida (1);
	}
	url_ws = prepara_url (host, port, size, chamada);

	for (i = 0, result = NULL; i < retries && !result; i++) {
		alexa_result->tempo_transferencia = alexa_result->rcode = alexa_result->tempo_conexao = alexa_result->size = alexa_result->redirs = alexa_result->last_rc = 0;


		clock_gettime(CLOCK_MONOTONIC, &ts_start);
		status = connect_tcp(host, port, family, &sock);
		clock_gettime(CLOCK_MONOTONIC, &ts_end);

		if (status < 0){
			alexa_result->rcode = status;
			free (url_ws);
			/* especificação de erro pro usuário
			switch (status) {
				case -1: // valor inválido em ai_flags
				case -7: // tipo de socket não suportado
				case -8: // serviço não suportado para o tipo de socket
				case -10: // falha alocação memória
				case -11: // erro do sistema
				case -12: // buffer overflow no parâmetro
				case -97: // buffer overflow no parâmetro
					rcode = 5; // erro interno
					break;
				case -2: // sem registro de IP para o nome no protocolo
					rcode = 2; // sem registro de IP para o nome no protocolo
					break;
				case -3: // erro temporário na resolução de nomes
				case -4: // erro não recuperavel na resolução de nome
					rcode = 3; // erro na resolução de nomes
					break;
				case -5: // não existe endereço associado ao nome
					rcode = 4; // não existe endereço associado ao nome
					break;
				case -6: // familia de IP não suportada
				case -9: // familia de IP não suportada
					rcode = 6; // familia de IP não suportada
					break;
				case -96:
					rcode = 1; // timeout
					break;
				case -98:
					rcode = 98; // erro desconhecido
				case -99:
					rcode = 99; // não realizou teste
			} */
			return alexa_result;
		}
		alexa_result->tempo_conexao = (TIMESPEC_NANOSECONDS(ts_end) - TIMESPEC_NANOSECONDS(ts_start))/1000;
		clock_gettime(CLOCK_MONOTONIC, &ts_start);
		result = chama_troca_msg_tcp (sock, host, port, url_ws, &size_recv);
		clock_gettime(CLOCK_MONOTONIC, &ts_end);
		if (size_recv <= 0) {
			alexa_result->rcode = -1;
			free (result);
			free (urlpartsp->host);
			free (urlpartsp->chamada);
			free (url_ws);
			return alexa_result;
		}
		alexa_result->tempo_transferencia += (TIMESPEC_NANOSECONDS(ts_end) - TIMESPEC_NANOSECONDS(ts_start))/1000;

		status = return_code(result, size_recv);
		INFO_PRINT ("host: %s, chamada: %s, cod %d\n", host, chamada, status);
		alexa_result->last_rc = status;

		if ((status >= 301) && (status <= 307)) {
			alexa_result->redirs++;
			site = encontra_redir (result);
			if (!site) {
				*size = 0;
				break;
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
			separa_protocolo_host_chamada (urlpartsp, site);
			if (urlpartsp->https) {
				if (!ssl_ctx) {
					INFO_PRINT ("reinicializa ssl_ctx\n");
					ssl_ctx = init_ssl_context(0);
				}
				alexa_result_aux = tempo_download_pagina_ssl (ssl_ctx, urlpartsp->host, PORT_HTTPS, family, size, retries, urlpartsp->chamada);
			}
			else {
				alexa_result_aux = tempo_download_pagina (ssl_ctx, urlpartsp->host, PORT_HTTP, family, size, retries, urlpartsp->chamada);
			}
			if ((!alexa_result_aux) || (alexa_result_aux->rcode == -1)) {
				alexa_result->tempo_transferencia = -1;
				alexa_result->tempo_conexao = -1;
				alexa_result->rcode = -1;
			}
			else {
				alexa_result->tempo_transferencia += alexa_result_aux->tempo_transferencia;
				alexa_result->tempo_conexao += alexa_result_aux->tempo_conexao;
			}
			alexa_result->redirs += alexa_result_aux->redirs;
			alexa_result->last_rc = alexa_result_aux->last_rc;
			if (alexa_result_aux)
				free (alexa_result_aux);
			free (urlpartsp->host);
			free (urlpartsp->chamada);
			free (site);
		}
	}
	free (url_ws);
	free (result);

	if (size)
		*size += size_recv;



	INFO_PRINT ("alexa (http): %s %s tempo_transferencia %"PRI64" us, tempo_conexao %"PRI64", size %"PRI64"\n", host, chamada, alexa_result->tempo_transferencia, alexa_result->tempo_conexao, *size);
	return (alexa_result);

}

site_download *testa_alexa (SSL_CTX *ssl_ctx, int retries, char* site, int family) {

	URL_PARTS urlparts, *urlpartsp;
	char *url, *aux;
	int64_t size;
	site_download *result = NULL;
	int i;
	struct timespec ts_end, ts_start;

	INFO_PRINT ("medicao alexa de %s\n", site);
	for (i = 0; i < 2; i++) {

		if (i == 0) {
			if ((strstr(site, HTTP)) || (strstr(site, HTTPS)))
				aux = strdup(site);
			else if (asprintf (&aux, "http://%s", site) <= 0) {
				ERROR_PRINT ("nao foi possivel alocar string alexa");
				saida (1);
			}
		}
		else {
			if ((strstr(site, HTTP)) || (strstr(site, HTTPS)))
				aux = strdup(site);
			else if (asprintf (&aux, "http://www.%s", site) <= 0) {
				ERROR_PRINT ("nao foi possivel alocar string alexa(2)");
				saida (1);
			}
		}
		url = aux;
		INFO_PRINT ("Baixando %s", url);

		urlpartsp = &urlparts;

		if (decode_url (url) < 0)  {
			INFO_PRINT ("bad url to decode\n");
			saida (1);
		}
		clock_gettime(CLOCK_MONOTONIC, &ts_start);

		separa_protocolo_host_chamada (urlpartsp, url);
		free (url);
		if (urlpartsp->https) {
			size = 0;
			result = tempo_download_pagina_ssl (ssl_ctx, urlpartsp->host, PORT_HTTPS , family, &size, retries, urlpartsp->chamada);
		}
		else {
			size = 0;
			result = tempo_download_pagina (ssl_ctx, urlpartsp->host, PORT_HTTP , family, &size, retries, urlpartsp->chamada);
		}
		clock_gettime(CLOCK_MONOTONIC, &ts_end);
		result->tempo_total = (TIMESPEC_NANOSECONDS(ts_end) - TIMESPEC_NANOSECONDS(ts_start))/1000;
		result->size = size;
		free (urlpartsp->host);
		free (urlpartsp->chamada);
		if (result->last_rc == 200)
			// se o rc foi 200 não precisa fazer o com www antes do nome.
			break;
		else {
			if (i == 0) {
				free (result);
				result = NULL;
			}
		}
	}

	return (result);
}



teste_alexa* parse_json (json_object *jobj) {
	int i;
	json_object *jvalue_array_item, *jvalue_array, *jobj_aux;
	teste_alexa *alexa;
	char *aux;
	alexa = (teste_alexa*) malloc (sizeof (teste_alexa));
	if (!alexa) {
		INFO_PRINT ("nao foi possivel alocar estrutura alexa\n");
		return NULL;
	}

/* parametros alexa
	aux = json_object_object_get (jobj, "num_retries");
	if (!aux)
		return NULL;
	alexa->retries = json_object_get_int (aux);
	json_object_put (aux);
	aux = json_object_object_get (jobj, "num_threads");
	if (!aux)
		return NULL;
	alexa->threads = json_object_get_int (aux);
	json_object_put (aux);
	aux = json_object_object_get (jobj, "user_agent");
	if (!aux)
		return NULL;
	alexa->user_agent = strdup (json_object_get_string (aux));
	json_object_put (aux);
	jvalue_array = json_object_object_get (jobj, "files_alexa");
	if (!aux)
		return NULL;
	*/
	if (!json_object_object_get_ex (jobj, "idAlexaDay", &jobj_aux))
		return NULL;
	alexa->id_alexa_day = json_object_get_int (jobj_aux);
	if (!json_object_object_get_ex (jobj, "siteList", &jvalue_array))
		return NULL;

	alexa->num_sites = json_object_array_length (jvalue_array); /*Getting the length of the array*/

	alexa->sites = (char**) malloc (sizeof (char*) * alexa->num_sites);
	if (!alexa->sites) {
		ERROR_PRINT ("nao alocou array de sites");
		saida (1);
	}

	for (i = 0; i < alexa->num_sites; i++) {
		jvalue_array_item = json_object_array_get_idx (jvalue_array, i); /*Getting the array element at position i*/
/* estrutura antiga
		if (!jvalue_array_item)
			return NULL;
		aux = json_object_object_get (jvalue_array_item, "site");
		if (!aux)
			return NULL;
		*/
		aux = strdup (json_object_get_string (jvalue_array_item));
		if (!aux) {
			ERROR_PRINT ("nao foi possivel alocar string alexa");
			saida (1);
		}

		alexa->sites[i] = aux;

/*		aux = json_object_object_get (jvalue_array_item, "files");
		if (!aux)
			return NULL;
		arraylen = alexa->sites[i].num_files = json_object_array_length (aux);
		if (arraylen) {
			alexa->sites[i].files = (char**) malloc (sizeof (char*) * arraylen);
			if (!alexa->sites[i].files)
				return NULL;
		}

		for (j = 0; j < arraylen; j++) {
			jvalue_array_item2 = json_object_array_get_idx (aux, j);
			if (!jvalue_array_item2)
				return NULL;
			alexa->sites[i].files[j] = strdup (json_object_get_string (jvalue_array_item2));
			//json_object_put (jvalue_array_item2);
		}
		//json_object_put (aux);
		*/
	}
	//json_object_put (jvalue_array);
	//json_object_put (jobj);

	return alexa;
}


char* executa_downloads (teste_alexa* alexa, SSL_CTX * ssl_ctx, char *mac_address, char *hash_measure, proto_disponiveis *disp, int *fez_teste) {
//	pthread_t thread_id [alexa->threads];
//	alexa_thread_arg alexa_arg;
//	pthread_mutex_t mutex;
	int i, proto, rc = 0;
	site_download *result;
	char *webservice, *aux, *intro, *temp, *versao, *vel;

//	pthread_mutex_init (&mutex, NULL);
//	alexa_arg.mutex = &mutex;
//	alexa_arg.ssl_ctx = ssl_ctx;
//	alexa_arg.retries = alexa->retries;
	////////remover
	//alexa->threads = 1;
	versao = obtem_versao();
	temp = strdup ("");

	if (asprintf (&intro, "{\"simet_alexa\":{\"version\":\"%s\",\"idAlexaDay\":%d,\"mac_address\":\"%s\",\"hash_measure\":\"%s\",", versao, alexa->id_alexa_day, mac_address, hash_measure) < 0) {
		ERROR_PRINT ("mem");
		saida (1);
	}
	free (versao);
	for (proto = PROTO_IPV4; proto <= PROTO_IPV6; proto++) {
		if (!disp->proto [proto])
			continue;
		*fez_teste = 1;
		aux = temp;
		switch (proto) {
			case PROTO_IPV4:
				rc = asprintf (&temp, "%s\"resultados_v4\":[", aux);
				break;
			case PROTO_IPV6:
				rc = asprintf (&temp, "%s\"resultados_v6\":[", aux);
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

		for (i = 0; i < alexa->num_sites; i++) {
			result = testa_alexa (ssl_ctx, alexa->retries, alexa->sites[i], (proto == PROTO_IPV4)? 4 : 6);
			vel = velocidade (result->size, result->tempo_total);
			INFO_PRINT ("site: %s, rcode %d, tempo: %"PRI64", tempo transferencia: %"PRI64", tempo conex: %"PRI64", size: %"PRI64", last_rc %d, velocidade %sbps\n", alexa->sites[i], result->rcode, result->tempo_total, result->tempo_transferencia, result->tempo_conexao, result->size, result->last_rc, vel);
			free (vel);

			aux = temp;
			rc = asprintf (&temp, "%s{\"site\":\"%s\",\"rcode\":%d,\"size\":%"PRI64",\"time_transfer\":%"PRI64",\"time_connection\":%"PRI64",\"total_time\":%"PRI64",\"redirects\":%d},", aux, alexa->sites[i], result->rcode, result->size, result->tempo_transferencia, result->tempo_conexao, result->tempo_total, result->redirs);
			if (rc < 0) {
				ERROR_PRINT ("mem");
				saida (1);
			}
			free (aux);
			free (result);
		}
		temp [rc - 1] = '\0';
		aux = temp;
		rc = asprintf (&temp, "%s],", aux);
		if (rc < 0) {
			ERROR_PRINT ("mem");
			saida (1);
		}
		free (aux);

	}
	if (alexa->num_sites)
		temp [rc - 1] = '\0';
	if (asprintf (&webservice, "%s%s}}", intro, temp) < 0) {
		ERROR_PRINT ("mem");
		saida (1);
	}
	free (temp);
	free (intro);
	return webservice;

}

int main_alexa (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options) {

	char doc[] = "uso:\n\tsimet_alexa\n\n\ttesta o top 10 alexa brasil\n";
	char *conteudo, *result, *ip, *ret_ws = NULL, *hash_measure;
	char ip_cliente[2][INET6_ADDRSTRLEN + 1];
//	ssize_t read;
//	void *handle;
	json_object *jobj;
	enum json_tokener_error error = json_tokener_success;
	teste_alexa *alexa;
	int i, proto, fez_teste = 0;
	int family = 0;
	proto_disponiveis disp;

	if (options)
		parse_args_basico (options, doc, &family);


	protocolos_disponiveis (&disp, family, 1, config, ssl_ctx);


	for (i = 0; i < 3; i++) {
		conteudo = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, family, NULL, 0, "/%s/%s", config->context_optional, "GetAlexaTopSites?national=true");
		//asprintf (&conteudo, "{\"idAlexaDay\":7,\"siteList\":[\"google.com.br\",\"uol.com.br\",\"globo.com\",\"live.com\",\"mercadolivre.com.br\",\"google.com\",\"facebook.com\",\"youtube.com\",\"yahoo.com\",\"baidu.com\",\"wikipedia.org\",\"twitter.com\",\"qq.com\",\"taobao.com\",\"amazon.com\"]}");
		if (strnlen(conteudo, 1) == 0) {
			INFO_PRINT ("Nao conseguiu obter string json\n");
		}
		else break;
	}
	jobj = json_tokener_parse_verbose(conteudo, &error);
	alexa = parse_json (jobj);
	if (!alexa) {
		INFO_PRINT ("falha na interpretacao do json\n");
		saida (1);
	}
	json_object_put(jobj);
	alexa->retries = 3;
	alexa->threads = 1;
	alexa->user_agent = strdup ("Simet Alexa");

	hash_measure = get_hash_measure (config->host, config->port, family, config->context_root, mac_address, ssl_ctx);

	result = executa_downloads (alexa, ssl_ctx, mac_address, hash_measure, &disp, &fez_teste);

	if (fez_teste) {
		INFO_PRINT ("chamar webservice PUT: [%s]\n", result);
		ret_ws = chama_web_service_put_ssl (ssl_ctx, config->host, config->port, family, NULL, result, "/%s/alexa-test", config->context_web_persistence_optional);
		if (ret_ws) {
			INFO_PRINT ("resultado put: [%s]\n", ret_ws);
			free (ret_ws);
		}
		else INFO_PRINT ("PUT sem resposta");
	}
	for (proto = PROTO_IPV4; proto <= PROTO_IPV6; proto++) {
		if (!disp.proto [proto])
			continue;
		switch (proto) {
			case PROTO_IPV4:
				inet_ntop(AF_INET, &disp.addr_v4_pub, ip_cliente[proto], INET_ADDRSTRLEN);
				break;
			case PROTO_IPV6:
				inet_ntop(AF_INET6, &disp.addr_v6_pub, ip_cliente[proto], INET6_ADDRSTRLEN);
				break;
		}
	}
	for (i = 0; i < alexa->num_sites; i++) {
		for (proto = PROTO_IPV4; proto <= PROTO_IPV6; proto++) {
			if (!disp.proto [proto])
				continue;
			ip = get_ip_by_host(alexa->sites[i], (proto == PROTO_IPV4)? 4 : 6);
			if (!ip) {
				INFO_PRINT ("sem IPv%d para %s\n", (proto == PROTO_IPV4)? 4 : 6, alexa->sites[i]);
				continue;
			}
			INFO_PRINT("Host %s, Endereco IP : %s\n", alexa->sites[i], ip);
			trace_route(ip, alexa->sites[i], hash_measure, ip_cliente[proto], (proto == PROTO_IPV4)? 4 : 6);
			free (ip);
		}
		free (alexa->sites[i]);
	}
	free (alexa->sites);
	free (alexa->user_agent);
	free (alexa);

	free (conteudo);
	free (result);
	free (hash_measure);


	return 0;
}