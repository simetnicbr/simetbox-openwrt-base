/** ----------------------------------------
 * @file  jitter_test.c
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 28, 2011
 *------------------------------------------*/

#include "config.h"

#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include "unp.h"
#include "jitter_test.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "udp_test.h"
#include "results.h"
#include "messages.h"
#include "rtt_test.h"


#define JITTER_TEST_TIME_IN_US 6000000
#define IGNORE_FIRST_N_PACKAGES_JITTER 10
static const uint32_t  INTERVAL_BETWEEN_PACKAGES = 40000;
//extern struct udp_results_array rtt_results;



int jitter_create_jitter_context(Context_t * context, enum test_direction direction) {
    struct Jitter_context_st *jc;

    if (context->data == NULL) {
        jc = malloc(sizeof(struct Jitter_context_st));
        TRACE_PRINT(">>>>> create context");
        context->data = jc;
        connect_udp(context->serverinfo->address_text, JITTER_PORT, context->family, &(jc->socket));
	}
	else jc = context->data;
    jc->direction = direction;
    TRACE_PRINT("<<<<< create context socket %d", jc->socket);
    return 1;
}


int jitter_create_socket_confirmation(Context_t * context) {
	 struct Jitter_context_st *jc;
	jc = context->data;
    send_udp(jc->socket, (int64_t) INT64_MAX, 0, 0);

    return 1;
}

int jitter_upload_test(Context_t * context) {
    struct timeval tv;
    int64_t ini, cur;
    uint16_t counter = 0;
    struct Jitter_context_st *jc = context->data;
	INFO_PRINT ("jitter upload start");

    gettimeofday(&tv, NULL);
    ini = TIMEVAL_MICROSECONDS(tv);
    cur = ini;
    while (cur - ini < JITTER_TEST_TIME_IN_US){
        send_rtp(jc->socket, counter, cur-ini);
        counter++;
        usleep(INTERVAL_BETWEEN_PACKAGES);
        gettimeofday(&tv, NULL);
        cur = TIMEVAL_MICROSECONDS(tv);
    }
//    close(jc->socket);
//    free(jc);
    send_control_message(context->serverinfo, M_JITTERUPLOADRESULT_SENDRESULTS);
	INFO_PRINT ("jitter upload finish");
    return 1;
}

static Result_t jitter_process_results(struct udp_result * u_result, uint64_t tv_ini, uint64_t tv_end, int64_t clock_offset_in_us, enum test_direction direction, int64_t t_ini_jitter);

