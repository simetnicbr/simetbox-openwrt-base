/** ----------------------------------------
 * @file  udp_test.c
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 21, 2011
 *------------------------------------------*/

#include "config.h"
#define _GNU_SOURCE
#include <stdint.h>
#ifdef SELECT
#include <sys/select.h>
#else
#include <poll.h>
#include <sys/socket.h>
#ifdef MMSG
#include <linux/sockios.h>
#endif
#ifdef IOVEC
#include <sys/uio.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#endif


#include "unp.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "udp_test.h"
#include "tcp_test.h"
#include "debug.h"
#include "messages.h"
#include "results.h"
#include "protocol.h"
#include "utils.h"
#include "byteconversion.h"


#define MAX_TOKEN_BUCKET  UDP_NUMBER_OF_SOCKETS*UDP_PACKAGE_SIZE_IN_BYTES*8
#define DEFAULT_ARRAY_SIZE 10
#define TIME_SLEEP 100

union number_buffer_u {
    char buffer[8];
    int64_t number;
};

volatile sig_atomic_t sigflag = 0;

void * udp_initialize_context(enum test_direction mode, int64_t rate_int_bps){
    struct udp_context_st * uc;
    uc = calloc(1, sizeof(struct udp_context_st));
    uc->mode = mode;
    uc->authorized = 0;
    if (rate_int_bps > 0){
    	uc->rate = rate_int_bps;
    }else {
    	uc->rate = UDP_DEFAULT_MAX_RATE;
    }
    return uc;
}
void * udp_initialize_context_download(enum test_direction mode, int64_t rate_int_bps, Context_t *context){

    struct udp_context_st * uc = context->data;
    if (!uc)
        uc = calloc(1, sizeof(struct udp_context_st));
    uc->mode = mode;
    uc->authorized = 0;
    if (rate_int_bps > 0){
    	uc->rate = rate_int_bps;
    }else {
    	uc->rate = UDP_DEFAULT_MAX_RATE;
    }
    return uc;

}

int request_sockets_udp(Context_t * context) {

    struct udp_context_st *udp_context;
    int i;
    int status;
    message_t message;
    char buffer[16];

    udp_context = context->data;
    if (udp_context->mode == UPLOAD) {
        for (i = 0; i < UDP_NUMBER_OF_SOCKETS; i++) {
            TRACE_PRINT("context->serverinfo->address_text = %s", context->serverinfo->address_text);
            status = connect_udp(context->serverinfo->address_text, UDP_PORT_TEST_STRING, context->family,
                    &(udp_context->sockets[i]));
            if (status < 0) {
                perror("Error creating socket, request_sockets_udp");
            }
        }
    }
    message.field = "UDPPORT";
    message.content = buffer;
    sprintf(buffer, "%d", get_local_port(udp_context->sockets[0]));
    send_control_message(context->serverinfo, message);

    return 0;
}

int confirm_port_rate_udp_test(Context_t * context, int64_t rate) {
    struct udp_context_st *udp_context;
    message_t message;
    char buffer[16];
    int status;
    int i, j;

    udp_context = context->data;

    union number_buffer_u data;

    TRACE_PRINT("confirm_port_rate_udp_test");

    data.number = htobe64(INT64_MAX);
    TRACE_PRINT("#define UDP_MAGIC_NUMBER %"PRI64, data.number);
    for (i = 0; i < UDP_NUMBER_OF_SOCKETS; i++) {
        for (j = 0; j < 20; j++) {
            status = send(udp_context->sockets[i], data.buffer, sizeof(union number_buffer_u), 0);
            usleep(5000);
        }
    }
    if (udp_context->mode == DOWNLOAD) {
    	message.field = "UDPDOWNLOADPACKAGESIZE";
    }
    else {
    	message.field = "UDPUPLOADPACKAGESIZE";
    }
    message.content = buffer;
    sprintf(buffer, "%d", UDP_PACKAGE_SIZE_IN_BYTES);
    send_control_message(context->serverinfo, message);
    return status;
}

int udp_confirm_download_package_size(Context_t * context) {
    TRACE_PRINT("confirm_packagesize_download_udp_test");
//    union number_buffer_u data;
//    struct udp_context_st *uc;
//    int i, j;
//    int status;

/*    data.number = htobe64(INT64_MAX);
    TRACE_PRINT("#define UDP_MAGIC_NUMBER %"PRI64, data.number);
    uc = context->data;

    for (i = 0; i < UDP_NUMBER_OF_SOCKETS; i++) {
        for (j = 0; j < UDP_NUMBER_OF_SOCKETS; j++) {
            status = send(uc->sockets[i], data.buffer, sizeof(union number_buffer_u), 0);
            usleep(5000);
        }
    }

    if (status < 0) {
        ERROR_PRINT("Error sending initial udp packets");
        return status;

    } */
    return send_control_message(context->serverinfo, M_UDPDOWNLOAD_START);
}
int udp_download_test(Context_t * context) {
//    INFO_PRINT("udp_download_test");
//    struct udp_recv_counters_parameters_st2 params[UDP_NUMBER_OF_SOCKETS];
//    struct udp_result * results[UDP_NUMBER_OF_SOCKETS];
	struct udp_context_st * uc = context->data;
	struct timespec ts_ini;
	struct result_st total_result;
	char * tempstring;
	int64_t t_ini_usec;
	int64_t t_ini [BLOCOS+1];
	int64_t t_fim [BLOCOS+1];
	int64_t total_packages [BLOCOS+1] = {0};
	int64_t total_bytes [BLOCOS+1] = {0};
	int64_t total_em_ordem [BLOCOS+1] = {0};
	int64_t total_fora_de_ordem [BLOCOS+1] = {0};
	int64_t total_perdidos [BLOCOS+1] = {0};
#ifdef SELECT
	fd_set rset, rset_master;
	int maxfdp1 = -1;
#else
	struct pollfd pfds[UDP_NUMBER_OF_SOCKETS];
#endif
	int length, bytes_recv;
//    uint64_t received_counter2[UDP_NUMBER_OF_SOCKETS] = {0};
//    int64_t expected_counter2[UDP_NUMBER_OF_SOCKETS] = {0};
	socklen_t len;
	int buf_size, max_buf_size, lowat, status, max_reads;
//    pthread_t recv_threads[UDP_NUMBER_OF_SOCKETS];
    int i, j, flags;
	sigset_t mask;
	signal_thread_vazao_arg signal_arg;
	pthread_t sig_thread_id;
	struct itimerval timer;

//    memset (params, 0, sizeof(params));
//    uc->params = (void*) params;

	signal_arg.mask = &mask;
	signal_arg.max_blocos = BLOCOS;
	signal_arg.indice_bloco = 0;
	signal_arg.timer = &timer;
	pthread_mutex_init(&signal_arg.mutex, NULL);
	pthread_cond_init(&signal_arg.cond, NULL);

//////////
	// block SIGALRM
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGPIPE);


	status = meu_pthread_create(&sig_thread_id, 0, thread_trata_sinais_vazao, (void*)&signal_arg);
	if (status != 0) {
		ERROR_PRINT("can't create thread\n");
		saida (1);
	}

//	status = pthread_detach(sig_thread_id);
//	if (status != 0)
//	{
//		ERROR_PRINT ("cannot detach sig_thread, exiting program\n");
//		saida (1);
//	}
///////
	clock_gettime(CLOCK_MONOTONIC, &ts_ini);
	t_ini_usec = TIMESPEC_NANOSECONDS(ts_ini)/1000;

	max_buf_size = -1;
#ifdef SELECT
	FD_ZERO (&rset_master);
#endif
	for (i = 0; i < UDP_NUMBER_OF_SOCKETS; i++) {
		flags = fcntl (uc->sockets[i], F_GETFL, 0);
		fcntl (uc->sockets[i], F_SETFL,flags | O_NONBLOCK);
#ifdef SELECT
		FD_SET (uc->sockets[i], &rset_master);

//        INFO_PRINT ("len antes %d", length);
		if (uc->sockets[i] > maxfdp1) {
			maxfdp1 = uc->sockets[i];
		}
#else
		pfds[i].fd = uc->sockets[i];
		pfds[i].events = POLLIN;
#endif

		len = sizeof(buf_size);
		if (getsockopt (uc->sockets[i], SOL_SOCKET, SO_RCVBUF, &buf_size, &len) == 0) {
			if (context->config->vazao_udp_download_buffer_multiplier == 0)
				buf_size = BUFSIZ;
			else {
				buf_size *= context->config->vazao_udp_download_buffer_multiplier;
				if (setsockopt(uc->sockets[i], SOL_SOCKET, SO_RCVBUF, &buf_size, len) != 0)
					ERRNO_PRINT ("setsockopt SO_RCVBUF");
				getsockopt (uc->sockets[i], SOL_SOCKET, SO_RCVBUF, &buf_size, &len);
			}
			if (buf_size > max_buf_size)
				max_buf_size = buf_size;
			lowat = UDP_PACKAGE_SIZE_IN_BYTES;
			if (setsockopt(uc->sockets[i], SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat)) != 0)
				ERRNO_PRINT ("setsockopt SO_RCVLOWAT");
			len = sizeof(lowat);
			getsockopt(uc->sockets[i], SOL_SOCKET, SO_RCVLOWAT, &lowat, &len);
		}
		else {
			ERRNO_PRINT ("getsockopt");
		}
	}
	length = max_buf_size;
