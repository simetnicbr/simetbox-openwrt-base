/********************************************
* @file  dns.c                              *
* @brief  teste de dns do simetbox          *
* @author  Michel Vale Ferreira             *
* @date 19 de outubro de 2012               *
********************************************/

#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/time.h>
#include <limits.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <json-c/json.h>

#include "base64.h"
#include "utils.h"
#include "unp.h"
#include "debug.h"
#include "byteconversion.h"
#include "dns.h"
#include "netsimet.h"
#include "ntp.h"
#include "gopt.h"

#define VALOR_TTL_ULTIMA_PRE_TTL 1

int force_no_sleep = 0;
char *servidor = NULL;
char *nome_host = NULL;
FILE *fileout;



void libera_short_query_response (SHORT_QUERY_RESPONSE *res) {
	int i;

	if (res->hostname)
		free (res->hostname);
	for (i = 0; i < res->num_resposta; i++)
		switch (res->resp[i].tipo_reg) {
			case TR_TXT:
				free (res->resp[i].txt.bindproperty);
				break;
			case TR_A_AAAA:
				free (res->resp[i].a.host_ip);
				break;
			case TR_RRSIG:
				free (res->resp[i].rrsig.assinante);
				free (res->resp[i].rrsig.assinatura);
				break;
		}
	if (res->resp)
		free (res->resp);
	free (res);
}
void libera_short_query_response_matrix (SHORT_QUERY_RESPONSE ***res, DNS_SERVERS *dns_servers, CONFIG_SIMET *config) {
	int i, j;
	for (i = 0; i < dns_servers->count; i++) {
		for (j = 0; j < NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round * 2; j ++)
			if (res [i][j])
				libera_short_query_response (res [i][j]);
		free (res[i]);
	}
	free (res);
}



char *obtem_hostname (char *host) {
	char *result;
	int colocapontonofim = 0;

	if (host[strlen (host) - 1] != '.')
		colocapontonofim = 1;
	result = (char*) malloc (strlen(host) + 1 + colocapontonofim);
	if (!result) {
		INFO_PRINT ("Nao conseguiu alocar nome em obtem_hostname");
		saida (1);
	}
	strcpy (result, host);
	if (colocapontonofim)
		strcat (result, ".");
	return result;
}




SHORT_QUERY_RESPONSE *** realiza_testes_dns (DNS_SERVERS *dns_servers, char *mac_address, char **hash_measures, int64_t *offset_time, char* ip_dns_autoritativo, CONFIG_SIMET *config, int family) {
	int32_t i, j, k, segue, num_pre_ttl_query;
	SHORT_QUERY_RESPONSE ***res;
	char *hostname, *hostname_direto_autoritativo;
	int64_t hora_ntp, hora_local, hora_atual, hora_ini, tempo_sleep;
	struct timespec ts;
	A_AAAA_SHORT_QUERY_RESPONSE *a = NULL;


	res = (SHORT_QUERY_RESPONSE***) malloc (sizeof (SHORT_QUERY_RESPONSE**) * dns_servers->count);
	if (!res) {
		INFO_PRINT ("sem memoria para alocar matriz de respostas curtas do servidor de DNS\n");
		saida (1);
	}

	for (i = 0; i < dns_servers->count; i++) {
		res [i] = (SHORT_QUERY_RESPONSE**) malloc (sizeof (SHORT_QUERY_RESPONSE*) * (NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round * 2));
		if (!res[i]) {
			INFO_PRINT ("sem memoria para alocar colunas da matriz de respostas curtas do servidor de DNS\n");
			saida (1);
		}
		memset (res[i], 0, sizeof (SHORT_QUERY_RESPONSE*) * (NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round * 2));
	}



	for (i = 0; i < dns_servers->count; i++) {
	// testes realizados uma vez
		// teste chaos TXT hostname.bind
		res[i][HOSTBIND] = consulta_dns("hostname.bind.", T_TXT, C_CH, dns_servers->ip[i], 0, *offset_time);
		// teste chaos TXT version.bind
		res[i][VERSIONBIND] = consulta_dns("version.bind.", T_TXT, C_CH, dns_servers->ip[i], 0, *offset_time);
		// teste assinatura invalida
		res[i][DNSSEC_INVALIDO] = consulta_dns("signfail.ceptro.br.", T_A, C_IN, dns_servers->ip[i], 1, *offset_time);
		// teste assinatura valida
		res[i][DNSSEC_VALIDO] = consulta_dns("ceptro.br.", T_A, C_IN, dns_servers->ip[i], 1, *offset_time);
		// teste assinatura expirada
		res[i][DNSSEC_EXPIRADO] = consulta_dns("signexpired.ceptro.br.", T_A, C_IN, dns_servers->ip[i], 1, *offset_time);

		// teste A dns inexistente
		if ((asprintf(&hostname, "www.%ssimet.com.br.", mac_address)) < 0) {
			INFO_PRINT ("Nao conseguiu alocar hostname para DNS_INEXISTENTE\n");
			saida (1);
		}
		res[i][DNS_INEXISTENTE_1] = consulta_dns(hostname, T_A, C_IN, dns_servers->ip[i], 0, *offset_time);
		free (hostname);
		// teste A dns inexistente2
		res[i][DNS_INEXISTENTE_2] = consulta_dns("www.sacanagemxxx.com.br.", T_A, C_IN, dns_servers->ip[i], 0, *offset_time);

		// teste ipv6
		res[i][IPV6] = consulta_dns("v6.ipv6.br.", T_AAAA, C_IN, dns_servers->ip[i], 0, *offset_time);

		// testes de respeito ao ttl
		if (strnlen (ip_dns_autoritativo, 1)) {
		// testes de respeito ao intra-ttl o TTL deve ser igual ou menor a 3600
			if ((asprintf(&hostname, "ttl%d.%s.", config->intra_ttl_value, config->sufixo_dns_autoritativo)) < 0) {
				INFO_PRINT ("Nao conseguiu alocar hostname para INTRA_TTL\n");
				saida (1);
			}
			for (j = NUM_TESTES_FIXOS ; j < NUM_TESTES_FIXOS + config->num_intra_ttl_queries; j++) {
				res[i][j] = consulta_dns(hostname, T_A, C_IN, dns_servers->ip[i], 0, *offset_time);
				hostname_direto_autoritativo = NULL;
				if ((res[i][j]->rcode == 0) && (res[i][j]->num_resposta)) {
					for (k = 0; k < res[i][j]->num_resposta; k++)
						if (res[i][j]->resp[k].tipo_reg == TR_A_AAAA) {
							a = &res[i][j]->resp[k].a;
							if ((asprintf(&hostname_direto_autoritativo, "%d-%s-%s-%"PRUI64"-%d-%s.%s.", a->ttl, dns_servers->ip[i], hash_measures[i], res[i][j]->hora_recv/1000, config->intra_ttl_value, a->host_ip, config->sufixo_dns_autoritativo)) < 0) {
								INFO_PRINT ("Nao conseguiu alocar hostname para INTRA_TTL\n");
								saida (1);
							}
						}
					if (hostname_direto_autoritativo) {
						INFO_PRINT ("Enviando resultado de consulta INTRA_TTL para o autoritativo");
						libera_short_query_response(consulta_dns(hostname_direto_autoritativo, T_A, C_IN, ip_dns_autoritativo, 0, *offset_time));
						free (hostname_direto_autoritativo);
					}
				}
			}
			free (hostname);
		}
		else {
			for (j = NUM_TESTES_FIXOS ; j < NUM_TESTES_FIXOS + config->num_intra_ttl_queries; j++) {
				res [i][j] = NULL;
			}
		}
	}

	// teste A pre-ttl
	if (strnlen (ip_dns_autoritativo, 1)) {
		if ((asprintf(&hostname, "ttl%d.%s.", config->pre_ttl_value, config->sufixo_dns_autoritativo)) < 0) {
			INFO_PRINT ("Nao conseguiu alocar hostname para PRE_TTL\n");
			saida (1);
		}
		clock_gettime(CLOCK_MONOTONIC, &ts);
		hora_ini = TIMESPEC_NANOSECONDS(ts)/1000;
		for (j = NUM_TESTES_FIXOS + config->num_intra_ttl_queries; j < NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round; j++) {
			hostname_direto_autoritativo = NULL;
			for (i = 0; i < dns_servers->count; i++) {
				res[i][j] = consulta_dns(hostname, T_A, C_IN, dns_servers->ip[i], 0, *offset_time);
				if ((res[i][j]->rcode == 0) && (res[i][j]->num_resposta)) {
					for (k = 0; k < res[i][j]->num_resposta; k++)
						if (res[i][j]->resp[k].tipo_reg == TR_A_AAAA) {
							a = &res[i][j]->resp[k].a;
							if ((asprintf(&hostname_direto_autoritativo, "%d-%s-%s-%"PRUI64"-%d-%s.%s.", a->ttl, dns_servers->ip[i], hash_measures[i], res[i][j]->hora_recv/1000, config->pre_ttl_value, a->host_ip, config->sufixo_dns_autoritativo)) < 0) {
								INFO_PRINT ("Nao conseguiu alocar hostname para PRE_TTL\n");
								saida (1);
							}
						}
					if (hostname_direto_autoritativo) {
						INFO_PRINT ("Enviando resultado de consulta PRE_TTL para o autoritativo\n");
						libera_short_query_response(consulta_dns(hostname_direto_autoritativo, T_A, C_IN, ip_dns_autoritativo, 0, *offset_time));
						free (hostname_direto_autoritativo);
					}
				}
			}
			clock_gettime(CLOCK_MONOTONIC, &ts);
			hora_atual = TIMESPEC_NANOSECONDS(ts)/1000;
			num_pre_ttl_query = j - (NUM_TESTES_FIXOS + config->num_intra_ttl_queries) + 1;
			tempo_sleep = ((1e6*(config->pre_ttl_value - VALOR_TTL_ULTIMA_PRE_TTL)) + hora_ini - hora_atual) / (config->num_pre_ttl_queries_per_round - num_pre_ttl_query);
			if ((tempo_sleep > 0)  && (num_pre_ttl_query < config->num_pre_ttl_queries_per_round)) {
				INFO_PRINT ("dormindo %3.3lfs apos %d consultas a PRETTL", (double)tempo_sleep/1e6, num_pre_ttl_query);
				usleep (tempo_sleep);
			}
		}
		INFO_PRINT ("Dormindo %d segundos\n", config->pre_ttl_rounds_interval);
		sleep (config->pre_ttl_rounds_interval);
		segue = i = 0;
		do {
			i++;
			hora_ntp = get_ntp_usec ();
			if (hora_ntp) {
				clock_gettime(CLOCK_REALTIME, &ts);
				hora_local = TIMESPEC_NANOSECONDS(ts)/1000;
				*offset_time = hora_local - hora_ntp;
				if (labs (*offset_time) < 5000000) {
					// difereça menor que 5s
					INFO_PRINT ("diferenca para NTP: %"PRI64"us.", *offset_time);
					segue = 1;
					break;
				}
				else {
					INFO_PRINT ("diferenca para NTP muito grande: %"PRI64"us, hora_ntp: %"PRI64"us e hora_local: %"PRI64"us", *offset_time, hora_ntp, hora_local);
					sleep (5);
				}
			}
			else {
				ERROR_PRINT ("Nao conseguiu consultar ntp");
			}
		} while ((!segue) && (i <= 5));
		if (!segue) {
			ERROR_PRINT ("diferenca para NTP muito grande: %"PRI64"us. Abortando", *offset_time);
			saida (1);
		}
		clock_gettime(CLOCK_MONOTONIC, &ts);
		hora_ini = TIMESPEC_NANOSECONDS(ts)/1000;
		for (j = NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round; j < NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round * 2; j++) {
			for (i = 0; i < dns_servers->count; i++) {
				res[i][j] = consulta_dns(hostname, T_A, C_IN, dns_servers->ip[i], 0, *offset_time);
				hostname_direto_autoritativo = NULL;
				if ((res[i][j]->rcode == 0) && (res[i][j]->num_resposta)) {
					for (k = 0; k < res[i][j]->num_resposta; k++)
						if (res[i][j]->resp[k].tipo_reg == TR_A_AAAA) {
							a = &res[i][j]->resp[k].a;
							if ((asprintf(&hostname_direto_autoritativo, "%d-%s-%s-%"PRUI64"-%d-%s.%s.", a->ttl, dns_servers->ip[i], hash_measures[i], res[i][j]->hora_recv/1000, config->pre_ttl_value, a->host_ip, config->sufixo_dns_autoritativo)) < 0) {
								INFO_PRINT ("Nao conseguiu alocar hostname para PRE_TTL");
								saida (1);
							}
						}
					if (hostname_direto_autoritativo) {
						INFO_PRINT ("Enviando resultado de consulta PRE_TTL para o autoritativo");
						libera_short_query_response(consulta_dns(hostname_direto_autoritativo, T_A, C_IN, ip_dns_autoritativo, 0, *offset_time));
						free (hostname_direto_autoritativo);
					}
				}
			}
			clock_gettime(CLOCK_MONOTONIC, &ts);
			hora_atual = TIMESPEC_NANOSECONDS(ts)/1000;
			num_pre_ttl_query = j - (NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round) + 1;
			tempo_sleep = ((1e6*(config->pre_ttl_value - VALOR_TTL_ULTIMA_PRE_TTL)) + hora_ini - hora_atual) / (config->num_pre_ttl_queries_per_round - num_pre_ttl_query);

			if ((tempo_sleep > 0)  && (num_pre_ttl_query < config->num_pre_ttl_queries_per_round)) {
				INFO_PRINT ("dormindo %3.3lfs apos %d consultas a PRETTL", (double)tempo_sleep/1e6, num_pre_ttl_query);
				usleep (tempo_sleep);
			}
			hostname_direto_autoritativo = NULL;
		}

		free (hostname);
	}
	else {
		for (j = NUM_TESTES_FIXOS + config->num_intra_ttl_queries; j < NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round * 2; j++) {
			for (i = 0; i < dns_servers->count; i++) {
				res [i][j] = NULL;
			}
		}
	}

	// testes de dns realizados
	return res;
}


