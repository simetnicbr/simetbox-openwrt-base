/** ----------------------------------------
 * @file  simet_client.c
 * @brief  Send the initial messages.
 * @authors  Michel Vale Ferreira and Rafael de O. Lopes Goncalves
 * @date 20 de outubro de 2011
 *------------------------------------------*/

/*
 * Changelog:
 * Created: 20 de outubro de 2011
 * Last Change: 31 de janeiro de 2013
 */

#define _GNU_SOURCE
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <limits.h>

#include "simet_config.h"
#include "simet_client.h"
#include "protocol.h"
#include "protocol_event.h"
#include "debug.h"
#include "server_finder.h"
#include "gopt.h"
#include "simet_tools.h"
#include "netsimet.h"
#include "utils.h"




struct arguments {
	const char * simet_server;
	const char * location_server;
	char * id_cookie;
	char * hash_measure;
	const char * max_rate_udp_download;
	enum test_type tests_to_do[CONTEXT_QUEUE_MAX_TESTS]; /*test in order */
	int64_t number_of_tests;
	const char *output; /* --output=FILE */
};

char * get_uname() {
	struct utsname buf;
	uname(&buf);
	return strdup(buf.nodename);
}

static void add_test(struct arguments * arguments, int key) {
	arguments->tests_to_do[arguments->number_of_tests++] = key;
}

static void parse_args_client(struct arguments * arguments, void * options, char doc[], int* family) {
	const char * aux;

	parse_args_basico(options, doc, family);


	if (gopt_arg(options, OPT_OUTPUT, &aux)) {
		arguments->output = aux;
	}
	if (gopt_arg(options, OPT_SIMET_SERVER, &aux)) {
		arguments->simet_server = aux;
	}
	if (gopt_arg(options, OPT_LOCATION_SERVER, &aux)) {
		arguments->location_server = aux;
	}
	if (gopt_arg(options, OPT_IDCOOKIE, &aux)) {
		arguments->id_cookie = (char*) aux;
	}
	if (gopt_arg(options, OPT_HASH_MEASURE, &aux)) {
		arguments->hash_measure = (char*) aux;
	}

	if (gopt_arg(options, OPT_RATE, &aux)) {
		arguments->max_rate_udp_download = aux;
	}
	if (gopt_arg(options, OPT_TEST, &aux)) {
		if (strncmp("rtt", aux, strlen("rtt")) == 0) {
			add_test(arguments, RTT_TEST_TYPE);
		}
		if (strncmp("udp", aux, strlen("udp")) == 0) {
			add_test(arguments, UDP_UPLOAD_TESTS_TYPE);
			add_test(arguments, UDP_DOWNLOAD_TESTS_TYPE);
		}
		if (strncmp("tcp", aux, strlen("tcp")) == 0) {
			add_test(arguments, TCP_TESTS_TYPE);
			add_test(arguments, TCP_UPLOAD_TESTS_TYPE);
			add_test(arguments, TCP_DOWNLOAD_TESTS_TYPE);
		}
		if (strncmp("jitter", aux, strlen("jitter")) == 0) {
			add_test(arguments, JITTER_UPLOAD_TESTS_TYPE);
			add_test(arguments, JITTER_DOWNLOAD_TESTS_TYPE);
		}
	} else {

		add_test(arguments, RTT_TEST_TYPE);
		add_test(arguments, JITTER_UPLOAD_TESTS_TYPE);
		add_test(arguments, JITTER_DOWNLOAD_TESTS_TYPE);
		add_test(arguments, TCP_TESTS_TYPE);
		add_test(arguments, TCP_UPLOAD_TESTS_TYPE);
		add_test(arguments, TCP_DOWNLOAD_TESTS_TYPE);
		add_test(arguments, UDP_UPLOAD_TESTS_TYPE);
		add_test(arguments, UDP_DOWNLOAD_TESTS_TYPE);
	}

}

void *args_client (int *argc, const char *argv[]) {
	void *options =
			gopt_sort(argc, argv,
					gopt_start(
							DEFAULT_OPTS,
							gopt_option(OPT_SIMET_SERVER, GOPT_ARG, gopt_shorts( 's' ), gopt_longs( "simetserver" )),
							gopt_option(OPT_LOCATION_SERVER, GOPT_ARG, gopt_shorts( 'l' ), gopt_longs( "location" )),
							gopt_option(OPT_IDCOOKIE, GOPT_ARG, gopt_shorts( 'i' ), gopt_longs( "idcookie" )),
							gopt_option(OPT_HASH_MEASURE, GOPT_ARG, gopt_shorts( 'm' ), gopt_longs( "hashmeasure" )),
							gopt_option(OPT_RATE, GOPT_ARG, gopt_shorts( 'r' ), gopt_longs( "rate" )),
							gopt_option(OPT_TEST, GOPT_ARG, gopt_shorts( 't' ), gopt_longs( "test" ))
					));
	parse_verbose (options);

	return options;
}

