#define _GNU_SOURCE
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>


#include "byteconversion.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "utils.h"
#include "dns.h"
#include "simet_dns_ping_traceroute.h"
#include "simet_porta25.h"
#include "simet_tools.h"
#include "simet_uptime.h"
#include "simet_client.h"
#include "simet_bcp38.h"
#include "simet_ws.h"
#include "simet_ntpq.h"
#include "simet_alexa.h"
#include "simet_hash_measure.h"
#include "simet_content_provider.h"


typedef enum {
	SIMET_CLIENT = 0,
	SIMET_DNS,
	SIMET_PORTA25,
	SIMET_BCP38,
	SIMET_UPTIME,
	SIMET_WS,
	SIMET_NTPQ,
	SIMET_ALEXA,
	SIMET_HASH_MEASURE,
	SIMET_DNS_PING_TRACEROUTE,
	SIMET_CONTENT
} EXECUTABLE_TYPE;

int getExecutableType(const char* name)
{
	int type = -1;
	if (strcmp(name, "simet_client") == 0)  {
		type = SIMET_CLIENT;
	}
	else if (strcmp(name, "simet_dns") == 0) {
		type = SIMET_DNS;
	}
	else if (strcmp(name, "simet_dns_ping_traceroute") == 0) {
		type = SIMET_DNS_PING_TRACEROUTE;
	}
	else if (strcmp(name, "simet_porta25") == 0) {
		type = SIMET_PORTA25;
	}
	else if (strcmp(name, "simet_bcp38") == 0) {
		type = SIMET_BCP38;
	}
	else if (strcmp(name, "simet_uptime") == 0) {
		type = SIMET_UPTIME;
	}
	else if (strcmp(name, "simet_ws") == 0) {
		type = SIMET_WS;
	}
	else if (strcmp(name, "simet_ntpq") == 0) {
		type = SIMET_NTPQ;
	}
	else if (strcmp(name, "simet_alexa") == 0) {
		type = SIMET_ALEXA;
	}
	else if (strcmp(name, "simet_hash_measure") == 0) {
		type = SIMET_HASH_MEASURE;
	}
	else if (strcmp(name, "simet_content_provider") == 0) {
		type = SIMET_CONTENT;
	}
	return type;
}

int needsConfig(int type)
{
	return (type != SIMET_WS);
}

int needsSSL(int type)
{
	return (type != SIMET_WS);
}

int needsWatchdog(int type)
{
	return (type != SIMET_WS && type != SIMET_NTPQ && type != SIMET_UPTIME && type != SIMET_DNS_PING_TRACEROUTE && type != SIMET_ALEXA);
}

int needsLockfile(int type)
{
	return (type != SIMET_WS && type != SIMET_NTPQ && type != SIMET_HASH_MEASURE);
}


int main ( int argc , const char *argv[]) {
	char *mac_address;
	const char *nome = argv [0];
	int i, lockfd, status, rc = 0, segue = 0, num_tentativa_mac = 0, len;
	pthread_t thread_watchdog;
	SSL_CTX * ssl_ctx = NULL;
	void *options = NULL;
	CONFIG_SIMET *config = NULL;
	sigset_t mask, oldmask;

	// block SIGALRM
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGPIPE);

	if ((status = pthread_sigmask(SIG_BLOCK, &mask, &oldmask)) != 0) {
        ERROR_PRINT ("SIG_BLOCK error\n");
        saida (1);
    }




	int (*main_func) (const char *, char *, int argc, const char* argv[], CONFIG_SIMET*,  SSL_CTX *, void*) = NULL;
	void* (*args) (int *, const char **) = args_basico;

	lockFilename = NULL;
	// ignora sigpipe
	signal(SIGPIPE, SIG_IGN);
	for (i = strlen (nome) - 1; i >= 0; i--) {
		if (nome[i] == '/') {
			nome = &nome[i + 1];
			break;
		}
	}

	EXECUTABLE_TYPE execType = getExecutableType(nome);


	switch (execType) {
		case SIMET_WS:
			main_func = main_ws;
			args = args_ws;
			break;
		case SIMET_NTPQ:
			main_func = main_ntpq;
			break;
		case SIMET_CLIENT:
			main_func = main_simet_client;
			args = args_client;
			break;
		case SIMET_DNS:
			main_func = main_dns;
			args = args_dns;
			break;
		case SIMET_DNS_PING_TRACEROUTE:
			main_func = main_dns_ping_traceroute;
			break;
		case SIMET_PORTA25:
			main_func = main_porta25;
			break;
		case SIMET_BCP38:
			main_func = main_bcp38;
			break;
		case SIMET_UPTIME:
			main_func = main_uptime;
			break;
		case SIMET_ALEXA:
			main_func = main_alexa;
			break;
		case SIMET_HASH_MEASURE:
			main_func = main_hash_measure;
			args = args_hash_measure;
			break;
		case SIMET_CONTENT:
			main_func = main_content;
			break;
		default:
			printf ("o simet_tools deve ser utilizado pelas ferramentas:\n\tsimet_alexa;\n\tsimet_bcp38;\n\tsimet_client;\n\tsimet_content_provider;\n\tsimet_dns;\n\tsimet_dns_ping_traceroute;\n\tsimet_ntpq;\n\tsimet_porta25;\n\tsimet_uptime;\n\tsimet_ws.");
			return 0;
		break;
	}

	options = args(&argc, argv);

	// obtem mac_address
	do {
		num_tentativa_mac++;
		mac_address = chama_pipe ("/usr/bin/get_mac_address.sh", "nao conseguiu alocar memoria para mac address\n");

		if (num_tentativa_mac >= 100)
			saida (1);
		len = strnlen(mac_address, 12);
		if (len >= 12) {
			segue = 1;
			INFO_PRINT ("mac: %s, len %d\n", mac_address, len);
		}
		else sleep (1);
	} while (!segue);

	if (needsConfig(execType)) {
			config = config_simet();
			if (!config)
				saida (1);
	}
	if (needsWatchdog(execType)) {
		// para o uptime o watchdog nao deve ser ativado
		INFO_PRINT("start watchdog");
		meu_pthread_create(&thread_watchdog, PTHREAD_STACK_MIN * 1.125, watchdog, NULL);
		pthread_detach(thread_watchdog);
	}

	// lockfile
	if (needsLockfile(execType)) {
		if (asprintf (&lockFilename, "/tmp/%s.pid", nome) < 0) {;
			ERROR_PRINT ("impossivel alocar memoria para nome do lockfile\n");
			exit (1);
		}

		if ((lockfd = captura_lock (1, lockFilename)) == -1)
			saida (1);
	}

	if (needsSSL(execType))
		ssl_ctx = init_ssl_context(config->mensagens_erro_ssl);

	rc = main_func (nome, mac_address, argc, argv, config, ssl_ctx, options);
	free (mac_address);

	if (ssl_ctx)
		SSL_CTX_free (ssl_ctx);

	free (options);
	libera_config(config);

	fflush (stdout);

	INFO_PRINT ("Saida: RC %d\n", rc);
	saida (rc);


	return 0;
}