int avalia_e_envia_resultados (SHORT_QUERY_RESPONSE *** res, DNS_SERVERS *dns_servers, char *mac_address, CONFIG_SIMET* config, SSL_CTX *ssl_ctx, char **hash_measures, uint64_t offset_time, char *ip_dns_autoritativo, int family) {
	int32_t i, j, k;
	TIPO_TESTE tipo_teste;
	uint8_t *respeita_valor_ttl, *valida_dnssec, *valida_dnssec_expirado, *aceita_ipv6, *respeita_reg_inexistente;
	char hostname [257];
	char hostbind [257];
	char versionbind [257];
	char tipo_teste_bd[257];
	char *aux, *ret_ws;
    A_AAAA_SHORT_QUERY_RESPONSE *a = NULL;
    RRSIG_SHORT_QUERY_RESPONSE *rrsig = NULL;
	char *json_str = NULL, *temp;
	struct sockaddr_storage addr;
	struct in_addr *addr_v4;


    respeita_valor_ttl = (uint8_t *) malloc (sizeof (uint8_t) * dns_servers->count);
    valida_dnssec = (uint8_t *) malloc (sizeof (uint8_t) * dns_servers->count);
    valida_dnssec_expirado = (uint8_t *) malloc (sizeof (uint8_t) * dns_servers->count);
    aceita_ipv6 = (uint8_t *) malloc (sizeof (uint8_t) * dns_servers->count);
	respeita_reg_inexistente = (uint8_t *) malloc (sizeof (uint8_t) * dns_servers->count);
    if ((!respeita_valor_ttl) || (!valida_dnssec) || (!valida_dnssec_expirado) ||
		(!aceita_ipv6) || (!respeita_reg_inexistente)) {
        INFO_PRINT ("sem memoria para alocar arrays de validacoes de servidores de DNS\n");
        saida (1);
    }

    memset (respeita_valor_ttl, 1, sizeof (uint8_t) * dns_servers->count);
    memset (respeita_reg_inexistente, 1, sizeof (uint8_t) * dns_servers->count);
	// valida testes de DNS



	for (i = 0; i < dns_servers->count; i++) {
    	if ((res[i][DNSSEC_INVALIDO]->rcode == 0) && (res[i][DNSSEC_INVALIDO]->num_resposta > 0)) {
	    // Não valida dnssec
    	    valida_dnssec [i] = 0;
        }
        else {
        // valida dnssec
    	    valida_dnssec [i] = 1;
        }
    	if ((res[i][DNSSEC_EXPIRADO]->rcode == 0) && (res[i][DNSSEC_EXPIRADO]->num_resposta > 0)) {
	    // Não valida dnssec expirado
    	    valida_dnssec_expirado [i] = 0;
        }
        else {
        // valida dnssec expirado
    	    valida_dnssec_expirado [i] = 1;
        }
    	if ((res[i][IPV6]->rcode == 0) && (res[i][IPV6]->num_resposta > 0)) {
	    // ipv6
    	    aceita_ipv6 [i] = 1;
        }
        else {
        // nao aceita ipv6
    	    aceita_ipv6 [i] = 0;
        }
        if ((respeita_valor_ttl[i]) && (strnlen (ip_dns_autoritativo, 1)))
			for (k = NUM_TESTES_FIXOS; k < NUM_TESTES_FIXOS + config->num_intra_ttl_queries; k++)
				if ((res[i][k]->rcode == 0) && (res[i][k]->num_resposta > 0))
					// respeita_valor_ttl
					for (j = 0; j < res[i][k]->num_resposta; j++)
						if ((res[i][k]->resp[j].tipo_reg == TR_A_AAAA) && (res[i][k]->resp[j].a.ttl > config->intra_ttl_value)) {
							respeita_valor_ttl[i] = 0;
							break;
						}
        if ((respeita_valor_ttl[i]) && (strnlen (ip_dns_autoritativo, 1)))
			for (k = NUM_TESTES_FIXOS + config->num_intra_ttl_queries; k < NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round * 2; k++)
				if ((res[i][k]->rcode == 0) && (res[i][k]->num_resposta > 0))
					// respeita_valor_ttl
					for (j = 0; j < res[i][k]->num_resposta; j++)
						if ((res[i][k]->resp[j].tipo_reg == TR_A_AAAA) && (res[i][k]->resp[j].a.ttl > config->pre_ttl_value)) {
							respeita_valor_ttl[i] = 0;
							break;
						}
		// respeita_reg_inexistente
    	if ((res[i][DNS_INEXISTENTE_1]->rcode == 0) && (res[i][DNS_INEXISTENTE_1]->num_resposta > 0))
			respeita_reg_inexistente[i] = 0;
    	if ((respeita_reg_inexistente[i]) && (res[i][DNS_INEXISTENTE_2]->rcode == 0) && (res[i][DNS_INEXISTENTE_2]->num_resposta > 0))
			respeita_reg_inexistente[i] = 0;
    }



	// envia resultados de todos os servidores
	if (asprintf (&json_str, "{\"dns_test\":{\"hash_device\":\"%s\",\"dns_servers\":[", mac_address) < 0) {
		ERROR_PRINT ("nao conseguiu alocar json str\n");
		saida (1);
	}
	for (i = 0; i < dns_servers->count; i++) {
		if (res[i][HOSTBIND]->num_resposta == 0) {
			switch (res[i][HOSTBIND]->rcode) {
				case 2:
					strcpy (hostbind, "BINDNAME_SERVFAIL");
					break;
				case 5:
					strcpy (hostbind, "BINDNAME_REFUSED");
					break;
				case 255:
					strcpy (hostbind, "BINDNAME_TIMEOUT");
					break;
				default:
					sprintf (hostbind, "BINDNAME_SEM_RESPOSTA_RCODE_%d", res[i][HOSTBIND]->rcode);
					break;
			}
		}
		else {
			strcpy (hostbind, res[i][HOSTBIND]->resp[0].txt.bindproperty);
			converte_str_ip (dns_servers->ip[i], &addr);
			addr_v4 = &(((struct sockaddr_in *) &addr)->sin_addr);
			if (is_rede_local (addr_v4))
				strcat (hostbind, "-DNS_REDE_LOCAL");
		}
		if (res[i][VERSIONBIND]->num_resposta == 0) {
			switch (res[i][VERSIONBIND]->rcode) {
				case 2:
					strcpy (versionbind, "BINDVERSION_SERVFAIL");
					break;
				case 5:
					strcpy (versionbind, "BINDVERSION_REFUSED");
					break;
				case 255:
					strcpy (versionbind, "BINDVERSION_TIMEOUT");
					break;
				default:
					sprintf (versionbind, "BINDVERSION_SEM_RESPOSTA_RCODE_%d", res[i][VERSIONBIND]->rcode);
					break;
			}
		}
		else {
			strcpy (versionbind, res[i][VERSIONBIND]->resp[0].txt.bindproperty);
		}

		if (asprintf (&temp, "{\"hash_measure\":\"%s\",\"ip\":\"%s\",\"bindname\":\"%s\","
			"\"bindversion\":\"%s\",\"valida_dnssec_invalido\":%d,\"valida_dnssec_expirado\":%d,"
			"\"ipv6\":%d,\"respeita_valor_ttl\":%d,\"respeita_reg_inexistente\":%d,\"consultas_dns\":[",
			hash_measures[i], dns_servers->ip[i], hostbind, versionbind, valida_dnssec[i], valida_dnssec_expirado[i], aceita_ipv6[i], respeita_valor_ttl[i], respeita_reg_inexistente[i]) < 0) {
				ERROR_PRINT ("nao conseguiu alocar json DNSServer\n");
				saida (1);
		}
		aux = (char*) realloc (json_str, strlen (json_str) + strlen (temp) + 1);
		if (!aux) {
			ERROR_PRINT ("nao conseguiu realocar json DNSServer (2)\n");
			saida(1);
		}
		json_str = aux;
		strcat (json_str, temp);
		free (temp);

	// envia resultados das consultas
		for (tipo_teste = DNSSEC_INVALIDO; tipo_teste < NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round * 2; tipo_teste++) {
			// não envia resultado da consulta do bindname e versionbind
			if ((tipo_teste >= NUM_TESTES_FIXOS) && (!strnlen (ip_dns_autoritativo, 1)))
				continue; // não encontrou nosso autoritário
			strcpy (hostname, res[i][tipo_teste]->hostname);
			switch (tipo_teste) {
				case DNSSEC_INVALIDO:
					strcpy (tipo_teste_bd, "DNSSEC_INVALIDO");
					break;
				case DNSSEC_VALIDO:
					strcpy (tipo_teste_bd, "DNSSEC_VALIDO");
					break;
				case DNSSEC_EXPIRADO:
					strcpy (tipo_teste_bd, "DNSSEC_EXPIRADO");
					break;
				case DNS_INEXISTENTE_1:
					strcpy (hostname, "www.MAC_ADDRESSsimet.com.br.");
					strcpy (tipo_teste_bd, "DNS_INEXSTENTE");
					break;
				case IPV6:
					strcpy (tipo_teste_bd, "IPV6");
					break;
				case DNS_INEXISTENTE_2:
					strcpy (tipo_teste_bd, "DNS_INEXSTENTE");
					break;
				default:
					if ((tipo_teste >= NUM_TESTES_FIXOS) && (tipo_teste < NUM_TESTES_FIXOS + config->num_intra_ttl_queries))
						strcpy (tipo_teste_bd, "INTRATTL");
					else if ((tipo_teste >= NUM_TESTES_FIXOS + config->num_intra_ttl_queries) && (tipo_teste < NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round))
						strcpy (tipo_teste_bd, "PRETTL_R_1");
					else if ((tipo_teste >= NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round) && (tipo_teste < NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round * 2))
						strcpy (tipo_teste_bd, "PRETTL_R_2");
					else
						strcpy (tipo_teste_bd, "TESTE_DESCONHECIDO");
			}
			// aponta para registro de assinatura
			rrsig = NULL;
			a = NULL;
			if (res[i][tipo_teste]->rcode) {
				if (asprintf (&temp, "{\"tipo_teste\":\"%s\",\"hostname\":\"%s\","
					"\"tempo_resposta\":%"PRUI64",\"ntp_resposta\":%"PRUI64",\"rcode\":%d},",
					tipo_teste_bd, hostname, res[i][tipo_teste]->tempo_resposta,
					res[i][tipo_teste]->hora_recv/1000, res[i][tipo_teste]->rcode) < 0) {
						ERROR_PRINT ("nao conseguiu alocar json DNSQuery\n");
						saida (1);
				}
				aux = (char*) realloc (json_str, strlen (json_str) + strlen (temp) + 5); //+5 para poder concatenar ]s e }s
				if (!aux) {
					ERROR_PRINT ("nao conseguiu realocar json DNSQuery(2)\n");
					saida(1);
				}
				json_str = aux;
				strcat (json_str, temp);
				free (temp);
			}
			else {
				for (j = 0; j < res[i][tipo_teste]->num_resposta; j++)
					if (res[i][tipo_teste]->resp[j].tipo_reg == TR_RRSIG) {
						rrsig = &res[i][tipo_teste]->resp[j].rrsig;
						break;
					}
				for (j = 0; j < res[i][tipo_teste]->num_resposta; j++)
					if (res[i][tipo_teste]->resp[j].tipo_reg == TR_A_AAAA) {
						a = &res[i][tipo_teste]->resp[j].a;
						if (rrsig) {
							if (asprintf (&temp, "{\"tipo_teste\":\"%s\",\"hostname\":\"%s\","
								"\"tempo_resposta\":%"PRUI64",\"ntp_resposta\":%"PRUI64",\"rcode\":%d,"
								"\"resposta\":{\"hostip\":\"%s\",\"ttl\":%d,\"n_rrsig\":1,"
								"\"rrsig\":{\"assinatura\":\"%s\",\"tipo\":%d,\"algoritmo\":%d,"
								"\"labels\":%d,\"ttl_original\":%d,\"assinante\":\"%s\","
								"\"data_assinatura\":%d,\"data_validade\":%d,"
								"\"id_signing_key_fingerprint\":%d}}},",
								tipo_teste_bd, hostname, res[i][tipo_teste]->tempo_resposta,
								res[i][tipo_teste]->hora_recv/1000, res[i][tipo_teste]->rcode, a->host_ip,
								a->ttl, rrsig->assinatura, rrsig->rrsig.type_covered,
								rrsig->rrsig.algorithm, rrsig->rrsig.labels, rrsig->rrsig.original_ttl,
								rrsig->assinante, rrsig->rrsig.time_signed,
								rrsig->rrsig.signature_expiration,
								rrsig->rrsig.id_of_signing_key_fingerprint) < 0) {
									ERROR_PRINT ("nao conseguiu alocar json DNSQuery (3)\n");
									saida (1);
							}
							aux = (char*) realloc (json_str, strlen (json_str) + strlen (temp) + 5); //+5 para poder concatenar ]s e }s
							if (!aux) {
								ERROR_PRINT ("nao conseguiu realocar json DNSQuery (4)\n");
								saida(1);
							}
							json_str = aux;
							strcat (json_str, temp);
							free (temp);

						}
						else {
							if (asprintf (&temp, "{\"tipo_teste\":\"%s\",\"hostname\":\"%s\","
								"\"tempo_resposta\":%"PRUI64",\"ntp_resposta\":%"PRUI64",\"rcode\":%d,"
								"\"resposta\":{\"hostip\":\"%s\",\"ttl\":%d,\"n_rrsig\":0}},",
								tipo_teste_bd, hostname, res[i][tipo_teste]->tempo_resposta,
								res[i][tipo_teste]->hora_recv/1000, res[i][tipo_teste]->rcode, a->host_ip,
								a->ttl) < 0) {
									ERROR_PRINT ("nao conseguiu alocar json DNSQuery (5)\n");
									saida (1);
							}
							aux = (char*) realloc (json_str, strlen (json_str) + strlen (temp) + 5); //+5 para poder concatenar ]s e }s
							if (!aux) {
								ERROR_PRINT ("nao conseguiu realocar json DNSQuery (6)\n");
								saida(1);
							}
							json_str = aux;
							strcat (json_str, temp);
							free (temp);
						}

					}

			}
		}
	json_str[strlen(json_str) - 1] = '\0';
	strcat(json_str, "]},");
	}

	json_str[strlen(json_str) - 1] = '\0';
	strcat(json_str, "]}}");
	INFO_PRINT ("json: %s\n", json_str);
	sleep (5); // dorme 5s para garantir que o autoritativo envie seus webservices
	ret_ws = chama_web_service_put_ssl (ssl_ctx, config->host, config->port, family, NULL, json_str, "/%s/dns-measures", config->context_web_persistence_optional);
	if (ret_ws) {
		INFO_PRINT ("resultado put: [%s]\n", ret_ws);
		free (ret_ws);
	}
	free (json_str);

	free (valida_dnssec);
	free (respeita_valor_ttl);
	free (aceita_ipv6);
	free (valida_dnssec_expirado);
	free (respeita_reg_inexistente);
	return 0;
}

