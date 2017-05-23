#include "config.h"
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "local_simet_config.h"


#define NUM_TESTES_FIXOS 8
#define VERSAO_DNS "SimetBox-2.6"



//#define INTERVALO_QUERY


#define C_IN 1 //Internet
#define C_CH 3 //Chaos

// tipo de registro_dns
#define T_A 1 // endereço Ipv4
#define T_NS 2 // Nameserver
#define T_CNAME 5 // canonical name
#define T_SOA 6 // start of authority zone 
#define T_PTR 12 // domain name pointer 
#define T_MX 15 // mail server
#define T_TXT 16 // TXT
#define T_AAAA 28 // endereço IPv6
#define T_RRSIG 46 // DNSSEC
#define T_NSEC 47 // NSEC
#define T_DNSKEY 48 // DNSKEY

// tipo de registro
#define TR_TXT 0
#define TR_A_AAAA 1
#define TR_RRSIG 2

// tipo de teste
typedef enum {
	HOSTBIND = 0,
	VERSIONBIND,
	DNSSEC_INVALIDO,
	DNSSEC_VALIDO,
	DNSSEC_EXPIRADO,
	DNS_INEXISTENTE_1,
	DNS_INEXISTENTE_2,
	IPV6,
// obs.: os tipos de INTRA_TTL e PRE_TTL não estão mais neste enum, pois suas
// quantidades são definidas dinamicamente através de arquivo de configuração
} TIPO_TESTE;

//DNS header structure
typedef struct {
	uint16_t id; // identification number
#if (defined BYTE_ORDER && BYTE_ORDER == BIG_ENDIAN) || (defined __sun && defined _BIG_ENDIAN)
	unsigned char qr :1; // query/response flag
	unsigned char opcode :4; // purpose of message
	unsigned char aa :1; // authoritive answer
	unsigned char tc :1; // truncated message
	unsigned char rd :1; // recursion desired

	unsigned char ra :1; // recursion available
	unsigned char z :1; // its z! reserved
	unsigned char ad :1; // authenticated data
	unsigned char cd :1; // checking disabled
	unsigned char rcode :4; // response code
#else
	unsigned char rd :1; // recursion desired
	unsigned char tc :1; // truncated message
	unsigned char aa :1; // authoritive answer
	unsigned char opcode :4; // purpose of message
	unsigned char qr :1; // query/response flag

	unsigned char rcode :4; // response code
	unsigned char cd :1; // checking disabled
	unsigned char ad :1; // authenticated data
	unsigned char z :1; // its z! reserved
	unsigned char ra :1; // recursion available
#endif
	uint16_t q_count; // number of question entries
	uint16_t ans_count; // number of answer entries
	uint16_t auth_count; // number of authority entries
	uint16_t add_count; // number of resource entries
} DNS_HEADER;

//Constant sized fields of query structure
typedef struct {
	uint16_t qtype;
	uint16_t qclass;
} QUESTION;

//Constant sized fields of the resource record structure
#pragma pack(push, 1)
typedef struct {
	uint16_t type;
	uint16_t _class;
	uint32_t ttl;
	uint16_t data_len;
} R_DATA;

typedef struct {
	uint16_t type_covered;
	uint8_t algorithm;
	uint8_t labels;
	uint32_t original_ttl;
	uint32_t signature_expiration;
	uint32_t time_signed;
	uint16_t id_of_signing_key_fingerprint;
} R_DATA_RRSIG;

typedef struct {
	uint32_t serial_number;
	uint32_t refresh_interval;
	uint32_t retry_interval;
	uint32_t expiration_limit;
	uint32_t minimum_ttl;
} R_DATA_SOA;

typedef struct {
	uint8_t name;
	uint16_t type_otp;
	uint16_t udp_payload_size;
	uint8_t higher_bits_xrcode;
	uint8_t edns0_version;
	uint16_t z;
	uint16_t data_len;
} ADDITIONAL_RECORDS;
#pragma pack(pop)

//Pointers to resource record contents
typedef struct {
	unsigned char *name;
	R_DATA *resource;
	unsigned char *rdata;
} RES_RECORD;

typedef struct {
	R_DATA *resource;
	unsigned char *signers_name;
	unsigned char *signature;
} RES_RECORD_RRSIG;

//Structure of a Query
typedef struct {
	unsigned char *name;
	QUESTION *ques;
} QUERY;

typedef struct {
	char **ip;
	int32_t count;
} DNS_SERVERS;

typedef struct {
	unsigned char* buf;
	int32_t size;
	uint64_t time;
	uint64_t hora_recv;
} QUERY_RESPONSE;

typedef struct {
	char * bindproperty;
} TXT_SHORT_QUERY_RESPONSE;

typedef struct {
	unsigned char * host_ip;
	uint32_t ttl;
} A_AAAA_SHORT_QUERY_RESPONSE;

typedef struct {
	unsigned char* assinante;
	unsigned char* assinatura;
	R_DATA_RRSIG rrsig;
} RRSIG_SHORT_QUERY_RESPONSE;

typedef struct {
	uint8_t tipo_reg;
	union {
		TXT_SHORT_QUERY_RESPONSE txt;
		A_AAAA_SHORT_QUERY_RESPONSE a;
		RRSIG_SHORT_QUERY_RESPONSE rrsig;
	};
} RESP;

typedef struct {
	unsigned char rcode;
	uint64_t tempo_resposta;
	uint64_t hora_recv;
	uint8_t num_resposta;
	uint8_t authoritative_flag;
	char *hostname;
	RESP *resp;
} SHORT_QUERY_RESPONSE;

typedef struct {
	int num_sites;
	char **sites;
	int id_array_day;
} malicious_dns_array;

typedef struct {
	int num_ips;
	char **ips;
} ip_list;

SHORT_QUERY_RESPONSE *consulta_dns(char*, int32_t, int32_t, char*, int, int64_t);
void troca_formato_nome_dns (unsigned char*, char*);
unsigned char* readname (unsigned char*, unsigned char*, int32_t*);
DNS_SERVERS *get_dns_servers(int family);
void libera_dns_servers (DNS_SERVERS*);
int32_t insere_dns_server (DNS_SERVERS*, char*);
int main_dns (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options);
void *args_dns (int *argc, const char *argv[]);
char *obtem_hostname (char *host);
