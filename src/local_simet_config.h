#ifndef CONFIG_H
#define CONFIG_H
#define ARQ_CONFIG	"/etc/config/simet.conf"
typedef struct {
	char *host;
	char *port;
	char *context_root;
	char *context_optional;
	char *context_web_persistence;
	char *context_web_persistence_optional;
	char *dns_autoritativo;
	char *sufixo_dns_autoritativo;
	char *bcp38_server;
	char *bcp38_local_ip;
	char *bcp38_remote_network_ip;
	char *client_host_data_receiver;
	char *porta25_server;
	char *porta25_from;
	char *porta25_to;	
	int num_intra_ttl_queries;
	int intra_ttl_value;
	int num_pre_ttl_queries_per_round;
	int pre_ttl_rounds_interval;
	int pre_ttl_value;
	int mensagens_erro_ssl;
	int bcp38_src_port;
	int bcp38_dest_port;
	int vazao_tcp_upload_buffer_multiplier;
	int vazao_tcp_upload_lowat_percentage;
	int vazao_udp_upload_buffer_multiplier;
	int vazao_udp_upload_max_buffer_percentage;
	int vazao_udp_upload_max_package_burst;
	int vazao_tcp_download_buffer_multiplier;
	int vazao_tcp_download_lowat_value;
	int vazao_udp_download_buffer_multiplier;
	int vazao_udp_download_lowat_value;
	int vazao_tcp_cork;
	int vazao_tcp_nodelay;
	int vazao_udp_upload_tcp_avg_percentage;
} CONFIG_SIMET;



CONFIG_SIMET* config_simet(void);
void libera_config(CONFIG_SIMET* config);
#endif