#ifdef MMSG
	int rc = 1;
	max_reads = max_buf_size/UDP_PACKAGE_SIZE_IN_BYTES;
	struct mmsghdr msgs[max_reads];
	struct iovec iovecs[max_reads];
	char *buffer[max_reads];
	memset(msgs, 0, sizeof(msgs));
	memset(iovecs, 0, sizeof(iovecs));
	for (i = 0; i < max_reads; i++) {
		buffer[i] = (char*) malloc (UDP_PACKAGE_SIZE_IN_BYTES + 1);
		if (!buffer[i]) {
			ERROR_PRINT ("aloca buffer\n");
			saida (1);
		}
		iovecs[i].iov_base = buffer[i];
		iovecs[i].iov_len = UDP_PACKAGE_SIZE_IN_BYTES;
		msgs[i].msg_hdr.msg_iov = &iovecs[i];
		msgs[i].msg_hdr.msg_iovlen = 1;
	}

#else
    char *buffer;
	buffer = (char*) malloc (length);
	if (!buffer) {
		ERROR_PRINT ("aloca buffer\n");
		saida (1);
	}
	max_reads = max_buf_size / UDP_PACKAGE_SIZE_IN_BYTES;
#endif
	INFO_PRINT ("lowat: %d, tam_buffer download: %d, max_reads %d\n", lowat, length, max_reads);

    for (j = 0; j < BLOCOS+1; j++) {
		t_ini[j] = t_ini_usec + j * 500000;
		t_fim[j] = t_ini[j] + 500000;
//		INFO_PRINT ("tni j %"PRI64", tfim j %"PRI64, t_ini[j], t_fim[j]);
    }
#ifdef SELECT
	size_fdset = maxfdp1/8 + 1;
	memcpy(&rset, &rset_master, sizeof(rset_master));