/*
 *
 * meu
 *
 * int jitter_download_test(Context_t * context) {

    struct udp_result * ur;
    char * tempstring;
    uint32_t i;
	union packetudp_u pac;
	int bytes_recv, length = sizeof (pac);

    struct Jitter_context_st *jc = context->data;

    for (i = 0; i < rtt_results.n; i++) {
        ur = rtt_results.results_array[i];
        Result_t result = jitter_process_results(ur, rtt_results.time_array[i].t_ini, rtt_results.time_array[i].t_fim, context->serverinfo->clockoffset_in_usec, DOWNLOAD);
        result.direction = DOWNLOAD;
        message_t m = M_JITTERRESULT;
        m.content = tempstring = result_string_jitter(result);
        store_results(context->output, m.field, m.content);
        send_control_message(context->serverinfo, m);
		free (m.content);
    }
    jitter_create_socket_confirmation (context);
	INFO_PRINT ("recebe pac jitter down\n");
	for (i = 0; i < 100; i++) {
		bytes_recv = recv(jc->socket, &pac, length, 0);
		INFO_PRINT ("bytes_recv %d , pac %d\n", bytes_recv, length);
	}

    send_control_message(context->serverinfo, M_JITTERDOWNLOADRESULT_DONE);


    close(jc->socket);
    free(jc);

    free (rtt_results.time_array);
    free (rtt_results.results_array);udp_result

    return 1;
}
*/
int jitter_download_test(Context_t * context) {

	struct udp_recv_counters_parameters_st params;
	struct timeval ini_tv;
	char * tempstring;
	uint64_t ini;
	struct Jitter_context_st *jc;
	struct timeval timeout;
	struct udp_result **results_array;
	struct udp_result *r_time_ref, *r;
	struct udp_result *r_prev;
	struct udp_results_time_array *time_array;
	uint32_t i, n;

	INFO_PRINT ("jitter download start");
	gettimeofday(&ini_tv, NULL);
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	jc = context->data;
	params.max_time = JITTER_TEST_TIME_IN_US / 1e6;
	params.max_counter = INT64_MAX;
	params.socket = jc->socket;

	if (setsockopt (jc->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		ERROR_PRINT ("setsockopt");
	r = udp_receive_counters(&params);
	ini = TIMEVAL_MICROSECONDS (ini_tv);

	i = n = 0;
	r_time_ref = r_prev = r;
	results_array = malloc (sizeof (struct udp_result*) * (JITTER_DOWN_SLICES + 1));
	if (!results_array)
		ERROR_PRINT ("faltou memoria alocacao array de resultados (1)");
	else memset (results_array, 0, sizeof (struct udp_result*) * (JITTER_DOWN_SLICES + 1));
	time_array = malloc (sizeof (struct udp_results_time_array) * (JITTER_DOWN_SLICES + 1));
	if (!time_array)
		ERROR_PRINT ("faltou memoria alocacao array de temp (1)");


	if (r) {
		results_array[0] = r;
		for (i = 0; i < JITTER_DOWN_SLICES + 1; i++) {
			time_array[i].t_ini = ini + i * 500000;
			time_array[i].t_fim = time_array[i].t_ini + 500000;
		}
	}
	i = 0;
	while (r)
	{
		if (r->time_recv < time_array[i].t_fim) {
			r_prev = r;
			r = r->next;
		}
		else
		{
			i++;
// 			if (i % JITTER_DOWN_SLICES == 0)
// 			{
// 				temp = realloc (results_array, sizeof (struct udp_result*) * (i + JITTER_DOWN_SLICES));
// 				if (!temp)
// 				ERROR_PRINT ("faltou memoria realocacao array de resultados (2)");
// 				results_array = temp;
// 				time_temp = realloc (time_array, sizeof (struct udp_results_time_array) * (i + JITTER_DOWN_SLICES));
// 				if (!time_temp)
// 				ERROR_PRINT ("faltou memoria realocacao array de tempos (2)");
// 				time_array = time_temp;
// 			}
			r_prev->next = NULL;
			results_array[i] = r_time_ref = r_prev = r;
			r = r->next;
		}

	}

	for (i = 0; i < JITTER_DOWN_SLICES; i++) {
		Result_t result = jitter_process_results(results_array[i], time_array[i].t_ini, time_array[i].t_fim, context->serverinfo->clockoffset_in_usec, DOWNLOAD, ini);
		result.direction = DOWNLOAD;
		message_t m = M_JITTERRESULT;
		m.content = tempstring = result_string_jitter(result);
		store_results(context->output, m.field, m.content);
		send_control_message(context->serverinfo, m);
	}
	send_control_message(context->serverinfo, M_JITTERDOWNLOADRESULT_DONE);

	close(jc->socket);
	free(jc);
	INFO_PRINT ("jitter download finish");
	return 1;
}

void update_jitter_value(struct udp_result *r, int64_t initial_time_recv, unsigned int *last_latency, double *jitter) {
    int64_t latency = (r->time_recv - initial_time_recv);
    if (r->counter > IGNORE_FIRST_N_PACKAGES_JITTER && last_latency > 0){
        int64_t delta = ((*last_latency) - latency);
        if(delta < 0) {
            delta = -delta;
        }
        *jitter = (((double) delta) - (*jitter))/16.0;
        INFO_PRINT("n=%"PRISIZ" jitter=%f timercv=%"PRI64" timestamp=%"PRI64"\tlatency=%lu\tdelta=%ld", r->counter, *jitter, (r->time_recv - initial_time_recv), r->time, latency, delta);
    }
    *last_latency = latency;
}

static struct count_jitter_result_st count_jitter_result(struct udp_result *results, int64_t t_ini) {

    struct count_jitter_result_st cr;
    struct udp_result *r = results;

    TRACE_PRINT("count_jitter_result_st");
    cr.out_of_order = 0;
    cr.lost = 0;
    cr.right = 0;
    cr.total_bytes = 0;
    cr.max_packet_size = 0;


    static int64_t expected_counter = 0;
    static int64_t received = 0;
    static double jitter = 0;
    int64_t Si, Ri;
    int64_t Di_menos1_Di = 0;
    static int64_t Si_menos1 = 0;
    static int64_t Ri_menos1 = 0;

//    if(r != NULL) {
//        initial_time_recv = r->time_recv;
//    }


    while (r != NULL){
        Si = r->time;
        Ri = r->time_recv - t_ini;
        Di_menos1_Di = (Ri - Ri_menos1) - (Si - Si_menos1);
//        D(i-1,i) = (Ri - Ri-1) - (Si - Si-1) = (Ri - Si) - (Ri-1 - Si-1)
//        J(i) = J(i-1) + (|D(i-1,i)| - J(i-1))/16
        if (Di_menos1_Di < 0)
            Di_menos1_Di *= -1; // módulo Di_menos1_Di
        cr.total_bytes += r->bytes_recv;
        cr.max_packet_size = MAX(cr.max_packet_size, r->bytes_recv);
		//INFO_PRINT ("max_packet_size: %"PRI64", cr.max_packet_size: %"PRI64", r->bytes_recv:%"PRI64"\n", cr.max_packet_size, cr.max_packet_size, r->bytes_recv);
		received++;
        if (r->counter == expected_counter){
            jitter = jitter + (Di_menos1_Di - jitter)/16.0;
    //        update_jitter_value(r, initial_time_recv, &last_latency, &jitter);
            cr.right++;
            expected_counter++;
            INFO_PRINT("right %"PRI64", received: %"PRI64"\n", r->counter, received);
        } else if (r->counter < expected_counter){
            /*pacote que nao foi perdido mas chegou fora de ordem */
            assert(cr.lost >= 0);
            cr.lost--;
            cr.out_of_order++;
            INFO_PRINT("out of order %"PRI64"\n", r->counter);
        } else{
            /*eh maior do que o esperado entao perderam-se todos no intervalo */
    //        update_jitter_value(r, initial_time_recv, &last_latency, &jitter);
            jitter = jitter + (Di_menos1_Di - jitter)/16.0;
            INFO_PRINT("lost %"PRI64" packages (counter: %"PRI64", expected_counter: %"PRI64", received: %"PRI64")\n", r->counter - expected_counter, r->counter, expected_counter, received);
            expected_counter = r->counter + 1;
            cr.lost += expected_counter - r->counter;
        }
        r = r->next;
        Si_menos1 = Si;
        Ri_menos1 = Ri;
    }
    cr.mean_jitter = jitter;
    return cr;
}

Result_t jitter_process_results(struct udp_result * u_result, uint64_t tv_ini, uint64_t tv_end,
        int64_t clock_offset_in_us, enum test_direction direction, int64_t t_ini_jitter) {

    struct udp_result *aux;
    Result_t result;
    struct count_jitter_result_st cr;

    result.direction = 'U';
    int64_t event_ini = tv_ini - clock_offset_in_us;
    int64_t event_end = tv_end - clock_offset_in_us;
    result.event_start_time_in_us = event_ini;
    result.event_end_time_in_us = event_end;
    result.number_of_packages = 0;
    result.package_size_in_bytes = 0;
    result.number_of_bytes = 0;
    result.lost_packages = 0;
    result.outoforder_packages = 0;
    cr = count_jitter_result(u_result, t_ini_jitter);
    result.time_in_us = cr.mean_jitter;
    result.number_of_packages += cr.right + cr.out_of_order;
    result.package_size_in_bytes = MAX(result.package_size_in_bytes , cr.max_packet_size);
    result.number_of_bytes += cr.total_bytes;
    result.lost_packages += cr.lost;
    result.outoforder_packages += cr.out_of_order;
    while (u_result != NULL){
        aux = u_result;
        u_result = u_result->next;
        free(aux);
    }
    return result;
}
