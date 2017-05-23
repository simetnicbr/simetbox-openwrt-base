/** ----------------------------------------
 * @file  tcp_test.c
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 7, 2011
 *------------------------------------------*/

#include "config.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <limits.h>
#ifndef SELECT
#include <poll.h>
#endif
#include "tcp_test.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "debug.h"
#include "unp.h"
#include "results.h"
#include "protocol.h"
#include "messages.h"
#include "utils.h"
static const message_t M_TCPDOWNLOADRESULT_DONE = { 60, "TCPDOWNLOADRESULT",
		"DONE" };
#define EXTRA_TIME 1
#define BLOCOS ((TCP_TOTAL_TIME_SEC + EXTRA_TIME) * 2)

struct tcp_download_test_args {
	int socket;
//	int64_t t_inicio;
	int64_t number_of_packages[BLOCOS]; // blocos de 500ms
	int64_t number_of_bytes[BLOCOS]; // blocos de 500ms
	int64_t t_ini[BLOCOS]; // blocos de 500ms
	int64_t t_fim[BLOCOS]; // blocos de 500ms

};

struct tcp_upload_test_args {
	int socket;
	char *buffer;
//	int64_t total_bytes;
};

int request_socket(Context_t *context) {
	struct tcp_context_st *tcp_ctxt = context->data;
	connect_tcp(context->serverinfo->address_text, TCP_TEST_PORT, context->family,
			&(tcp_ctxt->sockets[tcp_ctxt->sockets_created]));
	return tcp_ctxt->sockets_created;
}


/*
static void *download_tcp_thread(void *pargs) {

	struct tcp_download_test_args *args = pargs;
	struct timeval tv_cur;
	struct timeval tv_ini;
	struct timeval timeout_tv, timeout_tv_select;
	struct timeval tv_stop_test;
	int64_t bytes_received = 0;
	int64_t bytes_received_temp = 0;
	int64_t t_cur, t_ini;
	int32_t indice_bloco_resultado = -1;
    int32_t res;
    fd_set fds;
	timeout_tv.tv_sec = 0;
	timeout_tv.tv_usec = 1;
	timeout_tv_select.tv_sec = 0;
	timeout_tv_select.tv_usec = 100;
	setsockopt(args->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout_tv,
			sizeof(timeout_tv));

	char buf[TCP_PACKAGE_SIZE];
    gettimeofday(&tv_ini, NULL);

	t_ini = TIMEVAL_MICROSECONDS(tv_ini);
	tv_stop_test.tv_usec = tv_ini.tv_usec;
	tv_stop_test.tv_sec = tv_ini.tv_sec + TCP_TOTAL_TIME_SEC + EXTRA_TIME;
    indice_bloco_resultado = 0;

	do {
	    bytes_received = 0;
	    do {
    		bytes_received_temp = recv(args->socket, &buf[bytes_received], sizeof(buf) - bytes_received , 0);
    		if (bytes_received_temp <= 0)
    		{
				if (errno == EAGAIN || errno == EINTR)
				{
				    do
				    {
				        FD_ZERO(&fds);
				        FD_SET(args->socket, &fds);
				        res = select (FD_SETSIZE, NULL, &fds, NULL, &timeout_tv_select);
				        if (res > 0)
    				        break;
				        else if (res < 0)
				        {
				            if (errno == EINTR)
    				        { // recebeu um sinal
            				    ERROR_PRINT("EINTR - recebeu um sinal");
            				}
				        }
				        else if (res == 0) // timeout
           				    ERROR_PRINT("TIMEOUT");
				    } while (1);
				}
				else
				    ERROR_PRINT("%d", errno);
			}
			else
			{
        		bytes_received += bytes_received_temp;
        	}
        }
		while (bytes_received != TCP_PACKAGE_SIZE);
   		gettimeofday(&tv_cur, NULL);
		if (bytes_received_temp >= 0) {
		    t_cur = TIMEVAL_MICROSECONDS(tv_cur);
            for (; (indice_bloco_resultado  < BLOCOS ) && (t_cur > args->t_fim[indice_bloco_resultado]); indice_bloco_resultado++);
//    		INFO_PRINT("bloco_res %d - bloco anterior %d - ", indice_bloco_resultado, indice_bloco_anterior);

			if (indice_bloco_resultado < BLOCOS) {
    			args->number_of_bytes[indice_bloco_resultado] += bytes_received + 48; // tcp + ip headers;
	    		args->number_of_packages[indice_bloco_resultado]++;
//        	    INFO_PRINT("socket %d bytesrecv=%"PRISIZ" pacotesrecv=%"PRISIZ"\n", args->socket, args->number_of_bytes[indice_bloco_resultado], args->number_of_packages[indice_bloco_resultado]);
	    	}
//    		INFO_PRINT("args %"PRISIZ" - tcur %"PRISIZ"\n", args->t_ini[indice_bloco_resultado], t_cur);
#endif
		}


	} while (timercmp(&tv_cur, & tv_stop_test, <));

	return pargs;
}
*/