void parse_args_dns (void * options, char doc[], int * family) {

	const char * aux;
	parse_args_basico(options, doc, family);
	if (gopt(options, 'f')) {
		force_no_sleep = 1;
	}
	if (gopt_arg(options, 's', &aux)) {
		servidor = (char*) aux;
	}
	if (gopt_arg(options, 'n', &aux)) {
		nome_host = (char*) aux;
	}
}

void *args_dns (int *argc, const char *argv[]) {
	void *options =
	gopt_sort(argc, argv,
		gopt_start(
		DEFAULT_OPTS,
		gopt_option( 'f', 0, gopt_shorts( 'f' ), gopt_longs( "force" )),
		gopt_option( 's', GOPT_ARG, gopt_shorts( 's' ), gopt_longs( "server" )),
		gopt_option( 'n', GOPT_ARG, gopt_shorts( 'n' ), gopt_longs( "name" ))
		));

	parse_verbose (options);
	return options;
}

int insere_ip (ip_list *ips_nos_servidores_dns, char* host_ip) {
	int i;
	for (i = 0; i < ips_nos_servidores_dns->num_ips; i++) {
		if (strncmp (host_ip, (char*)ips_nos_servidores_dns->ips[i], strlen (ips_nos_servidores_dns->ips[i])) == 0)
			return 0;
	}
	ips_nos_servidores_dns->ips = (char **)realloc (ips_nos_servidores_dns->ips, sizeof (char*) * (ips_nos_servidores_dns->num_ips + 1));
	if (!ips_nos_servidores_dns->ips)
		return -1;
	INFO_PRINT ("insere indice: %d, ip %s\n", ips_nos_servidores_dns->num_ips, host_ip);
	ips_nos_servidores_dns->ips[ips_nos_servidores_dns->num_ips] = strdup (host_ip);
	if (!ips_nos_servidores_dns->ips[ips_nos_servidores_dns->num_ips])
		return -1;
	ips_nos_servidores_dns->num_ips++;
	return 1;
}


void libera_ip_list (ip_list *ips_list) {
	int i;
	for (i = 0; i < ips_list->num_ips; i++)
		free (ips_list->ips[i]);
	free (ips_list->ips);
	free (ips_list);
}
char *str_ips(ip_list *ips_list) {
	char *temp, *aux;
	int i, rc = 0;
	temp = NULL;
	for (i = 0; i < ips_list->num_ips; i++) {
		aux = temp;
		rc = asprintf (&temp, "%s\"%s\",", (aux)?aux:"", ips_list->ips[i]);
		if (rc <= 0) {
			ERROR_PRINT ("mem");
			saida (1);
		}
		if (aux)
			free (aux);
	}
	if (ips_list->num_ips)
		temp [rc - 1] = '\0';
	return temp;
}
ip_list *ips_unicos (SHORT_QUERY_RESPONSE ***res, int index_dns_server, char* ip_dns_autoritativo, CONFIG_SIMET *config) {
	int i, j;
	ip_list *ips_nos_servidores_dns;

	if (!strnlen (ip_dns_autoritativo, 1))
		return NULL;

	ips_nos_servidores_dns = (ip_list *)malloc (sizeof (ip_list));
	if (!ips_nos_servidores_dns)
		return NULL;
	ips_nos_servidores_dns->num_ips = 0;
	ips_nos_servidores_dns->ips = NULL;

	for (i = NUM_TESTES_FIXOS; i < NUM_TESTES_FIXOS + config->num_intra_ttl_queries + config->num_pre_ttl_queries_per_round * 2; i++)
		if ((res[index_dns_server][i]->rcode == 0) && (res[index_dns_server][i]->num_resposta > 0))
			// respeita_valor_ttl
			for (j = 0; j < res[index_dns_server][i]->num_resposta; j++)
				if ((res[index_dns_server][i]->resp[j].tipo_reg == TR_A_AAAA) &&
					(insere_ip (ips_nos_servidores_dns, (char*) res[index_dns_server][i]->resp[j].a.host_ip) < 0))
					ERROR_PRINT ("nao conseguiu inserir ip unico na lista de nos servidores dns\n");

	return ips_nos_servidores_dns;
}

