/** ----------------------------------------
 * @file  jitter_test.h
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 28, 2011
 *------------------------------------------*/

#ifndef JITTER_TEST_H_
#define JITTER_TEST_H_

#include "protocol.h"
#include "results.h"

#define JITTER_PORT "15020"
#define JITTER_DOWN_SLICES 12

struct Jitter_context_st {
    int socket;
    enum test_direction direction;
};

struct count_jitter_result_st {
    int64_t lost;
    int64_t out_of_order;
    int64_t right;
    int64_t total_bytes;
    int64_t max_packet_size;
    double mean_jitter;
};


int jitter_create_socket_confirmation(Context_t * context);

int jitter_upload_test(Context_t * context);

int jitter_download_test(Context_t * context);

int jitter_create_jitter_context(Context_t * context, enum test_direction direction);
#endif                          /* JITTER_TEST_H_ */
