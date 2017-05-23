/** ----------------------------------------
 * @file   latency_test.c
 * @brief
 * @author  Rafael Lopes <rafael@nic.br>
 * @date 12 de dezembro de 2011
 *------------------------------------------*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include "latency_test.h"
#include "protocol.h"
#include "results.h"
#include "unp.h"
#include "local_simet_config.h"
#include "netsimet.h"

#define INTERVAL_BETWEEN_PACKAGES_IN_US 40000
#define LATENCY_TEST_TIME_IN_US 6000000
#define LATENCY_TEST_PORT "42429"


int latency_upload_test(Context_t * context) {
    struct timeval tv;
    int64_t ini, cur;
    int64_t counter = 0;
    int64_t time_s;
    int socket;

    connect_udp(context->serverinfo->address_text, LATENCY_TEST_PORT, context->family, &socket);

    gettimeofday(&tv, NULL);
    ini = TIMEVAL_MICROSECONDS(tv);
    cur = ini;
    while (cur - ini < LATENCY_TEST_TIME_IN_US){
        gettimeofday(&tv, NULL);
        time_s = TIMEVAL_MICROSECONDS(tv);
        TRACE_PRINT("time_s:%"PRI64, time_s);
        send_udp(socket, counter, time_s,0);
        counter++;
        usleep(INTERVAL_BETWEEN_PACKAGES_IN_US);
        gettimeofday(&tv, NULL);
        cur = TIMEVAL_MICROSECONDS(tv);
    }
    close(socket);
    return 1;
}


int latency_download_test(Context_t * context) {
    struct timeval tv;
    uint64_t ini, cur;
    uint64_t counter_r = 0;
    uint64_t time_r;
    uint64_t time_s;

    uint64_t extra;
    struct latency_context_st * jc;
    jc = context->data;

    connect_udp(context->serverinfo->address_text, LATENCY_TEST_PORT, context->family, &jc->socket);

    gettimeofday(&tv, NULL);
    ini = TIMEVAL_MICROSECONDS(tv);
    cur = ini;
    while (cur - ini < LATENCY_TEST_TIME_IN_US){

        TRACE_PRINT("time_s:%"PRI64, time_s);
        recv_udp(jc->socket,&counter_r, &time_r, &extra);
        gettimeofday(&tv, NULL);
        time_s = TIMEVAL_MICROSECONDS(tv);
        TRACE_PRINT("time_r:%"PRI64" \t time_s=%"PRI64" delta=%"PRI64, time_r, time_s, time_r-  time_s);

        usleep(INTERVAL_BETWEEN_PACKAGES_IN_US);
    }
    close(jc->socket);
    return 1;
}
