#include "config.h"
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "local_simet_config.h"

void *args_ws (int *argc, const char *argv[]);
int main_ws (const char *nome, char* macAddress, int argc, const char* argv[], CONFIG_SIMET* config,  SSL_CTX* ctx, void *options);


