#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <linux/sockios.h>
//#include <linux/if.h>
#include <linux/ethtool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <resolv.h>
#include <netinet/tcp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "byteconversion.h"
#include "debug.h"
#include "simet_tools.h"
#include "local_simet_config.h"
#include "utils.h"
#include "simet_uptime.h"
#include "ntp.h"
#include "unp.h"
#include "netsimet.h"

#define TAM 0
#define MSG 1
#define TAM_TAM 2

#define TAM_MAX_MSG UINT16_MAX
#define TAM_MAX_BUF 2048
#define HEARTBEAT 0
#define TIPO_ID 1
#define TIPO_BUFF_RECV 2
#define TIPO_BUFF_APLIC 3

#define VERSAO_PROTOCOLO 3
//#define TESTA_LIXO

/*
void olha_mem(int line) {
	char *ret_func;
	char formata_parametro[100];

	sprintf (formata_parametro, "cat /proc/%d/status", getpid());

	INFO_PRINT ("linha %d\n", line);
	ret_func = chama_pipe (formata_parametro, "falha ao ler status\n");
	if (ret_func) {
		free (ret_func);
	}
}
*/


int pingando_set (int status, control_center *control) {
	int rc;
	char *formata_parametro, *ret_func;

	if (control->pingando != status) {
		control->pingando = status;
		INFO_PRINT ("pingando %d\n", control->pingando);
		rc = asprintf (&formata_parametro, "set_uci.sh system status.online %d", status);
		if (rc < 0) {
			ERROR_PRINT ("nao conseguiu alocar parametro set_uci.sh");
			control->finaliza = 1;
			return 1;
		}

		ret_func = chama_pipe(formata_parametro, "erro ao setar system.status.online no uci");
		free (formata_parametro);
		if (ret_func)
			free (ret_func);
	}
	return 0;
}

void meu_sleep (uint64_t min, uint64_t aleatorio) {
	uint64_t parte_aleatoria;
	parte_aleatoria = (uint64_t) get_rand_i () % (aleatorio + 1);
//	INFO_PRINT ("sleep por %lus", min + parte_aleatoria);
	sleep (min + parte_aleatoria);
}


uint64_t uptime_rel(void)
{
	int n;
	FILE *in=fopen("/proc/uptime", "r");
	uint64_t retval=0;
	if(in) {
		n = fscanf(in, "%"PRUI64, &retval);
		if (n == 1) {
		} else if (errno != 0) {
			perror("fscanf");
		} else {
			ERROR_PRINT ("uptime_rel\n");
		}
		fclose(in);
		in = NULL;
	}
	return retval;
}
//void desconecta_socket (int socket) {
void desconecta_socket (SSL *ssl, int socket, control_center *control) {
	SSL_shutdown (ssl);
	SSL_free (ssl);
	ssl = NULL;
	close (socket);
	limpa_sized_buffer (&control->sbuf_envio);
	limpa_sized_buffer (&control->sbuf_recebimento);
	limpa_sized_buffer (&control->sbuf_temp);
	limpa_sized_buffer (&control->checksum_recebido);
	if (control->nome_arq_uptime) {
		free (control->nome_arq_uptime);
		control->nome_arq_uptime = NULL;
	}
	control->id_transacao = 0;
	control->desconecte = control->connected = 0;
	meu_sleep (15, 30);
}

void checa_cabo_e_link (control_center *control) {
	struct ifreq ifr_cabo, ifr_iface;
	struct ethtool_value edata_cabo;

	if (!control->std_gw)
		return;
	memset(&edata_cabo, 0, sizeof(edata_cabo));
	memset(&ifr_iface, 0, sizeof(ifr_iface));
	strncpy(ifr_iface.ifr_name, control->std_gw, sizeof(ifr_iface.ifr_name));


	if (ioctl(control->sock_link_cabo, SIOCGIFFLAGS, &ifr_iface) < 0) {
		ERRNO_PRINT ("falha ao obter flags da iface \"%s\", socket %d", control->std_gw, control->sock_link_cabo);
		return;
	}

	if ((ifr_iface.ifr_flags & IFF_UP) == IFF_UP) {
		if (control->status_iface != 1) {
			control->status_iface = 1;
			INFO_PRINT ("status_iface = 1\n");
		}
	}
	else if (control->status_iface!= 0) {
		control->status_iface = 0;
		INFO_PRINT ("status_iface = 0\n");
	}
	if (control->status_iface) {
		memset(&ifr_cabo, 0, sizeof(ifr_iface));
		strncpy(ifr_cabo.ifr_name, control->std_gw, sizeof(ifr_cabo.ifr_name));
		ifr_cabo.ifr_data = (char*)&edata_cabo;

		edata_cabo.cmd = ETHTOOL_GLINK;
		if (ioctl(control->sock_link_cabo, SIOCETHTOOL, &ifr_cabo) < 0) {
			ERRNO_PRINT ("SIOCETHTOOL, socket: %d\n", control->sock_link_cabo);
			if (errno == EOPNOTSUPP)
				edata_cabo.data = 1;
		}
		if (control->status_cabo != edata_cabo.data) {
			control->status_cabo = edata_cabo.data;
			INFO_PRINT ("status_cabo = %d\n", control->status_cabo);
		}

	}
	else if (control->status_cabo != 0) {
		control->status_cabo = 0;
		INFO_PRINT ("status_cabo = 0\n");
	}
}


void *thread_trata_sinais_uptime (void *arg) {
    int err, signo;
	signal_thread_arg *signal_arg;
	signal_arg = (signal_thread_arg*) arg;



	INFO_PRINT ("Thread Sinais\n");

    for (;;) {
        err = sigwait(signal_arg->mask, &signo);
        if (err != 0) {
            INFO_PRINT ("falha sigwait\n");
            saida (1);
        }
        switch (signo) {
		case SIGALRM:
//			INFO_PRINT ("[alarm], connected %d, time_now %ld\n", connected, time_now);
			signal_arg->control->time_now = uptime_rel();
			checa_cabo_e_link (signal_arg->control);
			if (signal_arg->control->connected)  {
				if (signal_arg->control->time_now - signal_arg->control->last_net_activity_send >= signal_arg->config->uptime_heartbeat_interval) {
					signal_arg->control->envia_heartbeat = 1;
				}
				if (signal_arg->control->time_now - signal_arg->control->last_net_activity_recv >= signal_arg->config->uptime_timeout_heartbeat) {
	//				desconecta_socket (sock_detect);
					signal_arg->control->desconecte = 1;
					ERROR_PRINT ("desconectando por inatividade, time_now %"PRUI64", last_net_activity_recv %"PRUI64"\n", signal_arg->control->time_now, signal_arg->control->last_net_activity_recv);
				}
			}
/*			if (tem_hora_ntp) {
				if (((uint64_t)hora_ntp) + time_now - time_up >= atualiza_arq_off + PERIODO_ATUALIZA_ARQ_OFF) {
					// atualiza timestamp do arquivo de shutdown no mesmo segundo que muda o minuto
					executa_touch = 1;
					atualiza_arq_off += PERIODO_ATUALIZA_ARQ_OFF;
				}
			}
*/
		break;

		default:
            INFO_PRINT("sinal inesperado %d\n", signo);
        }
    }
    pthread_exit (NULL);
}

