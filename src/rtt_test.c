/** ----------------------------------------
* @file  rtt_test.c
* @brief
* @author  Rafael de O. Lopes Goncalves
* @date Oct 31, 2011
*------------------------------------------*/

#include "config.h"

#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <limits.h>

#include "unp.h"
#include "rtt_test.h"
#include "udp_test.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "protocol.h"
#include "debug.h"
#include "results.h"
#include "assert.h"
#include "udp_test.h"
#include "messages.h"
#include "utils.h"


#define RTT_TEST_TOTAL_TIME_IN_SECONDS 6
#define INTERVAL_BETWEEN_PACKAGES_IN_MICROSECONDS  40000L
#define MAX_NUMBER_OF_COUNTERS ((RTT_TEST_TOTAL_TIME_IN_SECONDS * 1e6)/INTERVAL_BETWEEN_PACKAGES_IN_MICROSECONDS)
#define RTT_SOCKET_TIMEOUT_SECONDS 3
struct timeval RTT_SOCKET_TIMEOUT = {.tv_sec = 3,.tv_usec = 0 };
//struct udp_results_array rtt_results;

int rtt_test(Simet_server_info_t * serverinfo, FILE * output, int family)
{
	int rtt_socket;
	int status;
	struct timeval tv_cur;
	struct timeval tv_ini;

	struct timeval tv_stop_test;
	struct udp_result **results_array, **temp;
	struct udp_result *r_time_ref;
	struct udp_result *r_prev;
	struct udp_result *r;
	struct udp_results_time_array *time_array, *time_temp;
	uint32_t i, n;


	int64_t counter = 0;
	int64_t time_s = 0;
	int64_t extra = 0;
	struct udp_recv_counters_parameters_st param_r;
	struct udp_result *result = NULL;
	pthread_t reciever_thread;
	message_t result_message;

	INFO_PRINT("Rtt Test:Start");

	status = connect_udp(serverinfo->address_text, RTT_TEST_PORT, family, &rtt_socket);
	setsockopt(rtt_socket, SOL_SOCKET, SO_RCVTIMEO, &RTT_SOCKET_TIMEOUT,
			sizeof(RTT_SOCKET_TIMEOUT));
	param_r.socket = rtt_socket;
	param_r.max_counter = MAX_NUMBER_OF_COUNTERS;
	param_r.max_time = RTT_TEST_TOTAL_TIME_IN_SECONDS * 2;

	if(status < 0) {
		TRACE_PRINT("status = %d\n", status);
		return status;
	}
	meu_pthread_create(&reciever_thread, PTHREAD_STACK_MIN * 2, udp_receive_counters_callback, &param_r);
	gettimeofday(&tv_ini, NULL);
	tv_stop_test.tv_usec = tv_ini.tv_usec;
	tv_stop_test.tv_sec = tv_ini.tv_sec + RTT_TEST_TOTAL_TIME_IN_SECONDS;

	do {

		gettimeofday(&tv_cur, NULL);
		time_s = (tv_cur.tv_sec * 1e6) + tv_cur.tv_usec;
		send_udp(rtt_socket, counter, time_s, extra);
		counter++;

		usleep(INTERVAL_BETWEEN_PACKAGES_IN_MICROSECONDS);
	} while(timercmp(&tv_cur, & tv_stop_test, <));
	param_r.max_counter = counter - 1;
	pthread_join(reciever_thread, (void *)&result);
	gettimeofday(&tv_cur, NULL);

	r_time_ref = r = r_prev = result;
	i = n = 0;
	results_array = malloc (sizeof (struct udp_result*) * NUM_SLICES);
	if (!results_array)
		ERROR_PRINT ("faltou memoria alocacao array de resultados (1)");
	else memset (results_array, 0, sizeof (struct udp_result*) * NUM_SLICES);
	time_array = malloc (sizeof (struct udp_results_time_array) * NUM_SLICES);
	if (!time_array)
		ERROR_PRINT ("faltou memoria alocacao array de temp (1)");


	if (r) {
		results_array[0] = r;
		time_array[0].t_ini = r->time;
	}
	while (r)
	{
		if (r->time - r_time_ref->time <= TIME_SLICE) {
			r_prev = r;
			r = r->next;
		}
		else
		{
			i++;
			if (i % NUM_SLICES == 0)
			{
				temp = realloc (results_array, sizeof (struct udp_result*) * (i + NUM_SLICES));
				if (!temp)
				ERROR_PRINT ("faltou memoria realocacao array de resultados (2)");
				results_array = temp;
				time_temp = realloc (time_array, sizeof (struct udp_results_time_array) * (i + NUM_SLICES));
				if (!time_temp)
				ERROR_PRINT ("faltou memoria realocacao array de tempos (2)");
				time_array = time_temp;
			}
			time_array[i-1].t_fim = r_prev->time_recv;
			time_array[i].t_ini = r->time;
			r_prev->next = NULL;
			results_array[i] = r_time_ref = r_prev = r;
			r = r->next;
		}
		n = i + 1;
	}
	if (results_array [i])
		time_array[i].t_fim = r_prev->time_recv;



	for (i = 0; i < n; i++)
	{
		result_message.field = "SIMET_COMMONS_RESULT_RTT";
		char * result_string = result_string_rtt(udp_process_results_rtt(&results_array[i], 1, time_array[i].t_ini, time_array[i].t_fim, serverinfo->clockoffset_in_usec));
		TRACE_PRINT("serverinfo->clockoffset_in_usec = %"PRI64, serverinfo->clockoffset_in_usec);
		result_message.content =  result_string;

		store_results(output, result_message.field, result_message.content);
		TRACE_PRINT("Rtt Counter: %"PRI64, counter);
		send_control_message(serverinfo, result_message);
		free(result_string);
	}
	close(rtt_socket);
//	rtt_results.results_array = results_array;
//	rtt_results.time_array = time_array;
//	rtt_results.n = n;
	free (time_array);
    free (results_array);
	INFO_PRINT("Rtt Test:End\n");
	return 1;
}





