int tcp_download_test(Context_t *context) {
	TRACE_PRINT("tcp_download_test");
/*	pthread_t receiver_thread[TCP_NUMBER_OF_SOCKETS];
	struct tcp_download_test_args args[TCP_NUMBER_OF_SOCKETS];
	struct tcp_context_st *tcp_context = context->data;

	struct timeval tv_ini;
	struct timeval tv_end;
	int64_t total_time = 0;
	int64_t t_ini_usec;
	int64_t t_ini [BLOCOS];
	int64_t t_fim [BLOCOS];
	int64_t total_packages [BLOCOS] = {0};
	int64_t total_bytes [BLOCOS] = {0};

	int i, j;
	int status;

	struct result_st result;
	struct message_st message;
	TRACE_PRINT("Tcp Download Test: Start");
	gettimeofday(&tv_ini, NULL);
	t_ini_usec = TIMEVAL_MICROSECONDS(tv_ini);
	memset (args, 0, sizeof (args));
	for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
        for (j = 0; j < BLOCOS; j++) {
   	    	t_ini[j] = args[i].t_ini[j] = t_ini_usec + j * 500000;
  	    	t_fim[j] = args[i].t_fim[j] = args[i].t_ini[j] + 500000;
   	    }

   	   	args[i].socket = tcp_context->sockets[i];
		pthread_create(&(receiver_thread[i]), NULL, download_tcp_thread,
				&(args[i]));
	}

	TRACE_PRINT("Tcp Download Test: join");
	for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
		pthread_join(receiver_thread[i], (void *) &(args[i]));
        for (j = 0; j < BLOCOS; j++) {
    		total_bytes[j] += args[i].number_of_bytes[j];
	    	total_packages[j] += args[i].number_of_packages[j];
	    }
	}
*/

	struct tcp_context_st *tcp_context = context->data;
	struct result_st result;
//	struct timeval init_tv, tv_end, tv_stop_test, tv_cur;
	struct timespec ts_ini;
//	int64_t t_cur;
//	int64_t total_time;
	int64_t t_ini_usec;
	int64_t t_ini [BLOCOS+1];
	int64_t t_fim [BLOCOS+1];
	int64_t total_packages [BLOCOS+1] = {0};
	int64_t total_bytes [BLOCOS+1] = {0};
	int i, j, length, status, buf_size, max_buf_size, lowat, state_nodelay = 1;
	int32_t bytes_recv;
	socklen_t len;
	struct message_st message;
	char *buffer;
	sigset_t mask;
	signal_thread_vazao_arg signal_arg;
	pthread_t sig_thread_id;
	struct itimerval timer;

	TRACE_PRINT("Tcp Download Test: Start");
#ifdef SELECT
	int maxfdp1 = -1;
    fd_set rset, rset_master;
#else
	struct pollfd pfds[TCP_NUMBER_OF_SOCKETS];
#endif
	int32_t  flags;

	signal_arg.mask = &mask;
	signal_arg.max_blocos = BLOCOS;
	signal_arg.indice_bloco = 0;
	signal_arg.timer= &timer;
	pthread_mutex_init(&signal_arg.mutex, NULL);
	pthread_cond_init(&signal_arg.cond, NULL);

//	tv_stop_test.tv_usec = init_tv.tv_usec;
//	tv_stop_test.tv_sec = init_tv.tv_sec +  TCP_TOTAL_TIME_SEC + EXTRA_TIME;

    length = TCP_PACKAGE_SIZE;
    max_buf_size = -1;
#ifdef SELECT
    FD_ZERO (&rset_master);
#endif
//////////
	// block SIGALRM
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGPIPE);




	status = meu_pthread_create(&sig_thread_id, PTHREAD_STACK_MIN * 2, thread_trata_sinais_vazao, (void*)&signal_arg);
    if (status != 0) {
        ERROR_PRINT("can't create thread\n");
        saida (1);
    }