#endif
	INFO_PRINT ("liga timer udp download\n");
	timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
	timer.it_value.tv_usec = timer.it_interval.tv_usec = 500000;
	setitimer (ITIMER_REAL, &timer, NULL);


	do {
#ifdef SELECT
		memcpy(&rset, &rset_master, size_fdset);
		tv_timeo.tv_sec = 3;
		tv_timeo.tv_usec = 0; // select zera o timeout
		if (select (maxfdp1 + 1, &rset, NULL, NULL, &tv_timeo) <= 0) {
#else
		if (poll(pfds, UDP_NUMBER_OF_SOCKETS, 3000) < 0) {
#endif
			continue;
		}
//        else INFO_PRINT ("resultado select %d em %"PRI64, ressel, t_cur - t_ini_usec);

//        INFO_PRINT ("tempo entre recvs: %.0Lf, tempo do recv: %.0Lf, bytes recv %d", TIMEVAL_MICROSECONDS(tv_cur) - t_cur, TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_aux), bytes_recv);
		for (i = 0; i < UDP_NUMBER_OF_SOCKETS; i++) {
#ifdef SELECT
			if (FD_ISSET(uc->sockets[i], &rset)) {
#else
			if (pfds[i].revents & POLLIN) {
#endif

//                INFO_PRINT ("\tsocket %d", i);
//                while (1) {
#ifdef MMSG
				if (signal_arg.indice_bloco < BLOCOS) {
					rc = recvmmsg(uc->sockets[i], msgs, max_reads, 0, NULL);
					if (rc < 0) {
						ERRNO_PRINT ("recvmmsg, bloco: %d/%d", signal_arg.indice_bloco, BLOCOS);
						saida (1);
					}
//					else INFO_PRINT ("%d rc, bloco: %d/%d", rc, signal_arg.indice_bloco, BLOCOS);
					bytes_recv = 0;
					for (j = 0; j < rc; j++) {
						bytes_recv += msgs[j].msg_len;
						msgs[j].msg_len = 0;
					}
#else
				j = 0;
//				while (((bytes_recv = recv(uc->sockets[i], buffer, length, 0)) > 0) && (j++ < max_reads)) {
				while ((bytes_recv = recv(uc->sockets[i], buffer, length, 0)) > 0) {
#endif
					total_bytes[signal_arg.indice_bloco] += bytes_recv;
				}
			}
		}
	} while (signal_arg.indice_bloco < BLOCOS);

	pthread_mutex_lock (&signal_arg.mutex);
	pthread_cond_signal (&signal_arg.cond);
	pthread_mutex_unlock (&signal_arg.mutex);
	pthread_join(sig_thread_id, NULL);

	pthread_mutex_destroy (&signal_arg.mutex);
	pthread_cond_destroy (&signal_arg.cond);

	for (i = EXTRA_TIME; i < BLOCOS - EXTRA_TIME; i++)
	{
		total_em_ordem[i] = total_packages[i] = total_bytes[i] / UDP_PACKAGE_SIZE_IN_BYTES;
//		INFO_PRINT("bloco  %d - bytes %"PRISIZ" - packages %"PRISIZ" - em ordem %"PRISIZ" - fora de ordem %"PRISIZ", - perdidos %"PRISIZ"\n", i, total_bytes[i], total_packages[i], total_em_ordem[i], total_fora_de_ordem[i], total_perdidos[i]);
		total_result = udp_process_results2(total_bytes[i], total_packages[i], total_em_ordem[i], total_fora_de_ordem[i], total_perdidos[i], t_ini[i], t_fim[i], 0);
		//        context->serverinfo->clockoffset_in_usec);
		total_result.direction = 'D';
		message_t m;
		memset (&m, 0, sizeof (message_t));
		m.field = "SIMET_COMMONS_RESULT_UDP";
		m.content = tempstring = result_string_udp(total_result);
		store_results(context->output,m.field, m.content);
		send_control_message(context->serverinfo, m);
		free(tempstring);
	}
	send_control_message(context->serverinfo, M_UDPDOWNLOADSENDRATE_NOW);

//    INFO_PRINT("udp_download_test");
	return 1;
}

struct udp_result * udp_receive_counters(struct udp_recv_counters_parameters_st *params) {
    uint64_t received_counter = 0;
    uint64_t time_s;
    uint64_t extra;

    struct timeval tv_cur;
    struct timeval tv_ini;
	struct timeval tv_stop_test;

    int bytes_recv;

    struct udp_result head;
    struct udp_result *r = &head;
    r->next = NULL;
    TRACE_PRINT("udp_receive_counters socket=%d--> INI",  params->socket);
    gettimeofday(&tv_cur, NULL);
    gettimeofday(&tv_ini, NULL);
	tv_stop_test.tv_usec = tv_ini.tv_usec;
	tv_stop_test.tv_sec = tv_ini.tv_sec +  params->max_time + 1;

	//INFO_PRINT ("1u, received_counter %"PRI64", params->socket %d, params->max_counter %"PRI64", params->max_time%"PRI64", tv_cur.tv_sec: %"PRI64", tv_stop_test.tv_sec: %"PRI64"\n", received_counter, params->socket, params->max_counter, params->max_time, tv_cur.tv_sec, tv_stop_test.tv_sec);
    while (received_counter < params->max_counter && timercmp(&tv_cur, & tv_stop_test, <)) {
        bytes_recv = recv_udp(params->socket, &received_counter, &time_s, &extra);
        gettimeofday(&tv_cur, NULL);
        if (bytes_recv > 0) {
            r->next = malloc(sizeof(head));
            if (r->next == NULL) {
                ERROR_PRINT("Error creating result");
            }
            r->next->time_recv = ((tv_cur.tv_sec * 1e6) + tv_cur.tv_usec);
            r->next->delta = r->next->time_recv - time_s;
            r->next->time = time_s;
            r->next->bytes_recv = bytes_recv;
            r->next->counter = received_counter;
            r->next->next = NULL;
            r = r->next;
        }
    }
    TRACE_PRINT("udp_receive_counters socket=%d --> FIM",  params->socket);
    return (head.next);
}


struct udp_recv_counters_parameters_st2 * udp_receive_counters2(struct udp_recv_counters_parameters_st2 *params) {
    uint64_t received_counter = 0;
	int64_t t_cur;
    int64_t expected_counter = 0;

    struct timeval tv_cur;
    struct timeval tv_ini;
	struct timeval tv_stop_test;

//    socklen_t peer_addr_len;
    union packetudp_u pac;
    uint8_t buffer[BUFSIZ];


    int32_t bytes_recv, indice_bloco_resultado = 0;
    int64_t tempo [100000];
    int64_t contador [100000];
    uint8_t bloco [100000];
    int32_t num = 0;
    int32_t i;

//    struct udp_result head;
//    struct udp_result *r = &head;
    TRACE_PRINT("udp_receive_counters socket=%d--> INI",  params->socket);
    gettimeofday(&tv_cur, NULL);
    gettimeofday(&tv_ini, NULL);
	tv_stop_test.tv_usec = tv_ini.tv_usec;
	tv_stop_test.tv_sec = tv_ini.tv_sec +  params->max_time + 1;


    printf("socket: %d",  params->socket);


    while (received_counter < params->max_counter && timercmp(&tv_cur, & tv_stop_test, <)) {
//        bytes_recv = recv_udp2(params->socket, &received_counter, &time_s, &extra);

//        gettimeofday(&tv_aux, NULL);

//        bytes_recv = recvfrom(params->socket, buffer, BUFSIZ, 0, peeraddr, &peer_addr_len);
        bytes_recv = read(params->socket, buffer, BUFSIZ);
//    INFO_PRINT("udp recv %d bytes", status);

        if (bytes_recv > 0) {
            gettimeofday(&tv_cur, NULL);
//            INFO_PRINT ("tempo entre recvs: %.0Lf, tempo do recv: %.0Lf, bytes recv %d", TIMEVAL_MICROSECONDS(tv_cur) - t_cur, TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_aux), bytes_recv);
    	    t_cur = TIMEVAL_MICROSECONDS(tv_cur);
            memcpy(pac.buffer, buffer, sizeof(pac.buffer));
//            decode_pac(pac, &received_counter, &time_s, &extra);
            received_counter = be64toh(pac.numbers[0]);
//            time_s = be64toh(pac.numbers[1]);
//            extra = be64toh(pac.numbers[2]);


//            gettimeofday(&tv_aux, NULL);
//            INFO_PRINT ("tempo do memcpy e decode: %.0Lf", TIMEVAL_MICROSECONDS(tv_aux) - TIMEVAL_MICROSECONDS(tv_cur));
//            gettimeofday(&tv_aux, NULL);
            for (; (indice_bloco_resultado < BLOCOS) && (params->t_ini[indice_bloco_resultado] < t_cur); indice_bloco_resultado++);

//            tempo [num] = t_cur;
//            contador [num] = received_counter;
//            bloco [num++] = indice_bloco_resultado;
//            gettimeofday(&tv_aux2, NULL);
//            INFO_PRINT ("tempo do for: %.0Lf", TIMEVAL_MICROSECONDS(tv_aux2) - TIMEVAL_MICROSECONDS(tv_aux));
//    		INFO_PRINT("bloco_res %d - bloco anterior %d - ", indice_bloco_resultado, indice_bloco_anterior);

//            gettimeofday(&tv_aux, NULL);
			if (indice_bloco_resultado < BLOCOS) {
    			params->number_of_bytes[indice_bloco_resultado] += bytes_recv;
	    		params->number_of_packages[indice_bloco_resultado]++;
	    		if (received_counter == expected_counter) {
	    		    params->em_ordem[indice_bloco_resultado]++;
	    		    expected_counter++;
	    		} else if (received_counter < expected_counter) {
	    		/*pacote que nao foi perdido mas chegou fora de ordem */
	    		    assert (params->perdidos[indice_bloco_resultado] >= 0);
	    		    params->perdidos[indice_bloco_resultado]--;
	    		    params->fora_de_ordem[indice_bloco_resultado]++;
	    		} else {
	    		/*eh maior do que o esperado entao perderam-se todos no intervalo */
	    		    params->perdidos[indice_bloco_resultado] += received_counter - expected_counter;
	    		    expected_counter = received_counter + 1;
	    		}
//        	    INFO_PRINT("socket %d bytesrecv=%"PRISIZ" pacotesrecv=%"PRISIZ"\n", args->socket, args->number_of_bytes[indice_bloco_resultado], args->number_of_packages[indice_bloco_resultado]);
	    	}
	    	else INFO_PRINT ("FORA DO BLOCO");
//            gettimeofday(&tv_aux2, NULL);
//            INFO_PRINT ("tempo do if: %.0Lf", TIMEVAL_MICROSECONDS(tv_aux2) - TIMEVAL_MICROSECONDS(tv_aux));
//    		INFO_PRINT("args %"PRISIZ" - tcur %"PRISIZ"\n", args->t_ini[indice_bloco_resultado], t_cur);
			/*TODO:remover*/
        }
    else INFO_PRINT ("recv udp %d bytes", bytes_recv);
    }TRACE_PRINT("udp_receive_counters socket=%d --> FIM",  params->socket);
    INFO_PRINT("max_counters=%"PRI64"--> INI --> %"PRI64,  params->max_counter, received_counter);

    for (i = 1; i < num ; i++) {
        printf ("socket: %d - diff tempo = %"PRI64", contador %"PRI64", bloco %d\n", params->socket, tempo [i] - tempo [i-1], contador [i], bloco [i]);
    }
    return (params);
}


static struct count_result_st count_udp_result(struct udp_result *results) {

    struct udp_result *r;
    struct count_result_st cr;
    TRACE_PRINT("count_rtt_result");

    r = results;
    cr.out_of_order = 0;
    cr.lost = 0;
    cr.right = 0;
    cr.total_bytes = 0;
    cr.max_packet_size = 0;

    static int64_t expected_counter = 0;
    while (r != NULL) {
        cr.total_bytes += r->bytes_recv;
        cr.max_packet_size = MAX(cr.max_packet_size, r->bytes_recv);
        if (r->counter == expected_counter) {
            cr.right++;
            expected_counter++;
            INFO_PRINT("right %"PRI64"\n", r->counter);
        } else if (r->counter < expected_counter) {

            /*pacote que nao foi perdido mas chegou fora de ordem */
            assert(cr.lost >= 0);
            cr.lost--;
            cr.out_of_order++;
            INFO_PRINT("out of order %"PRI64"\n", r->counter);
        } else {

            /*eh maior do que o esperado entao perderam-se todos no intervalo */
            INFO_PRINT("lost %"PRI64" packages (counter: %"PRI64", expected_counter: %"PRI64")\n", r->counter - expected_counter, r->counter, expected_counter);
			cr.lost += r->counter - expected_counter;
            expected_counter = r->counter + 1;
        }
        r = r->next;
    }
    return cr;
}



static int64_t calculate_send_rate(int64_t total_bytes, int64_t event_time_init, int64_t event_time_end) {
    int64_t bps = 0;
    double delta = (event_time_end - event_time_init) / 1e6;
    bps = (total_bytes * 8) / delta;
    return bps;
}



struct result_st udp_process_results(struct udp_result **results_array, int64_t number_of_results,
        struct timeval tv_ini, struct timeval tv_end, int64_t clock_offset_in_us) {

    struct udp_result *r;
    struct udp_result *aux;
    struct result_st result;
    int i;

    result.direction = 'U';
    int64_t event_ini = TIMEVAL_MICROSECONDS(tv_ini) - clock_offset_in_us;
    int64_t event_end = TIMEVAL_MICROSECONDS(tv_end) - clock_offset_in_us;
    result.event_start_time_in_us = event_ini;
    result.event_end_time_in_us = event_end;
    result.number_of_packages = 0;
    result.package_size_in_bytes = 0;
    result.number_of_bytes = 0;
    result.lost_packages = 0;
    result.outoforder_packages = 0;
    result.time_in_us = event_end - event_ini;

    for (i = 0; i < number_of_results; i++) {
        struct count_result_st cr = count_udp_result(results_array[i]);
        result.number_of_packages += cr.right + cr.out_of_order;
        result.package_size_in_bytes = MAX(result.package_size_in_bytes , cr.max_packet_size);
        result.number_of_bytes += cr.total_bytes;
        result.lost_packages += cr.lost;
        result.outoforder_packages += cr.out_of_order;
        r = results_array[i];
        while (r != NULL) {
            aux = r;
            r = r->next;
            free(aux);
        }
    }
    result.send_rate_in_bps = calculate_send_rate(result.number_of_bytes, event_ini, event_end);
    return result;

}


struct result_st udp_process_results2(int64_t total_bytes, int64_t number_of_packages, int64_t right, int64_t out_of_order, int64_t lost, int64_t tv_ini, int64_t tv_end, int64_t clock_offset_in_us) {

//    struct udp_result *r;
//    struct udp_result *aux;
    struct result_st result;
//    int i;

    result.direction = 'U';
    int64_t event_ini = tv_ini - clock_offset_in_us;
    int64_t event_end = tv_end - clock_offset_in_us;
    result.event_start_time_in_us = event_ini;
    result.event_end_time_in_us = event_end;
    result.lost_packages = 0;
    result.outoforder_packages = 0;
//    result.time_in_us = event_end - event_ini;
    result.time_in_us = 0;
//        struct count_result_st cr = count_udp_result(results_array[i]);
    result.number_of_packages = number_of_packages; // right + out_of_order;
//    result.number_of_packages = right + out_of_order;
    result.package_size_in_bytes = 1200; //MAX(result.package_size_in_bytes , cr.max_packet_size);
    result.number_of_bytes = total_bytes;
    result.lost_packages = lost;
    result.outoforder_packages = out_of_order;
    result.send_rate_in_bps = calculate_send_rate(result.number_of_bytes, event_ini, event_end);
//    result.send_rate_in_bps = 0;

    return result;

}



struct result_st udp_process_results_rtt (struct udp_result **results_array, int64_t number_of_results,
        int64_t t_ini, int64_t t_fim, int64_t clock_offset_in_us) {

    struct udp_result *r;
    struct udp_result *aux;
    struct result_st result;
    int64_t *delta_array = NULL;
    int32_t i, j = 0;

    result.direction = 'U';
    int64_t event_ini = t_ini - clock_offset_in_us;
    int64_t event_end = t_fim - clock_offset_in_us;
    result.event_start_time_in_us = event_ini;
    result.event_end_time_in_us = event_end;
    result.number_of_packages = 0;
    result.package_size_in_bytes = 0;
    result.number_of_bytes = 0;
    result.lost_packages = 0;
    result.outoforder_packages = 0;
    result.time_in_us = 0;

    for (i = 0; i < number_of_results; i++) {
        struct count_result_st cr = count_udp_result(results_array[i]);
        result.number_of_packages += cr.right + cr.out_of_order + cr.lost;
        result.package_size_in_bytes = MAX(result.package_size_in_bytes , cr.max_packet_size);
        result.number_of_bytes += cr.total_bytes;
        result.lost_packages += cr.lost;
        result.outoforder_packages += cr.out_of_order;
        r = results_array[i];
        // monta array para cálculo de mediana rtt
        for (aux = r; aux != NULL; aux = aux->next)
        {
            if (j % DEFAULT_ARRAY_SIZE == 0)
            {
                delta_array = realloc (delta_array, sizeof (uint64_t) * (j + DEFAULT_ARRAY_SIZE));
                if (!delta_array)
                    ERROR_PRINT ("nao conseguiu alocar array para mediana rtt");
            }
            delta_array[j++] = aux->delta;
        }
        if (j != 0)
            result.time_in_us = mediana (delta_array, j - 1);



//        while (r != NULL) {
//            aux = r;
//            r = r->next;
//            free(aux);
//        }
        free (delta_array);
    }
    result.send_rate_in_bps = calculate_send_rate(result.number_of_bytes, event_ini, event_end);

    return result;

}




void *udp_receive_counters_callback(void *receive_counters_parameters) {
    TRACE_PRINT("receive_counters_callback");
    struct udp_recv_counters_parameters_st * params = receive_counters_parameters;
    return udp_receive_counters(params);
}

void *udp_receive_counters_callback2(void *receive_counters_parameters) {
    TRACE_PRINT("receive_counters_callback");
    struct udp_recv_counters_parameters_st2 * params = receive_counters_parameters;
    return udp_receive_counters2(params);
}


union packet_udp_upload {
    uint64_t counter;
    uint8_t buffer[UDP_PACKAGE_SIZE_IN_BYTES];
};


int udp_upload_test2(Context_t *context)
{
    union packet_udp_upload packages[UDP_NUMBER_OF_SOCKETS];
    struct udp_context_st *utx = context->data;

    struct timeval tv_ini;
    struct timeval tv_cur;
    struct timeval tv_end;
    struct timeval tv_to;
	struct timeval tv_stop_test;
    fd_set wset, wset_master;
	int32_t i, maxfdp1, flags;
    int64_t pkt_counter[UDP_NUMBER_OF_SOCKETS] = {0};
	int length, status;
	socklen_t lenlen;
//	int64_t t_cur;



    /*tokens para fazer um token bucket
     * a cada iteracao sao adicionadas */
//    double bits_per_usec;
//    double tokens = 8 * UDP_PACKAGE_SIZE_IN_BYTES;

//    if (context->bytes_tcp_upload_test == 0)
//    	context->bytes_tcp_upload_test = 30000 * TCP_TOTAL_TIME_SEC;

//    bits_per_usec = context->bytes_tcp_upload_test * 8 / (1e6  * TCP_TOTAL_TIME_SEC); //bits por microsegundo

//    INFO_PRINT("UDP Upload Test: Start");
//    INFO_PRINT("bits_per_usec %lf, context->bytes_tcp_upload_test %"PRI64, bits_per_usec, context->bytes_tcp_upload_test);

//    int iteration=0;
//    int socket_number;

    tv_to.tv_sec = 0;
    tv_to.tv_usec = 100000;

    maxfdp1 = -1;
    //lenlen = sizeof (length);
    length = UDP_PACKAGE_SIZE_IN_BYTES;
    FD_ZERO (&wset_master);
    for (i = 0; i < UDP_NUMBER_OF_SOCKETS; i++) {
        flags = fcntl (utx->sockets[i], F_GETFL, 0);
        fcntl (utx->sockets[i], F_SETFL,flags | O_NONBLOCK);
        status = getsockopt(utx->sockets[i], SOL_SOCKET, SO_SNDBUF, (char *)&length, &lenlen);
        INFO_PRINT ("SO_SNDBUF antes %d", length);
/*        length = UDP_PACKAGE_SIZE_IN_BYTES;
        status = setsockopt(utx->sockets[i], SOL_SOCKET, SO_SNDLOWAT, (char *)&length, sizeof(length));
        if (status < 0)
        switch (errno) {
            case EBADF:
                INFO_PRINT ("EBADF: The argument sockfd is not a valid descriptor.");
                break;
            case EFAULT:
                INFO_PRINT (" EFAULT: The address pointed to by optval is not in a valid part of the process address space.  For getsockopt(), this error may also be returned if optlen is not in a valid part of the process address space.");
                break;
            case EINVAL:
                INFO_PRINT ("EINVAL: optlen invalid in setsockopt().  In some cases this error can also occur for an invalid value in optval (e.g., for the IP_ADD_MEMBERSHIP option described in ip(7)).");
                break;
            case ENOPROTOOPT:
                INFO_PRINT ("ENOPROTOOPT: The option is unknown at the level indicated.");
                break;
            case ENOTSOCK:
                INFO_PRINT ("ENOTSOCK: The argument sockfd is a file, not a socket.");
                break;
            default: INFO_PRINT ("unknown error");
                break;
        }
        status = getsockopt(utx->sockets[i], SOL_SOCKET, SO_SNDLOWAT, (char *)&length, &lenlen);
        INFO_PRINT ("SO_SNDLOWAT depois %d", length);
*/        FD_SET (utx->sockets[i], &wset_master);
        if (utx->sockets[i] > maxfdp1) {
            maxfdp1 = utx->sockets[i];
        }
    }


    gettimeofday(&tv_ini, NULL);
	tv_stop_test.tv_usec = tv_ini.tv_usec;
	tv_stop_test.tv_sec = tv_ini.tv_sec + UDP_TEST_TIME;


    do {
        memcpy(&wset, &wset_master, sizeof(wset_master));
        while (select (maxfdp1 + 1, NULL, &wset, NULL, &tv_to) < UDP_NUMBER_OF_SOCKETS);
        gettimeofday(&tv_cur, NULL);
//        INFO_PRINT ("tempo entre recvs: %.0Lf, tempo do recv: %.0Lf, bytes recv %d", TIMEVAL_MICROSECONDS(tv_cur) - t_cur, TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_aux), bytes_recv);
//   	    t_cur = TIMEVAL_MICROSECONDS(tv_cur);
        for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
            if (FD_ISSET(utx->sockets[i], &wset)) {
           		packages[i].counter = htobe64(pkt_counter[i]);
           		pkt_counter[i]++;
         		status = send(utx->sockets[i], packages[i].buffer, UDP_PACKAGE_SIZE_IN_BYTES, 0);
         	}
         }
    } while (timercmp(&tv_cur, &tv_stop_test, <));

    gettimeofday(&tv_end, NULL);
    send_control_message(context->serverinfo, M_UDPUPLOADRESULT_SENDRESULTS);
//    INFO_PRINT("Tcp Upload Test: End");
    return status;
}




int udp_upload_test_busy_waiting(Context_t *context)
{
    union packet_udp_upload packages[UDP_NUMBER_OF_SOCKETS];
    struct udp_context_st *utx = context->data;

    struct timeval tv_ini;
    struct timeval tv_before;
    struct timeval tv_cur;
    struct timeval tv_end;
    struct timeval tv_aux;

	struct timeval tv_stop_test;


    int status = 0;

    /*tokens para fazer um token bucket
     * a cada iteracao sao adicionadas */
    double bits_per_usec;
    double tokens = 8 * UDP_PACKAGE_SIZE_IN_BYTES;

    if (context->bytes_tcp_upload_test == 0)
    	context->bytes_tcp_upload_test = 30000 * (TCP_TOTAL_TIME_SEC + EXTRA_TIME);

    bits_per_usec = context->bytes_tcp_upload_test * 8 / (1e6  * (TCP_TOTAL_TIME_SEC + EXTRA_TIME)); //bits por microsegundo

//    INFO_PRINT("UDP Upload Test: Start");
//    INFO_PRINT("bits_per_usec %lf, context->bytes_tcp_upload_test %"PRI64, bits_per_usec, context->bytes_tcp_upload_test);

    int iteration=0;
    int socket_number;



    gettimeofday(&tv_ini, NULL);
	tv_stop_test.tv_usec = tv_ini.tv_usec;
	tv_stop_test.tv_sec = tv_ini.tv_sec + UDP_TEST_TIME;

    tv_aux = tv_cur = tv_ini;

    tv_before = tv_cur;
    while (timercmp(&tv_cur, & tv_stop_test, <)) {
    	tokens += bits_per_usec * (TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_before));
//        INFO_PRINT ("delta %lf - tokens %"PRI64, delta, tokens);
//    	tokens = MIN(tokens, MAX_TOKEN_BUCKET);

   		socket_number =iteration%UDP_NUMBER_OF_SOCKETS;
   		packages[socket_number].counter = htobe64(iteration/UDP_NUMBER_OF_SOCKETS);
        if (tokens * 8 >= UDP_PACKAGE_SIZE_IN_BYTES) {
       	    status = send(utx->sockets[socket_number], packages[socket_number].buffer, UDP_PACKAGE_SIZE_IN_BYTES, 0);
   	        if (status < 0){
   	            ERROR_PRINT("fail send udp: status = %d\n", status);
       	    }else {
//                INFO_PRINT ("enviei UDP %d - status %d, tokens %lf", iteration, status, tokens);
   	        	tokens -= status * 8;
    	    }
       		iteration++;
        }
      	tv_before = tv_cur;
        if (TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_aux) > 2500000) {
//            INFO_PRINT ("aumentei vazao, diff_t %"PRI64", intervalo antes %"PRI64", intervalo depois %lf", TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_ini), intervalo, (intervalo/1.05));
            bits_per_usec *= 1.05; // a cada 2,5s, aumenta a vazao em 5%
            tv_aux = tv_cur;
        }
        gettimeofday(&tv_cur, NULL);

    }
    gettimeofday(&tv_end, NULL);
    send_control_message(context->serverinfo, M_UDPUPLOADRESULT_SENDRESULTS);
