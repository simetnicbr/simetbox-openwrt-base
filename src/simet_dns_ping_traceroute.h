#include "config.h"
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "local_simet_config.h"


#define VERSAO_DNS_TRACEROUTE "SimetBox-2.1"
int main_dns_ping_traceroute (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options);