/*---------------------------------------------------------------------*/
/*--- sig_handler - catch and send heartbeat.                       ---*/
/*---------------------------------------------------------------------*/
/*void sig_handler(int signum)
{
	switch (signum) {
		case SIGALRM:
			time_now = uptime_rel();
			if ((connected) && ((time_now % uptime_heartbeat_interval) == 0)) {
				rc = send(sock_detect, buf, TAM_TAM, 0);
				printf ("[alarm], connected %d, time_now %ld\n", connected, time_now);
				printf ("rc send heartbeat = %d\n", rc);
			}
			if ((connected) && (time_now - last_net_activity_recv >= uptime_timeout_heartbeat)) {
				desconecta_socket (sock_detect);
				printf ("desconectando por inatividade\n");
			}
			if (tem_hora_ntp) {
				if (((uint64_t)hora_ntp) + time_now - time_up >= atualiza_arq_off + PERIODO_ATUALIZA_ARQ_OFF) {
					// atualiza timestamp do arquivo de shutdown no mesmo segundo que muda o minuto
					executa_touch = 1;
					atualiza_arq_off += PERIODO_ATUALIZA_ARQ_OFF;
				}
			}
			break;
		default:
			INFO_PRINT ("sinal nao tratado: [%d]\n", signum);
	}
}
*/


/*void do_sleep(struct sigaction *sa) {
	sigset_t mask;
	sigfillset(&(sa->sa_mask));
//	sigaction(SIGALRM, &sa, NULL);
	// Get the current signal mask
	sigprocmask(0, NULL, &mask);

	// Unblock SIGALRM
	sigdelset(&mask, SIGALRM);

	// Wait with this mask
//	alarm(seconds);
	sigsuspend(&mask);

}




int le_arquivo_e_envia_resultados (char *nomearq, char *host, char *port, char *chamada) {
	FILE *fp;
	int rc, i = 0;
	int64_t tam_resto;
	fpos_t position;
	char mac[13] = {0};
	UPTIME uptime;
//	char nomearq[]="/tmp/bla";
	char *resto_arquivo = NULL;
	char *ws = NULL;


	memset (mac, 0, sizeof (mac));

	if((fp = fopen(nomearq, "rb+")) == NULL) {
		printf("não pode abrir arquivo %s.\n", nomearq);
		return 0;
	}

	fgetpos(fp, &position); // marca posicao do inicio do arquivo
	printf ("li do arq uivo %s:\n", nomearq);
	while ((rc = fread (&uptime, sizeof(UPTIME), 1, fp)) == 1) {
		if (feof (fp)) {
			perror( "feof");
			break;
		}
		if (ferror (fp)) {
			perror( "Read error");
			break;
		}
		memcpy (mac, uptime.mac_address, sizeof (uptime.mac_address));
		printf ("%d:\tmac: %s, action %d, t_up %"PRUI64", t_iface_up %"PRUI64", t_cabo_up %"PRUI64", t_iface_down %"PRUI64", t_cabo_down %"PRUI64"\n",
				++i, mac, uptime.action, uptime.t_up, uptime.t_iface_up, uptime.t_cabo_up, uptime.t_iface_down, uptime.t_cabo_down);
		sprintf(chamada, "/DataServices/uptime?mac_address=%s&action=%d&up=%"PRUI64"&iface_up=%"PRUI64"&t_cabo_up=%"PRUI64"&t_iface_down=%"PRUI64"&t_cabo_down=%"PRUI64,
			mac, uptime.action, uptime.t_up, uptime.t_iface_up, uptime.t_cabo_up, uptime.t_iface_down, uptime.t_cabo_down);
		//ws=chama_web_service_ssl(host, port, chamada, ssl_ctx);
		printf ("%s\n", chamada);
		if (ws) {
			if (strcmp(ws, "true") == 0) {
				// se enviou o webservice
				fgetpos(fp, &position);
				printf ("enviou com sucesso\n");
			}
			else {
				fsetpos(fp, &position);
// TODO retirar comentário quando mandar ws
//				free (ws);
				break;
			}
// TODO retirar comentário quando mandar ws
//			free (ws);
		}
	}
	if (!feof (fp)) {
		fseek(fp, 0, SEEK_END);
		tam_resto = ftell(fp) - (i * sizeof(UPTIME));
		fsetpos(fp, &position);
		resto_arquivo = (char *) malloc (tam_resto);
		if (!resto_arquivo)
			printf ("nao consegui alocar memoria para resto do arquivo\n");
		if ((rc = fread (resto_arquivo, 1, tam_resto, fp)) != tam_resto)
			printf ("nao consegui ler resto do arquivo\n");
		fclose (fp);
		if((fp = fopen(nomearq, "wb")) == NULL) {
			printf("não pode abrir arquivo %s para escrita.\n", nomearq);
			return 2;
		}
		rc = fwrite(resto_arquivo, 1, tam_resto, fp);
		if (rc != tam_resto)
			printf ("escrevi com problemas");
		fclose (fp);
	}
	else {
		fclose (fp);
		if (remove (nomearq)) {
			printf ("nao consegui deletar %s\n", nomearq);
			return 1;
		}
		else printf ("arquivo %s deletado com sucesso\n", nomearq);

	}
	if (resto_arquivo)
		free (resto_arquivo);

	return 0;
}



int filtro1 (const struct dirent *dir) {
	const char * s = dir->d_name;

	if (strncmp(s,"simet_uptime_sdown",17) == 0)
		return 1;
	else return 0;
}



int filtro2 (const struct dirent *dir) {
	const char * s = dir->d_name;

	if (strncmp(s,"simet_uptime_data",16) == 0)
		return 1;
	else return 0;
}




//TODO chamar checa_arqs_envia
//checa necessidade de enviar dados salvos em arquivos
void checa_arqs_envia (uint64_t t_up, char *mac_address, char *host, char *port) {
// Precisa mandar o tempo que o simetbox foi desligado?
	uint32_t i_arq, n_arq;
	struct dirent **namelist;
	struct stat sb;
	time_t t_file;
	char *ws = NULL;
	char aux [100], chamada [512]= {0};
//char *ws;

	n_arq = scandir("/tmp/", &namelist, filtro1, alphasort);
	if (n_arq < 0)
		perror("scandir");
	else {
		// TODO escolher diretório que não seja dentro do tmp
		for (i_arq = 0; i_arq < n_arq; i_arq++) {
			sprintf (aux, "/tmp/%s", namelist[i_arq]->d_name);
			t_file = atoi (namelist[i_arq]->d_name + 18);

			if (stat(aux, &sb) == -1) {
				perror("stat");
				saida (1);
			}
			if ((uint64_t) t_file != t_up) {
				sprintf(chamada,
					"/DataServices/uptime_sdown?mac_address=%s&up=%"PRUI64"&down=%"PRUI64, mac_address, t_up, (uint64_t) t_file);
				// TODO webservice
				ws = chama_web_service_ssl(host, port, chamada, ssl_ctx);
				printf ("%s\n", chamada);
				if (ws) {
					if (strcmp (ws, "true") == 0) { // enviou webservice com sucesso
						printf("up: %s", ctime (&t_file));
						printf("Last file modification:   %s", ctime(&sb.st_mtime));
						if (remove (aux))
							printf ("nao conseguiu deletar %s\n", aux);
						else printf ("arquivo %s deletado com sucesso\n", aux);
					}
//						TODO libera webservie
//						free (ws);
				}
			}
			free(namelist[i_arq]);
		}

		if (n_arq)
			free(namelist);
	}

	// algum arquivo de dados tem algum conteudo?
		n_arq = scandir("/tmp/", &namelist, filtro2, alphasort);

	if (n_arq < 0)
		perror("scandir");
	else {
		for (i_arq = 0; i_arq < n_arq; i_arq++) {
			t_file = atoi (namelist[i_arq]->d_name + 17);
			sprintf (aux, "/tmp/%s", namelist[i_arq]->d_name);
			printf("conteudo de %s:\n", namelist[i_arq]->d_name);
			le_arquivo_e_envia_resultados (aux, host, port, chamada);
			free(namelist[i_arq]);
		}

		if (n_arq)
			free(namelist);
	}
}

*/
void cria_buffer_confirma_aplica_buffer (simet_confirma_recv_aplic *buf, uint8_t tipo, uint8_t rc, control_center *control) {
	uint8_t versao = VERSAO_PROTOCOLO;

	buf->tam = (htobe16 (sizeof (simet_confirma_recv_aplic) - sizeof (buf->tam)));
	buf->ver_protocol = versao;
	buf->tipo_msg = tipo;
	buf->id_transacao = htobe64 (control->id_transacao);
	buf->rc = rc;
}




