#define VERSAO_UPTIME "0.0.1"
//#define    TIMEVAL_MICROSECONDS(tv)  (((((struct timeval) tv).tv_sec) * 1e6L) + (((struct timeval) tv).tv_usec))
#define SELECT_TIMEOUT 1
#define PERIODO_ATUALIZA_ARQ_OFF 60
// 1 min
#define TMPDIRUPTEMPLATE "/tmp/simet_uptime.XXXXXX"

#pragma pack(1)
typedef struct {
	uint16_t tam;
	uint8_t ver_protocol;
	uint8_t tipo_msg;
	uint16_t versao_sb;
	uint64_t uptime;
	uint8_t size_mac;
	char mac_address[12];
	uint8_t size_type;
	char type [9];
} simet_id;

typedef struct {
	uint16_t tam;
	uint8_t ver_protocol;
	uint8_t tipo_msg;
	uint64_t id_transacao;
	uint8_t rc;
} simet_confirma_recv_aplic;

typedef struct {
	uint64_t time_now;
	uint64_t last_net_activity_recv;
	uint64_t last_net_activity_send;
	uint64_t id_transacao;
	uint64_t hora_ntp;
	uint8_t tem_hora_ntp:1;
	uint8_t desconecte:1;
	uint8_t connected:1;
	uint8_t envia_heartbeat:1;
	uint8_t envia_id:1;
	uint8_t finaliza:1;
	uint8_t status_iface:1;
	uint8_t status_cabo:1;
	int8_t pingando;
	struct timeval tv;
	sized_buffer sbuf_envio;
	sized_buffer sbuf_recebimento;
	sized_buffer sbuf_temp;
	sized_buffer checksum_recebido;
	char *nome_arq_uptime;
	char *nome_buffer_aplic;
	char *std_gw;
	int sock_link_cabo;
} control_center;

typedef struct {
	sigset_t *mask;
	CONFIG_SIMET *config;
	control_center *control;
} signal_thread_arg;

typedef struct {
	control_center *control;
} ping_thread_arg;




int main_uptime (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET *config, SSL_CTX * ssl_ctx, void* options);