malicious_dns_array* parse_malicious_dns_json (json_object *jobj) {
	int i;
	json_object *jvalue_array_item, *jvalue_array, *jobj_aux;
	malicious_dns_array *array;
	char *aux;
	array = (malicious_dns_array*) malloc (sizeof (malicious_dns_array));
	if (!array) {
		INFO_PRINT ("nao foi possivel alocar estrutura array\n");
		return NULL;
	}

// parametros array
	if (!json_object_object_get_ex (jobj, "idMaliciousSet", &jobj_aux)) {
		json_object_put (jobj);
		return NULL;
	}
	array->id_array_day = json_object_get_int (jobj_aux);
	//json_object_put (jobj_aux);
	if (!json_object_object_get_ex (jobj, "siteList", &jvalue_array)) {
		json_object_put (jobj);
		return NULL;
	}
	array->num_sites = json_object_array_length (jvalue_array); /*Getting the length of the array*/

	array->sites = (char**) malloc (sizeof (char*) * array->num_sites);
	if (!array->sites) {
		ERROR_PRINT ("nao alocou array de sites");
		saida (1);
	}

	for (i = 0; i < array->num_sites; i++) {
		jvalue_array_item = json_object_array_get_idx (jvalue_array, i); /*Getting the array element at position i*/
		aux = strdup (json_object_get_string (jvalue_array_item));
		if (!aux) {
			ERROR_PRINT ("nao foi possivel alocar string array");
			saida (1);
		}

		//json_object_put (jvalue_array_item);
		array->sites[i] = aux;

	}
	json_object_put (jobj);
	return array;
}

void realiza_testes_dns_malicioso (SHORT_QUERY_RESPONSE ***res, DNS_SERVERS *dns_servers, char *mac_address, char **hash_measures, int64_t *offset_time, char* ip_dns_autoritativo, CONFIG_SIMET *config, int family, SSL_CTX *ssl_ctx) {
	int i, j, k, rc = 0, num_answers;
	SHORT_QUERY_RESPONSE *query_v4, *query_v6;
	char *aux, *temp = NULL, *nome, *conteudo, *ips = NULL, *ret_ws;
	malicious_dns_array* dominio;
	ip_list *ips_unicos_nos_clusters_servidores;
	json_object *jobj;
	enum json_tokener_error error = json_tokener_success;

	//obtem lista de domínios para serem testados por malicidade
	conteudo = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, family, NULL, 0, "/%s/%s", config->context_optional, "GetMaliciousDnsDomains");
	//asprintf (&conteudo, "{\"idMaliciousSet\":1,\"siteList\":[\"bankline.itau.com.br\",\"cartaobndes.gov.br\",\"internetbanking.caixa.gov.br\",\"internetbankingpf.caixa.gov.br\",\"sistemas.bndes.gov.br\",\"web.bndes.gov.br\",\"www.americanexpress.com\",\"www.americanexpress.com.br\",\"www.bancobrasil.com.br\",\"www.bancodobrasil.com.br\",\"www.bancoitau.com.br\",\"www.banknet.brb.com.br\",\"www.bb.com.br\",\"www.bnb.gov.br\",\"www.bndes.gov.br\",\"www.bradesco.b.br\",\"www.bradesco.com\",\"www.bradesco.com.br\",\"www.bradesconetempresa.com.br\",\"www.bradescopessoajuridica.com.br\",\"www.bradescoprime.com.br\",\"www.brb.com.br\",\"www.caixa.com.br\",\"www.caixa.gov.br\",\"www.caixaeconomica.com.br\",\"www.caixaeconomica.gov.br\",\"www.caixaeconomicafederal.com.br\",\"www.caixaeconomicafederal.gov.br\",\"www.cartaobndes.gov.br\",\"www.cef.com.br\",\"www.cef.gov.br\",\"www.cetelem.com.br\",\"www.cielo.com.br\",\"www.citibank.com.br\",\"www.credicard.com.br\",\"www.facebook.com\",\"www.facebook.com.br\",\"www.globo.com\",\"www.gmail.com\",\"www.gmail.com.br\",\"www.google.com\",\"www.google.com.br\",\"www.hipercard.com.br\",\"www.hotmail.com\",\"www.hotmail.com.br\",\"www.hsbc.b.br\",\"www.hsbc.com.br\",\"www.hsbcadvance.com.br\",\"www.hsbcpremier.com.br\",\"www.itau.com.br\",\"www.itaupersonnalite.com.br\",\"www.live.com\",\"www.mercadolivre.com.br\",\"www.mercantil.com.br\",\"www.mercantildobrasil.com.br\",\"www.paypal.com\",\"www.paypal.com.br\",\"www.portal.brb.com.br\",\"www.safra.com.br\",\"www.safranet.com.br\",\"www.santander.com.br\",\"www.santanderempresarial.com.br\",\"www.santandernet.com.br\",\"www.serasa.com\",\"www.serasa.com.br\",\"www.serasaexperian.com\",\"www.serasaexperian.com.br\",\"www.sicredi.com.br\",\"www.tribanco.com.br\",\"www.tribancoonline.com.br\",\"www.twitter.com\",\"www.uol.com.br\",\"www.yahoo.com\",\"www.yahoo.com.br\",\"www.youtube.com\",\"www.youtube.com.br\",\"www14.bb.com.br\",\"www57.bb.com.br\"]}");

	INFO_PRINT ("[%s]\n", conteudo);
	if (strnlen (conteudo, 1) == 0)
		return;
	jobj = json_tokener_parse_verbose(conteudo, &error);
	dominio = parse_malicious_dns_json (jobj);
	if (!dominio) {
		INFO_PRINT ("falha na interpretacao do json\n");
		saida (1);
	}

	free (conteudo);

	// escreve prefixo json
	if (asprintf (&temp, "{\"hashDevice\":\"%s\",\"idMaliciousSet\":%d,\"dnsServer\":[", mac_address, dominio->id_array_day) < 0) {
		ERROR_PRINT ("mem");
		saida (1);
	}

	for (i = 0; i < dns_servers->count; i++) {
		ips_unicos_nos_clusters_servidores = ips_unicos (res, i, ip_dns_autoritativo, config);
		ips = str_ips (ips_unicos_nos_clusters_servidores);
		libera_ip_list (ips_unicos_nos_clusters_servidores);


		// escreve cabeçalho dns_server
		aux = temp;
		if (ips) {
			if (asprintf (&temp, "%s{\"hashMeasure\":\"%s\",\"serverIp\":\"%s\",\"ipNodesDNSServer\":[%s],\"results\":[", aux, hash_measures[i], dns_servers->ip[i], ips) < 0) {
				free (ips);
				ERROR_PRINT ("mem");
				saida (1);
			}
			free (ips);
		}
		else {
			if (asprintf (&temp, "%s{\"hashMeasure\":\"%s\",\"serverIp\":\"%s\",\"results\":[", aux, hash_measures[i], dns_servers->ip[i]) < 0) {
				ERROR_PRINT ("mem");
				saida (1);
			}
		}
		if (aux)
			free (aux);

		for (j = 0; j < dominio->num_sites; j++) {
			// escreve json resposta
			aux = temp;
			nome = obtem_hostname (dominio->sites[j]);
			query_v4 = consulta_dns (nome, T_A, C_IN, dns_servers->ip[i], 0, 0);
			query_v6 = consulta_dns (nome, T_AAAA, C_IN, dns_servers->ip[i], 0, 0);
			num_answers = 0;

			for (k = 0; k < query_v4->num_resposta; k++)
				if  ((query_v4->resp[k].tipo_reg == TR_A_AAAA) && (query_v4->resp[k].a.host_ip))
					num_answers++;
			for (k = 0; k < query_v6->num_resposta; k++)
				if  ((query_v6->resp[k].tipo_reg == TR_A_AAAA) && (query_v6->resp[k].a.host_ip))
					num_answers++;
			aux = temp;
			rc = asprintf (&temp, "%s{\"site\":\"%s\",\"numAnswers\":%d,", aux, dominio->sites[j], num_answers);
			if (rc < 0) {
				ERROR_PRINT ("mem");
				saida (1);
			}
			free (aux);
			if (num_answers) {
				aux = temp;
				if (asprintf (&temp, "%s\"answers\":[", aux) < 0) {
					ERROR_PRINT ("mem");
					saida (1);
				}
				free (aux);

				for (k = 0; k < query_v4->num_resposta; k++)
					if  (query_v4->resp[k].tipo_reg == TR_A_AAAA) {
						aux = temp;
						rc = asprintf (&temp, "%s{\"authoritative\":\"%s\",\"ip\":\"%s\"},", aux, query_v4->authoritative_flag?"true":"false",query_v4->resp[k].a.host_ip);
						if (rc < 0) {
							ERROR_PRINT ("mem");
							saida (1);
						}
						free (aux);
						break;
					}
				for (k = 0; k < query_v6->num_resposta; k++)
					if  (query_v6->resp[k].tipo_reg == TR_A_AAAA) {
						aux = temp;
						rc = asprintf (&temp, "%s{\"authoritative\":\"%s\",\"ip\":\"%s\"},", aux, query_v6->authoritative_flag?"true":"false",query_v6->resp[k].a.host_ip);
						if (rc < 0) {
							ERROR_PRINT ("mem");
							saida (1);
						}
						free (aux);
						break;
					}
			}
			temp [rc - 1] = '\0';
			aux = temp;
			rc = asprintf (&temp, "%s%s},", aux, (num_answers)?"]":"");
			if (rc < 0) {
				ERROR_PRINT ("mem");
				saida (1);
			}
			free (aux);
			libera_short_query_response(query_v4);
			libera_short_query_response(query_v6);
			free (nome);

		}
		if (dominio->num_sites)
			temp [rc - 1] = '\0';
		aux = temp;
		rc = asprintf (&temp, "%s]},", aux);
		if (rc < 0) {
			ERROR_PRINT ("mem");
			saida (1);
		}
		free (aux);
	}
	// escreve sufixo json
	if (dns_servers->count)
		temp [rc - 1] = '\0';
	// escreve rodapé dns_server
	aux = temp;

	rc = asprintf (&temp, "%s]}", aux);
	if (rc < 0) {
		ERROR_PRINT ("mem");
		saida (1);
	}
	free (aux);
	INFO_PRINT ("[%s]\n", temp);
	ret_ws = chama_web_service_put_ssl (ssl_ctx, config->host, config->port, family, NULL, temp, "/%s/malicious-dns-domains", config->context_web_persistence_optional);
	if (ret_ws) {
		INFO_PRINT ("resultado put: [%s]\n", ret_ws);
		free (ret_ws);
	}

	//free dominio
	for (i = 0; i < dominio->num_sites; i++)
		free (dominio->sites[i]);

	free (dominio->sites);
	free (dominio);
	free (temp);
}