///////
    for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
        flags = fcntl (tcp_context->sockets[i], F_GETFL, 0);
        fcntl (tcp_context->sockets[i], F_SETFL,flags | O_NONBLOCK);
		if (context->config->vazao_tcp_nodelay)
			if (setsockopt(tcp_context->sockets[i], IPPROTO_TCP, TCP_NODELAY, &state_nodelay, sizeof(state_nodelay)) != 0)
				ERRNO_PRINT ("setsockopt TCP_NODELAY");
        status = setsockopt(tcp_context->sockets[i], SOL_SOCKET, SO_RCVLOWAT, (char *)&length,
 sizeof(length));
/*        if (status < 0)
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
*/
		len= sizeof(buf_size);
#ifdef SELECT
		FD_SET (tcp_context->sockets[i], &rset_master);
/*		if (getsockopt (tcp_context->sockets[i], SOL_SOCKET, SO_RCVBUF, &buf_size, &len) == 0) {
			if (buf_size > max_buf_size)
				max_buf_size = buf_size;
		}
		else ERRNO_PRINT ("getsockopt");
*/
		if (tcp_context->sockets[i] > maxfdp1) {
			maxfdp1 = tcp_context->sockets[i];
		}
#else
		pfds[i].fd = tcp_context->sockets[i];
		pfds[i].events = POLLIN;
#endif

		len = sizeof(buf_size);
		if (getsockopt (tcp_context->sockets[i], SOL_SOCKET, SO_RCVBUF, &buf_size, &len) == 0) {
			if (context->config->vazao_tcp_download_buffer_multiplier == 0)
				buf_size = BUFSIZ;
			else {
				buf_size *= context->config->vazao_tcp_download_buffer_multiplier;
				if (setsockopt(tcp_context->sockets[i], SOL_SOCKET, SO_RCVBUF, &buf_size, len) != 0)
					ERRNO_PRINT ("setsockopt SO_RCVBUF");
				getsockopt (tcp_context->sockets[i], SOL_SOCKET, SO_RCVBUF, &buf_size, &len);
			}
			if (buf_size > max_buf_size)
				max_buf_size = buf_size;
			lowat = context->config->vazao_tcp_download_lowat_value;
			if (setsockopt(tcp_context->sockets[i], SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat)) != 0)
				ERRNO_PRINT ("setsockopt SO_RCVLOWAT");
			len = sizeof(lowat);
			getsockopt(tcp_context->sockets[i], SOL_SOCKET, SO_RCVLOWAT, &lowat, &len);
		}
		else {
			ERRNO_PRINT ("getsockopt");
		}
	}
	length = max_buf_size;
	buffer = (char*) malloc (length);
	if (!buffer) {
		ERROR_PRINT ("aloca buffer\n");
		saida (1);
	}

	INFO_PRINT ("lowat: %d, tam_buffer download: %d%s\n", lowat, length, length==BUFSIZ?" (BUFSIZ)":"");
	clock_gettime(CLOCK_MONOTONIC, &ts_ini);
	t_ini_usec = TIMESPEC_NANOSECONDS(ts_ini)/1000;

	for (j = 0; j < BLOCOS + 1; j++) {
		t_ini[j] = t_ini_usec + j * 500000;
		t_fim[j] = t_ini[j] + 500000;
//       	INFO_PRINT ("tni j %"PRI64", tfim j %"PRI64, t_ini[j], t_fim[j]);
	}
#ifdef SELECT
	memcpy(&rset, &rset_master, sizeof(rset_master));
    size_fdset = maxfdp1/8 + 1;
