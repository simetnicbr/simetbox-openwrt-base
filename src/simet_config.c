#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <simet_config.h>
#include "local_simet_config.h"
#include "debug.h"


/*char* le_arquivo (char *nome_arq) {
	FILE *fp;
	char *result = NULL;
	int string_size, read_size;

	if((fp = fopen(nome_arq, "r"))==NULL) {
		INFO_PRINT ("não pode abrir arquivo %s.\n", nome_arq);
		return NULL;
	}

	fseek (fp, 0, SEEK_END);
	string_size = ftell (fp);
	rewind(fp);
	result = (char *) malloc (sizeof (char) * (string_size + 1));
	if (!result) {
		INFO_PRINT ("erro ao alocar mem para arquivo de entrada\n");
		return NULL;
	}
	read_size = fread (result, sizeof(char), string_size, fp);
	if (read_size > string_size) {
		INFO_PRINT ("erro ao ler arquivo de entrada\n");
		return NULL;
	}
	result [read_size] = '\0';
	fclose (fp);
	return result;
}


void remove_comentarios_e_barra_n (char *file) {
	char *source, *destination;
	int barra_n = 0;

	for (source = file, destination = file; *source != '\0'; source++) {
		if (!isblank(*source)) {
			if (*source == '\n') {
				if (!barra_n) {
					barra_n = 1;
					*destination++ = *source;
				}
			}
			else {
				barra_n = 0;
				if (*source == '#') {
					for (; *source != '\0' && *source != '\n'; source++);
				}
				else *destination++ = *source;
			}
		}
	}
	*destination = '\0';
}
*/


void libera_config (CONFIG_SIMET* config) {
	if (config) {
		if (config->host)
			free (config->host);
		if (config->port)
			free (config->port);
		if (config->context_root)
			free (config->context_root);
		if (config->context_optional)
			free (config->context_optional);
		if (config->context_web_persistence)
			free (config->context_web_persistence);
		if (config->context_web_persistence_optional)
			free (config->context_web_persistence_optional);
		if (config->dns_autoritativo)
			free (config->dns_autoritativo);
		if (config->sufixo_dns_autoritativo)
			free (config->sufixo_dns_autoritativo);
		if (config->bcp38_server)
			free (config->bcp38_server);
		if (config->bcp38_local_ip)
			free (config->bcp38_local_ip);
		if (config->bcp38_remote_network_ip)
			free (config->bcp38_remote_network_ip);
		if (config->client_host_data_receiver)
			free (config->client_host_data_receiver);
		if (config->porta25_server)
			free (config->porta25_server);
		if (config->porta25_from)
			free (config->porta25_from);
		if (config->porta25_to)
			free (config->porta25_to);
		free (config);
	}
}