//    INFO_PRINT("Tcp Upload Test: End");
    return status;
}

// int udp_upload_test(Context_t *context)
// // versão com sleep
// {
//     union packet_udp_upload packages[UDP_NUMBER_OF_SOCKETS];
//     struct udp_context_st *utx = context->data;
//     double intervalo_em_usec;
//     struct timeval tv_ini;
// //    struct timeval tv_before;
//     struct timeval tv_cur;
//     struct timeval tv_end;
//
//         struct timeval tv_stop_test;
//         int fd;
//     int socket_number;
//         uint64_t counter = 0, counter_send, t_ini, t_aux, t_cur, t_stop, calculo_pacotes_enviados=0;
//
//
//         for (socket_number = 0; socket_number < UDP_NUMBER_OF_SOCKETS; socket_number++) {
//                 fd = open("/dev/urandom", O_RDONLY);
//                 if (fd != -1)
//                 {
//                         if (!read(fd, (void *)packages[socket_number].buffer, UDP_PACKAGE_SIZE_IN_BYTES)) {
//                                 INFO_PRINT ("nao foi possivel ler /dev/urandom\n");
//                                 return -1;
//                         }
//                         close(fd);
//                 }
//         }
//
//     int status = 0;
//
//     /*tokens para fazer um token bucket
//      * a cada iteracao sao adicionadas */
//
//     if (context->bytes_tcp_upload_test == 0)
//         context->bytes_tcp_upload_test = 30000 * (TCP_TOTAL_TIME_SEC + EXTRA_TIME);
//
//     intervalo_em_usec = (1e6 * UDP_PACKAGE_SIZE_IN_BYTES * (TCP_TOTAL_TIME_SEC + EXTRA_TIME)) *0.85 / context->bytes_tcp_upload_test; //intervalo em usec entre envio de pacotes (obs.: * 0.85 para equilibrar com o tcp)
//
// //    INFO_PRINT("UDP Upload Test: Start");
// //    INFO_PRINT("intervalo %lf, context->bytes_tcp_upload_test %"PRI64, intervalo_em_usec, context->bytes_tcp_upload_test);
//
// //    uint64_t iteration=0;
//
//
//
//     gettimeofday(&tv_ini, NULL);
//         t_ini = TIMEVAL_MICROSECONDS(tv_ini);
//         tv_stop_test.tv_usec = tv_ini.tv_usec;
//         tv_stop_test.tv_sec = tv_ini.tv_sec + UDP_TEST_TIME + EXTRA_TIME;
//         t_stop = TIMEVAL_MICROSECONDS(tv_stop_test);
//
// //    tv_cur.tv_sec = tv_aux.tv_sec = tv_ini.tv_sec;
// //    tv_cur.tv_usec = tv_aux.tv_usec = tv_ini.tv_usec;
//         t_cur = t_aux = t_ini;
//
//     do {
// //        INFO_PRINT ("delta %lf - tokens %"PRI64, delta, tokens);
//                 counter_send = htobe64(counter);
// //        if (calculo_pacotes_enviados <= (TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_ini)) / intervalo_em_usec) {
//         if (calculo_pacotes_enviados <= (t_cur - t_ini) / intervalo_em_usec) {
//         // se o número de pacotes enviados for menor do que o cálculo da quantidade que deveria ser enviada num determinado instante ou não enviou por todos os sockets, então envia pacote
//                         for (socket_number = 0; socket_number < UDP_NUMBER_OF_SOCKETS; socket_number++) {
//                                 packages[socket_number].counter = counter_send;
//                                 //INFO_PRINT ("socket %d, buffer %s, UDP_PACKAGE_SIZE_IN_BYTES %d",utx->sockets[socket_number], packages[socket_number].buffer, UDP_PACKAGE_SIZE_IN_BYTES
//                 status = send(utx->sockets[socket_number], packages[socket_number].buffer, UDP_PACKAGE_SIZE_IN_BYTES, 0);
//                                 if (status < 0){
//                                         ERROR_PRINT("fail send udp: status = %d. abortando\n", status);
//                                         saida (1);
//                                 }
//                         }
// //                      iteration += UDP_NUMBER_OF_SOCKETS;
//                         calculo_pacotes_enviados += UDP_NUMBER_OF_SOCKETS;
//                         counter++;
//
//         }
//         // caso contrário (já enviou a quantidade de pacotes devidos num determinado instante e já colocou pacotes para envio em todos os sockets
//         else usleep (intervalo_em_usec);
//         if (t_cur - t_aux > 2500000) {
//             //INFO_PRINT ("aumentei vazao, diff_t %"PRUI64", intervalo antes %lf, intervalo depois %lf, calculo_pacotes_enviados %"PRUI64", calculo_pacotes_enviados depois %lf", t_cur - t_ini, intervalo_em_usec, (intervalo_em_usec/1.05), calculo_pacotes_enviados, calculo_pacotes_enviados *1.05);
//             intervalo_em_usec /= 1.05; // a cada 2,5s, aumenta a vazao em 5%
//                 calculo_pacotes_enviados *= 1.05; // para manter a proporção do cálculo de vazão UDP
// //            tv_aux.tv_sec = tv_cur.tv_sec;
// //            tv_aux.tv_usec = tv_cur.tv_usec;
//                         t_aux = t_cur;
//         }
// //        tv_aux.tv_sec = tv_cur.tv_sec;
// //        tv_aux.tv_usec = tv_cur.tv_usec;
//         gettimeofday(&tv_cur, NULL);
//                 t_cur = TIMEVAL_MICROSECONDS(tv_cur);
//
// //    } while (timercmp(&tv_cur, & tv_stop_test, <));
//     } while (t_cur < t_stop);
//     gettimeofday(&tv_end, NULL);
//     send_control_message(context->serverinfo, M_UDPUPLOADRESULT_SENDRESULTS);
// //    INFO_PRINT("Tcp Upload Test: End");
//
// //    for (i = 0; i < UDP_NUMBER_OF_SOCKETS; i++) {
// //        close (utx->sockets[i]);
// //    }
//     return status;
// }


