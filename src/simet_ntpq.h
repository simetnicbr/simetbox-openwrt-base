#include "config.h"
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "local_simet_config.h"

int main_ntpq (const char *nome, char* macAddress, int argc, const char* argv[], CONFIG_SIMET* config,  SSL_CTX* ctx, void *options);

typedef struct {
#if (defined BYTE_ORDER && BYTE_ORDER == BIG_ENDIAN) || (defined __sun && defined _BIG_ENDIAN)
	unsigned char leap_indicator :2;
	unsigned char version_number :3;
	unsigned char mode :3;

	unsigned char response :1;
	unsigned char error :1;
	unsigned char more :1;
	unsigned char opcode :5;
#else
	unsigned char mode :3;
	unsigned char version_number :3;
	unsigned char leap_indicator :2;

	unsigned char opcode :5;
	unsigned char more :1;
	unsigned char error :1;
	unsigned char response :1;
#endif
	uint16_t sequence;
#if (defined BYTE_ORDER && BYTE_ORDER == BIG_ENDIAN) || (defined __sun && defined _BIG_ENDIAN)
	unsigned char leap_indicator_2 :2;
	unsigned char clock_src :6;
	
	unsigned char system_evt_counter :4;
	unsigned char system_evt_code :4;
#else
	unsigned char clock_src :6;
	unsigned char leap_indicator_2 :2;
	
	unsigned char system_evt_code :4;
	unsigned char system_evt_counter :4;
#endif
	uint16_t association_id;
	uint16_t offset;
	uint16_t count;
} ntpq_response;