/*CONFIG_SIMET* config_simet () {
	char *key, *value, *pointer;
	CONFIG_SIMET* config_simet;
	char* file = le_arquivo (ARQ_CONFIG);

	config_simet = (CONFIG_SIMET*) malloc (sizeof (CONFIG_SIMET));
	if (!config_simet)
		return NULL;

	memset (config_simet, 0, sizeof (CONFIG_SIMET));

	remove_comentarios_e_barra_n (file);
	for (pointer = file; (key = strtok(pointer,"=")) ; pointer = NULL) {
		value = strtok (NULL, "\n");
		if ((key) && (value)) {
			if (strcmp (key, "cf_host") == 0) {
				config_simet->host = strdup (value);
			}
			else if (strcmp (key, "cf_port") == 0) {
				config_simet->port = strdup (value);
			}
			else if ((!config_simet->context_root) && (strcmp (key, "cf_simet_web_services") == 0)) {
				config_simet->context_root = strdup (value);
			}
			else if ((!config_simet->context_optional) && (strcmp (key, "cf_simet_web_services_optional") == 0)) {
				config_simet->context_optional = strdup (value);
			}
			else if ((!config_simet->context_web_persistence) && (strcmp (key, "cf_simet_web_persistence") == 0)) {
				config_simet->context_web_persistence = strdup (value);
			}
			else if ((!config_simet->context_web_persistence_optional) && (strcmp (key, "cf_simet_web_persistence_optional") == 0)) {
				config_simet->context_web_persistence_optional = strdup (value);
			}
			else if ((!config_simet->num_intra_ttl_queries) && (strcmp (key, "cf_num_intra_ttl_queries") == 0)) {
				config_simet->num_intra_ttl_queries = atoi (value);
				if (config_simet->num_intra_ttl_queries < 0) {
					INFO_PRINT ("chave cf_num_intra_ttl_queries = %d, menor que zero", config_simet->num_intra_ttl_queries);
					libera_config (config_simet);
					free (file);
					return NULL;
				}
			}
			else if ((!config_simet->intra_ttl_value) && (strcmp (key, "cf_intra_ttl_value") == 0)) {
				config_simet->intra_ttl_value = atoi (value);
				if (config_simet->intra_ttl_value < 0) {
					INFO_PRINT ("chave cf_intra_ttl_value = %d, menor que zero", config_simet->intra_ttl_value);
					libera_config (config_simet);
					free (file);
					return NULL;
				}
			}
			else if ((!config_simet->num_pre_ttl_queries_per_round) && (strcmp (key, "cf_num_pre_ttl_queries_per_round") == 0)) {
				config_simet->num_pre_ttl_queries_per_round = atoi (value);
				if (config_simet->num_pre_ttl_queries_per_round < 0) {
					INFO_PRINT ("chave cf_num_pre_ttl_queries_per_round = %d, menor que zero", config_simet->num_pre_ttl_queries_per_round);
					libera_config (config_simet);
					free (file);
					return NULL;
				}
			}
			else if ((!config_simet->pre_ttl_value) && (strcmp (key, "cf_pre_ttl_value") == 0)) {
				config_simet->pre_ttl_value = atoi (value);
				if (config_simet->pre_ttl_value < 0) {
					INFO_PRINT ("chave cf_pre_ttl_value = %d, menor que zero", config_simet->pre_ttl_value);
					libera_config (config_simet);
					free (file);
					return NULL;
				}
			}
		}
		else {
			INFO_PRINT ("arquivo de configuracao %s ilegal: chave %s, valor %s", ARQ_CONFIG, key?key:"sem chave", value?value:"sem valor");
			libera_config (config_simet);
			free (file);
			return NULL;
		}
	}
	free (file);
	return config_simet;
}
*/