// int udp_upload_test_flood(Context_t *context)
// // versão com sleep
// {
//     union packet_udp_upload packages[UDP_NUMBER_OF_SOCKETS];
//     struct udp_context_st *utx = context->data;
//     double intervalo_em_usec;
//     struct timeval tv_ini;
// //    struct timeval tv_before;
//     struct timeval tv_cur;
//     struct timeval tv_end;
//
// 	struct timeval tv_stop_test;
// 	int fd, socket_number, i, status, buf_size, max_buf_size, occupied_buff, sleep, max_burst, min_free_buff;
// 	uint64_t counter [UDP_NUMBER_OF_SOCKETS] = {0}, counter_send, t_ini, t_aux, t_cur, t_stop, calculo_pacotes_enviados=0;
// 	socklen_t len;
// 	sigset_t mask, oldmask;
// 	signal_thread_vazao_arg signal_arg;
// 	pthread_t sig_thread_id;
// 	struct itimerval timer;
//
// 	signal_arg.mask = &mask;
// 	signal_arg.max_blocos = BLOCOS;
// 	signal_arg.indice_bloco = 0;
//
// //////////
// 	// block SIGALRM
// 	sigemptyset(&mask);
// 	sigaddset(&mask, SIGALRM);
// 	sigaddset(&mask, SIGPIPE);
//
//
// 	status = meu_pthread_create(&sig_thread_id, 0, thread_trata_sinais_vazao, (void*)&signal_arg);
//     if (status != 0) {
//         ERROR_PRINT("can't create thread\n");
//         saida (1);
//     }
//
// 	status = pthread_detach(sig_thread_id);
// 	if (status != 0)
// 	{
// 		ERROR_PRINT ("cannot detach sig_thread, exiting program\n");
// 		saida (1);
// 	}
// ///////
//
// 	for (socket_number = 0; socket_number < UDP_NUMBER_OF_SOCKETS; socket_number++) {
// 		fd = open("/dev/urandom", O_RDONLY);
// 		if (fd != -1) {
// 			if (!read(fd, (void *)packages[socket_number].buffer, UDP_PACKAGE_SIZE_IN_BYTES)) {
// 				INFO_PRINT ("nao foi possivel ler /dev/urandom\n");
// 				return -1;
// 			}
// 			close(fd);
// 		}
// 		len = sizeof(buf_size);
// 		if (getsockopt (utx->sockets[socket_number], SOL_SOCKET, SO_SNDBUF, &buf_size, &len) == 0) {
// 			buf_size *= context->config->vazao_tcp_upload_buffer_multiplier;
// 			if (setsockopt(utx->sockets[socket_number], SOL_SOCKET, SO_SNDBUF, &buf_size, len) != 0)
// 				ERRNO_PRINT ("setsockopt SO_SNDBUF");
// 			getsockopt (utx->sockets[socket_number], SOL_SOCKET, SO_SNDBUF, &buf_size, &len);
// 			if (buf_size > max_buf_size)
// 				max_buf_size = buf_size;
// 		}
// 		else {
// 			ERRNO_PRINT ("getsockopt");
// 		}
// 	}
//
//     status = 0;
//
//     /*tokens para fazer um token bucket
//      * a cada iteracao sao adicionadas */
//
// //    if (context->bytes_tcp_upload_test == 0)
// //    	context->bytes_tcp_upload_test = 30000 * (TCP_TOTAL_TIME_SEC + EXTRA_TIME);
//
// //    intervalo_em_usec = (1e6 * UDP_PACKAGE_SIZE_IN_BYTES * (TCP_TOTAL_TIME_SEC + EXTRA_TIME)) *0.85 / context->bytes_tcp_upload_test; //intervalo em usec entre envio de pacotes (obs.: * 0.85 para equilibrar com o tcp)
//
// //    INFO_PRINT("UDP Upload Test: Start");
// //    INFO_PRINT("intervalo %lf, context->bytes_tcp_upload_test %"PRI64, intervalo_em_usec, context->bytes_tcp_upload_test);
//
// //    uint64_t iteration=0;
//
//
//
// //	INFO_PRINT ("lowat: %d, tam_buffer upload: %d\n", lowat, length);
//     min_free_buff = max_buf_size * 0.25;
// 	max_burst = (max_buf_size / UDP_PACKAGE_SIZE_IN_BYTES) * 0.2;
// 	INFO_PRINT ("tam_buffer upload: max_buffer %d, min_free_buff %d, max_burst: %d\n", max_buf_size, min_free_buff, max_burst);
//
//     gettimeofday(&tv_ini, NULL);
// 	t_ini = TIMEVAL_MICROSECONDS(tv_ini);
// 	tv_stop_test.tv_usec = tv_ini.tv_usec;
// 	tv_stop_test.tv_sec = tv_ini.tv_sec + UDP_TEST_TIME + EXTRA_TIME;
// 	t_stop = TIMEVAL_MICROSECONDS(tv_stop_test);
//
// //    tv_cur.tv_sec = tv_aux.tv_sec = tv_ini.tv_sec;
// //    tv_cur.tv_usec = tv_aux.tv_usec = tv_ini.tv_usec;
// 	t_cur = t_aux = t_ini;
// 	INFO_PRINT ("liga timer udp upload");
// 	timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
// 	timer.it_value.tv_usec = timer.it_interval.tv_usec = 500000;
// 	setitimer (ITIMER_REAL, &timer, NULL);
//     do {
// //        INFO_PRINT ("delta %lf - tokens %"PRI64, delta, tokens);
// //        if (calculo_pacotes_enviados <= (TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_ini)) / intervalo_em_usec) {
// //        if (calculo_pacotes_enviados <= (t_cur - t_ini) / intervalo_em_usec) {
//         // se o número de pacotes enviados for menor do que o cálculo da quantidade que deveria ser enviada num determinado instante ou não enviou por todos os sockets, então envia pacote
// 		sleep = 1;
//
// 		for (socket_number = 0; socket_number < UDP_NUMBER_OF_SOCKETS; socket_number++) {
// 			if (ioctl(utx->sockets[socket_number], SIOCOUTQ, &occupied_buff) < 0) {
// 				ERRNO_PRINT ("ioctl");
// 				saida (1);
// 			}
//
// 			if (max_buf_size - occupied_buff > min_free_buff) {
// 				for (i = 0; i < max_burst; i++) {
// 					sleep = 0;
// 					packages[socket_number].counter = htobe64(counter[socket_number]++);
// 					//INFO_PRINT ("socket %d, buffer %s, UDP_PACKAGE_SIZE_IN_BYTES %d",utx->sockets[socket_number], packages[socket_number].buffer, UDP_PACKAGE_SIZE_IN_BYTES
// 					status = send(utx->sockets[socket_number], packages[socket_number].buffer, UDP_PACKAGE_SIZE_IN_BYTES, 0);
// 					if (status < 0){
// 						ERROR_PRINT("fail send udp: status = %d. abortando\n", status);
// 						saida (1);
// 					}
// 				}
// 			}
// 		}
// 		if (sleep)
// 			usleep (10);
//
// //			iteration += UDP_NUMBER_OF_SOCKETS;
// //		calculo_pacotes_enviados += UDP_NUMBER_OF_SOCKETS;
//
// //        }
//         // caso contrário (já enviou a quantidade de pacotes devidos num determinado instante e já colocou pacotes para envio em todos os sockets
// //        else usleep (intervalo_em_usec);
// /*        if (t_cur - t_aux > 2500000) {
//             //INFO_PRINT ("aumentei vazao, diff_t %"PRUI64", intervalo antes %lf, intervalo depois %lf, calculo_pacotes_enviados %"PRUI64", calculo_pacotes_enviados depois %lf", t_cur - t_ini, intervalo_em_usec, (intervalo_em_usec/1.05), calculo_pacotes_enviados, calculo_pacotes_enviados *1.05);
//             intervalo_em_usec /= 1.05; // a cada 2,5s, aumenta a vazao em 5%
//        		calculo_pacotes_enviados *= 1.05; // para manter a proporção do cálculo de vazão UDP
// //            tv_aux.tv_sec = tv_cur.tv_sec;
// //            tv_aux.tv_usec = tv_cur.tv_usec;
// 			t_aux = t_cur;
//         }
// */
// //        tv_aux.tv_sec = tv_cur.tv_sec;
// //        tv_aux.tv_usec = tv_cur.tv_usec;
// //        gettimeofday(&tv_cur, NULL);
// //		t_cur = TIMEVAL_MICROSECONDS(tv_cur);
//
// //    } while (timercmp(&tv_cur, & tv_stop_test, <));
// //    } while (t_cur < t_stop);
//     } while (signal_arg.indice_bloco < BLOCOS);
// 	INFO_PRINT ("desliga timer tcp upload\n");
// 	timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
// 	timer.it_value.tv_usec = timer.it_interval.tv_usec = 0;
// 	setitimer (ITIMER_REAL, &timer, NULL);
//
// 	gettimeofday(&tv_end, NULL);
//     send_control_message(context->serverinfo, M_UDPUPLOADRESULT_SENDRESULTS);
// //    INFO_PRINT("Tcp Upload Test: End");
//
// //    for (i = 0; i < UDP_NUMBER_OF_SOCKETS; i++) {
// //        close (utx->sockets[i]);
// //    }
//     return status;
// }