#endif
	// timer gerador de SIGALRM
	INFO_PRINT ("liga timer tcp download");
	timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
	timer.it_value.tv_usec = timer.it_interval.tv_usec = 500000;
	setitimer (ITIMER_REAL, &timer, NULL);

	do {
#ifdef SELECT
		memcpy(&rset, &rset_master, size_fdset);
		tv_timeo.tv_sec = 3;
		tv_timeo.tv_usec = 0; // select zera o timeout

		if (select (maxfdp1 + 1, &rset, NULL, NULL, &tv_timeo) <= 0)
#else
		if (poll(pfds, TCP_NUMBER_OF_SOCKETS, 3000) < 0)
#endif
			continue;
//        gettimeofday(&tv_cur, NULL);
//        INFO_PRINT ("tempo entre recvs: %.0Lf, tempo do recv: %.0Lf, bytes recv %d", TIMEVAL_MICROSECONDS(tv_cur) - t_cur, TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_aux), bytes_recv);
//		t_cur = TIMEVAL_MICROSECONDS(tv_cur);
		for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
#ifdef SELECT
			if (FD_ISSET(tcp_context->sockets[i], &rset)) {
#else
			if (pfds[i].revents & POLLIN) {
#endif
				while (((bytes_recv = recv(tcp_context->sockets[i], buffer, length, 0)) > 0) && (signal_arg.indice_bloco < BLOCOS)) {
//				if (bytes_recv == 0) {
//					ERROR_PRINT ("desconectou antes do fim do teste\n");
//					saida (1);
//				}
//                if (bytes_recv != length) INFO_PRINT ("recebi :%d", bytes_recv);
//					if (signal_arg.indice_bloco < BLOCOS) {
					total_bytes[signal_arg.indice_bloco] += bytes_recv;
	//            	    INFO_PRINT("socket %d bytesrecv=%"PRISIZ" pacotesrecv=%"PRISIZ"\n", args->socket, args->number_of_bytes[signal_arg.indice_bloco], args->number_of_packages[signal_arg.indice_bloco]);
//					}
//					else
//						break;
                }
	    	}
	    }
//    } while (timercmp(&tv_cur, & tv_stop_test, <));
    } while (signal_arg.indice_bloco < BLOCOS);
	for (i = 0; i < BLOCOS; i++) {
		total_packages[i] = total_bytes[i]/TCP_PACKAGE_SIZE;
	}
	// timer gerador de SIGALRM
//	INFO_PRINT ("desliga timer tcp download\n");
//	timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
//	timer.it_value.tv_usec = timer.it_interval.tv_usec = 0;
//	setitimer (ITIMER_REAL, &timer, NULL);
	pthread_mutex_lock (&signal_arg.mutex);
	pthread_cond_signal (&signal_arg.cond);
	pthread_mutex_unlock (&signal_arg.mutex);
	pthread_join(sig_thread_id, NULL);

	pthread_mutex_destroy (&signal_arg.mutex);
	pthread_cond_destroy (&signal_arg.cond);

	TRACE_PRINT("Tcp Download Test: close");
//	gettimeofday(&tv_end, NULL);
	for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
		close_tcp(tcp_context->sockets[i]);
	}


	/* Refatorar para uma funcao separada */

	result.direction = 'D';
    message.field = "SIMET_COMMONS_RESULT_TCP";
    for (i = EXTRA_TIME; i < BLOCOS - EXTRA_TIME; i++) {
    	//total_time = t_fim[i] - t_ini[i];

    	result.event_start_time_in_us = t_ini[i];
	    result.event_end_time_in_us = t_fim[i];
    	result.number_of_packages = total_packages[i];
  	    result.package_size_in_bytes = 1400; //total_bytes[i] / total_packages[i];
    	result.number_of_bytes = total_bytes[i];
	    result.time_in_us = t_fim[i] - t_ini[i];

    	char * result_string = result_string_tcp(result);
	    message.content = result_string;

//	    INFO_PRINT("\n\nTcp Download Test: packets=%"PRISIZ"\t bytes=%"PRISIZ"\ttotal_time=%"PRI64"\nTcp Download Test: tcp download rate:%f kbps \n\n", total_packages[i], total_bytes[i], total_time, (total_bytes[i] * 8e3) / total_time);
    	store_results(context->output, message.field, message.content);
    	status = send_control_message(context->serverinfo, message);
    	if (status < 0) {

	    	ERROR_PRINT(
				"Error sending message %s=%s", message.field, message.content);
	    	return status;
	    }
	    free(result_string);
	}
   	status = send_control_message(context->serverinfo, M_TCPDOWNLOADRESULT_DONE);
    if (status < 0) {

    	ERROR_PRINT(
			"Error sending message %s=%s", M_TCPDOWNLOADRESULT_DONE.field, M_TCPDOWNLOADRESULT_DONE.content);
       	return status;
    }
	TRACE_PRINT("Tcp Download Test: End\n");

	return i;
}