CONFIG_SIMET* config_simet () {
	char *key, *value;
	CONFIG_SIMET* config_simet;
	CONFIG_ENTRY* entry;

	CONFIG config;

	loadConfig(&config, ARQ_CONFIG);
	if (!config.readSuccess)
		return NULL;

	config_simet = (CONFIG_SIMET*) malloc (sizeof (CONFIG_SIMET));
	if (!config_simet)
		return NULL;

	memset (config_simet, 0, sizeof (CONFIG_SIMET));



	for (entry = config.head; entry != NULL ; entry = entry->next) {
		key = entry->key;
		value = entry->value;
		if ((key) && (value)) {
			if (strcmp (key, "cf_host") == 0) {
				config_simet->host = strdup (value);
			}
			else if (strcmp (key, "cf_port") == 0) {
				config_simet->port = strdup (value);
			}
			else if ((!config_simet->context_root) && (strcmp (key, "cf_simet_web_services") == 0)) {
				config_simet->context_root = strdup (value);
			}
			else if ((!config_simet->context_optional) && (strcmp (key, "cf_simet_web_services_optional") == 0)) {
				config_simet->context_optional = strdup (value);
			}
			else if ((!config_simet->context_web_persistence) && (strcmp (key, "cf_simet_web_persistence") == 0)) {
				config_simet->context_web_persistence = strdup (value);
			}
			else if ((!config_simet->context_web_persistence_optional) && (strcmp (key, "cf_simet_web_persistence_optional") == 0)) {
				config_simet->context_web_persistence_optional = strdup (value);
			}
			else if ((!config_simet->dns_autoritativo) && (strcmp (key, "cf_dns_autoritativo") == 0)) {
				config_simet->dns_autoritativo = strdup (value);
			}
			else if ((!config_simet->sufixo_dns_autoritativo) && (strcmp (key, "cf_sufixo_dns_autoritativo") == 0)) {
				config_simet->sufixo_dns_autoritativo = strdup (value);
			}
			else if ((!config_simet->bcp38_server) && (strcmp (key, "cf_bcp38_server") == 0)) {
				config_simet->bcp38_server = strdup (value);
			}
			else if ((!config_simet->bcp38_local_ip) && (strcmp (key, "cf_bcp38_local_ip") == 0)) {
				config_simet->bcp38_local_ip = strdup (value);
			}
			else if ((!config_simet->bcp38_remote_network_ip) && (strcmp (key, "cf_bcp38_remote_network_ip") == 0)) {
				config_simet->bcp38_remote_network_ip = strdup (value);
			}
			else if ((!config_simet->client_host_data_receiver) && (strcmp (key, "cf_client_host_data_receiver") == 0)) {
				config_simet->client_host_data_receiver = strdup (value);
			}
			else if ((!config_simet->porta25_server) && (strcmp (key, "cf_porta25_server") == 0)) {
				config_simet->porta25_server = strdup (value);
			}
			else if ((!config_simet->porta25_from) && (strcmp (key, "cf_porta25_from") == 0)) {
				config_simet->porta25_from = strdup (value);
			}
			else if ((!config_simet->porta25_to) && (strcmp (key, "cf_porta25_to") == 0)) {
				config_simet->porta25_to = strdup (value);
			}
			else if ((!config_simet->num_intra_ttl_queries) && (strcmp (key, "cf_num_intra_ttl_queries") == 0)) {
				config_simet->num_intra_ttl_queries = atoi (value);
				if (config_simet->num_intra_ttl_queries < 0) {
					INFO_PRINT ("chave cf_num_intra_ttl_queries = %d, menor que zero", config_simet->num_intra_ttl_queries);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->intra_ttl_value) && (strcmp (key, "cf_intra_ttl_value") == 0)) {
				config_simet->intra_ttl_value = atoi (value);
				if (config_simet->intra_ttl_value < 0) {
					INFO_PRINT ("chave cf_intra_ttl_value = %d, menor que zero", config_simet->intra_ttl_value);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->num_pre_ttl_queries_per_round) && (strcmp (key, "cf_num_pre_ttl_queries_per_round") == 0)) {
				config_simet->num_pre_ttl_queries_per_round = atoi (value);
				if (config_simet->num_pre_ttl_queries_per_round < 0) {
					INFO_PRINT ("chave cf_num_pre_ttl_queries_per_round = %d, menor que zero", config_simet->num_pre_ttl_queries_per_round);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->pre_ttl_value) && (strcmp (key, "cf_pre_ttl_value") == 0)) {
				config_simet->pre_ttl_value = atoi (value);
				if (config_simet->pre_ttl_value < 0) {
					INFO_PRINT ("chave cf_pre_ttl_value = %d, menor que zero", config_simet->pre_ttl_value);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->pre_ttl_rounds_interval) && (strcmp (key, "cf_pre_ttl_rounds_interval") == 0)) {
				config_simet->pre_ttl_rounds_interval = atoi (value);
				if (config_simet->pre_ttl_rounds_interval < 0) {
					INFO_PRINT ("chave cf_pre_ttl_rounds_interval = %d, menor que zero", config_simet->pre_ttl_rounds_interval);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->bcp38_src_port) && (strcmp (key, "cf_bcp38_src_port") == 0)) {
				config_simet->bcp38_src_port = atoi (value);
				if (config_simet->bcp38_src_port < 0) {
					INFO_PRINT ("chave cf_bcp38_src_port = %d, menor ou igual a zero", config_simet->bcp38_src_port);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->bcp38_dest_port) && (strcmp (key, "cf_bcp38_dest_port") == 0)) {
				config_simet->bcp38_dest_port = atoi (value);
				if (config_simet->bcp38_dest_port < 0) {
					INFO_PRINT ("chave cf_bcp38_dest_port = %d, menor ou igual a zero", config_simet->bcp38_dest_port);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_tcp_upload_buffer_multiplier) && (strcmp (key, "cf_vazao_tcp_upload_buffer_multiplier") == 0)) {
				config_simet->vazao_tcp_upload_buffer_multiplier = atoi (value);
				if (config_simet->vazao_tcp_upload_buffer_multiplier < 0) {
					INFO_PRINT ("chave cf_vazao_tcp_upload_buffer_multiplier = %d, menor ou igual a zero", config_simet->vazao_tcp_upload_buffer_multiplier);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_tcp_upload_lowat_percentage) && (strcmp (key, "cf_vazao_tcp_upload_lowat_percentage") == 0)) {
				config_simet->vazao_tcp_upload_lowat_percentage = atoi (value);
				if (config_simet->vazao_tcp_upload_lowat_percentage < 0) {
					INFO_PRINT ("chave cf_vazao_tcp_upload_lowat_percentage = %d, menor ou igual a zero", config_simet->vazao_tcp_upload_lowat_percentage);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_udp_upload_buffer_multiplier) && (strcmp (key, "cf_vazao_udp_upload_buffer_multiplier") == 0)) {
				config_simet->vazao_udp_upload_buffer_multiplier= atoi (value);
				if (config_simet->vazao_udp_upload_buffer_multiplier < 0) {
					INFO_PRINT ("chave cf_vazao_udp_upload_buffer_multiplier = %d, menor ou igual a zero", config_simet->vazao_udp_upload_buffer_multiplier);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_udp_upload_max_buffer_percentage) && (strcmp (key, "cf_vazao_udp_upload_max_buffer_percentage") == 0)) {
				config_simet->vazao_udp_upload_max_buffer_percentage= atoi (value);
				if (config_simet->vazao_udp_upload_max_buffer_percentage < 0) {
					INFO_PRINT ("chave cf_vazao_udp_upload_max_buffer_percentage = %d, menor ou igual a zero", config_simet->vazao_udp_upload_max_buffer_percentage);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_udp_upload_max_package_burst) && (strcmp (key, "cf_vazao_udp_upload_max_package_burst") == 0)) {
				config_simet->vazao_udp_upload_max_package_burst= atoi (value);
				if (config_simet->vazao_udp_upload_max_package_burst < 0) {
					INFO_PRINT ("chave cf_vazao_udp_upload_max_package_burst = %d, menor ou igual a zero", config_simet->vazao_udp_upload_max_package_burst);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_tcp_download_buffer_multiplier) && (strcmp (key, "cf_vazao_tcp_download_buffer_multiplier") == 0)) {
				config_simet->vazao_tcp_download_buffer_multiplier = atoi (value);
				if (config_simet->vazao_tcp_download_buffer_multiplier < 0) {
					INFO_PRINT ("chave cf_vazao_tcp_download_buffer_multiplier = %d, menor ou igual a zero", config_simet->vazao_tcp_download_buffer_multiplier);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_tcp_download_lowat_value) && (strcmp (key, "cf_vazao_tcp_download_lowat_value") == 0)) {
				config_simet->vazao_tcp_download_lowat_value = atoi (value);
				if (config_simet->vazao_tcp_download_lowat_value < 0) {
					INFO_PRINT ("chave cf_vazao_tcp_download_lowat_value = %d, menor ou igual a zero", config_simet->vazao_tcp_download_lowat_value);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_udp_download_buffer_multiplier) && (strcmp (key, "cf_vazao_udp_download_buffer_multiplier") == 0)) {
				config_simet->vazao_udp_download_buffer_multiplier = atoi (value);
				if (config_simet->vazao_udp_download_buffer_multiplier < 0) {
					INFO_PRINT ("chave cf_vazao_udp_download_buffer_multiplier = %d, menor ou igual a zero", config_simet->vazao_udp_download_buffer_multiplier);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_tcp_cork) && (strcmp (key, "cf_vazao_tcp_cork") == 0)) {
				config_simet->vazao_tcp_cork = atoi (value);
				if (config_simet->vazao_tcp_cork < 0) {
					INFO_PRINT ("chave cf_vazao_tcp_cork = %d, menor ou igual a zero", config_simet->vazao_tcp_cork);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_tcp_nodelay) && (strcmp (key, "cf_vazao_tcp_nodelay") == 0)) {
				config_simet->vazao_tcp_nodelay = atoi (value);
				if (config_simet->vazao_tcp_nodelay < 0) {
					INFO_PRINT ("chave cf_vazao_tcp_nodelay = %d, menor ou igual a zero", config_simet->vazao_tcp_nodelay);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->vazao_udp_upload_tcp_avg_percentage) && (strcmp (key, "cf_vazao_udp_upload_tcp_avg_percentage") == 0)) {
				config_simet->vazao_udp_upload_tcp_avg_percentage = atoi (value);
				if (config_simet->vazao_udp_upload_tcp_avg_percentage < 0) {
					INFO_PRINT ("chave cf_vazao_udp_upload_tcp_avg_percentage = %d, menor ou igual a zero", config_simet->vazao_udp_upload_tcp_avg_percentage);
					libera_config (config_simet);
					return NULL;
				}
			}
			else if ((!config_simet->mensagens_erro_ssl) && (strcmp (key, "cf_mensagens_erro_ssl") == 0)) {
				config_simet->mensagens_erro_ssl = atoi (value);
				if (config_simet->mensagens_erro_ssl < 0) {
					INFO_PRINT ("chave cf_mensagens_erro_ssl = %d, menor ou igual a zero", config_simet->mensagens_erro_ssl);
					libera_config (config_simet);
					return NULL;
				}
			}
		}

		else {
			INFO_PRINT ("arquivo de configuracao %s ilegal: chave %s, valor %s", ARQ_CONFIG, key?key:"sem chave", value?value:"sem valor");
			libera_config (config_simet);
			return NULL;
		}
	}
	destroyConfig(&config);
	return config_simet;
}