void *thread_trata_sinais_vazao_udp_upload (void *arg) {
	int err, signo;
	signal_thread_vazao_udp_upload_arg *signal_arg;
	struct itimerval timer;
	struct timespec ts_fim;
	uint64_t t_next_sig;

	signal_arg = (signal_thread_vazao_udp_upload_arg*) arg;
	INFO_PRINT ("Thread Sinais Vazao\n");

	clock_gettime(CLOCK_MONOTONIC, &signal_arg->ts_cur);
	signal_arg->t_cur = signal_arg->t_aux = TIMESPEC_NANOSECONDS(signal_arg->ts_cur)/1000;
	signal_arg->t_ini = signal_arg->t_cur; // para calcular quantidade de pacotes até o próximo relógio
	ts_fim.tv_nsec = signal_arg->ts_cur.tv_nsec;
	ts_fim.tv_sec = signal_arg->ts_cur.tv_sec + UDP_TEST_TIME + EXTRA_TIME;
	signal_arg->t_stop = TIMESPEC_NANOSECONDS(ts_fim)/1000;
	t_next_sig = TIMESPEC_NANOSECONDS(signal_arg->ts_cur) + signal_arg->periodo_relogio * 1100;
	signal_arg->ts_cur.tv_sec = t_next_sig / 1000000000;
	signal_arg->ts_cur.tv_nsec = t_next_sig % 1000000000;
	pthread_mutex_lock (&signal_arg->mutex);
	pthread_cond_signal (&signal_arg->cond);
	pthread_mutex_unlock (&signal_arg->mutex);
	timer.it_value.tv_sec = timer.it_interval.tv_sec = signal_arg->periodo_relogio/1000000;
	timer.it_value.tv_usec = timer.it_interval.tv_usec = signal_arg->periodo_relogio%1000000;
	setitimer (ITIMER_REAL, &timer, NULL);


//	INFO_PRINT ("antes: %"PRUI64"%"PRUI64"\n", signal_arg.ts_cur.tv_sec, signal_arg.ts_cur.tv_nsec/1000);
//	INFO_PRINT ("depois: %"PRUI64"%"PRUI64"\n", signal_arg.ts_cur.tv_sec, signal_arg.ts_cur.tv_nsec/1000);

	while (1) {
		err = sigwait(signal_arg->mask, &signo);
		if (err != 0) {
			INFO_PRINT ("falha sigwait\n");
			saida (1);
		}
		switch (signo) {
			case SIGALRM:
// 				clock_gettime(CLOCK_MONOTONIC, &signal_arg->ts_cur);
				sigflag = 1;
				pthread_mutex_lock (&signal_arg->mutex);
// 				signal_arg->t_cur = TIMESPEC_NANOSECONDS(signal_arg->ts_cur)/1000;
// 				//INFO_PRINT ("tcur %"PRUI64"\n", signal_arg->t_cur);
//
 				if (signal_arg->t_cur >= signal_arg->t_stop) {
 					INFO_PRINT ("finalizando sinais udp upload\n");
					pthread_cond_signal (&signal_arg->cond);
 					pthread_mutex_unlock (&signal_arg->mutex);
 					timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
 					timer.it_value.tv_usec = timer.it_interval.tv_usec = 0;
 					setitimer (ITIMER_REAL, &timer, NULL);
 					INFO_PRINT ("pronto\n");
 					pthread_exit(NULL);
 				}
// 				if (signal_arg->t_cur - signal_arg->t_aux > 2500000) {
// 					INFO_PRINT ("aumentei vazao, envios %"PRUI64". previstos: %"PRUI64", tcur - taux: %"PRUI64", t_cur - t_ini: %"PRUI64", counter: %"PRUI64"\n", signal_arg->calculo_envios, signal_arg->envios_previstos, signal_arg->t_cur - signal_arg->t_aux, signal_arg->t_cur - signal_arg->t_ini, signal_arg->counter);
// 					//INFO_PRINT ("aumentei vazao, diff_t %"PRUI64", intervalo antes %lf, intervalo depois %lf, calculo_envios %"PRUI64", calculo_envios depois %lf", t_cur - t_ini, intervalo_em_usec, (intervalo_em_usec/1.05), calculo_envios, calculo_pacotes_enviados *1.05);
// //					signal_arg->intervalo_em_usec /= 1.05; // a cada 2,5s, aumenta a vazao em 5%
// //					timer.it_value.tv_usec = timer.it_interval.tv_usec = signal_arg->periodo_relogio = signal_arg->intervalo_em_usec;
// //					setitimer (ITIMER_REAL, &timer, NULL);
// 					signal_arg->calculo_envios *= 1.05; // para manter a proporção do cálculo de vazão UDP
// 					signal_arg->t_aux += 2500000;
// 					signal_arg->envios_previstos_por_burst *= 1.05;
// 				}
// 				t_next_sig = (uint64_t) TIMESPEC_NANOSECONDS(signal_arg->ts_cur) + signal_arg->periodo_relogio * 1000;
// 				signal_arg->envios_previstos = (uint64_t) ((t_next_sig /1000 - signal_arg->t_ini) * signal_arg->envios_previstos_por_burst/ signal_arg->intervalo_em_usec);
// 				//t_next_sig += 50000;
// 				signal_arg->ts_cur.tv_sec = (uint64_t) t_next_sig / 1000000000;
// 				signal_arg->ts_cur.tv_nsec = (uint64_t) t_next_sig % 1000000000;
				pthread_cond_signal (&signal_arg->cond);
				pthread_mutex_unlock (&signal_arg->mutex);
				break;

			default:
				INFO_PRINT("sinal inesperado %d\n", signo);
		}
	}
	pthread_mutex_lock (&signal_arg->mutex);
	pthread_cond_signal (&signal_arg->cond);
	pthread_mutex_unlock (&signal_arg->mutex);
	timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
	timer.it_value.tv_usec = timer.it_interval.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, NULL);
	pthread_exit (NULL);
}