static void fill_buffer_with_random(char *buffer, int len) {
	int status;
	int randfd = open("/dev/urandom", O_RDONLY);
	status = read(randfd, buffer, len);
	if (status < 0) {
		ERROR_PRINT("Error generating random buffer");
		abort();
	}
	close(randfd);
}

void *upload_tcp_thread(void *pargs) {

	int status;
	struct tcp_upload_test_args *args = pargs;
	TRACE_PRINT("upload_tcp_thread %d", args->socket);

	struct timeval tv_ini;
	struct timeval tv_cur;
	struct timeval tv_stop_test;

	gettimeofday(&tv_ini, NULL);
	tv_stop_test.tv_usec = tv_ini.tv_usec;
	tv_stop_test.tv_sec = tv_ini.tv_sec + TCP_TOTAL_TIME_SEC;
	do {
		status = gettimeofday(&tv_cur, NULL);
		if (status < 0) {
			ERROR_PRINT(
					"Error getting time=%ld", tv_cur.tv_sec - tv_ini.tv_sec);
		}
		status = write(args->socket, args->buffer, TCP_PACKAGE_SIZE);
		if (status < 0) {
			ERROR_PRINT(
					"Error writing socket upload to tcp time=%ld", tv_cur.tv_sec - tv_ini.tv_sec);
		}
	} while (timercmp(&tv_cur, & tv_stop_test, <));
	return pargs;
}

int tcp_upload_test(Context_t *context) {
	TRACE_PRINT("tcp_upload_test()");
/*	pthread_t sender_thread[TCP_NUMBER_OF_SOCKETS];
	struct tcp_upload_test_args args[TCP_NUMBER_OF_SOCKETS];
	struct tcp_context_st *tcp_context = context->data;

	struct timeval tv_ini;
	struct timeval tv_end;
//	int64_t total_bytes = 0;

	int i;
	int status = 0;
	context->bytes_tcp_upload_test = 0;
	char buffer[TCP_PACKAGE_SIZE];
	TRACE_PRINT("Tcp Upload Test: Start");


	fill_buffer_with_random(buffer);

	gettimeofday(&tv_ini, NULL);
	for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
		args[i].socket = tcp_context->sockets[i];
		args[i].buffer = buffer;
//		args[i].total_bytes = 0;
		pthread_create(&(sender_thread[i]), NULL, upload_tcp_thread,
				&(args[i]));
	}
	for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
		pthread_join(sender_thread[i], (void *) &(args[i]));
//		total_bytes += args[i].total_bytes;
//        INFO_PRINT ("thread %d, bytes thread %"PRI64", total_bytes %"PRI64, i, args[i].total_bytes, total_bytes);
	}
//	context->max_rate_in_bps = 8 * total_bytes / TCP_TOTAL_TIME_SEC;
//    INFO_PRINT ("rate %"PRI64", total bytes %"PRI64, context->max_rate_in_bps, total_bytes);

	gettimeofday(&tv_end, NULL);
	send_control_message(context->serverinfo, M_TCPUPLOADRESULT_SENDRESULTS);
	TRACE_PRINT("Tcp Upload Test: End");
	return status;
*/
	int32_t i, flags, status;
//	int64_t t_cur, t_fim;
	int length, lowat = 0, free_buff, occupied_buff, buf_size, max_buf_size, state_cork = 1, state_nodelay = 1;
	socklen_t len;
	char *buffer;
//    struct timeval tv_stop_test;
#ifdef SELECT
	int maxfdp1 = -1;
	fd_set wset, wset_master;
#else
	struct pollfd pfds[TCP_NUMBER_OF_SOCKETS];
#endif
	struct tcp_context_st *tcp_context = context->data;
//    int64_t quantidadepkt[TCP_NUMBER_OF_SOCKETS] = {0};
	sigset_t mask;
	signal_thread_vazao_arg signal_arg;
	pthread_t sig_thread_id;
	struct itimerval timer;

	signal_arg.mask = &mask;
	signal_arg.max_blocos = BLOCOS;
	signal_arg.indice_bloco = 0;
	signal_arg.timer = &timer;
	pthread_mutex_init(&signal_arg.mutex, NULL);
	pthread_cond_init(&signal_arg.cond, NULL);

	context->bytes_tcp_upload_test = 0;


#ifdef SELECT
    FD_ZERO (&wset_master);
#endif
    max_buf_size = -1;
//////////
	// block SIGALRM
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGPIPE);


	status = meu_pthread_create(&sig_thread_id, PTHREAD_STACK_MIN * 2, thread_trata_sinais_vazao, (void*)&signal_arg);
    if (status != 0) {
        ERROR_PRINT("can't create thread\n");
        saida (1);
    }
