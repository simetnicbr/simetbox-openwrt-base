#define VERSAO_ALEXA "SimetBox-2.1"
#ifndef DATA_TYPE
#define DATA_TYPE void *
#endif
#include "local_simet_config.h"
typedef struct {
	int retries;
	int threads;
	char *user_agent;
	int num_sites;
	char **sites;
	int id_alexa_day;
} teste_alexa;



typedef struct {
	pthread_mutex_t *mutex;
	char *site;
	int retries;
	SSL_CTX *ssl_ctx;
} alexa_thread_arg;


 
int main_alexa (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options);