int main_dns (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options) {
	SHORT_QUERY_RESPONSE ***res; //ponteiro para uma matriz de ponteiros
	DNS_SERVERS *dns_servers;
	const char versao [] = VERSAO_DNS;
	char doc[] =
		"uso:\n\tsimet_dns\n\n\tTesta a qualidade dos servidores de DNS\n\n\tsimet_dns -s IP servidor de DNS -n NOME a ser consultado\n\n";
	char **hash_measures, *ip_dns_autoritativo, *aux;
	int i, segue;
	uint64_t hora_ntp, hora_local, tempo_espera;
	int64_t offset_time;
	struct timespec ts;
	int family = 4; // migrar servidor modificado de dns antes de mudar para 0

	if (options)
		parse_args_dns (options, doc, &family);

	//obtem servidores de dns
	dns_servers = get_dns_servers(family);
	if ((!dns_servers) || (!dns_servers->count))
	{
		INFO_PRINT("Nenhum servidor de DNS valido encontrado");
		return 1;
	}


	if ((servidor) && (nome_host)) {
		fileout = stdout;
		aux = nome_host;
		nome_host = obtem_hostname (aux);
		libera_short_query_response(consulta_dns(nome_host, T_A, C_IN, servidor, 0, 0));
		return (EXIT_SUCCESS);
	}
	else if ((servidor) && (!nome_host)) {
		fprintf(stdout, "%s", doc);
		return (EXIT_SUCCESS);
	}
	else if ((!servidor) && (nome_host)) {
		fileout = stderr;
		aux = nome_host;
		nome_host = obtem_hostname (aux);
		for (i = 0; i < dns_servers->count; i++)
			libera_short_query_response(consulta_dns(nome_host, T_A, C_IN, dns_servers->ip [i], 0, 0));
		return (EXIT_SUCCESS);
	}


	fileout = stderr;
	INFO_PRINT("%s versao %s", nome, versao);
	if (!force_no_sleep) {
		tempo_espera =  get_rand_i () % 60;
		INFO_PRINT("Aguardando %"PRUI64"s", tempo_espera);
		if (tempo_espera)
			sleep (tempo_espera);
	}




	hash_measures = (char**) malloc (sizeof (char*) * (dns_servers->count));
	if (!hash_measures) {
		INFO_PRINT("nao consegui alocar array para hash_measures");
		saida (1);
	}

	//obtem hashMeasures
	for (i = 0; i < dns_servers->count; i++)
		hash_measures[i] = get_hash_measure (config->host, config->port, family, config->context_root, mac_address, ssl_ctx);

	ip_dns_autoritativo = get_ip_by_host(config->dns_autoritativo, family);
	if (!ip_dns_autoritativo)
		INFO_PRINT ("IP autoritativo [%s] invalido. Testes de INTRA-TTL e PRE_TTL nao serao realizados", ip_dns_autoritativo);
	else
		INFO_PRINT ("IP autoritativo [%s]", ip_dns_autoritativo);


	segue = i = 0;
	do {
		i++;
		hora_ntp = get_ntp_usec ();
		if (hora_ntp) {
			clock_gettime(CLOCK_REALTIME, &ts);
			hora_local = TIMESPEC_NANOSECONDS(ts)/1000;
			offset_time = hora_local - hora_ntp;
			if (labs (offset_time) < 5e6) {
				// difereça menor que 5s
				INFO_PRINT ("diferenca para NTP: %"PRI64"us", offset_time);
				segue = 1;
				break;
			}
			else {
				INFO_PRINT ("diferenca para NTP muito grande: %"PRI64"us, hora_ntp: %"PRI64"us e hora_local: %"PRI64"us", offset_time, hora_ntp, hora_local);
				sleep (5);
			}
		}
		else {
			ERROR_PRINT ("Nao conseguiu consultar ntp");
		}
	} while ((!segue) && (i <= 5));
	if (!segue) {
		ERROR_PRINT ("diferenca para NTP muito grande: %"PRI64"us. Abortando", offset_time);
		saida (1);
	}

	//faz testes de dns
	res = realiza_testes_dns (dns_servers, mac_address, hash_measures, &offset_time, ip_dns_autoritativo, config, family);
	avalia_e_envia_resultados (res, dns_servers, mac_address, config, ssl_ctx, hash_measures, offset_time,  ip_dns_autoritativo, family);

	realiza_testes_dns_malicioso (res, dns_servers, mac_address, hash_measures, &offset_time, ip_dns_autoritativo, config, family, ssl_ctx);


	//libera memoria DNS
	libera_short_query_response_matrix (res, dns_servers, config);
	for (i = 0; i < dns_servers->count; i++)
		free (hash_measures[i]);
	free (hash_measures);
	libera_dns_servers (dns_servers);
	if (ip_dns_autoritativo)
		free (ip_dns_autoritativo);

	return 0;
}





unsigned char *converte_time (int32_t tm_sec) {
	char *timebuf;
	const time_t nsec = tm_sec;
	const time_t *pnsec = &nsec;
	struct tm *tm_val;
	char *aux;

	timebuf = malloc (26);
	tm_val = malloc (sizeof (struct tm));
	if ((!timebuf) || (!tm_val)) {
		INFO_PRINT ("Naso conseguiu converter hora");
		saida (1);
	}
	localtime_r(pnsec, tm_val);
	sprintf (timebuf, "%s", asctime (tm_val));
	aux = strrchr(timebuf, '\n');
	if (aux)
		*aux = '\0';
	free (tm_val);

	return (unsigned char*)timebuf;
}




void tipo_registro (int32_t query_type, char *tipo) {
	switch (query_type) {
		case T_A:
			strcpy(tipo, "A");
			break;
		case T_NS:
			strcpy(tipo, "NS");
			break;
		case T_CNAME:
			strcpy(tipo, "CNAME");
			break;
		case T_SOA:
			strcpy(tipo, "SOA");
			break;
		case T_PTR:
			strcpy(tipo, "PTR");
			break;
		case T_MX:
			strcpy(tipo, "MX");
			break;
		case T_TXT:
			strcpy(tipo, "TXT");
			break;
		case T_AAAA:
			strcpy(tipo, "AAAA");
			break;
		case T_RRSIG:
			strcpy(tipo, "RRSIG");
			break;
		case T_NSEC:
			strcpy(tipo, "NSEC");
			break;
		case T_DNSKEY:
			strcpy(tipo, "DNSKEY");
			break;
		default:
			sprintf (tipo, "COD %d", query_type);
	}
}

/*
* Perform a DNS query by sending a packet
* */

QUERY_RESPONSE* envia_recebe_pacotes_dns (char *host , int32_t query_type, int32_t query_class, char* dns_server, int16_t dnssec, int s, uint16_t q_id, int family) {
	unsigned char buf[65536], *qname;
	int32_t i, length, flags;
	uint64_t tempo_aux;
	long size;
	QUERY_RESPONSE *ret;
	char tipo[10];
	struct timeval tv_timeo;
	fd_set rset, wset, set_master;
	int rc;

	struct sockaddr_storage dest;

	DNS_HEADER *dns = NULL;
	QUESTION *qinfo = NULL;
	ADDITIONAL_RECORDS *add_rec = NULL;
	struct timespec init_ts, end_ts, start_mono, end_mono;
	struct tm time_query;
	time_t time_tmp;


	tipo_registro (query_type, tipo);
	fprintf(fileout, "\n\nResolvendo: %s, tipo: %s, classe: %s, servidor dns: %s, dnssec: %s, q_id %u\n" , host, tipo, query_class == C_IN?"Internet":"Chaos", dns_server, dnssec?"true":"false", q_id);


	fill_socket_addr(&dest, dns_server, 53, family);

	//Monta a query
	dns = (DNS_HEADER *)&buf;

	dns->id = htobe16(q_id);
	dns->qr = 0;
	dns->opcode = 0;
	dns->aa = 0;
	dns->tc = 0;
	dns->rd = 1;
	dns->ra = 0;
	dns->z = 0;
	dns->ad = 0;
	dns->cd = 0;
	dns->rcode = 0;
	dns->q_count = htobe16(1);
	dns->ans_count = 0;
	dns->auth_count = 0;
	dns->add_count = htobe16(dnssec);


	qname =(unsigned char*)&buf[sizeof(DNS_HEADER)];

	troca_formato_nome_dns(qname , host);
	qinfo =(QUESTION*)&buf[sizeof(DNS_HEADER) + (strlen((const char*)qname) + 1)];

	qinfo->qtype = htobe16 (query_type);
	qinfo->qclass = htobe16 (query_class);
	if (dnssec) {
		add_rec = (ADDITIONAL_RECORDS*)&buf[sizeof(DNS_HEADER) + (strlen((const char*)qname) + 1) + sizeof (QUESTION)];
		size = sizeof(DNS_HEADER) + (strlen((const char*)qname)+1) + sizeof(QUESTION) + sizeof (ADDITIONAL_RECORDS);
		add_rec->name = 0;
		add_rec->type_otp = htobe16(41);
		add_rec->udp_payload_size = htobe16(4096);
		add_rec->higher_bits_xrcode = 0;
		add_rec->edns0_version = 0;
		add_rec->z = htobe16 (32768);
		add_rec->data_len = 0;
	}
	else
		size = sizeof(DNS_HEADER) + (strlen((const char*)qname)+1) + sizeof(QUESTION);


//	INFO_PRINT("Enviando consulta...");
	//hexdump (buf, size);
	clock_gettime(CLOCK_REALTIME, &init_ts);
	clock_gettime(CLOCK_MONOTONIC, &start_mono);
	FD_ZERO (&set_master);
	FD_SET (s, &set_master);
	memcpy(&wset, &set_master, sizeof(set_master));
	tv_timeo.tv_sec = SHORT_TIMEOUT;
	tv_timeo.tv_usec = 0; // select zera o timeout

	select (s + 1, NULL, &wset, NULL, &tv_timeo);
	if (FD_ISSET(s, &wset)) {
		rc = sendto(s, (char*) buf, size, 0, (struct sockaddr*) &dest, sizeof(dest));
		if (rc < 0) {
			ERRNO_PRINT ("falha no envio da consulta dns, rc %d\n", rc);
			saida (1);
		}
	}
	else {
		ERROR_PRINT ("timeout no envio da consulta dns\n");
		saida (1);
	}

	ret = (QUERY_RESPONSE*) malloc (sizeof (QUERY_RESPONSE));
	if (!ret) {
		INFO_PRINT ("Nao conseguiu alocar query response");
		saida (1);
	}
	ret->time = 0;
	ret->buf = NULL;
	ret->size = 0;


	i = sizeof dest;
//	INFO_PRINT("Recebendo resposta...");
	//espera pelo menos 3 bytes no buffer de recebimento
	length = 3;
	flags = fcntl (s, F_GETFL, 0);
	fcntl (s, F_SETFL,flags | O_NONBLOCK);
	setsockopt(s, SOL_SOCKET, SO_RCVLOWAT, (char *)&length, sizeof(length));

	memcpy(&rset, &set_master, sizeof(set_master));
	tv_timeo.tv_sec = SHORT_TIMEOUT;
	tv_timeo.tv_usec = 0; // select zera o timeout

	select (s + 1, &rset, NULL, NULL, &tv_timeo);
	if (FD_ISSET(s, &rset)) {
		size = recvfrom (s,(unsigned char*)buf , 65536 , 0 , (struct sockaddr*)&dest , (socklen_t*)&i );
		if(size < 0) {
			ERRNO_PRINT ("falha ao receber consulta dns\n");
			return ret;
//	        saida (1);
		}
		clock_gettime(CLOCK_REALTIME, &end_ts);
		clock_gettime(CLOCK_MONOTONIC, &end_mono);
		tempo_aux = TIMESPEC_NANOSECONDS(init_ts)/1000;
		time_tmp = (time_t) (tempo_aux / 1000000);
		localtime_r(&time_tmp, &time_query);
		fprintf (fileout, "\thora_antes  (%"PRUI64"): %2.2d/%2.2d/%4.4d %2.2d:%2.2d:%2.2d.%3.3"PRUI64"\n", tempo_aux/1000, time_query.tm_mday, time_query.tm_mon+1, time_query.tm_year + 1900, time_query.tm_hour, time_query.tm_min, time_query.tm_sec, ((tempo_aux / 1000)%1000));
		tempo_aux = TIMESPEC_NANOSECONDS(end_ts)/1000;
		time_tmp = (time_t) (tempo_aux / 1000000);
		localtime_r(&time_tmp, &time_query);
		fprintf (fileout, "\thora_depois (%"PRUI64"): %2.2d/%2.2d/%4.4d %2.2d:%2.2d:%2.2d.%3.3"PRUI64" do IP %s\n", tempo_aux/1000, time_query.tm_mday, time_query.tm_mon+1, time_query.tm_year + 1900, time_query.tm_hour, time_query.tm_min, time_query.tm_sec, ((tempo_aux / 1000)%1000), dns_server);
		ret->time = (TIMESPEC_NANOSECONDS(end_mono) - TIMESPEC_NANOSECONDS(start_mono))/1000;
		ret->hora_recv = tempo_aux;
		ret->buf = (unsigned char*) malloc (size);
		if (!ret->buf) {
			INFO_PRINT ("Nao conseguiu alocar o buffer de query response");
			saida (1);
		}
		memcpy (ret->buf, buf, size);
		ret->size = size;

		return ret;
	}
	INFO_PRINT ("Saindo sem resposta a consulta de DNS");
	return ret;

}