// void trafego_iface (char *ifname) {
// 	int sock;
//     struct ifreq ifr;
//
//     sock = socket(AF_UNSPEC, SOCK_STREAM, 0);
//
//     //Type of address to retrieve - IPv4 IP address
//     ifr.ifr_addr.sa_family = AF_UNSPEC;
//
//     //Copy the interface name in the ifreq structure
//     strncpy(ifr.ifr_name , ifname , IFNAMSIZ);
//
//     ioctl(sock, SIOCGIFADDR, &ifr);
//
//     close(sock);
// }

int main_simet_client (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET *config, SSL_CTX * ssl_ctx, void *options) {

	Context_t *context;
	struct arguments arguments;
	int i, proto;
	uint64_t bytes_antes = 0, bytes_depois = 0, bytes_total;
	int64_t size;
    char *std_gw, *temp, *build = NULL, *modelo = NULL, *ret_func;
    int family = 0;
	proto_disponiveis disp;
	char doc[] =
		"\nsimet_client: Uma implementacao em C do cliente de medicao de qualidade de internet Simet.\n"
				"uso: simet_client [argumentos]\n\n"
				"Argumentos:\n"
				"\t-4 testa em IPv4\n"
				"\t-6 testa em IPv6\n"
				"\t-t <teste>\n"
				"\tteste pode ser um dos seguintes:\n"
				"\t\trtt \t Round to Trip Time\n"
				"\t\tudp \t Udp Upload and Download\n"
				"\t\ttcp \t Tcp Upload and Download\n"
				"\t\tjitter \t Jitter Upload and Download"
				"\n\t-s <server> \t Definir o endereco simet server."
				"\n\t-r <RATE> \t Definir a razao de bits por segundo no teste de UDP upload."
				"\n\t-l <locationserver> \t Definir o enedereco do location server."
				"\n\t-i <id> \t Envia o idcookie <cookie> ao inves do uname\n"
				"\n\t-m <hash_measure> \t Usa o hashmeasure passado como parametro\n"
				"\n\t-f \t Forca a execucao do teste, independente da vazao do cliente no momento\n";


/*
    ret_func = chama_pipe("/usr/bin/get_model.sh", "falha ao executar o script para detectar o hardware\n");
    if (ret_func) {
        modelo = ret_func;
    }
    else {
    // não bateu com nenhum modelo
        ret_func = chama_pipe("/bin/cat /proc/cpuinfo |grep vendor_id", "falha ao executar o programa cat para detectar o hardware\n");
        if (ret_func) {
            if (strstr (ret_func, "GenuineIntel")) {
                INFO_PRINT ("Executando a partir de um PC. Assumindo um valor válido para modelo do simetbox\n");
                modelo = strdup ("07400004");
				if (!modelo) {
					ERROR_PRINT ("%s\n", NAO_ALOCA_MEM);
					saida (1);
				}
            }
            free (ret_func);
        }
        else {
            INFO_PRINT ("Nao foi encontrado modelo do simetbox. Abortando");
            saida (1);
        }
    }

	ret_func = chama_pipe ("/bin/uname -v", "falha ao executar o programa uname para detectar a versao do sistema operacional\n");
    if (ret_func) {
        pbuild = strchr (ret_func, '#');
        if (pbuild) {
            pbuild++;
            pfim_build = pbuild;
            while (isdigit (*pfim_build)) {
                pfim_build++;
            }
            if (pfim_build)
                *pfim_build = '\0';
            build = strdup (pbuild);
        }
        free (ret_func);
    }
    else {
		INFO_PRINT ("Nao foi encontrada versao do simetbox. Abortando");
		saida(1);
	}
*/
	build = obtem_versao();
	modelo = obtem_modelo(NULL);

	const char versao [] = VERSAO_SIMET;
	INFO_PRINT("%s versao %s\n", nome, versao);


	ret_func = chama_pipe("ps w |grep auto_upgrade|grep -v grep|grep -v Z|grep -v gedit", "falha ao detectar execucao paralela ao teste do auto-upgrade\n");
	if (ret_func) {
		saida (1);
	}
	arguments.number_of_tests = 0;
	arguments.output = "-";
	arguments.location_server = config->client_host_data_receiver;
	arguments.simet_server = NULL;
	arguments.id_cookie = NULL;
	arguments.hash_measure = NULL;
	arguments.max_rate_udp_download =NULL;

	parse_args_client (&arguments, options, doc, &family);
	if ((arguments.hash_measure != NULL) && (family == 0)) {
		INFO_PRINT ("Nao e possivel especificar um hash_measure sem especificar uma familia de IP (IPv4 ou IPv6)\n");
		saida (1);
	}


	protocolos_disponiveis (&disp, family, 1, config, ssl_ctx);
	for (proto = PROTO_IPV4; proto <= PROTO_IPV6; proto++) {
		if (!disp.proto [proto])
			continue;
		INFO_PRINT ("Inicio teste familia %d\n", (proto == PROTO_IPV4) ? 4 : 6);

		if (proto == PROTO_IPV4)
			std_gw = disp.iface_stdgw.if_ipv4;
		else
			std_gw = disp.iface_stdgw.if_ipv6;
		if (strnlen(std_gw, 1) == 0)
			saida (1);

//		if ((asprintf(&temp, "ifconfig %s | grep \"RX bytes\" | awk '{print $2}' | sed -e \"s/bytes://\"", std_gw)) < 0) {
		if ((asprintf(&temp, "grep %s < /proc/net/dev |awk '{print $2}'", std_gw)) < 0) {


			ERROR_PRINT ("Nao conseguiu alocar\n");
			saida (1);
		}




		create_context(&context);
		for (i = 0; i < arguments.number_of_tests; i++) {
			context->tests_to_do[i] = arguments.tests_to_do[i];
		}

		if (arguments.id_cookie == NULL) {
			context->id_cookie = chama_pipe ("/usr/bin/get_mac_address.sh", "nao conseguiu alocar memoria para mac address\n");
		} else {
			context->id_cookie = strdup (arguments.id_cookie);
		}
		INFO_PRINT("idcookie == %s", context->id_cookie );
			context->family = (proto == PROTO_IPV4) ? 4 : 6;
#ifdef SELECT
		INFO_PRINT ("SELECT");
#else
		INFO_PRINT ("POLL");
#endif
#ifdef IOVEC
		INFO_PRINT ("IOVEC, IOV_MAX: %d, SSIZE_MAX: %"PRI64, IOV_MAX, SSIZE_MAX);
#endif
#ifdef MMSG
		INFO_PRINT ("MMSG");
#endif
//		trafego_iface(std_gw);
		bytes_antes = uint64_t_pipe(temp, "falha ao detectar volume de dados na interface std_gw\n");
		while (1) {
			sleep (5);
			bytes_depois = uint64_t_pipe(temp, "falha ao detectar volume de dados na interface std_gw\n");
			bytes_total = (bytes_depois - bytes_antes)/5*8;
			if (bytes_total >= 128000) {
				INFO_PRINT ("muita banda em uso: %"PRUI64"bps\n", bytes_total);
				ret_func = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, 0, &size, 0, "/%s/InsertSimetBoxFail?macAddress=%s&versaoAtual=%s&modelo=%s&detectedBandwidth=%"PRUI64"&failDescription=MUITABANDAEMUSO", config->context_optional, context->id_cookie, build, modelo, bytes_total);
				if (ret_func)
					free (ret_func);
				bytes_antes = uint64_t_pipe(temp, "falha ao detectar volume de dados na interface std_gw\n");
			}
			else {
				free (temp);
				break; // inicia o teste
			}
		}
		if (arguments.hash_measure == NULL) {
			context->hash_measure = get_hash_measure (config->host, config->port, 0, config->context_root, context->id_cookie, ssl_ctx);
		} else {
			context->hash_measure = strdup (arguments.hash_measure);
		}
		INFO_PRINT("hash_measure == %s", context->hash_measure);
		if ((strstr(std_gw, "eth")) || (strstr(std_gw, "br-lan")) || (strstr(std_gw, "pppoe-wan")))
			context->tipo_conexao = strdup("CABEADO");
		else if (strstr(std_gw, "wlan"))
			context->tipo_conexao = strdup("WIFI");
		else if (strstr(std_gw, "3g"))
			context->tipo_conexao = strdup("MOVEL");
		else context->tipo_conexao = strdup("UNKNOWN");

		INFO_PRINT("tipo_conexao == %s", context->tipo_conexao);
		context->config = config;

		if (arguments.simet_server != NULL) {
			context->serverinfo = malloc(sizeof(Simet_server_info_t));
			context->serverinfo->address_text = strdup(arguments.simet_server);
			context->serverinfo->location = strdup("São Paulo - SP");
			context->serverinfo->id_pool_server = 3;
			context->serverinfo->description = NULL;
		}
		else {
			if (get_simet_servers(config->host, config->context_root, config->port, arguments.location_server, &context->serverinfo, context->family, ssl_ctx) == 0) {
				INFO_PRINT ("Sem servidores para a familia %d\n", context->family);
				saida (1);
			}
		}

		if (strcmp(arguments.output, "-") != 0) {
			context->output = fopen(arguments.output, "a+");
			if (context->output == NULL) {
				ERROR_PRINT("Error oppening %s", arguments.output);
				saida(EXIT_FAILURE);
			}
		} else {
			context->output = stderr;
		}
		if (arguments.max_rate_udp_download != NULL ) {
			TRACE_PRINT("max rate = %s",arguments.max_rate_udp_download);
			context->max_rate_in_bps = atoll(arguments.max_rate_udp_download);
			TRACE_PRINT("max rate = %"PRI64,context->max_rate_in_bps);
		}

		INFO_PRINT("simet server: %s", context->serverinfo->address_text);

		context->ssl_ctx = ssl_ctx;


		register_event(context, create_event(&event_start_control_connection, DISPATCH, context->serverinfo));
		do_events_loop(context);
		INFO_PRINT ("Fim teste familia %d\n", (proto == PROTO_IPV4) ? 4 : 6);
	}
	if (modelo)
		free (modelo);
	if (build)
		free (build);


	saida(EXIT_SUCCESS);
	return (EXIT_SUCCESS);
}
