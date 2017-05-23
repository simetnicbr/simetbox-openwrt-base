#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include "debug.h"
#include "local_simet_config.h"
#include "byteconversion.h"
#include "unp.h"
#include "netsimet.h"
#include "ntp.h"





#define SA      struct sockaddr
#define MAXLINE 16384
#define READMAX 16384		//must be less than MAXLINE or equal
#define NUM_BLK 20
#define MAXSUB  512
#define URL_LEN 256
#define MAXHSTNAM 512
#define MAXPAGE 1024
#define MAXPOST 1638

#define LISTENQ         1024





/*
 * NTP uses two fixed point formats.  The first (l_fp) is the "long"
 * format and is 64 bits long with the decimal between bits 31 and 32.
 * This is used for time stamps in the NTP packet header (in network
 * byte order) and for internal computations of offsets (in local host
 * byte order). We use the same structure for both signed and unsigned
 * values, which is a big hack but saves rewriting all the operators
 * twice. Just to confuse this, we also sometimes just carry the
 * fractional part in calculations, in both signed and unsigned forms.
 * Anyway, an l_fp looks like:
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                         Integral Part                         |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                         Fractional Part                       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * REF http://www.eecis.udel.edu/~mills/database/rfc/rfc2030.txt
 */





void prepara_ntp_v3(char *msg, int *len) {
	*len = 48;
	bzero(msg, *len);
	msg[0] = 0x1b;
}

void prepara_ntp_v4 (struct ntp_pkt_v4 *msg, int *len, uint64_t *hora_local) {
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	*hora_local = TIMESPEC_NANOSECONDS(ts)/1000;
	bzero (msg, sizeof (*msg));
	msg->li_vn_mode = 0x23;
	msg->xmt.l_ui = htobe32 (UNIX_TO_NTP(*hora_local / 1000000));
	msg->xmt.l_uf = htobe32 (*hora_local % 1000000);
	*len = 4 * sizeof (uint8_t) + 3 * sizeof (uint32_t) + 8 * sizeof (uint32_t);
}

uint64_t obtem_hora_ntp (int sockfd) {
	int n;
	int len;
	uint64_t seconds = 0, microsecs, hora_local;
    fd_set rset;
	struct timeval tv_timeo;
	struct ntp_pkt_v4 msg;

	prepara_ntp_v4 (&msg, &len, &hora_local);

	FD_ZERO (&rset);
	FD_SET (sockfd, &rset);
	tv_timeo.tv_sec = SHORT_TIMEOUT;
	tv_timeo.tv_usec = 0; // select zera o timeout
	n = 0;
	send(sockfd, (char *) &msg, len, 0);
	if (select (sockfd + 1, &rset, NULL, NULL, &tv_timeo) > 0)
		if (FD_ISSET(sockfd, &rset))
			n = recvfrom(sockfd, &msg, sizeof(msg), 0, NULL, NULL);
	if (n > 0) {
		seconds = NTP_TO_UNIX(be32toh(msg.rec.l_ui));
		microsecs = ((uint64_t)be32toh(msg.rec.l_uf) * 1000000) >> 32;
		seconds = seconds * 1e6L + microsecs;
	}

	return seconds;

}





uint64_t get_ntp_usec(void) {
	int sockfd, flags, ind_ntp;
	uint64_t hora=0;
	char ntpservers[3][9] = {"a.ntp.br", "b.ntp.br", "c.ntp.br"};



	for (ind_ntp = 0; ((ind_ntp < 3) && (!hora)); ind_ntp++) {
		INFO_PRINT ("consultando %s\n", ntpservers[ind_ntp]);
		if (connect_udp(ntpservers[ind_ntp], "123", 0, &sockfd) < 0)
			continue;

		flags = fcntl (sockfd, F_GETFL, 0);
		fcntl (sockfd, F_SETFL,flags | O_NONBLOCK);
		hora = obtem_hora_ntp (sockfd);
		close(sockfd);
	}


	return hora;
}


uint64_t get_ntp(void) {
	return get_ntp_usec() / 1e6L;
}
