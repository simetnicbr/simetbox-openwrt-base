typedef struct {
	union {
		uint32_t Xl_ui;
		int32_t Xl_i;
	} Ul_i;
	union {
		uint32_t Xl_uf;
		int32_t Xl_f;
	} Ul_f;
} l_fp;


#define l_ui    Ul_i.Xl_ui              /* unsigned integral part */
#define l_i     Ul_i.Xl_i               /* signed integral part */
#define l_uf    Ul_f.Xl_uf              /* unsigned fractional part */
#define l_f     Ul_f.Xl_f               /* signed fractional part */





struct ntp_pkt_v4 {
	uint8_t  li_vn_mode;     /* leap indicator, version and mode */
	uint8_t  stratum;        /* peer stratum */
	uint8_t  ppoll;          /* peer poll interval */
	int8_t  precision;      /* peer clock precision */
	uint32_t    rootdelay;      /* distance to primary clock */
	uint32_t    rootdispersion; /* clock dispersion */
	uint32_t refid;          /* reference clock ID */
	l_fp    ref;        /* time peer clock was last updated */
	l_fp    org;            /* originate time stamp */
	l_fp    rec;            /* receive time stamp */
	l_fp    xmt;            /* transmit time stamp */
};



/*
 * Time of day conversion constant.  Ntp's time scale starts in 1900,
 * Unix in 1970.
 */
#define JAN_1970        0x83aa7e80      /* 2208988800 1970 - 1900 in seconds */
#define NTP_TO_UNIX(n) (n - JAN_1970)
#define UNIX_TO_NTP(n) (n + JAN_1970)

uint64_t get_ntp(void);
uint64_t get_ntp_usec(void);
void prepara_ntp_v3(char *msg, int *len);
void prepara_ntp_v4 (struct ntp_pkt_v4 *msg, int *len, uint64_t *hora_local);
