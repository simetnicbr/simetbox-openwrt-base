/**
 * @file   latency_test.h
 * @brief
 * @author  Rafael Lopes <rafael@nic.br>
 * @date 12 de dezembro de 2011
 */


#ifndef LATENCY_TEST_H
#define LATENCY_TEST_H
#include <stdio.h>
#include <stdlib.h>
#include "latency_test.h"
#include "protocol.h"
#include "results.h"

struct latency_context_st {
    int socket;
    int keep_running;
};

int latency_upload_test(Context_t * context);
int latency_download_test(Context_t * context);

#endif