void copia_rdata (RES_RECORD *response, unsigned char* reader, int32_t *stop) {
	response->rdata = (unsigned char*)malloc(be16toh(response->resource->data_len) + 1);
	if (!response->rdata) {
		INFO_PRINT ("Nao conseguiu alocar memoria para rdata\n");
		saida (1);
	}
	memcpy (response->rdata, reader, be16toh(response->resource->data_len));
	response->rdata[be16toh(response->resource->data_len)] = '\0';
	*stop = be16toh(response->resource->data_len);
}

void copia_rdata_ch_txt (RES_RECORD *response, unsigned char* reader, int32_t *stop) {
	response->rdata = (unsigned char*)malloc(be16toh(response->resource->data_len));
	if (!response->rdata) {
		INFO_PRINT ("Nao conseguiu alocar memoria para rdata txt\n");
		saida (1);
	}
	memcpy (response->rdata, reader + 1, be16toh(response->resource->data_len) - 1);
	response->rdata[be16toh(response->resource->data_len) - 1] = '\0';
	*stop = be16toh(response->resource->data_len);
}

void decode_rrsig (unsigned char *buf, unsigned char* campo, RES_RECORD *response, int32_t tam, RRSIG_SHORT_QUERY_RESPONSE *rrsigresp, int32_t *stop) {
	R_DATA_RRSIG *rrsig;
	unsigned char *aux, *aux2, *aux3, *nome, base64_enc [4096], *pbase64_enc;
	int32_t stop2, tam_b64 = 4096;

	response->rdata = (unsigned char*) malloc(4096);

	rrsig = (R_DATA_RRSIG*) campo;
	aux = campo + sizeof (R_DATA_RRSIG);
//    fprintf (fileout, "type covered: %d, algorithm: %d, labels %d, original ttl %d, signature_expiration: %s, time_signed: %s, id_of_signing_key_fingerprint: %d ", be16toh(rrsig->type_covered), rrsig->algorithm, rrsig->labels, be32toh(rrsig->original_ttl), converte_time(be32toh(rrsig->signature_expiration)), converte_time(be32toh(rrsig->time_signed)), be16toh(rrsig->id_of_signing_key_fingerprint));
	aux2 = converte_time(be32toh(rrsig->signature_expiration));
	aux3 = converte_time(be32toh(rrsig->time_signed));
	nome = readname(aux, buf, &stop2);
	aux += stop2;
	base64_encode (base64_enc, &tam_b64, aux, tam - (aux - campo));
//    fprintf (fileout, "base64: [%s]\n", base64 (aux, tam - (aux - campo)));
	pbase64_enc = (unsigned char*) malloc (tam_b64 + 1);
	if (!pbase64_enc)
		saida (1);
	strcpy ((char*)pbase64_enc, (char*)base64_enc);
	sprintf ((char*)response->rdata,
			"\n\t\t\ttype covered: %d,\n\t\t\talgorithm: %d,\n\t\t\tlabels: %d,\n\t\t\toriginal ttl: %d,\n\t\t\tsignature_expiration: %s,"
			"\n\t\t\ttime_signed: %s,\n\t\t\tid_of_signing_key_fingerprint: %d,\n\t\t\tnome: %s\n\t\t\ttam chave: %d\n\t\t\tbase64: [%s]",
			be16toh(rrsig->type_covered), rrsig->algorithm, rrsig->labels, be32toh(rrsig->original_ttl), aux2, aux3,
			be16toh(rrsig->id_of_signing_key_fingerprint), nome, tam_b64, pbase64_enc);
	free (aux2);
	free (aux3);

//    hexdump (aux, tam - (aux - campo));
	*stop = be16toh(response->resource->data_len);
	response->rdata = (unsigned char*)realloc(response->rdata, strlen((char*)response->rdata) + 1);

	if (rrsigresp) {
		rrsigresp->rrsig.type_covered = be16toh(rrsig->type_covered);
		rrsigresp->rrsig.algorithm = rrsig->algorithm;
		rrsigresp->rrsig.labels = rrsig->labels;
		rrsigresp->rrsig.original_ttl = be32toh(rrsig->original_ttl);
		rrsigresp->rrsig.signature_expiration = be32toh(rrsig->signature_expiration);
		rrsigresp->rrsig.time_signed = be32toh(rrsig->time_signed);
		rrsigresp->rrsig.id_of_signing_key_fingerprint = be16toh(rrsig->id_of_signing_key_fingerprint);
		rrsigresp->assinante = nome;
		rrsigresp->assinatura = pbase64_enc;
	}
	else {
		free (nome);
		free (pbase64_enc);
	}


}
void decode_soa (unsigned char *buf, unsigned char* campo, RES_RECORD *response, int32_t tam, int32_t *stop) {
	R_DATA_SOA *soa;
	unsigned char *aux, *primary_ns, *responsible_mail;
	int32_t stop2;

	response->rdata = (unsigned char*) malloc(4096);

	aux = campo;
	primary_ns = readname(aux, buf, &stop2);
	aux += stop2;
	responsible_mail = readname(aux, buf, &stop2);
	aux += stop2;
	soa = (R_DATA_SOA*) aux;
	aux += sizeof (R_DATA_SOA);
	sprintf ((char*)response->rdata,
			"\n\t\t\tprimary name server: %s,\n\t\t\tresponsible authority's mailbox: %s,"
			"\n\t\t\tserial_number %d,\n\t\t\trefresh_interval %u,\n\t\t\tretry_interval %u,"
			"\n\t\t\texpiration_limit %u,\n\t\t\tminimum_ttl %u",
			primary_ns, responsible_mail, be32toh(soa->serial_number), be32toh(soa->refresh_interval),
			be32toh(soa->retry_interval), be32toh(soa->expiration_limit), be32toh(soa->minimum_ttl));

//    hexdump (aux, tam - (aux - campo));
	*stop = be16toh(response->resource->data_len);
	response->rdata = (unsigned char*)realloc(response->rdata, strlen((char*)response->rdata) + 1);

	free (primary_ns);
	free (responsible_mail);

}

void decode_nsec (unsigned char *buf, unsigned char* campo, RES_RECORD *response, int32_t tam, int32_t *stop) {
	unsigned char *aux, *next_domain, *window, *tam_bitfield, *bitfield = NULL;
	int32_t i, stop2;
	char tipo [10];

	response->rdata = (unsigned char*) malloc(4096);

	aux = campo;
	next_domain = readname(aux, buf, &stop2);
	sprintf ((char*)response->rdata, "\n\t\t\tnext domain: %s,", next_domain);
	free (next_domain);
	aux += stop2;
	while (aux - campo < tam) {
		window = (unsigned char*) aux;
		aux += sizeof(unsigned char);
		tam_bitfield = (unsigned char*) aux;
		aux += sizeof(unsigned char);
		bitfield = realloc (bitfield, (int)*tam_bitfield);
		if (!bitfield) {
			INFO_PRINT ("falha ao alocar memoria para registro tipo NSEC");
			saida (1);
		}
		memcpy (bitfield, aux, *tam_bitfield);
//        for (i = 0; i < *tam_bitfield; i++)
//            bitfield[i] = aux[i];
		aux += *tam_bitfield;
		for (i = 0; i < *tam_bitfield * CHAR_BIT; i++) {
			if(BITTEST(bitfield, i)) {
				tipo_registro ((*window * 256) + i, tipo);
				sprintf((char*)response->rdata,"%s\n\t\t\tTipo RR no Bitmap: %s", (char*)response->rdata, tipo);
			}
		}
	}
	free (bitfield);
//    hexdump (aux, tam - (aux - campo));
	*stop = be16toh(response->resource->data_len);
	response->rdata = (unsigned char*)realloc(response->rdata, strlen((char*)response->rdata) + 1);
}



void libera_memoria_records (RES_RECORD* res, int tam) {
	int i;
	for (i = 0; i < tam; i++) {
		if (res[i].rdata)
			free (res[i].rdata);
		if (res[i].name)
			free (res[i].name);
		}
	if (tam)
		free (res);

}

