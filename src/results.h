/** ----------------------------------------
 * @file  results.h
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 1, 2011
 *------------------------------------------*/

#ifndef RESULTS_H_
#define RESULTS_H_

#include <time.h>
#include <stdio.h>
#include "protocol.h"

#define TIME_SLICE 500000
// 500ms
#define NUM_SLICES 10


typedef struct result_st {
    enum test_direction direction;
    int64_t event_start_time_in_us;
    int64_t event_end_time_in_us;
    int64_t package_size_in_bytes;
    int64_t number_of_packages;
    int64_t number_of_bytes;

    int64_t time_in_us;
    int64_t lost_packages;
    int64_t send_rate_in_bps;
    int64_t outoforder_packages;

} Result_t;

char *result_string_rtt(struct result_st r);
char *result_string_udp(struct result_st r);
char *result_string_tcp(struct result_st r);
char *result_string_jitter(struct result_st r);

int store_results(FILE * file, const char *type, const char *result_string);

int store_result_recvmsg(FILE * file, const char *result_string);
#endif                          /* RESULTS_H_ */