int bloco_sbuf_recebimento (sized_buffer *sbuf, int tipo_msg, int tam, control_center *control) {
	uint8_t tipo_checksum, tam_checksum, id_bloco, md5_sucesso;
	uint16_t aux, tam_nome_arq, tam_buf;
	uint64_t aux64;
//	char *tmpfile, *conteudo, *formata_parametro, *ret_func;
	static uint8_t num_blocos;
	int rc;
	sized_buffer *checksum_calculado;
	simet_confirma_recv_aplic envio_confirmacao;



	if ((tipo_msg == 1) || (tipo_msg == 3)) { // primeiros blocos
		limpa_sized_buffer (&control->sbuf_temp);
		if (control->nome_arq_uptime)
			free (control->nome_arq_uptime);
		control->nome_arq_uptime = NULL;
		memcpy (&aux64, sbuf->buffer, sizeof (control->id_transacao));
		control->id_transacao = be64toh (aux64);
		tam -= sizeof (control->id_transacao);
		if (remove_bytes_buffer (sbuf, sizeof (control->id_transacao)) != sizeof (control->id_transacao)) {
			ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (control->id_transacao));
			return 1;
		}
		memcpy (&aux, sbuf->buffer, sizeof (tam_nome_arq));
		tam_nome_arq = be16toh (aux);
		tam -= sizeof (tam_nome_arq);
		if (remove_bytes_buffer (sbuf, sizeof (tam_nome_arq)) != sizeof (tam_nome_arq)) {
			ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (tam_nome_arq));
			return 2;
		}
		if (tam_nome_arq) {
			control->nome_arq_uptime = malloc (tam_nome_arq + 1);
			if (!control->nome_arq_uptime) {
				ERROR_PRINT ("impossivel alocar nome_arq bloco");
				return 3;
			}
			memcpy (control->nome_arq_uptime, sbuf->buffer, tam_nome_arq);
			control->nome_arq_uptime [tam_nome_arq] = '\0';
			tam -= tam_nome_arq;
			if (remove_bytes_buffer (sbuf, tam_nome_arq) != tam_nome_arq) {
				ERROR_PRINT ("impossivel remover %d bytes do buffer de entrada", tam_nome_arq);
				return 4;
			}
		}
/*		else { //nao tem nome
			tam_nome_arq = strlen (TMPDIRUPTEMPLATE);
			tmpfile = malloc (tam_nome_arq + 1);
			memcpy (tmpfile, TMPDIRUPTEMPLATE, tam_nome_arq);
			tmpfile [tam_nome_arq] = '\0';
			nome_arq_uptime = mktemp (tmpfile);

			if (!nome_arq_uptime) {
				ERROR_PRINT ("Falha ao criar arquivo temporario\n");
				return 5;
			}
		}
*/
		memcpy (&tipo_checksum, sbuf->buffer, sizeof (tipo_checksum));
		tam -= sizeof (tipo_checksum);
		if (remove_bytes_buffer (sbuf, sizeof (tipo_checksum)) != sizeof (tipo_checksum)) {
			ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (tipo_checksum));
			return 6;
		}
		memcpy (&tam_checksum, sbuf->buffer, sizeof (tam_checksum));
		tam -= sizeof (tam_checksum);
		if (remove_bytes_buffer (sbuf, sizeof (tam_checksum)) != sizeof (tam_checksum)) {
			ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (tam_checksum));
			return 7;
		}
/*		checksum = malloc (tam_checksum + 1);
		if (!checksum) {
			ERROR_PRINT ("impossivel alocar checksum");
			return 8;
		}
		memcpy (checksum, sbuf->buffer, tam_checksum);
		checksum [tam_checksum] = '\0';
		if (remove_bytes_buffer (sbuf, tam_checksum) != tam_checksum) {
			ERROR_PRINT ("impossivel remover %d bytes do buffer de entrada", tam_checksum);
			return 9;
		}
*/
		armazena_dados_s_buffer (&control->checksum_recebido, sbuf->buffer, tam_checksum);
		tam -= tam_checksum;
		if (remove_bytes_buffer (sbuf, tam_checksum) != tam_checksum) {
			ERROR_PRINT ("impossivel remover %d bytes do buffer de entrada", tam_checksum);
			return 9;
		}

		// TODO salvar arquivo_checksum
