/** ----------------------------------------
 * @file  rtt_test.h
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Oct 31, 2011
 *------------------------------------------*/

#ifndef RTT_TEST_H_
#define RTT_TEST_H_

#include "protocol.h"

#define RTT_TEST_PORT "15030"
int rtt_test(Simet_server_info_t * serverinfo, FILE * output, int family);


struct udp_results_time_array
{
    int64_t t_ini;
    int64_t t_fim;
};

struct udp_results_array
{
    struct udp_result **results_array;
    struct udp_results_time_array *time_array;
    uint32_t n;
};
    
extern struct udp_results_array rtt_results;
    

#endif                          /* RTT_TEST_H_ */