///////
    for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
        flags = fcntl (tcp_context->sockets[i], F_GETFL, 0);
        fcntl (tcp_context->sockets[i], F_SETFL,flags | O_NONBLOCK);
#ifdef SELECT
        FD_SET (tcp_context->sockets[i], &wset_master);
		if (tcp_context->sockets[i] > maxfdp1) {
            maxfdp1 = tcp_context->sockets[i];
        }
#else
		pfds[i].fd = tcp_context->sockets[i];
		pfds[i].events = POLLOUT;
#endif
		len = sizeof(buf_size);
		if (context->config->vazao_tcp_cork)
			if (setsockopt(tcp_context->sockets[i], IPPROTO_TCP, TCP_CORK, &state_cork, sizeof(state_cork)) != 0)
				ERRNO_PRINT ("setsockopt TCP_CORK");
		if (context->config->vazao_tcp_nodelay)
			if (setsockopt(tcp_context->sockets[i], IPPROTO_TCP, TCP_NODELAY, &state_nodelay, sizeof(state_nodelay)) != 0)
				ERRNO_PRINT ("setsockopt TCP_NODELAY");
		if (getsockopt (tcp_context->sockets[i], SOL_SOCKET, SO_SNDBUF, &buf_size, &len) == 0) {
			if (context->config->vazao_tcp_upload_buffer_multiplier == 0)
				buf_size = BUFSIZ;
			else {
				buf_size *= context->config->vazao_tcp_upload_buffer_multiplier;
				if (setsockopt(tcp_context->sockets[i], SOL_SOCKET, SO_SNDBUF, &buf_size, len) != 0)
					ERRNO_PRINT ("setsockopt SO_SNDBUF");
				getsockopt (tcp_context->sockets[i], SOL_SOCKET, SO_SNDBUF, &buf_size, &len);
			}
			if (buf_size > max_buf_size)
				max_buf_size = buf_size;
			lowat = buf_size * ((float)context->config->vazao_tcp_upload_lowat_percentage / 100);
// inicio sem SIOCOUTQ
			if (setsockopt(tcp_context->sockets[i], SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat)) != 0)
				ERRNO_PRINT ("setsockopt SO_SNDLOWAT");
			len = sizeof(lowat);
			getsockopt(tcp_context->sockets[i], SOL_SOCKET, SO_SNDLOWAT, &lowat, &len);
			INFO_PRINT ("lowat: %d\n", lowat);
// fim sem SIOCOUTQ
		}
		else {
			ERRNO_PRINT ("getsockopt");
		}

    }
#ifdef SELECT
    size_fdset = maxfdp1/8 + 1;
	memcpy(&wset, &wset_master, sizeof(wset_master));
#endif
//	length = max_buf_size;
// ini sem SIOCOUTQ
	length = TCP_PACKAGE_SIZE;
// fim sem SIOCOUTQ

