#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "simet_ws.h"
#include "debug.h"
#include "utils.h"
#include "netsimet.h"
#include "gopt.h"

int global_put = 0, global_file = 0;
const char *global_name_file;


static void parse_args_ws(void * options, char doc[], int* family) {

	parse_args_basico(options, doc, family);
	if (gopt(options, 'p')) {
		global_put = 1;
	}
	if (gopt_arg(options, 'f', &global_name_file)) {
		global_file = 1;
	}

}

void *args_ws (int *argc, const char *argv[]) {
	void *options =
		gopt_sort(argc, argv,
			gopt_start(
				DEFAULT_OPTS,
				gopt_option('p', 0, gopt_shorts( 'p' ), gopt_longs( "global_put" )),
				gopt_option('f', GOPT_ARG, gopt_shorts( 'f' ), gopt_longs( "global_file" ))
			));
	parse_verbose (options);

	return options;
}


int main_ws (const char *nome, char* macAddress, int argc, const char* argv[], CONFIG_SIMET* config,  SSL_CTX* ssl_ctx, void *options) {
	FILE *fp;
	int rc;
	int64_t *psize, tam = 0;
	char *conteudo, *string = NULL;
	URL_PARTS urlparts, *urlpartsp;
	int family = 4;
	char doc[] =
		"uso:\n"
		"\tsimet_ws [http[s]://]servidor/pagina?parametro1=x&parametro2=y\n"
		"\t\tO simet_ws acessa uma página web (padrão https) para webservice, download ou global_put\n\n"
		"\t-p usa HTTP PUT em vez de HTTP GET\n"
		"\t-4 usa o protocolo IPv4 (default)\n"
		"\t-6 usa o protocolo IPv6\n"
		"\t\tsimet_ws -p servidor/pagina \"string\"\n\n"
		"\t-f nomearq salva em arquivo a resposta do servidor\n"
		"\t\tsimet_ws -p servidor/pagina \"string\"\n\n"
		"\texemplos:\n"
		"\t\tsimet_ws servidor/webservice?param1=x&param2=y (usa https)\n"
		"\t\tsimet_ws https://servidor/data_services/web_service?x=1&y=2\n"
		"\t\tsimet_ws http://servidor/data_services/web_service?x=1&y=2\n"
		"\t\tsimet_ws -f /tmp/nomearquivo.txt http://servidor/arquivo.txt\n"
		"\t\tsimet_ws -p https://servidor/pagina \"string json\"\n\n"
		"\t\tsimet_ws -p https://servidor/pagina -f /tmp/nomearquivo.txt\n\n"
		"\tobs.: O simet_ws solicita que nao seja feito cache quando a opcao -f nao for usada\n";

	urlpartsp = &urlparts;

	if (options)
		parse_args_ws(options, doc, &family);

	if (global_file) {
		psize = &tam;
		if (global_put)
			string = le_arq ((char*)global_name_file);
	}
	else {
		psize = NULL;
		string = (char*) argv[2];
	}

	separa_protocolo_host_chamada (urlpartsp, (char*) argv[1]);


	if (!global_put) {
		if (urlpartsp->https) {
			ssl_ctx = init_ssl_context(0);
			conteudo = chama_web_service_ssl_arg (ssl_ctx, urlpartsp->host, PORT_HTTPS , family, psize, 0, urlpartsp->chamada);
			SSL_CTX_free (ssl_ctx);
		}
		else {
			conteudo = chama_web_service (ssl_ctx, urlpartsp->host, PORT_HTTP, family, psize, 0, urlpartsp->chamada);
		}
	}
	else  {
		if (urlpartsp->https) {
			ssl_ctx = init_ssl_context(0);
			conteudo = chama_web_service_put_ssl (ssl_ctx, urlpartsp->host, PORT_HTTPS, family, psize, string, urlpartsp->chamada);
		}
		else {
			conteudo = chama_web_service_put (urlpartsp->host, PORT_HTTP, family, psize, string, urlpartsp->chamada);
		}
	}
	if ((global_file) && (!global_put)) {
		if ((psize) && (*psize > 0)) {
			if((fp = fopen(global_name_file, "wb")) == NULL) {
				INFO_PRINT("nao pode abrir arquivo %s para escrita.\n", global_name_file);
				saida (1);
			}
			rc = fwrite(conteudo, 1, *psize, fp);
			if (rc != *psize)
				INFO_PRINT ("Escrita do arquivo com problemas");
			fclose (fp);
		}
		else {
			INFO_PRINT ("sem conteudo, nao salva arquivo");
		}
	}
	else {
		printf ("%s", conteudo);
	}

	free (urlpartsp->host);
	free (urlpartsp->chamada);
	free (conteudo);



	return 0;
}