/*		tam_nome_arq_checksum = strlen (TMPDIRUPTEMPLATE);
		tmpfile = malloc (tam_nome_arq_checksum + 1);
		memcpy (tmpfile, TMPDIRUPTEMPLATE, tam_nome_arq_checksum);
		tmpfile [tam_nome_arq_checksum] = '\0';
		nome_arq_checksum = mktemp (tmpfile);

		if (!nome_arq_checksum) {
			ERROR_PRINT ("Falha ao criar arquivo temporario checksum\n");
			return 10;
		}
		rc = asprintf (&conteudo_arq_md5, "%s  %s", checksum, nome_arq_uptime);
		if (rc < 0) {
			ERROR_PRINT ("nao conseguiu alocar conteudo arquivo md5");
			return 11;
		}
		free (checksum);
		rc = escreve_arquivo (nome_arq_checksum, conteudo_arq_md5, rc, "w");
		free (conteudo_arq_md5);
		if (rc) {
			ERROR_PRINT ("nao conseguiu salvar conteudo arquivo md5");
			return 12;
		}
*/

		memcpy (&num_blocos, sbuf->buffer, sizeof (num_blocos));
		tam -= sizeof (num_blocos);
		if (remove_bytes_buffer (sbuf, sizeof (num_blocos)) != sizeof (num_blocos)) {
			ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (num_blocos));
			return 13;
		}
	}

	memcpy (&id_bloco, sbuf->buffer, sizeof (id_bloco));
	tam -= sizeof (id_bloco);
	if (remove_bytes_buffer (sbuf, sizeof (id_bloco)) != sizeof (id_bloco)) {
		ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (id_bloco));
		return 14;
	}
	memcpy (&aux, sbuf->buffer, sizeof (tam_buf));
	tam_buf = be16toh (aux);
	tam -= sizeof (tam_buf);
	if (remove_bytes_buffer (sbuf, sizeof (tam_buf)) != sizeof (tam_buf)) {
		ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (tam_buf));
		return 15;
	}
	/*
	conteudo = malloc (tam_arq + 1);
	if (!conteudo) {
		ERROR_PRINT ("impossivel alocar nome_arq bloco");
		return 16;
	}
	memcpy (conteudo, sbuf->buffer, tam_arq);
	conteudo [tam_arq] = '\0';
	if (remove_bytes_buffer (sbuf, tam_arq) != tam_arq) {
		ERROR_PRINT ("impossivel remover %d bytes do buffer de entrada", tam_arq);
		return 17;
	}
	// TODO salva arquivo em disco
	if (nome_arq_uptime)  {
		rc = escreve_arquivo (nome_arq_uptime, conteudo, tam_arq, "a");
		free (conteudo);
		if (rc) {
			ERROR_PRINT ("nao conseguiu salvar conteudo em arquivo");
			return 18;
		}
	}
	else {

// 	}
*/
	rc = armazena_dados_s_buffer (&control->sbuf_temp, sbuf->buffer, tam_buf);
	if (rc != tam_buf) {
		ERROR_PRINT ("nao alocou");
		return 18;
	}
	tam -= tam_buf;
	if (remove_bytes_buffer (sbuf, tam_buf) != tam_buf) {
		ERROR_PRINT ("impossivel remover %d bytes do buffer de entrada", tam_buf);
		return 19;
	}
	if (id_bloco == num_blocos) {
		// TODO checa md5 se for o último bloco
/*		if (asprintf(&formata_parametro, "/usr/bin/md5sum -c %s", nome_arq_checksum) < 0) {
			ERROR_PRINT ("nao alocou parametro");
			return 19;
		}
		ret_func = chama_pipe (formata_parametro, "falha ao ler checar checksum md5sum\n");
		free (formata_parametro);
		if (ret_func) {
			if (asprintf(&formata_parametro, "%s: OK", nome_arq_uptime) < 0) {
				ERROR_PRINT ("nao alocou parametro");
				return 20;
			}
			if (!strstr (ret_func, formata_parametro)) {
				ERROR_PRINT ("nao bate md5");
				return 21;
			}
			free (ret_func);
		}
*/

		checksum_calculado = gera_md5 (&control->sbuf_temp);
		if (!strncmp (control->checksum_recebido.buffer, checksum_calculado->buffer, control->checksum_recebido.size)) {
			INFO_PRINT ("MD5 ok");
			md5_sucesso = 1;
		}
		else {
			INFO_PRINT ("MD5 ERRADO");
			md5_sucesso = 0;
		}
		libera_sized_buffer (checksum_calculado);
		limpa_sized_buffer (&control->checksum_recebido);

		cria_buffer_confirma_aplica_buffer (&envio_confirmacao, TIPO_BUFF_RECV, 1 - md5_sucesso, control);
		rc = armazena_dados_s_buffer (&control->sbuf_envio, &envio_confirmacao, sizeof (envio_confirmacao));
		if (rc != sizeof (envio_confirmacao)) {
			ERROR_PRINT ("nao bufferizou");
			return 20;
		}



		INFO_PRINT ("id_transacao: %"PRUI64"\n", control->id_transacao);
		if (control->nome_arq_uptime)
			INFO_PRINT ("nome_arq_uptime %s\n", control->nome_arq_uptime);
	}
	if (tam)
		return 21;

	return 0;
}


int confirmacao_commit_id_transacao (sized_buffer *sbuf, int tam, control_center *control) {
	uint64_t id_transacao_confimacao, aux64;
	uint8_t rc;
	char *formata_parametro, *ret_func;


	memcpy (&aux64, sbuf->buffer, sizeof (id_transacao_confimacao));
	id_transacao_confimacao = be64toh (aux64);
	tam -= sizeof (id_transacao_confimacao);
	if (remove_bytes_buffer (sbuf, sizeof (id_transacao_confimacao)) != sizeof (id_transacao_confimacao)) {
		ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (id_transacao_confimacao));
		return 1;
	}
	memcpy (&rc, sbuf->buffer, sizeof (rc));
	tam -= sizeof (rc);
	if (remove_bytes_buffer (sbuf, sizeof (rc)) != sizeof (rc)) {
		ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (rc));
		return 2;
	}
	if (tam)
		return 3;
	if (rc == 0) {
		if (control->id_transacao == id_transacao_confimacao) {
			// aplica lua
			if (asprintf(&formata_parametro, "/usr/bin/aplica_transacao.sh json_reload_services %s", control->nome_buffer_aplic) < 0) {
				unlink("/tmp/auto_upgrade.pid"); // libera lock do auto-upgrade
				unlink(control->nome_buffer_aplic);
				free (control->nome_buffer_aplic);
				ERROR_PRINT ("nao alocou parametro");
				control->id_transacao = 0;
				return 8;
			}
			INFO_PRINT ("formata_parametro reload: %s", formata_parametro);
			ret_func = chama_pipe (formata_parametro, "falha ao aplicar transacao\n");
			free (ret_func);
			free (formata_parametro);

			unlink("/tmp/auto_upgrade.pid"); // libera lock do auto-upgrade
			unlink(control->nome_buffer_aplic);
			free (control->nome_buffer_aplic);

			limpa_sized_buffer (&control->sbuf_temp);
		}
		else {
			control->id_transacao = 0;
			ERROR_PRINT ("id_transacao != id_transacao_confimacao");
			return 11;
		}
		control->id_transacao = 0;
	}

	return 0;
}