//	INFO_PRINT ("lowat: %d, tam_buffer upload: %d\n", lowat, length);
	INFO_PRINT ("tam_buffer upload: %d\n", length);
	buffer = (char*) malloc (length);
	if (!buffer) {
		ERROR_PRINT ("aloca buffer\n");
		saida (1);
	}
	fill_buffer_with_random(buffer, length);
	// timer gerador de SIGALRM
	INFO_PRINT ("liga timer tcp upload");
	timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
	timer.it_value.tv_usec = timer.it_interval.tv_usec = 500000;
	setitimer (ITIMER_REAL, &timer, NULL);
    do {
//		for (j = 0; j < 5; j++) {
		// este for foi colocado aqui para evitar desperdiçar muita CPU obtendo o relógio do kernel para focar a CPU no envio de pacotes
		// envia cinco conjuntos de pacotes para uma checagem de relógio
//		memcpy(&wset, &wset_master, sizeof(wset_master));
//        while (select (maxfdp1 + 1, NULL, &wset, NULL, NULL) <= 0);
#ifdef SELECT
		memcpy(&wset, &wset_master, size_fdset);
		if (select (maxfdp1 + 1, NULL, &wset, NULL, NULL) <= 0)
#else
		if (poll(pfds, TCP_NUMBER_OF_SOCKETS, -1) < 0)
#endif
			continue;
//        INFO_PRINT ("tempo entre recvs: %.0Lf, tempo do recv: %.0Lf, bytes recv %d", TIMEVAL_MICROSECONDS(tv_cur) - t_cur, TIMEVAL_MICROSECONDS(tv_cur) - TIMEVAL_MICROSECONDS(tv_aux), bytes_recv);
		for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
#ifdef SELECT
			if (FD_ISSET(tcp_context->sockets[i], &wset)) {
#else
			if (pfds[i].revents & POLLOUT) {
#endif
// 				if (ioctl(tcp_context->sockets[i], SIOCOUTQ, &occupied_buff) < 0) {
// 					ERRNO_PRINT ("ioctl");
// 					saida (1);
// 				}
//
// 				if ((free_buff = max_buf_size - occupied_buff) < lowat)
// 					continue;
// 				status = write(tcp_context->sockets[i], buffer, free_buff);
// inicio sem SIOCOUTQ
				while (send(tcp_context->sockets[i], buffer, TCP_PACKAGE_SIZE, 0) > 0);
// fim sem SIOCOUTQ

//         		if (status < TCP_PACKAGE_SIZE)
//         		    INFO_PRINT ("enviou tamanho menor");
//      		quantidadepkt [i]++;
			}
		}
//		}
//    } while (timercmp(&tv_cur, &tv_stop_test, <));
    } while (signal_arg.indice_bloco < BLOCOS);
	// timer gerador de SIGALRM
//	INFO_PRINT ("desliga timer tcp upload\n");
//	timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
//	timer.it_value.tv_usec = timer.it_interval.tv_usec = 0;
//	setitimer (ITIMER_REAL, &timer, NULL);
	pthread_mutex_lock (&signal_arg.mutex);
	pthread_cond_signal (&signal_arg.cond);
	pthread_mutex_unlock (&signal_arg.mutex);
	pthread_join(sig_thread_id, NULL);

	pthread_mutex_destroy (&signal_arg.mutex);
	pthread_cond_destroy (&signal_arg.cond);
	if (context->config->vazao_tcp_cork) {
		state_cork = 0;
		for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++)
			if (setsockopt(tcp_context->sockets[i], IPPROTO_TCP, TCP_CORK, &state_cork, sizeof(state_cork)) != 0)
				ERRNO_PRINT ("setsockopt TCP_CORK");
	}
/*    for ( i = 0; i < TCP_NUMBER_OF_SOCKETS; i++)
    {
        INFO_PRINT ("quantidade de pacotes: %"PRI64, quantidadepkt[i]);
    }
*/
//    gettimeofday(&tv_end, NULL);
	send_control_message(context->serverinfo, M_TCPUPLOADRESULT_SENDRESULTS);
	TRACE_PRINT("Tcp Upload Test: End");
	return 0;
}
