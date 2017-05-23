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
#include "simet_hash_measure.h"
#include "simet_tools.h"
#include "gopt.h"

const char *global_server_hash_measure;
int global_server = 0;
static void parse_args_hash_measure(void * options, char doc[], int* family) {

	parse_args_basico(options, doc, family);

	if (gopt_arg(options, 's', &global_server_hash_measure)) {
		global_server = 1;
	}
}

void *args_hash_measure (int *argc, const char *argv[]) {
	void *options =
		gopt_sort(argc, argv,
			gopt_start(
				DEFAULT_OPTS,
				gopt_option('s', GOPT_ARG, gopt_shorts( 's' ), gopt_longs( "server" ))

			));
	parse_verbose (options);

	return options;
}



int main_hash_measure (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options) {

	char doc[] = "uso:\n\tsimet_hash_measure\n\n\tobtem um hash_measure\nuso: simet_hash_measure -s host\n\n";
	char *result;
	URL_PARTS urlparts, *urlpartsp;

	int family = 4;
	urlpartsp = &urlparts;
	if (options)
		parse_args_hash_measure (options, doc, &family);

	if (!global_server) {
		fprintf(stdout, "%s", doc);
		exit (EXIT_SUCCESS);
	}

	separa_protocolo_host_chamada (urlpartsp, (char*) global_server_hash_measure);
	result = get_hash_measure (urlpartsp->host, config->port, family, config->context_root, mac_address, ssl_ctx);
	printf ("%s", result);

	return 0;
}