int confirmacao_recebimento_buffer (sized_buffer *sbuf, int tam, control_center *control) {
	uint64_t id_transacao_confimacao, aux64;
	uint8_t rc;
	simet_confirma_recv_aplic envio_confirmacao;
	int fd, lockfd, tentativas_lock, ret_code, tam_nome_temp;
	char *formata_parametro, *ret_func;


	memcpy (&aux64, sbuf->buffer, sizeof (id_transacao_confimacao));
	id_transacao_confimacao = be64toh (aux64);
	tam -= sizeof (id_transacao_confimacao);
	if (remove_bytes_buffer (sbuf, sizeof (id_transacao_confimacao)) != sizeof (id_transacao_confimacao)) {
		ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (id_transacao_confimacao));
		return 1;
	}
	memcpy (&rc, sbuf->buffer, sizeof (rc));
	tam -= sizeof (rc);
	if (remove_bytes_buffer (sbuf, sizeof (rc)) != sizeof (rc)) {
		ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (rc));
		return 2;
	}
	if (tam)
		return 3;
	if (rc == 0) {
		if (control->nome_arq_uptime)  {
			ret_code = escreve_arquivo_nome (control->nome_arq_uptime, control->sbuf_temp.buffer, control->sbuf_temp.size, "a");
			if (ret_code) {
				ERROR_PRINT ("nao conseguiu salvar conteudo em arquivo");
				return 4;
			}
			free (control->nome_arq_uptime);
			control->nome_arq_uptime = NULL;
		}
		else {
			if (control->id_transacao == id_transacao_confimacao) {
				for (lockfd = -1, tentativas_lock = 0; (lockfd <= 0 && tentativas_lock < 10); tentativas_lock++)
					lockfd = captura_lock (1, "/tmp/auto_upgrade.pid");
				if (lockfd == -1) {
					INFO_PRINT ("auto_upgrade em andamento...");
					return 5;
				}

				tam_nome_temp = strlen (TMPDIRUPTEMPLATE);
				control->nome_buffer_aplic = (char*) malloc (tam_nome_temp + 1);
				if (!control->nome_buffer_aplic) {
					ERROR_PRINT ("nao alocou");
					return 6;
				}
				memcpy (control->nome_buffer_aplic, TMPDIRUPTEMPLATE, tam_nome_temp);
				control->nome_buffer_aplic [tam_nome_temp] = '\0';
				errno = 0;
				fd = mkstemp (control->nome_buffer_aplic);
				if (errno) {
					ERRNO_PRINT ("mkstemp");
				}

				ret_code = escreve_arquivo_fd (fd, control->sbuf_temp.buffer, control->sbuf_temp.size);
				if (ret_code) {
					free (control->nome_buffer_aplic);
					ERROR_PRINT ("nao conseguiu salvar conteudo em arquivo, %s", control->nome_buffer_aplic);
					return 7;
				}

				// aplica lua
				if (asprintf(&formata_parametro, "/usr/bin/aplica_transacao.sh json_apply_configs %s", control->nome_buffer_aplic) < 0) {
					control->id_transacao = 0;
					unlink("/tmp/auto_upgrade.pid"); // libera lock do auto-upgrade
					unlink(control->nome_buffer_aplic);
					free (control->nome_buffer_aplic);
					ERROR_PRINT ("nao alocou parametro");
					return 8;
				}
				INFO_PRINT ("formata_parametro aplica: %s", formata_parametro);
				ret_func = chama_pipe (formata_parametro, "falha ao aplicar transacao\n");
				if (!ret_func) {
					cria_buffer_confirma_aplica_buffer (&envio_confirmacao, TIPO_BUFF_APLIC, 1, control);
					control->id_transacao = 0;
					rc = armazena_dados_s_buffer (&control->sbuf_envio, &envio_confirmacao, sizeof (envio_confirmacao));
					unlink("/tmp/auto_upgrade.pid"); // libera lock do auto-upgrade
					unlink(control->nome_buffer_aplic);
					free (control->nome_buffer_aplic);
				}
				else {
					free (ret_func);
					cria_buffer_confirma_aplica_buffer (&envio_confirmacao, TIPO_BUFF_APLIC, 0, control);
					rc = armazena_dados_s_buffer (&control->sbuf_envio, &envio_confirmacao, sizeof (envio_confirmacao));
				}
				free (formata_parametro);

				if (rc != sizeof (envio_confirmacao)) {
					control->id_transacao = 0;
					ERROR_PRINT ("nao bufferizou");
					unlink("/tmp/auto_upgrade.pid"); // libera lock do auto-upgrade
					unlink(control->nome_buffer_aplic);
					free (control->nome_buffer_aplic);
					return 10;
				}

			}
			else {
				control->id_transacao = 0;
				ERROR_PRINT ("id_transacao != id_transacao_confimacao");
				return 11;
			}
		}
	}

	return 0;
}
int analisa_msg (sized_buffer *sbuf, int versao_sb, control_center *control) {
	uint16_t tam, aux;
	uint8_t versao_protocolo, tipo_msg;
	int rc;

	while (sbuf->size >= TAM_TAM) {
		memcpy ((void*)&aux, sbuf->buffer, TAM_TAM);
		tam = be16toh(aux);
		if (sbuf->size >= tam) {
			if (remove_bytes_buffer (sbuf, TAM_TAM) != TAM_TAM) {
				ERROR_PRINT ("impossivel remover %d bytes do buffer de entrada", TAM_TAM);
				return 1;
			}
			if (tam > TAM_MAX_MSG) {
				ERROR_PRINT ("mensagem com tamanho %d acima do permitido %d\n", tam, TAM_MAX_MSG);
				return 2;
			}
			else if (!tam) {
				INFO_PRINT ("Heartbeat received, %"PRUI64"\n", control->time_now);
			}
			else {
				//hexdump ((unsigned char*)sbuf->buffer, tam);
				memcpy ((void*)&versao_protocolo, sbuf->buffer, sizeof (versao_protocolo));
				if (remove_bytes_buffer (sbuf, sizeof (versao_protocolo)) != sizeof (versao_protocolo)) {
					ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (versao_protocolo));
					return 3;
				}
				tam -= sizeof (versao_protocolo);
				switch (versao_protocolo) {
					case 3:
						memcpy ((void*)&tipo_msg, sbuf->buffer, sizeof (tipo_msg));
						if (remove_bytes_buffer (sbuf, sizeof (tipo_msg)) != sizeof (tipo_msg)) {
							ERROR_PRINT ("impossivel remover %zu bytes do buffer de entrada", sizeof (tipo_msg));
							return 4;
						}
						tam -= sizeof (tipo_msg);
						switch (tipo_msg) {
							case 1: case 2: case 3: case 4:
								rc = bloco_sbuf_recebimento (sbuf, tipo_msg, tam, control);
								if (rc) {
									ERROR_PRINT ("erro no recebimento do buffer %d", rc);
									return 6;
								}
								break;
							case 5:
								rc = confirmacao_recebimento_buffer (sbuf, tam, control);
								if (rc) {
									ERROR_PRINT ("erro no recebimento da confirmacao %d", rc);
									return 6;
								}
								break;
							case 6:
								rc = confirmacao_commit_id_transacao (sbuf, tam, control);
								if (rc) {
									ERROR_PRINT ("erro no recebimento da confirmacao para aplicacao %d", rc);
									return 6;
								}
								break;

							default:
								ERROR_PRINT ("tipo mensagem invalido");
								return 5;
						}

						break;
				}

			}
		}
		else return 0;
	}
	return 0;

}

