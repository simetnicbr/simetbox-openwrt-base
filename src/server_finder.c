/** ----------------------------------------
* @file   server_finder.c
* @brief
* @author  Rafael Lopes <rafael@nic.br>
* @date 13 de outubro de 2011
*------------------------------------------*/

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "simet_tools.h"
#include <json-c/json.h>

#include "unp.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "server_finder.h"
#include "debug.h"
#include "utils.h"
#include "ntp.h"







static int parse_server_info(json_object *jvalue, Simet_server_info_t ** server_info)
{
	Simet_server_info_t *si;
	json_object *aux;
	*server_info = calloc(1, sizeof(Simet_server_info_t));
	si = *server_info;

	if (!json_object_object_get_ex (jvalue, "description", &aux))
		return 0;
	si->description = strdup (json_object_get_string(aux));
	if (!json_object_object_get_ex (jvalue, "location", &aux))
		return 0;
	si->location = strdup (json_object_get_string(aux));
	if (!json_object_object_get_ex (jvalue, "address", &aux))
		return 0;
	si->address_text = strdup (json_object_get_string(aux));
	if (!json_object_object_get_ex (jvalue, "priority", &aux))
		return 0;
	si->priority = json_object_get_int(aux);
	if (!json_object_object_get_ex (jvalue, "idPoolServer", &aux))
		return 0;
	si->id_pool_server = json_object_get_int(aux);

	INFO_PRINT ("location: %s, address_text: %s, id_pool_server: %d, priority: %d, description: %s", si->location, si->address_text, si->id_pool_server, si->priority, si->description);

	si->rtt = INTMAX_MAX;
	si->socket_control_fd = -1;
	si->next = NULL;
	return 1;
}

