/*
*   Copyright 2010 Gabriel Serme
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
*    Unless required by applicable law or agreed to in writing, software
*    distributed under the License is distributed on an "AS IS" BASIS,
*    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*    See the License for the specific language governing permissions and
*    limitations under the License.
*/
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
// #include <netinet/tcp.h>
#include <linux/udp.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netdb.h>
#include "utils.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "unp.h"
#include "debug.h"
#include "simet_bcp38.h"
#include "simet_config.h"
#define NUM_TESTES 5
#define LEN 512

typedef unsigned short u16;
typedef unsigned long u32;

//2nd
unsigned short csum (unsigned short *buf, int nwords);
uint16_t udp_checksum(const struct iphdr  *ip,
					const struct udphdr *udp,
					const uint16_t *buf);



int main_bcp38 (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options) {
	proto_disponiveis disp;
	int family = 4;
	char doc[] =
		"uso:\n\tsimet_bcp38\n\n\tTesta boas praticas de antispoofing\n";

	if (options)
		parse_args_basico (options, doc, &family);

	if(geteuid() != 0) {
	// Tell user to run app as root, then exit.

		printf ("user must be root to spoof ip. try again with superuser privileges\n");
		saida (1);
	}


//	if(argc != 6) {
//		printf("- Usage %s <IP source> <port source> <IP dest> <port dest> <message>\n", argv[0]);
//		saida(1);
//	}
//	else {
//		printf ("Args : \n"
//		"\tip source : %s:%s\n"
//		"\tip dest : %s:%s\n",
//		argv[1], argv[2], argv[3], argv[4]);
//	}

// #!/bin/sh
//
// mac_address=`get_mac_address.sh`
//
// simet_bcp38 `wget -q http://op.ceptro.br/cgi-bin/myip/myip2 -O -` 3000 200.160.4.197 3000 "`wget -q http://op.ceptro.br/cgi-bin/myip/myip2 -O -`#`wget -q http://op.ceptro.br/cgi-bin/myip/myip2 -O -`#$mac_address#MESMOIP"
//
// simet_bcp38 `wget -q http://op.ceptro.br/cgi-bin/myip/myip3 -O -` 3000 200.160.4.197 3000 "`wget -q http://op.ceptro.br/cgi-bin/myip/myip2 -O -`#`wget -q http://op.ceptro.br/cgi-bin/myip/myip3 -O -`#$mac_address#MESMAREDE"
//
// simet_bcp38 172.26.6.6 3000 200.160.4.197 3000 "`wget -q http://op.ceptro.br/cgi-bin/myip/myip2 -O -`#172.26.6.6#$mac_address#PRIVADO"
//
// simet_bcp38 200.160.6.66 3000 200.160.4.197 3000 "`wget -q http://op.ceptro.br/cgi-bin/myip/myip2 -O -`#200.160.6.66#$mac_address#OUTRAREDE"

	char source [40];
	char dest [40];
	char conteudo [512];
	char ip_publico[INET6_ADDRSTRLEN + 1], ip_iface[INET6_ADDRSTRLEN + 1], *ip_rede, *aux;
	int s, i;
	struct sockaddr_storage daddr, saddr;
	struct sockaddr_in *pdaddr, *psaddr;
	char packet[LEN];
	/* point the iphdr to the beginning of the packet */
	struct iphdr *ip = (struct iphdr *)packet;
	struct udphdr *udp = (struct udphdr *)((void *) ip + sizeof(struct iphdr));
	//struct dnshdr *dns = (struct dnshdr *)((void *) udp + sizeof(struct udphdr));
	protocolos_disponiveis (&disp, family, 1, config, ssl_ctx);

	int family_type = convert_family_type(family);

	if ((s = socket(family_type, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("error 1:");
		saida(EXIT_FAILURE);
	}

	if (!disp.proto[PROTO_IPV4]) {
		INFO_PRINT ("sem teste");
		saida (EXIT_FAILURE);
	}


	inet_ntop(AF_INET, &disp.addr_v4_pub, ip_publico, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &disp.addr_v4, ip_iface, INET_ADDRSTRLEN);


	ip_rede = strdup(ip_publico);
	INFO_PRINT ("iface %s, pub %s, rede %s\n", ip_iface, ip_publico, ip_rede);
	aux = strrchr (ip_rede, '.') + 1;
	if (strncmp(aux, "1", 3))
		*aux = '1';
	else *aux = '2';

	*(aux+1) = '\0';
	INFO_PRINT ("ip_rede [%s]\n", ip_rede);
	strcpy (dest, config->bcp38_server);

	for (i = 1 - disp.rede_local[PROTO_IPV4]; i < NUM_TESTES; i++) { // não realiza o passo 0 se não estiver em rede local
		switch (i) {
			case 0:
				strcpy (source, ip_iface);
				sprintf (conteudo, "%s#%s#%s#MESMOIP", ip_iface, source, mac_address);
				break;
			case 1:
				strcpy (source, ip_publico);
				sprintf (conteudo, "%s#%s#%s#MESMOIP", ip_publico, source, mac_address);
				break;
			case 2:
				strcpy (source, ip_rede);
				sprintf (conteudo, "%s#%s#%s#MESMAREDE", ip_publico, source, mac_address);
				break;
			case 3:
				strcpy (source, config->bcp38_local_ip);
				sprintf (conteudo, "%s#%s#%s#PRIVADO", ip_publico, source, mac_address);
				break;
			case 4:
				strcpy (source, config->bcp38_remote_network_ip);
				sprintf (conteudo, "%s#%s#%s#OUTRAREDE", ip_publico, source, mac_address);
				break;
		}

		fill_socket_addr(&saddr, source, config->bcp38_src_port, family_type);
		fill_socket_addr(&daddr, dest, config->bcp38_dest_port, family_type);
		psaddr = (struct sockaddr_in*) &saddr;
		pdaddr = (struct sockaddr_in*) &daddr;
		memset(udp, 0, sizeof(struct udphdr));
		memset(ip, 0, sizeof(struct iphdr));

		ip->ihl = 5; //header length
		ip->version = 4;
		ip->tos = 0x0;
		ip->id = 0;
		ip->frag_off = htons(0x4000);		/* DF */
		ip->ttl = 64;			/* default value */
		ip->protocol = 17;	//IPPROTO_RAW;	/* protocol at L4 */
		ip->check = 0;			/* not needed in iphdr */
		ip->saddr = psaddr->sin_addr.s_addr;
		ip->daddr = pdaddr->sin_addr.s_addr;

		udp->source = htons(config->bcp38_src_port);
		udp->dest = htons (config->bcp38_dest_port);

		// int sizedata = 100;
		// memset(((void *) udp) + sizeof(struct udphdr), 'A', sizedata);
		// char conteudo[] = "TESTE";
		int sizedata = strlen(conteudo);
		memcpy(((void *) udp) + sizeof(struct udphdr), conteudo, sizedata);

		int sizeudpdata = sizeof(struct udphdr) + sizedata;
		ip->tot_len = htons(sizeudpdata + sizeof(struct iphdr));	/* 16 byte value */
		udp->len = htons(sizeudpdata);

		udp->check = udp_checksum(ip, udp, (uint16_t*) udp);
		INFO_PRINT ("Checksum : 0x%x\n", udp->check);
		INFO_PRINT ("Sizes :  \n\t[+] iphdr %zu\n\t[+] udphdr %zu\n",
		sizeof(struct iphdr), sizeof(struct udphdr));
		INFO_PRINT ("Total size : %d\n", sizeudpdata);

		int optval = 1;
		//HDRINCL parece ser flag padrao para IPPROTO no linux, nao funciona para IPv6
		if (setsockopt(s, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(int)) < 0)
			perror ("IP HDRINCL");


		int sizepacket = sizeof(struct iphdr) + sizeudpdata;
		INFO_PRINT ("packet: ");
		hexdump ((unsigned char*)packet, sizepacket);


		if (sendto(s, (char *)packet, sizepacket, 0,
			(struct sockaddr *)&daddr, (socklen_t)sizeof(struct sockaddr_in)) < 0)
			INFO_PRINT("Error sending packet: %s\n", strerror(errno));
		else
			INFO_PRINT("Sent packet\n");
	}
	free (ip_rede);


	saida(EXIT_SUCCESS);
	return 0;
}

//http://www.linuxquestions.org/questions/linux-networking-3/udp-checksum-algorithm-845618/
//modified by Gabriel Serme
struct pseudo_hdr {
	u_int32_t source;
	u_int32_t dest;
	u_int8_t zero; //reserved, check http://www.rhyshaden.com/udp.htm
	u_int8_t protocol;
	u_int16_t udp_length;
};

uint16_t udp_checksum(const struct iphdr  *ip,
					const struct udphdr *udp,
					const uint16_t *buf)
{
	//take in account padding if necessary
	int calculated_length = ntohs(udp->len)%2 == 0 ? ntohs(udp->len) : ntohs(udp->len) + 1;

	struct pseudo_hdr ps_hdr = {0};
	bzero (&ps_hdr, sizeof(struct pseudo_hdr));
	uint8_t data[sizeof(struct pseudo_hdr) + calculated_length];
	bzero (data, sizeof(struct pseudo_hdr) + calculated_length );

	ps_hdr.source = ip->saddr;
	ps_hdr.dest = ip->daddr;
	ps_hdr.protocol = IPPROTO_UDP; //17
	ps_hdr.udp_length = udp->len;

	memcpy(data, &ps_hdr, sizeof(struct pseudo_hdr));
	memcpy(data + sizeof(struct pseudo_hdr), buf, ntohs(udp->len) ); //the remaining bytes are set to 0

	return csum((uint16_t *)data, sizeof(data)/2);
}


unsigned short csum (unsigned short *buf, int nwords)
{
	unsigned long sum;

	for (sum = 0; nwords > 0; nwords--)
		sum += *buf++;

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