void *envia_cep_e_hash () {
	char *formata_parametro, *ret_func;
	if (asprintf(&formata_parametro, "/usr/bin/envia_cep_e_hash.sh") < 0) {
		ERROR_PRINT ("nao alocou parametro");
		pthread_exit (NULL);
	}
	ret_func = chama_pipe (formata_parametro, "falha ao enviar cep e hash\n");
	if (!ret_func) {
		free (formata_parametro);
		pthread_exit (NULL);
	}
	free (formata_parametro);
	free (ret_func);
	pthread_exit (NULL);
}


void *pinga_nic_br (void *arg) {
	char *ret_func;
	uint8_t num_tentativa_ping = 0;
	ping_thread_arg *args = (ping_thread_arg*) arg;

	while (!args->control->finaliza) {
		if ((!args->control->status_cabo) || (!args->control->status_iface)) {
			if (args->control->pingando)
				pingando_set (0, args->control);
		}
		else if ((!args->control->connected) || (num_tentativa_ping % 5 == 0)) {
			if (args->control->pingando <= 0) {
				ret_func = chama_pipe("ping -c 1 -W 10 nic.br >/dev/null; echo $?", "erro ao detectar conectividade resultados simet");
				if (!ret_func) {
					args->control->finaliza = 1;
					pthread_exit (NULL);
				}
				else {
					if (strncmp (ret_func, "0", 1) == 0) {
						pingando_set (1, args->control);
					}
					else {
						pingando_set (0, args->control);
					}
					free (ret_func);
				}
			}
		}
		else {
			num_tentativa_ping++;
		}
		meu_sleep (1, 0);
	}
	pthread_exit (NULL);

}



int main_uptime (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET *config, SSL_CTX * ssl_ctx, void *options) {
	int sock_detect, rc, status, num_bytes, versao_sb, ssl_err, read_blocked_on_write = 0, write_blocked_on_read = 0, shutdown_wait = 0, read_blocked = 0;
	fd_set readfds, writefds;
	struct itimerval timer;
	simet_id id;
	pthread_t sig_thread_id, envia_cep_e_hash_id, pingando_id;
	signal_thread_arg signal_arg;
	ping_thread_arg ping_arg;
	sigset_t mask, oldmask;
	char id_buffer [sizeof (id)], buffer [TAM_MAX_BUF], *versao_str;
	struct timeval timeout;
	const char buffer_msg_nula[TAM_TAM] = {};
	SSL *ssl = NULL;
	control_center *control;
	int family = 4;
	proto_disponiveis disp;

#ifdef TESTA_LIXO
// testa o envio de mensagens nao reconhecidas pelo protocolo atual do simet_box
#define TAM_LIXO 50

	char lixo[TAM_LIXO];
	uint16_t i;
	i = htobe16 (TAM_LIXO - TAM_TAM);
	memcpy (lixo, &i, sizeof (i));
	for (i = TAM_TAM; i < TAM_LIXO; i++) {
		lixo[i]=i;
	}
#endif
	//olha_mem (__LINE__);
	char doc[] =
		"uso:\n\tsimet_uptime\n\n\tMede o tempo de uptime de conexao com a internet";

	if (options)
		parse_args_basico (options, doc, &family);
/*	if(geteuid() != 0) {
	// Tell user to run app as root, then exit.

		printf ("user must be root to detect cable removal. try again with superuser privileges\n");
		saida (1);
	}
*/

	control = (control_center*) malloc(sizeof (control_center));
	if (!control) {
		ERROR_PRINT ("nao alocou");
		saida (1);
	}

	memset (control, 0, sizeof (control_center));

	signal_arg.mask = &mask;
	signal_arg.config = config;
	signal_arg.control = control;
	ping_arg.control = control;
//	pthread_mutex_init(&mutex, NULL);
//	signal_arg.mutex = &mutex;
	// TODO remover todas variaveis abaixo desta linha
	memset (&control->sbuf_envio, 0, sizeof (control->sbuf_envio));
	memset (&control->sbuf_recebimento, 0, sizeof (control->sbuf_recebimento));
	memset (&control->sbuf_temp, 0, sizeof (control->sbuf_temp));
	memset (&control->checksum_recebido, 0, sizeof (control->checksum_recebido));

	do {
		meu_sleep (1, 0);
		protocolos_disponiveis (&disp, family, 1, config, ssl_ctx);
		if (control->std_gw)
			free (control->std_gw);
		control->std_gw = strdup (disp.iface_stdgw.if_ipv4);
	} while (strnlen(control->std_gw, 1) == 0);

// checar como buscar info do cabo
	control->pingando = -1;

// 	if ((sock_detect = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
// 		perror("socket");
// 		saida (1);
// 	}
	if ((control->sock_link_cabo = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		perror("socket");
		saida (1);
	}



	// block SIGALRM
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGPIPE);

	if ((status = pthread_sigmask(SIG_BLOCK, &mask, &oldmask)) != 0) {
        ERROR_PRINT ("SIG_BLOCK error\n");
        saida (1);
    }


	status = meu_pthread_create(&sig_thread_id, 0, thread_trata_sinais_uptime, (void*)&signal_arg);
    if (status != 0) {
        ERROR_PRINT("can't create thread\n");
        saida (1);
    }

	status = pthread_detach(sig_thread_id);
	if (status != 0)
	{
		ERROR_PRINT ("cannot detach sig_thread, exiting program\n");
		saida (1);
	}


	status = meu_pthread_create(&pingando_id, 0, pinga_nic_br, (void*)&ping_arg);
	if (status != 0) {
		ERROR_PRINT("can't create thread\n");
		saida (1);
	}
	status = pthread_detach(pingando_id);
	if (status != 0) {
		ERROR_PRINT ("cannot detach pinga_nic_br, exiting program\n");
		saida (1);
	}


	// timer gerador de SIGALRM
	timer.it_value.tv_sec = timer.it_interval.tv_sec = 1;
	timer.it_value.tv_usec = timer.it_interval.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, NULL);




	control->time_now = control->last_net_activity_recv = uptime_rel();
	INFO_PRINT ("up em %"PRUI64"\n", control->time_now);