SHORT_QUERY_RESPONSE* consulta_dns(char *host , int32_t query_type, int32_t query_class, char* dns_server, int dnssec, int64_t offset_time)
{
	unsigned char *buf = NULL, *qname, *reader, *aux;
	int32_t i, stop;
	int32_t ip_bin;
	QUERY_RESPONSE *dnsbuf = NULL;
	char str[INET6_ADDRSTRLEN];
	struct sockaddr_in6 a6;
	struct sockaddr_in a;
	RES_RECORD *answers, *auth, *addit; //the replies from the DNS server
	DNS_HEADER *dns = NULL;
	ADDITIONAL_RECORDS *add_rec = NULL;
	uint16_t add_count, q_id = 0;
	SHORT_QUERY_RESPONSE *res;
	int s, family, timeout_resp = 1;

	family = ip_type_family(dns_server);
	for (i = 0; (i < 3) && (timeout_resp); i++) {
		if ((s = socket(family , SOCK_DGRAM , IPPROTO_UDP)) == -1) {
			ERRNO_PRINT ("socket invalido.");
			saida (1);
		}


		q_id = (uint16_t) get_rand_i ();
		dnsbuf = envia_recebe_pacotes_dns (host, query_type, query_class, dns_server, dnssec, s, q_id, family);
		buf = dnsbuf->buf;
		close (s);
		if (!dnsbuf->size) {
			if (dnsbuf->buf)
				free (dnsbuf->buf);
			free (dnsbuf);
			timeout_resp = 1;
		}
		else timeout_resp = 0;
	}

	res = (SHORT_QUERY_RESPONSE *) malloc (sizeof (SHORT_QUERY_RESPONSE));
	if (!res) {
		INFO_PRINT ("sem memoria para alocar respostas curtas do servidor de DNS\n");
		saida (1);
	}
	res->hostname = (char*) malloc (strlen (host) + 1);
	if (!res->hostname) {
		INFO_PRINT ("sem memoria para alocar nome do host para consulta DNS\n");
		saida (1);
	}
	strcpy (res->hostname, host);

	if (timeout_resp) {
		res->rcode = -1;
		res->num_resposta = 0;
		res->tempo_resposta = 0;
		res->hora_recv = 0;
		res->resp = NULL;
		return res;
	}

	dns = (DNS_HEADER*) buf;
	if (be16toh(dns->id) != q_id) {
		INFO_PRINT ("Recebido id: %u diferente do enviado:%u. Ignorando resposta de possível ataque", be16toh(dns->id), q_id);
		free (dnsbuf->buf);
		free (dnsbuf);
		res->rcode = -1;
		res->num_resposta = 0;
		res->tempo_resposta = 0;
		res->hora_recv = 0;
		res->resp = NULL;
		return res;
	}

	qname =(unsigned char*)&buf[sizeof(DNS_HEADER)];
	if (be16toh(dns->q_count)) {
		troca_formato_nome_dns(qname , host);
		reader = &buf[sizeof(DNS_HEADER) + (strlen((const char*)qname)+1) + sizeof(QUESTION)];
	}
	else reader = buf;
//	ini_reader = reader;

	fprintf (fileout, "\tflags:%s%s%s%s. A resposta contem: %d Perguntas, %d Respostas, %d Servidores impositivos, %d Registros adicionais, RCODE %d, em %"PRUI64" microssegundos\n", (dns->qr)?" qr":"",(dns->aa)?" aa":"",(dns->tc)?" tc":"",(dns->rd)?" rd":"", be16toh(dns->q_count), be16toh(dns->ans_count), be16toh(dns->auth_count), be16toh(dns->add_count), dns->rcode, dnsbuf->time);
	res->authoritative_flag = dns->aa;
	res->rcode = dns->rcode;
	res->num_resposta = be16toh(dns->ans_count);
	res->tempo_resposta = dnsbuf->time;
	res->hora_recv = dnsbuf->hora_recv - offset_time;

	if (dns->ans_count) {
		answers = malloc (sizeof (RES_RECORD) * be16toh(dns->ans_count));
		res->resp = malloc (sizeof (RESP) * be16toh(dns->ans_count));
	}
	else {
		answers = NULL;
		res->resp = NULL;
	}
	if (dns->auth_count)
		auth = malloc (sizeof (RES_RECORD) * be16toh(dns->auth_count));
	else auth = NULL;
	if (dns->add_count)
		addit = malloc (sizeof (RES_RECORD) * be16toh(dns->add_count));
	else addit = NULL;

	if ((dns->ans_count && (!answers || !res->resp)) || (dns->auth_count && !auth) || (dns->add_count && !addit)) {
		INFO_PRINT ("sem memoria para alocar respostas do servidor de DNS\n");
		saida (1);
	}
	if (dns->ans_count) {
		memset (answers, 0, sizeof (RES_RECORD) * be16toh(dns->ans_count));
		memset (res->resp, 0, sizeof (RESP) * be16toh(dns->ans_count));
	}
	if (dns->auth_count) {
		memset (auth, 0, sizeof (RES_RECORD) * be16toh(dns->auth_count));
	}
	if (dns->add_count) {
		memset (addit, 0, sizeof (RES_RECORD) * be16toh(dns->add_count));
	}
	stop=0;
//	INFO_PRINT("Lendo respostas: ");

	for(i=0;i<be16toh(dns->ans_count);i++)
	{
		answers[i].name=readname(reader,buf,&stop);
		reader += stop;
		answers[i].resource = (R_DATA*)(reader);
		reader = reader + sizeof(R_DATA);
		if ((be16toh(answers[i].resource->_class) == C_IN) && (be16toh(answers[i].resource->type) == T_A)) // classe IN e tipo endereco ipv4
		{
			copia_rdata (&answers[i], reader, &stop);
			reader += stop;
			res->resp[i].tipo_reg = TR_A_AAAA;
//			p=(int64_t*)answers[i].rdata;
//			a.sin_addr.s_addr=(*p); //working without be32toh
			memcpy (&ip_bin, answers[i].rdata, sizeof (ip_bin));
			a.sin_addr.s_addr = ip_bin;
			aux = (unsigned char*)inet_ntoa(a.sin_addr);

			res->resp[i].a.host_ip = malloc (strlen ((char*)aux) + 1);
			if (!res->resp[i].a.host_ip) {
				INFO_PRINT ("nao conseguiu alocar memoria para estrutura de resposta %d\n", i);
				saida (1);
			}
			memcpy(res->resp[i].a.host_ip, aux, strlen ((char*) aux) + 1);
			res->resp[i].a.ttl = be32toh (answers[i].resource->ttl);
		}
		else if((be16toh(answers[i].resource->_class) == C_IN) && (be16toh(answers[i].resource->type) == T_AAAA)) // IPv6
		{
			copia_rdata (&answers[i], reader, &stop);
			reader += stop;
			res->resp[i].tipo_reg = TR_A_AAAA;
			memcpy (a6.sin6_addr.s6_addr, answers[i].rdata, 16);
			aux = (unsigned char*)inet_ntop(AF_INET6, &(a6.sin6_addr), str, INET6_ADDRSTRLEN);
			res->resp[i].a.host_ip = malloc (strlen ((char*)aux) + 1);
			if (!res->resp[i].a.host_ip) {
				INFO_PRINT ("nao conseguiu alocar memoria para estrutura de resposta %d\n", i);
				saida (1);
			}
			memcpy(res->resp[i].a.host_ip, aux, strlen ((char*)aux) + 1);
			res->resp[i].a.ttl = be32toh (answers[i].resource->ttl);
		}
		else if ((be16toh(answers[i].resource->_class) == C_IN) && (be16toh(answers[i].resource->type) == T_RRSIG)) // classe IN e tipo RR_SIG
		{
			res->resp[i].tipo_reg = TR_RRSIG;
			decode_rrsig (buf, reader, &answers[i], be16toh(answers[i].resource->data_len), &res->resp[i].rrsig, &stop);
			reader += stop;
		}
		else if ((be16toh(answers[i].resource->_class) == C_CH) && (be16toh(answers[i].resource->type) == T_TXT)) // classe CH e tipo TXT
		{
			copia_rdata_ch_txt (&answers[i], reader, &stop);
			reader += stop;
			res->resp[i].tipo_reg = TR_TXT;
			res->resp[i].txt.bindproperty = malloc (strlen ((char*)answers[i].rdata) + 1);
			if (!res->resp[i].txt.bindproperty) {
				INFO_PRINT ("nao conseguiu alocar memoria para estrutura de resposta %d\n", i);
				saida (1);
			}
			memcpy(res->resp[i].txt.bindproperty, answers[i].rdata, strlen ((char*)answers[i].rdata) + 1);
		}
		else
		{
			answers[i].rdata = readname(reader,buf,&stop);
			reader = reader + stop;
		}
	}
	//hexdump (ini_reader, reader - ini_reader);
	//read authorities
//	INFO_PRINT("Lendo Impositivos: ");
	//ini_reader = reader;
	for(i=0;i<be16toh(dns->auth_count);i++)
	{
		auth[i].name=readname(reader,buf,&stop);
		reader+=stop;
		auth[i].resource=(R_DATA*)(reader);
		reader+=sizeof(R_DATA);
		if (((be16toh(auth[i].resource->_class) == C_IN) &&
			((be16toh (auth[i].resource->type) == T_A) || (be16toh (auth[i].resource->type) == T_NS))) || // classe IN e (tipo IPv4 ou NS)
			((be16toh(auth[i].resource->_class) == C_CH) && (be16toh(auth[i].resource->type) == T_NS))) // classe CHAOS e tipo NS
		{
			auth[i].rdata=readname(reader,buf,&stop);
			reader+=stop;
		}
		else if (be16toh (auth[i].resource->type) == T_RRSIG) {
			decode_rrsig (buf, reader, &auth[i], be16toh(auth[i].resource->data_len), NULL, &stop);
			reader += stop;
		}
		else if (be16toh (auth[i].resource->type) == T_SOA) {
			decode_soa (buf, reader, &auth[i], be16toh(auth[i].resource->data_len), &stop);
			reader += stop;
		}
		else if (be16toh (auth[i].resource->type) == T_NSEC) {
			decode_nsec (buf, reader, &auth[i], be16toh(auth[i].resource->data_len), &stop);
			reader += stop;
		}
	}
	//hexdump (ini_reader, reader - ini_reader);
	//ini_reader = reader;
	//read additional
//	INFO_PRINT("Lendo Adicionais: ");
	if ((dnssec) && (be16toh(dns->add_count) > 0))
		add_count = be16toh(dns->add_count) - 1;
	else
		add_count = be16toh(dns->add_count);
	for(i = 0; i < add_count; i++)
	{
		addit[i].name=readname(reader,buf,&stop);
		reader+=stop;

		addit[i].resource=(R_DATA*)(reader);
		reader+=sizeof(R_DATA);

		if ((be16toh (addit[i].resource->type) == T_A ) || (be16toh (addit[i].resource->type) == T_AAAA ))// IPv4 ou IPv6
		{
			copia_rdata (&addit[i], reader, &stop);
			reader += stop;
		}
		else if (be16toh (addit[i].resource->type) == T_RRSIG ) // RRSIG
		{
			decode_rrsig (buf, reader, &addit[i], be16toh(addit[i].resource->data_len), NULL, &stop);
			reader += stop;
		}
		else
		{
			addit[i].rdata=readname(reader,buf,&stop);
			reader+=stop;
		}

	}
	if ((dnssec) && (be16toh(dns->add_count))) {
		add_rec = (ADDITIONAL_RECORDS*)reader;
		reader+=sizeof(ADDITIONAL_RECORDS);
	}
	//hexdump (ini_reader, reader - ini_reader);

	//print answers
	if (dns->ans_count) {
		fprintf(fileout, "\tRegistros de resposta: %d\n" , be16toh(dns->ans_count) );
		for(i=0 ; i < be16toh(dns->ans_count) ; i++)
		{
			fprintf(fileout, "\t\tNome: %s ",answers[i].name);

			if ((be16toh(answers[i].resource->_class) == C_IN) && (be16toh(answers[i].resource->type) == T_A)) //IPv4 address
			{
//	    		p=(int64_t*)answers[i].rdata;
//	    		a.sin_addr.s_addr=(*p); //working without be32toh
				memcpy (&ip_bin, answers[i].rdata, sizeof (ip_bin));
				a.sin_addr.s_addr = ip_bin;
				fprintf(fileout, "com IPv4: %s com ttl %u",inet_ntoa(a.sin_addr), be32toh (answers[i].resource->ttl));
			}
			else if((be16toh(answers[i].resource->_class) == C_IN) && (be16toh(answers[i].resource->type) == T_AAAA)) // IPv6
			{
				memcpy (a6.sin6_addr.s6_addr, answers[i].rdata, 16);
				fprintf(fileout, "com IPv6: %s com ttl %u",inet_ntop(AF_INET6, &(a6.sin6_addr), str, INET6_ADDRSTRLEN), be32toh (answers[i].resource->ttl));
			}
			else if((be16toh(answers[i].resource->_class) == C_IN) && (be16toh(answers[i].resource->type) == T_CNAME))
			{
				//Canonical name for an alias
				fprintf(fileout, "com alias: %s",answers[i].rdata);
			}
			else if ((be16toh(answers[i].resource->_class) == C_IN) && (be16toh(answers[i].resource->type) == T_RRSIG))
			{
				//hexdump (answers[i].rdata, be16toh(answers[i].resource->data_len));
				fprintf(fileout, "com RRSIG: %s",answers[i].rdata);
			}
			else if ((be16toh(answers[i].resource->_class) == C_CH) && (be16toh(answers[i].resource->type) == T_TXT))
			{
				//hexdump (answers[i].rdata, be16toh(answers[i].resource->data_len));
				fprintf(fileout, "com TXT: %s", answers[i].rdata);
			}
			if (dns->ans_count)
				fprintf(fileout, "\n");
		}
	}

	//print authorities
	if (dns->auth_count) {
		fprintf(fileout, "\tRegistros impositivos: %d \n" , be16toh(dns->auth_count) );
		for( i=0 ; i < be16toh(dns->auth_count) ; i++)
		{
			fprintf(fileout, "\t\tNome: %s ",auth[i].name);
			if(be16toh(auth[i].resource->type) == T_NS)
			{
				fprintf(fileout, "com nameserver: %s",auth[i].rdata);
			}
			else if (be16toh(auth[i].resource->type) == T_RRSIG)
			{
				//hexdump (auth[i].rdata, be16toh(auth[i].resource->data_len));
				fprintf(fileout, "com RRSIG: %s",auth[i].rdata);
			}
			else if (be16toh(auth[i].resource->type) == T_SOA)
			{
				//hexdump (auth[i].rdata, be16toh(auth[i].resource->data_len));
				fprintf(fileout, "com SOA: %s",auth[i].rdata);
			}
			else if (be16toh(auth[i].resource->type) == T_NSEC)
			{
				//hexdump (auth[i].rdata, be16toh(auth[i].resource->data_len));
				fprintf(fileout, "com NSEC: %s",auth[i].rdata);
			}
			if (dns->auth_count)
				fprintf(fileout, "\n");
		}
	}
	//print additional resource records
	if (dns->add_count) {
		fprintf(fileout, "\tRegistros adicionais: %d \n" , be16toh(dns->add_count) );
		for(i=0; i < add_count ; i++)
		{
			fprintf(fileout, "\t\tNome: %s ",addit[i].name);
			if(be16toh(addit[i].resource->type) == T_A)
			{
//	    		p=(int64_t*)addit[i].rdata;
//	    		a.sin_addr.s_addr=(*p);
				memcpy (&ip_bin, addit[i].rdata, sizeof (ip_bin));
				a.sin_addr.s_addr = ip_bin;
				fprintf(fileout, "com IPv4: %s",inet_ntoa(a.sin_addr));
			}
			else if(be16toh(addit[i].resource->type) == T_AAAA)
			{
				memcpy (a6.sin6_addr.s6_addr, addit[i].rdata, 16);
				fprintf(fileout, "com IPv6: %s",inet_ntop(AF_INET6, &(a6.sin6_addr), str, INET6_ADDRSTRLEN));
			}
			else if (be16toh(addit[i].resource->type) == T_RRSIG)
			{
				fprintf(fileout, "com RRSIG: %s",addit[i].rdata);
				//hexdump (addit[i].rdata, be16toh(addit[i].resource->data_len));
			}
			if (dns->add_count)
				fprintf(fileout, "\n");
		}
		if (dnssec) {
		//hexdump ((unsigned char*) add_rec, sizeof (ADDITIONAL_RECORDS));
			fprintf (fileout, "\t\tDNSSEC:\n\t\t\tNome: %s,\n\t\t\ttipo: %d,\n\t\t\tUDP payload size: %d,\n\t\t\t"
				"Higher bits in extended RCODE: %d,\n\t\t\tEDNS0 version: %d,\n\t\t\tZ: %d,\n\t\t\tData Length: %d\n",
				add_rec->name?"nao sei":"Root", be16toh(add_rec->type_otp), be16toh(add_rec->udp_payload_size),
				be16toh(add_rec->higher_bits_xrcode), be16toh(add_rec->edns0_version), be16toh(add_rec->z),
				be16toh(add_rec->data_len));
		}
	}

	libera_memoria_records (answers, be16toh(dns->ans_count));
	libera_memoria_records (auth, be16toh(dns->auth_count));
	libera_memoria_records (addit, be16toh(dns->add_count));
	free (dnsbuf->buf);
	free (dnsbuf);
	return res;
}