int parse_result_servers_parallel (json_object *jobj, Simet_server_info_t ** cabeca, int family) {
	int arraylen, i, j, rc_sel, max_ping, arraylen_final, *socket_array = NULL, *socket_send = NULL, *num_ping = 0, status, len, n, max_sockfd = 0, num_total_ping_recv = 0, flags = 1;
	uint64_t hora_local, hora_recv;
	int64_t **tempos_envio_ping = NULL, **tempos_ping = NULL;
	void json_parse(json_object * jobj); /*Forward Declaration*/
	json_object *jvalue_array_item;
	json_object *jarray = jobj; /*Simply get the array*/
	Simet_server_info_t *si, **next, **vetor = NULL;
	struct ntp_pkt_v4 msg_send, msg_recv;
    fd_set rset, rsetmaster;
	struct timeval tv_timeo;
	struct timespec ts;
	INFO_PRINT ("Escolha Servidor\n");
	next = cabeca;
	arraylen_final = arraylen = json_object_array_length(jarray); /*Getting the length of the array*/

	if(arraylen > 0) {
		vetor = malloc(sizeof(Simet_server_info_t*) * arraylen);
		socket_array = malloc(sizeof(int) * arraylen);
		socket_send = malloc(sizeof(int) * arraylen);
		num_ping = malloc(sizeof(int) * arraylen);
		tempos_envio_ping = calloc(arraylen, sizeof(int64_t*));
		tempos_ping = calloc(arraylen, sizeof(int64_t*));
		if ((!vetor) || (!socket_array) || (!socket_send) || (!num_ping) || (!tempos_envio_ping) || (!tempos_ping)) {
			INFO_PRINT ("nao alocou memoria vetor\n");
			saida (1);
		}
		for (i = 0; i < arraylen; i++) {
			tempos_envio_ping[i] = calloc (NUM_RTT_SELECT_SERVER, sizeof (int64_t));
			tempos_ping[i] = calloc (NUM_RTT_SELECT_SERVER, sizeof (int64_t));
			if ((!tempos_envio_ping[i]) || (!tempos_ping[i])) {
				INFO_PRINT ("nao alocou memoria vetor\n");
				saida (1);
			}
		}

		FD_ZERO (&rsetmaster);
		tv_timeo.tv_sec = TIMEOUT;
		tv_timeo.tv_usec = 0;

		for (i = 0; i < arraylen; i++){
			num_ping[i] = 0;
			jvalue_array_item = json_object_array_get_idx(jarray, i); /*Getting the array element at position i*/
			parse_server_info(jvalue_array_item, &vetor[i]);
			si = vetor[i];
			si->next=NULL;
			*next = vetor[i];
			si->rtt = INTMAX_MAX;
			status = connect_udp(si->address_text, "123", family, &socket_array[i]);
			if(status < 0) {
				ERROR_PRINT("connecting to %s", si->address_text);
				socket_array[i] = -1;
				continue;
			}
			if (socket_array[i] > 0) {
				socket_send[i] = 1;
				if (socket_array[i] > max_sockfd)
					max_sockfd = socket_array[i];
				FD_SET (socket_array[i], &rsetmaster);
				free(si->address_text);
				si->address_text = get_remote_ip(socket_array[i]);
				TRACE_PRINT("utilizando %s como endereco", si->address_text);
				flags = fcntl (socket_array[i], F_GETFL, 0);
				fcntl (socket_array[i], F_SETFL,flags | O_NONBLOCK);
			}
			else
				socket_send[i] = 0;
		}
	}

	max_ping = arraylen * NUM_RTT_SELECT_SERVER;
	INFO_PRINT ("teste NTP servidor de teste...");
	// select
	while (num_total_ping_recv < max_ping) {
		prepara_ntp_v4 (&msg_send, &len, &hora_local);
		for (i = 0; i < arraylen; i++) {
			if (socket_send[i]) {
				send(socket_array[i], (char *) &msg_send, len, 0);
				tempos_envio_ping[i][num_ping[i]] = hora_local;
				socket_send[i] = 0;
			}
		}
		memcpy (&rset, &rsetmaster, sizeof(rset));
		rc_sel = select (max_sockfd + 1, &rset, NULL, NULL, &tv_timeo);
		if (!rc_sel){
			INFO_PRINT ("timeout! saindo selecao servidor\n");
			break;
		}
		clock_gettime(CLOCK_REALTIME, &ts);
		hora_recv = TIMESPEC_NANOSECONDS(ts)/1000;

//		INFO_PRINT ("rc_sel %d, send: %"PRUI64", recv: %"PRUI64"\n", rc_sel, hora_local, hora_recv);
		for (i = j = 0; i < arraylen && j < rc_sel; i++) {
//			INFO_PRINT ("i: %d, j: %d, socket_array[%d]: %d antes recv: rc_sel %d, num_total_ping_recv %d\n", i, j, i, socket_array[i], rc_sel, num_total_ping_recv);
			if ((socket_array[i] > 0) && (FD_ISSET(socket_array[i], &rset))) {
				j++;
				n = recvfrom(socket_array[i], &msg_recv, sizeof(msg_recv), 0, NULL, NULL);
				num_total_ping_recv++;
				tempos_ping[i][num_ping[i]] = hora_recv - tempos_envio_ping[i][num_ping[i]];
				num_ping[i]++;
//				INFO_PRINT ("recebi %do ping de %s(%s) em %"PRI64"us\n", num_ping[i], vetor[i]->address_text, vetor[i]->description, tempos_ping[i][num_ping[i]-1]);
				if ((n > 0) && (num_ping[i] < NUM_RTT_SELECT_SERVER)) {
					socket_send[i] = 1;
				}
			}
		}
	}

	for (i = 0; i < arraylen; i++) {
		si = vetor[i];
		if (num_ping[i] == 0) {
			INFO_PRINT ("liberando servidor simet de descricao: %s\n", si->description);
			free_server_infos_list(si);
			arraylen_final --;
		}
		else {
			*next = si;
			next = &((*next)->next);
			if (num_ping[i] > 0)
				si->rtt = mediana (tempos_ping[i], num_ping[i] - 1);
			INFO_PRINT ("server [%s]: num_ping: %d, mediana = %"PRUI64", prioridade: %d", si->description, num_ping[i], si->rtt, si->priority);
		}
		if (socket_array[i] > 0) {
			close (socket_array[i]);
		}
	}

	free (vetor);
	free (socket_array);
	free (num_ping);
	for (i = 0; i < arraylen; i++) {
		free (tempos_envio_ping[i]);
		tempos_envio_ping[i] = NULL;
		free (tempos_ping[i]);
		tempos_ping[i] = NULL;
	}
	free (tempos_envio_ping);
	free (tempos_ping);
	INFO_PRINT ("pronto\n");

	return arraylen_final;
}

int parse_result_servers (json_object *jobj, Simet_server_info_t ** cabeca, int family) {
	int arraylen, i, arraylen_final;
	void json_parse(json_object * jobj); /*Forward Declaration*/
	json_object *jvalue_array_item;
	json_object *jarray = jobj; /*Simply get the array*/
	Simet_server_info_t *si;
	Simet_server_info_t **next;
	next = cabeca;
	int status;



	arraylen_final = arraylen = json_object_array_length(jarray); /*Getting the length of the array*/

	for (i=0; i< arraylen; i++){
		jvalue_array_item = json_object_array_get_idx(jarray, i); /*Getting the array element at position i*/
		parse_server_info(jvalue_array_item, &si);
		si->next=NULL;
		status = test_server_time(si, family);
		if(status < 0) {
			INFO_PRINT ("liberando servidor simet de descricao: %s\n", si->description);
			free_server_infos_list(si);
			arraylen_final --;
		}
		else {
			*next = si;
			next = &((*next)->next);
		}
	}
	return arraylen_final;
}

static int cmp_simet_server_info(const void *p1, const void *p2)
{

	Simet_server_info_t *s1 = *(Simet_server_info_t * const *)p1;
	Simet_server_info_t *s2 = *(Simet_server_info_t * const *)p2;
	int priority;
	priority = s1->priority - s2->priority;
	if(priority == 0) {
		if(s1->rtt > s2->rtt) {
			return 1;
		} else if(s1->rtt < s2->rtt) {
			return -1;
		} else {
			return 0;
		}
	} else {
		return priority;
	}
}