int udp_upload_test(Context_t *context){
	struct udp_context_st *utx = context->data;
	socklen_t len;
	int fd, status, socket_number, max_buf_size = -1, buf_size, rc, i;
	uint64_t num_signals = 0, num_timeo = 0, t_next_sig;

	sigset_t mask;
	signal_thread_vazao_udp_upload_arg signal_arg;
	pthread_t sig_thread_id;
	pthread_condattr_t attr;
#ifdef MMSG
	int previsao_max_envios;
#else
	union packet_udp_upload packages;
#endif

	if (context->bytes_tcp_upload_test == 0)
		context->bytes_tcp_upload_test = (uint64_t)UDP_DEFAULT_RATE_IN_BITS_PER_SEC * (TCP_TOTAL_TIME_SEC + EXTRA_TIME)/8.0;

	for (socket_number = 0; socket_number < UDP_NUMBER_OF_SOCKETS; socket_number++) {
		len = sizeof(buf_size);
		if (getsockopt (utx->sockets[socket_number], SOL_SOCKET, SO_SNDBUF, &buf_size, &len) == 0) {
			INFO_PRINT ("Buffer UDP UP %d\n", buf_size);
			if (context->config->vazao_udp_upload_buffer_multiplier == 0)
				buf_size = BUFSIZ;
			else {
				buf_size *= context->config->vazao_udp_upload_buffer_multiplier;
				if (setsockopt(utx->sockets[socket_number], SOL_SOCKET, SO_SNDBUF, &buf_size, len) != 0)
					ERRNO_PRINT ("setsockopt SO_SNDBUF");
				INFO_PRINT ("setando Buffer UDP UP para %d\n", buf_size);
				getsockopt (utx->sockets[socket_number], SOL_SOCKET, SO_SNDBUF, &buf_size, &len);
			}
			if (buf_size > max_buf_size)
				max_buf_size = buf_size;
			INFO_PRINT ("Buffer UDP UP para %d\n", buf_size);
		}
		else {
			ERRNO_PRINT ("getsockopt");
		}
	}

	memset (&signal_arg, 0, sizeof(signal_arg));
	signal_arg.envios_previstos = MIN (context->config->vazao_udp_upload_max_package_burst, max_buf_size * (context->config->vazao_udp_upload_max_buffer_percentage/100.0) / UDP_PACKAGE_SIZE_IN_BYTES);
	do {
		signal_arg.intervalo_em_usec = (1e6 * (uint64_t) UDP_PACKAGE_SIZE_IN_BYTES * UDP_NUMBER_OF_SOCKETS * (TCP_TOTAL_TIME_SEC)) * signal_arg.envios_previstos/ (context->bytes_tcp_upload_test * context->config->vazao_udp_upload_tcp_avg_percentage/100.0); //intervalo em usec entre envio de envios_previstos em UDP_NUMBER_OF_SOCKETS pacotes
		if (signal_arg.intervalo_em_usec > 500000) {
			signal_arg.envios_previstos /= 2;
		}
	} while (signal_arg.intervalo_em_usec > 500000 && signal_arg.envios_previstos > 1);
	signal_arg.periodo_relogio = signal_arg.intervalo_em_usec;
	//signal_arg.envios_previstos = (signal_arg.periodo_relogio / signal_arg.intervalo_em_usec) + 1;

	signal_arg.envios_previstos_por_burst = signal_arg.envios_previstos;
	signal_arg.mask = &mask;
	pthread_condattr_init(&attr);
	pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	pthread_cond_init(&signal_arg.cond, &attr);
	pthread_mutex_init(&signal_arg.mutex, NULL);
#ifdef MMSG
	previsao_max_envios = signal_arg.envios_previstos * 1.5;
	union packet_udp_upload packages[previsao_max_envios];
	struct iovec msg1[previsao_max_envios];
	struct mmsghdr msg[previsao_max_envios];
#endif

	fd = open("/dev/urandom", O_RDONLY);
	if (fd != -1) {
#ifdef MMSG
		memset(msg, 0, sizeof(msg));
		memset(msg1, 0, sizeof (msg1));
		for (i = 0; i < previsao_max_envios; i++) {
			if (!read(fd, (void *)packages[i].buffer, UDP_PACKAGE_SIZE_IN_BYTES)) {
				INFO_PRINT ("nao foi possivel ler /dev/urandom\n");
				return -1;
			}
			msg1[i].iov_base = packages[i].buffer;
			msg1[i].iov_len = UDP_PACKAGE_SIZE_IN_BYTES;
			msg[i].msg_hdr.msg_iov = &msg1[i];
			msg[i].msg_hdr.msg_iovlen = 1;
		}
#else
		if (!read(fd, (void *)packages.buffer, UDP_PACKAGE_SIZE_IN_BYTES)) {
			INFO_PRINT ("nao foi possivel ler /dev/urandom\n");
			return -1;
		}
#endif
		close(fd);
	}

//////////
	// block SIGALRM
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGPIPE);


	pthread_mutex_lock (&signal_arg.mutex);
	status = meu_pthread_create(&sig_thread_id, 0, thread_trata_sinais_vazao_udp_upload, (void*)&signal_arg);
    if (status != 0) {
        ERROR_PRINT("can't create thread\n");
        saida (1);
    }
    pthread_cond_wait (&signal_arg.cond, &signal_arg.mutex);
	pthread_mutex_unlock (&signal_arg.mutex);
	INFO_PRINT ("liga timer udp upload, intervalo: %"PRUI64", envios_previstos: %"PRUI64", buf_size: %d\n", signal_arg.periodo_relogio, signal_arg.envios_previstos, max_buf_size);