/*
*
* */
unsigned char* readname(unsigned char* reader, unsigned char* buffer, int32_t* count)
{
	unsigned char name[256];
	unsigned char *ret_name;
	uint32_t p=0,jumped=0,offset;
	int i , j;

	*count = 1;

	name[0]='\0';

	//read the names in 3www6google3com format
	while(*reader!=0)
	{
		if(*reader>=192)
		{
			offset = (*reader)*256 + *(reader+1) - 49152; //49152 = 11000000 00000000
			reader = buffer + offset - 1;
			jumped = 1; //we have jumped to another location so counting wont go up!
		}
		else
		{
			name[p++]=*reader;
		}

		reader = reader+1;

		if(jumped==0)
		{
			*count = *count + 1; //if we havent jumped to another location then we can count up
		}
	}

	name[p]='\0'; //string complete
	if(jumped==1)
	{
		*count = *count + 1; //number of steps we actually moved forward in the packet
	}

	//now convert 3www6google3com0 to www.google.com
	for(i=0;i<(int)strlen((const char*)name);i++)
	{
		p=name[i];
		for(j=0;j<(int)p;j++)
		{
			name[i]=name[i+1];
			i=i+1;
		}
		name[i]='.';
	}
	name[i-1]='\0'; //remove the last dot
	ret_name = (unsigned char*)malloc(strlen((char*)name) + 1);
	if (!ret_name) {
		INFO_PRINT ("nao conseguiu alocar memoria para o nome\n");
		saida (1);
	}
	strcpy ((char*)ret_name, (char*)name);

	return ret_name;
}


void libera_dns_servers (DNS_SERVERS *dns_servers) {
	int i;
	if (dns_servers) {
		for (i = 0; i < dns_servers->count; i++)
			free (dns_servers->ip[i]);
		free (dns_servers->ip);
		free (dns_servers);
	}
}


int32_t insere_dns_server (DNS_SERVERS *dns_servers, char *ip) {
#define BLOCO_ALLOC 5
	char **temp;
	int32_t i;

	// nao insere dns server igual a um já presente na lista
	for (i = 0; i < dns_servers->count; i++)
		if (strcmp (dns_servers->ip[i], ip) == 0) {
			INFO_PRINT ("servidor com IP %s repetido", ip);
			return 1;
		}
	// verifica se precisa realocar memória
	if (dns_servers->count % BLOCO_ALLOC == 0)
	{
		temp = realloc (dns_servers->ip, sizeof (unsigned char*) * (dns_servers->count + BLOCO_ALLOC));
		if (!temp) {
			libera_dns_servers(dns_servers);
			return 0;
		}
		dns_servers->ip = temp;
	}
	dns_servers->ip[dns_servers->count] = malloc (strlen (ip) + 1);
	if (!dns_servers->ip[dns_servers->count]) {
		libera_dns_servers (dns_servers);
		return 0;
	}
	INFO_PRINT ("inserindo o servidor de DNS %s", ip);
	strcpy(dns_servers->ip[dns_servers->count++] , ip);
	return 1;
}
/*
* Lê os servidores de DNS para os arquivos do array nome_arq_dns
* */
DNS_SERVERS * get_dns_servers(int family)
{
#define NUM_ARQS 6
	FILE *fp;
	char line[BUFSIZ], *p, *q;
	char *nome_arq_dns [] = {"/etc/resolv.conf", "/tmp/resolv.conf", "/tmp/resolv.conf.auto", "/tmp/resolv.conf.ppp", "/tmp/etc/dnsmasq.conf", "/etc/config/dhcp"};
//	char *nome_arq_dns [] = {"/etc/resolv.conf", "/tmp/resolv.conf", "/tmp/resolv.conf.auto", "/tmp/resolv.conf.ppp", "/tmp/etc/dnsmasq.conf", "/etc/config/dhcp", "/tmp/teste", "/tmp/teste2"};
	// olhar /tmp/etc/dnsmasq.conf
	//server=200.221.11.100
	//server=200.220.148.2
	//server=8.8.8.8
	//olhar /etc/config/dhcp
	//list server '200.221.11.100'
	//list server '200.220.148.2'
	//list server '8.8.8.8'

	DNS_SERVERS *dns_servers = NULL;
	int i;

	dns_servers = malloc (sizeof (DNS_SERVERS));
	if (!dns_servers)
		return NULL;
	dns_servers->count = 0;
	dns_servers->ip = NULL;

	for (i = 0; i < NUM_ARQS; i++)
	{
		if((fp = fopen(nome_arq_dns [i], "r")) == NULL)
		{
			INFO_PRINT("Nao foi possivel abrir o arquivo de servidores de DNS %s. Continuando...\n", nome_arq_dns [i]);
			continue;
		}

		while(fgets(line , BUFSIZ , fp))
		{
			p = line;
			while (isspace(*p))
				p++;

			if(*p == '#')
			{
				continue;
			}
			p = NULL;
			if (strstr (line , "nameserver")) {
				p = strtok (line , " ");
				p = strtok (NULL , " ");
			}
			else if (strstr (line , "list server")) {
				p = strtok (line , " ");
				p = strtok (NULL , " ");
				p = strtok (NULL , " ");
				if (p)
					p++;

			}
			else if (strstr (line , "server")) {
				p = strtok (line , "=");
				p = strtok (NULL , "=");
			}
			if (p) {
				q = strrchr(p, '\n');
				if (q)
					*q = '\0';
				q = strrchr(p, '\'');
				if (q)
					*q = '\0';

				q = get_ip_by_host(p, 0);

				if (strncmp(p, "127.", 4) == 0 || strcmp(p, "::1") == 0 || (!q))
					continue;
				free (q);

				INFO_PRINT("DNS Server: %s", p);

				if (!insere_dns_server (dns_servers, p)) {
					return NULL;
				}
			}
		}
		fclose (fp);
	}
	return dns_servers;
}

/*
* This will convert www.google.com to 3www6google3com
* got it
* */
void troca_formato_nome_dns(unsigned char* dns, char* host)
{
	int lock = 0 , i;

	for(i = 0 ; i < strlen((char*)host) ; i++)
	{
		if(host[i]=='.')
		{
			*dns++ = i-lock;
			for(;lock<i;lock++)
			{
				*dns++=host[lock];
			}
			lock++; //or lock=i+1;
		}
	}
	*dns++='\0';
}


