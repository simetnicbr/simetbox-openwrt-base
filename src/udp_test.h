/** ----------------------------------------
 * @file  udp_test.h
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 21, 2011
 *------------------------------------------*/

#ifndef UDP_TEST_H
#define UDP_TEST_H

#include "protocol.h"
#include "rtt_test.h"



#define UDP_NUMBER_OF_SOCKETS 6
#define UDP_PORT_TEST_STRING "15010"
#define UDP_PACKAGE_SIZE_IN_BYTES 1200
#define UDP_TEST_TIME 11
#define UDP_DEFAULT_MAX_RATE  30000000
#define UDP_DEFAULT_RATE_IN_BITS_PER_SEC 256000
//#define UDP_DEFAULT_RATE_IN_BITS_PER_SEC (1000000000 * 0.95)
#define EXTRA_TIME 1
#define BLOCOS ((UDP_TEST_TIME + EXTRA_TIME) * 2)


struct udp_context_st {
    int sockets[UDP_NUMBER_OF_SOCKETS];
    enum test_direction mode;
    int64_t rate;
    int authorized;
    void * params;
};
struct udp_result {
    int64_t delta;
    int64_t time_recv;
    int64_t counter;
    int64_t time;
    int64_t bytes_recv;
    struct udp_result *next;
};

struct udp_recv_counters_parameters_st {
    int socket;
    int64_t max_counter;
    int64_t max_time;
};

struct udp_recv_counters_parameters_st2 {
	int socket;
	int64_t number_of_packages[BLOCOS]; // blocos de 500ms
	int64_t number_of_bytes[BLOCOS]; // blocos de 500ms
	int64_t t_ini[BLOCOS]; // blocos de 500ms
	int64_t t_fim[BLOCOS]; // blocos de 500ms
    int64_t max_counter;
    int64_t max_time;
    int64_t em_ordem [BLOCOS];
    int64_t fora_de_ordem [BLOCOS];
    int64_t perdidos [BLOCOS];
    
};

struct count_result_st {
    int64_t lost;
    int64_t out_of_order;
    int64_t right;
    int64_t total_bytes;
    int64_t max_packet_size;
    struct udp_result *first_next_block;
};

void * udp_initialize_context(enum test_direction mode, int64_t rate_int_bps);
void * udp_initialize_context_download(enum test_direction mode, int64_t rate_int_bps, Context_t * context);
int request_sockets_udp(Context_t * context);

int confirm_port_rate_udp_test(Context_t * context, int64_t rate);
int udp_confirm_download_package_size(Context_t * context);
int udp_download_test(Context_t * context);

int udp_upload_test(Context_t * context);


struct udp_result * udp_receive_counters(struct udp_recv_counters_parameters_st  *receive_counters_parameters);

struct result_st udp_process_results(struct udp_result **results_array,int64_t number_of_results, struct timeval tv_ini,
                                 struct timeval tv_end, int64_t clock_offset);
struct result_st udp_process_results2(int64_t total_bytes, int64_t number_of_packages, int64_t right, int64_t out_of_order, int64_t lost, int64_t tv_ini, int64_t tv_end, int64_t clock_offset_in_us);                                 
struct result_st udp_process_results_rtt (struct udp_result **results_array, int64_t number_of_results,
        int64_t t_ini, int64_t t_fim, int64_t clock_offset_in_us);
void *udp_receive_counters_callback(void *receive_counters_parameters);
void *udp_receive_counters_callback2(void *receive_counters_parameters);
void *thread_trata_sinais_vazao_udp_upload (void *arg);

#endif
