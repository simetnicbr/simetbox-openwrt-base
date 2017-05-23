#include <inttypes.h>

#define ERROR 0
#define MAX_INPUT_LINE 80
#define SUCCESS 1
/* an idea from 'Advanced Programming in the Unix Environment'
	Stevens 1993 - see BecomeDaemonProcess() */
#define OPEN_MAX_GUESS 256

#define BITMASK(b) (128 >> ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))

#define HTTPS "https://"
#define HTTP "http://"
#define PORT_HTTPS "443"
#define PORT_HTTP "80"

#define NAO_ALOCA_MEM "nao conseguiu alocar mem"
#define TEMPO_WATCHDOG 600
#define MAX_HASH_MEASURE_ATTEMPTS 5
#define DEVICE_TYPE "SIMET-BOX"

#define DEFAULT_OPTS gopt_option( 'h', 0, gopt_shorts( 'h', '?' ), gopt_longs( "help", "HELP" )), gopt_option( 'v', GOPT_REPEAT, gopt_shorts( 'v' ), gopt_longs( "verbose" )), gopt_option('4', GOPT_REPEAT, gopt_shorts( '4' ), gopt_longs( "ipv4" )), gopt_option('6', GOPT_REPEAT, gopt_shorts( '6' ), gopt_longs( "ipv6" ))



char *lockFilename;


typedef struct {
	int https;
	char* host;
	char* chamada;
} URL_PARTS;

typedef struct {
	char *buffer;
	uint32_t size;
	uint32_t alloc_size;
} sized_buffer;

typedef struct {
	sigset_t *mask;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	int32_t indice_bloco;
	int32_t max_blocos;
	struct itimerval *timer;
} signal_thread_vazao_arg;

typedef struct {
	sigset_t *mask;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	uint64_t t_ini;
	uint64_t t_aux;
	uint64_t t_cur;
	uint64_t t_stop;
	uint64_t calculo_envios;
	uint64_t envios_previstos;
	uint64_t envios_previstos_por_burst;
	uint64_t periodo_relogio;
	uint64_t counter;
	int tam_buffer;
	double intervalo_em_usec;
	struct timespec ts_cur;
} signal_thread_vazao_udp_upload_arg;

void *link_biblioteca_dinamica();
int captura_lock (int tentativa, char *lockfile);
void saida (int rc);
//char *chama_web_service (char* host, char *port, char *chamada);
char *chama_pipe (char *comando, char *erro);
int hexdump ( unsigned char *c, int size );
//char *chama_troca_msg_tcp (int sock, char* host, char *port, char *chamada);
char *remove_barra_r_barra_n (char *str);
void *watchdog ();
char * get_json_string_from_ws (char *host, char *port, int family, char *formata_parametro, char *parametro, SSL_CTX *ssl_ctx);
char * get_hash_measure (char *host, char *port, int family, char *context_root, char* mac_address, SSL_CTX *ssl_ctx);
char* converte_url (char *assinatura);
char* converte_url2 (char *assinatura);
int decode_url(char *s);
char *detecta_interface_std_gw ();
void parse_verbose (void *options);
void parse_args_basico (void * options, char doc[], int* family);
void *args_basico (int *argc, const char *argv[]);
char *meu_sprintf (const char *fmt, ...);

void separa_protocolo_host_chamada (URL_PARTS *urlp, char* url);
char* le_arq (char *nome_arq);
uint32_t remove_bytes_buffer (sized_buffer *sbuf, uint32_t tam);
uint32_t armazena_dados_s_buffer (sized_buffer *sbuf, void *dados, int32_t tam_dados);
void libera_sized_buffer (sized_buffer *sbuf);
void limpa_sized_buffer (sized_buffer *sbuf);
sized_buffer* gera_md5 (sized_buffer *sbuf);
uint64_t get_rand_i ();
char *obtem_versao ();
char *obtem_modelo (int *pc);
int cria_arquivo_temporario (char *template_str);
int escreve_arquivo_nome (char *nome_arq, char *conteudo, int size, char *modo);
int escreve_arquivo_fd (int fd, char *conteudo, int size);
int meu_pthread_create (pthread_t *thread, size_t tam_stack_thread, void *(*start_routine) (void *), void *arg);
char *velocidade (int64_t size, int64_t tempo_total);
uint64_t uint64_t_pipe(char *temp, char *erro);
void *thread_trata_sinais_vazao (void *arg);
int64_t mediana (int64_t *list, int32_t right);
void remove_comentarios_e_barra_n (char *file);