static int sort_server_infos(Simet_server_info_t ** head, int number_of_ptts)
{
	Simet_server_info_t **vetor;
	Simet_server_info_t *next;
	int i, aux;

	if(number_of_ptts > 0) {
		vetor = malloc(sizeof(next) * number_of_ptts);
		if (!vetor) {
			INFO_PRINT ("nao alocou memoria vetor\n");
			saida (1);
		}
/*        next = head[0];
		i = 0;
		while(next != NULL) {
			vetor[i] = next;
			next = next->next;
			i++;
		}
*/
		for (i = 0, next = head[0]; i < number_of_ptts && next != NULL; i++, next = next->next) {
			vetor[i] = next;
		}
		aux = i;
		assert(i == number_of_ptts);
		qsort(vetor, aux, sizeof(next), cmp_simet_server_info);
		for(i = 1; i < aux; ++i) {
			vetor[i - 1]->next = vetor[i];
		}
		vetor[i - 1]->next = NULL;
		head[0] = vetor[0];
		free(vetor);
	}

	return number_of_ptts;
}
int get_simet_servers(char *host, char *context_root, char * port, const char *location_server, Simet_server_info_t ** head, int family, SSL_CTX *ssl_ctx)
// location_server não é usado aqui
{
	int number_of_ptts, status, tentativa=0;
	json_object * jobj;
	enum json_tokener_error error = json_tokener_success;
	char *string;

	while (tentativa < 5) {
		string = chama_web_service_ssl_arg (ssl_ctx, host, port, family, NULL, 0, "/%s/SimetServerList?v=%d", context_root, family);
		if ((!string) || (!strstr (string, "idPoolServer"))){
			INFO_PRINT ("Nao conseguiu obter lista de servidores de teste na tentativa %d\n", ++tentativa);
			free (string);
			string = NULL;
			sleep(5);
		}
		else break;
	}
	if (!string) {
		INFO_PRINT ("Nao conseguiu obter lista de servidores de teste. Desistindo.\n");
		saida (1);
	}


	jobj = json_tokener_parse_verbose(string, &error);
	free (string);
//	number_of_ptts = parse_result_servers(jobj, head, family);
	number_of_ptts = parse_result_servers_parallel(jobj, head, family);
	INFO_PRINT("number_of_ptts: %d\n", number_of_ptts);
	json_object_put(jobj);
	status = sort_server_infos(head, number_of_ptts);
	return status;
}


/**
* @brief Envia uma mensagem de request ntp ao socket
* \callgraph
* @param socket_fd socket conectado pelo qual os dados serao enviados.
*
* @return numero de bytes enviados. Normalmente 48.
*/
static int send_ntp_message_v3(int socket_fd) {
	char data[48];
	int len, total_len;
	bzero(data, 48);
	data[0] = 0x1b;

	total_len = 0;
	while (total_len < 48) {
		len = send(socket_fd, data,48,0);
		total_len+=len;
	}
	return total_len;
}

static int receive_ntp_messages_v3(int socket_fd) {
	char data[48];
	int len;
	len = recv_timeout(socket_fd,data, 48, TIMEOUT);
	return len;
}

int test_server_time(Simet_server_info_t * server_info, int family)
{

	int socket_fd;
	struct timeval tv_ini, tv_fim;
	int64_t t_ini, t_fim;
	int status;

	server_info->rtt = INTMAX_MAX;
	status = connect_udp(server_info->address_text, "123", family, &socket_fd);
	if(status < 0) {
		ERROR_PRINT("connecting to %s", server_info->address_text);
		return -1;
	}
	free(server_info->address_text);
	server_info->address_text = get_remote_ip(socket_fd);
	TRACE_PRINT("utilizando %s como endereco",server_info->address_text);

	if(gettimeofday(&tv_ini, NULL) != 0) {
		perror("Error getting time of day");
		return -1;
	}
	t_ini = TIMEVAL_MICROSECONDS(tv_ini);

	TRACE_PRINT("((((((  send ntp");
	send_ntp_message_v3(socket_fd);
	status = receive_ntp_messages_v3(socket_fd);
	TRACE_PRINT("))))))  recv ntp");
	if(status > 0) {
		if(gettimeofday(&tv_fim, NULL) != 0) {
			perror("Error getting time of day");
			return -1;
		}
		t_fim =TIMEVAL_MICROSECONDS(tv_fim);
		server_info->rtt = t_fim - t_ini;
	} else {
		return -1;
	}

	return 0;
}

void free_server_infos_list(Simet_server_info_t * head)
{
	Simet_server_info_t *next;
	Simet_server_info_t *prev;

	next = head;
	while(next != NULL) {
		prev = next;
		next = next->next;
		free_server_info(prev);
	}
}

void free_server_info(Simet_server_info_t * node)
{
	if (node) {
		free(node->address_text);
		free(node->location);
		free(node->description);
		free(node);
	}
}