//	memset (&uptime, 0, sizeof (UPTIME));
//	memcpy (uptime.mac_address, mac_address, sizeof (uptime.mac_address));








//	modelo = obtem_modelo (NULL);
	versao_str = obtem_versao();
	versao_sb = atoi (versao_str);
	free (versao_str);



	while ((!control->status_cabo) || (!control->status_iface))
		meu_sleep (1, 0);

	while (!control->finaliza) {


/*		if (executa_touch) {
			rc = utimes (nome_simet_uptime_arq, NULL);
			if (rc) {
				ERROR_PRINT ("nao conseguiu atualizar o timestamp do arquivo %s. rc %d\n", nome_simet_uptime_arq, rc);
				saida (1);
			}
			else
				INFO_PRINT ("atualizando timestamp do arquivo %s\n", nome_simet_uptime_arq);
			executa_touch = 0;
		}

*/

		if (!control->connected) {
			if ((!control->status_cabo) || (!control->status_iface)) {
				meu_sleep (1, 0);
				continue;
			}


			status = connect_tcp(config->host_uptime, config->port_uptime, 0, &sock_detect);
			if (status < 0) {
				meu_sleep (15, 30);
				continue;
			}
			else {
				ssl = connect_ssl_bio (sock_detect, ssl_ctx);

				if (!ssl) {
					shutdown (sock_detect, SHUT_RDWR);
					close (sock_detect);
					ERR_print_errors_fp (stderr);
					meu_sleep (15, 30);
					continue;
				}
//				check_cert(ssl,host);

				// conectou agora
				control->connected = control->envia_id = 1;
				status = meu_pthread_create(&envia_cep_e_hash_id, 0, envia_cep_e_hash, NULL);
				if (status != 0) {
					ERROR_PRINT("falha ao criar thread\n");
					saida (1);
				}

				status = pthread_detach(envia_cep_e_hash_id);
				if (status != 0)
				{
					ERROR_PRINT ("fala ao detach envia_cep_e_hash_id. saindo\n");
					saida (1);
				}



				limpa_sized_buffer (&control->sbuf_envio);
				limpa_sized_buffer (&control->sbuf_recebimento);
				limpa_sized_buffer (&control->sbuf_temp);
				limpa_sized_buffer (&control->checksum_recebido);


				control->last_net_activity_recv = control->last_net_activity_send = control->time_now;
				INFO_PRINT ("conectou\n");



				if (!control->tem_hora_ntp) {
					if ((control->hora_ntp = get_ntp())) {
/*						clock_time_uptime = ((uint64_t) hora_ntp) - time_up;
						sprintf (nome_simet_uptime_arq, "/tmp/simet_uptime_sdown%"PRUI64, clock_time_uptime);
						atualiza_arq_off = (uint64_t) clock_time_uptime - ((uint64_t) clock_time_uptime % PERIODO_ATUALIZA_ARQ_OFF);
						//sprintf (nome_simet_uptime_data_arq, "/tmp/simet_uptime_data%"PRUI64, uptime.t_up);
						if((fp=fopen(nome_simet_uptime_arq, "wb"))==NULL) {
							printf("não pode abrir arquivo %s.\n", nome_simet_uptime_arq);
							saida (1);
						}
						executa_touch = 1;
						fclose (fp);
*/
						control->tem_hora_ntp = 1;
					}
					else {
						control->desconecte = 1;
						ERROR_PRINT ("nao conseguiu ntp");
					}
				}



				//converte estruturas UPTIME com htobe64 antes de enviar para o servidor.
/*					uptime_buf.action = ALL_UP;
				memcpy (uptime_buf.mac_address, uptime.mac_address, sizeof (uptime.mac_address));
				uptime_buf.t_up = htobe64 (uptime.t_up);
				uptime_buf.t_iface_up = htobe64 (uptime.t_iface_up);
				uptime_buf.t_iface_down = htobe64 (uptime.t_iface_down);
				uptime_buf.t_cabo_up = htobe64 (uptime.t_cabo_up);
				uptime_buf.t_cabo_down = htobe64 (uptime.t_cabo_down);
*/
//					rc = send(sock_detect, &uptime_buf, sizeof (UPTIME), 0);



				INFO_PRINT ("mac: %s, dev_type %s, uptime: %"PRUI64"\n", mac_address, DEVICE_TYPE, control->time_now);

				id.tam = htobe16(sizeof (id) - sizeof (id.tam));
				id.ver_protocol = VERSAO_PROTOCOLO;
				id.tipo_msg = TIPO_ID;
				id.versao_sb = htobe16(versao_sb);
				id.uptime = htobe64 (control->time_now);
				id.size_mac = (uint8_t) sizeof (id.mac_address);
				memcpy (id.mac_address, mac_address, sizeof (id.mac_address));
				id.size_type = (uint8_t) sizeof (id.type);
				memcpy (id.type, DEVICE_TYPE, sizeof (id.type));
				memcpy (id_buffer, &id, sizeof (id));

			}
		}

		else {
			if ((!control->status_cabo) || (!control->status_iface)) {
				INFO_PRINT ("desconecta %s", ssl?"ssl":"sem ssl");
				if (ssl)
					desconecta_socket (ssl, sock_detect, control);
					continue;
			}


			FD_ZERO(&readfds);
			FD_ZERO(&writefds);

			if (control->envia_id) {
				rc = armazena_dados_s_buffer (&control->sbuf_envio, id_buffer, sizeof (id_buffer));
				if (rc != sizeof (id_buffer)) {
					ERROR_PRINT ("nao alocou buffer id");
					saida (1);
				}
				INFO_PRINT ("bufferizando identificacao - tam: %zu mac: %s, dev_type %s, uptime: %"PRUI64" rc %d, write_len %d\n", sizeof(id) - sizeof (id.tam), mac_address, DEVICE_TYPE, control->time_now, rc, control->sbuf_envio.size);
				control->envia_id = 0;

			}

			if (control->envia_heartbeat) {
				rc = armazena_dados_s_buffer (&control->sbuf_envio, (char*)buffer_msg_nula, TAM_TAM);
				if (rc != TAM_TAM) {
					ERROR_PRINT ("nao alocou buffer id");
					saida (1);
				}
				INFO_PRINT ("heartbeat bufferizado %"PRUI64", rc %d\n", control->time_now, rc);
				control->envia_heartbeat = 0;

#ifdef TESTA_LIXO
				rc += armazena_dados_s_buffer (&control->sbuf_envio, lixo, TAM_LIXO);
				if (rc != TAM_LIXO) {
					ERROR_PRINT ("nao alocou buffer lixo");
					saida (1);
				}

#endif
			}


			FD_SET(sock_detect, &readfds);
			if (!write_blocked_on_read) {

				if ((control->sbuf_envio.size) || (read_blocked_on_write))
					FD_SET(sock_detect, &writefds);
			}


			timeout.tv_sec = 1;
			timeout.tv_usec = 0;


			rc = select(sock_detect + 1, &readfds, &writefds, NULL, &timeout);

			switch (rc) {
				case 0:
//					printf("select() timed out. Continue.\n");
					if (control->desconecte) {
						desconecta_socket (ssl, sock_detect, control);

					}
					continue;
				case - 1:
					if (errno == EINTR) {
						INFO_PRINT("select() interrupted. Continue.\n");
						continue;
					}
					else {
						perror("select() failed\n");
						ERROR_PRINT ("rc %d, errno %d\n", rc, errno);
						saida (1);
					}
					break;
				default:

					if ((FD_ISSET(sock_detect, &readfds)) && (!write_blocked_on_read)) {
						INFO_PRINT ("sock_detect readable\n");
						do {
							read_blocked_on_write = read_blocked = 0;
							rc = SSL_read (ssl, buffer, TAM_MAX_BUF);
							ssl_err = SSL_get_error(ssl,rc);
							switch(ssl_err){
								case SSL_ERROR_NONE:
									break;
								case SSL_ERROR_ZERO_RETURN:
									/* End of data */
									if(!shutdown_wait)
										control->desconecte = 1;
										ERROR_PRINT ("ssl_get_error");
									break;
								case SSL_ERROR_WANT_READ:
									read_blocked=1;
									break;
									/* We get a WANT_WRITE if we're
										trying to rehandshake and we block on
										a write during that rehandshake.

										We need to wait on the socket to be
										writeable but reinitiate the read
										when it is */
								case SSL_ERROR_WANT_WRITE:
									read_blocked_on_write=1;
									break;
								default:
									control->desconecte = 1;
									ERR_error_string(ssl_err, buffer);
									ERROR_PRINT ("ssl_get_error, rc %d, ssl_err %d, [%s]", rc, ssl_err, buffer);
									ERR_print_errors_fp (stderr);
									//saida (1);
							}

//							rc = recv(sock_detect, &buffer, tamtam, 0);

							if (rc > 0) {
								/*

								if (tam_ou_msg == TAM) {
									memcpy ((void*)&tamtam, buffer, TAM_TAM);
									tamtam = be16toh(tamtam);

									if (tamtam > TAM_MAX_MSG) {
										INFO_PRINT ("mensagem com tamanho %d acima do permitido %d\n", tamtam, TAM_MAX_MSG);
										desconecte = 1;
										tamtam = TAM_TAM;
									}
									else if (tamtam) {
										tam_ou_msg = MSG;
										hexdump ((unsigned char*)buffer, rc);
									}
									else {
										last_net_activity_recv = time_now;
										tamtam = TAM_TAM;
										hexdump ((unsigned char*)buffer, rc);
										INFO_PRINT ("Heartbeat received, %"PRUI64"\n", time_now);

									}
								}
								else {
									hexdump ((unsigned char*)buffer, rc);
									INFO_PRINT ("INCONSISTENCIA: tam %d\n", tamtam);
									tamtam = TAM_TAM;
									tam_ou_msg = TAM;

								}
								*/
								control->last_net_activity_recv = control->time_now;
								INFO_PRINT ("rc read: %d\n", rc);
								hexdump ((unsigned char*) buffer, rc);
								num_bytes = armazena_dados_s_buffer (&control->sbuf_recebimento, buffer, rc);
								if ((num_bytes < rc) || (analisa_msg (&control->sbuf_recebimento, versao_sb, control))) {
									limpa_sized_buffer (&control->sbuf_recebimento);
									control->desconecte = 1;
								}


							}

							else if (rc == 0) {
								ERROR_PRINT("Connection closed\n");
								control->desconecte = 1;
							}
/*							if (rc > 0) {
								if (memcmp (buffer, "\0\0", tamtam)) {
									buffer [rc] = '\0';
									INFO_PRINT ("INCONSISTENCIA:\n", buffer, buffer[0]);
									hexdump ((unsigned char*)buffer, rc);
								}
								else {
									INFO_PRINT ("Heartbeat received, %"PRUI64"\n", time_now);
									last_net_activity_recv = time_now;
									memset (buffer, 1, sizeof(buffer));
								}
							}
							else if (rc == 0) {
								ERROR_PRINT("Connection closed\n");
								desconecte = 1;
							}
*/
						} while ((SSL_pending (ssl)) && (!read_blocked) && (!control->desconecte));
					}
					// le entradas recebidas de forma padrão
					if ((!control->desconecte) && (((FD_ISSET(sock_detect, &writefds)) && (control->sbuf_envio.size)) || (write_blocked_on_read && FD_ISSET(sock_detect, &readfds)))) {
						INFO_PRINT ("sock_detect writable\n");
						write_blocked_on_read=0;


						rc = SSL_write (ssl, control->sbuf_envio.buffer, control->sbuf_envio.size);
						if (rc < 0) {
							ERROR_PRINT ("erro no envio");
							control->desconecte = 1;
						}
						ssl_err = SSL_get_error(ssl,rc);
						switch(ssl_err){
						/* We wrote something*/
						case SSL_ERROR_NONE:
							INFO_PRINT ("rc write: %d\n", rc);
							if (rc > 0) {
								hexdump ((unsigned char*)control->sbuf_envio.buffer, rc);
								remove_bytes_buffer (&control->sbuf_envio, rc);
								control->last_net_activity_send = control->time_now;
							}
							break;

							/* We would have blocked */
						case SSL_ERROR_WANT_WRITE:
							break;

							/* We get a WANT_READ if we're
							trying to rehandshake and we block on
							write during the current connection.

							We need to wait on the socket to be readable
							but reinitiate our write when it is */
						case SSL_ERROR_WANT_READ:
							write_blocked_on_read=1;
							break;

							/* Some other error */
						default:
							control->desconecte = 1;
							ERR_error_string(ssl_err, buffer);
							ERROR_PRINT ("ssl_get_error, rc %d, ssl_err %d, [%s]", rc, ssl_err, buffer);
							ERR_print_errors_fp (stderr);
						}
					}

					if (control->desconecte) {
						INFO_PRINT ("desconecta %s", ssl?"ssl":"sem ssl");
						if (ssl)
							desconecta_socket (ssl, sock_detect, control);

					}
			}
		}
//		} // if ((status_iface) && (status_cabo)) {
/*		else {
			printf ("29\n");
			do_sleep (&act);
		}
*/
	}
	//free (modelo);
	//close(sock_link_cabo);
	return (0);
}

