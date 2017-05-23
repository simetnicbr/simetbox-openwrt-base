/** ----------------------------------------
 * @file  tcp_test.h
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 7, 2011
 *------------------------------------------*/

#ifndef TCP_TEST_H_
#define TCP_TEST_H_

#include "protocol.h"

#define TCP_NUMBER_OF_SOCKETS 6
#define TCP_TEST_PORT "15000"
#define TCP_TOTAL_TIME_SEC 11
#define TCP_UPLOAD_TIME_STRING "11"
#define TCP_DOWNLOAD_TIME_STRING "11"
#define TCP_PACKAGE_SIZE 1400
#define TCP_PACKAGE_SIZE_STRING "1400"

struct tcp_context_st {
    unsigned int sockets_created;
    int sockets[TCP_NUMBER_OF_SOCKETS];
    int do_download;
};

int request_socket(Context_t *context);

int tcp_download_test(Context_t *context);
int tcp_upload_test(Context_t *context);

#endif                          /* TCP_TEST_H_ */
