#define OPT_HASH_MEASURE 250
#define OPT_RATE 251
#define OPT_TEST 252
#define OPT_IDCOOKIE 253
#define OPT_LOCATION_SERVER 254
#define OPT_SIMET_SERVER 255
#define OPT_OUTPUT 256
#define VERSAO_SIMET "SimetBox-2.8"
#include "local_simet_config.h"

void *args_client (int *argc, const char *argv[]);
int main_simet_client (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET *config, SSL_CTX * ssl_ctx, void* options);