///////


	status = 0;

	/*tokens para fazer um token bucket
	 * a cada iteracao sao adicionadas */





	do {
		//INFO_PRINT ("delta %lf - tokens %"PRI64, delta, tokens);
		//if (calculo_pacotes_enviados <= (TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_ini)) / intervalo_em_usec) {
		if (signal_arg.calculo_envios < signal_arg.envios_previstos) {
			// se o número de pacotes enviados for menor do que o cálculo da quantidade que deveria ser enviada num determinado instante ou não enviou por todos os sockets, então envia pacote
#ifdef MMSG

			for (i = 0; i < signal_arg.envios_previstos_por_burst; ++i) {
				packages[i].counter = htobe64(signal_arg.counter++);
/*				msg1[i].iov_base = packages[i].buffer;
				msg1[i].iov_len = UDP_PACKAGE_SIZE_IN_BYTES;
				msg[i].msg_hdr.msg_iov = &msg1[i];
				msg[i].msg_hdr.msg_iovlen = 1;
*/
			}

			for (socket_number = 0; socket_number < UDP_NUMBER_OF_SOCKETS; socket_number++) {
				status = sendmmsg(utx->sockets[socket_number], msg, signal_arg.envios_previstos_por_burst, 0);
				if (status != signal_arg.envios_previstos_por_burst) {
					ERRNO_PRINT ("mandei %d. esperado: %d\n", status, signal_arg.envios_previstos_por_burst);
				}
#else
			for (i = 0; i < signal_arg.envios_previstos_por_burst; ++i) {
				packages.counter = htobe64(signal_arg.counter++);
				for (socket_number = 0; socket_number < UDP_NUMBER_OF_SOCKETS; socket_number++) {
					//INFO_PRINT ("socket %d, buffer %s, UDP_PACKAGE_SIZE_IN_BYTES %d",utx->sockets[socket_number], packages[socket_number].buffer, UDP_PACKAGE_SIZE_IN_BYTES
					status = send(utx->sockets[socket_number], packages.buffer, UDP_PACKAGE_SIZE_IN_BYTES, 0);
					if (status != UDP_PACKAGE_SIZE_IN_BYTES) {
						ERRNO_PRINT ("mandei %d. esperado: %d\n", status, UDP_PACKAGE_SIZE_IN_BYTES);
						if (status < 0){
							ERROR_PRINT("fail send udp: status = %d. abortando\n", status);
							saida (1);
						}
					}
				}
#endif
			}
			signal_arg.calculo_envios += signal_arg.envios_previstos_por_burst;
		}
		// caso contrário (já enviou a quantidade de pacotes devidos num determinado instante e já colocou pacotes para envio em todos os sockets
		else if (sigflag == 1) {
				clock_gettime(CLOCK_MONOTONIC, &signal_arg.ts_cur);
				sigflag = 0;
				signal_arg.t_cur = TIMESPEC_NANOSECONDS(signal_arg.ts_cur)/1000;
				//INFO_PRINT ("tcur %"PRUI64"\n", signal_arg->t_cur);

				if (signal_arg.t_cur - signal_arg.t_aux > 2500000) {
					INFO_PRINT ("aumentei vazao, envios %"PRUI64". previstos: %"PRUI64", tcur - taux: %"PRUI64", t_cur - t_ini: %"PRUI64", counter: %"PRUI64"\n", signal_arg.calculo_envios, signal_arg.envios_previstos, signal_arg.t_cur - signal_arg.t_aux, signal_arg.t_cur - signal_arg.t_ini, signal_arg.counter);
					signal_arg.calculo_envios *= 1.05; // para manter a proporção do cálculo de vazão UDP
					signal_arg.t_aux += 2500000;
					signal_arg.envios_previstos_por_burst *= 1.05;
				}
				t_next_sig = (uint64_t) TIMESPEC_NANOSECONDS(signal_arg.ts_cur) + signal_arg.periodo_relogio * 1000;
				signal_arg.envios_previstos = (uint64_t) ((t_next_sig /1000 - signal_arg.t_ini) * signal_arg.envios_previstos_por_burst/ signal_arg.intervalo_em_usec);
				//t_next_sig += 50000;
				signal_arg.ts_cur.tv_sec = (uint64_t) t_next_sig / 1000000000;
				signal_arg.ts_cur.tv_nsec = (uint64_t) t_next_sig % 1000000000;
		}
		else {
			pthread_mutex_lock (&signal_arg.mutex);
			rc = pthread_cond_timedwait (&signal_arg.cond, &signal_arg.mutex, &signal_arg.ts_cur);
			//rc = pthread_cond_wait (&signal_arg.cond, &signa(i = 0; i < signal_arg.envios_previstos_por_burst; ++i) {l_arg.mutex);
			pthread_mutex_unlock (&signal_arg.mutex);
			if (rc == 0)
				num_signals++;
			else
				num_timeo++;
			//usleep (signal_arg.intervalo_em_usec);
		}
		//tv_aux.tv_sec = tv_cur.tv_sec;
		//tv_aux.tv_usec = tv_cur.tv_usec;

	//} while (timercmp(&tv_cur, & tv_stop_test, <));
	} while (signal_arg.t_cur < signal_arg.t_stop);
	INFO_PRINT ("desliga timer tcp upload, num_signals %"PRUI64", num_timeos %"PRUI64", counter %"PRUI64"\n", num_signals, num_timeo, signal_arg.counter);
	// colocar signal para garantir que a thread sinais feche depois desta linha
	pthread_join(sig_thread_id, NULL);
	pthread_mutex_destroy (&signal_arg.mutex);
	//gettimeofday(&tv_end, NULL);
	send_control_message(context->serverinfo, M_UDPUPLOADRESULT_SENDRESULTS);
	//    INFO_PRINT("Tcp Upload Test: End");

	//    for (i = 0; i < UDP_NUMBER_OF_SOCKETS; i++) {
	//        close (utx->sockets[i]);
	//    }
	return status;
}
