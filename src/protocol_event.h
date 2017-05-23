/** ----------------------------------------
 * @file  protocol_event.h
 * @brief
 * @author  Rafael de O. Lopes Goncalves
 * @date Nov 23, 2011
 *------------------------------------------*/


#ifndef PROTOCOL_EVENT_H_
#define PROTOCOL_EVENT_H_

/* ********** CONTROL EVENTS ************************************************/
int event_start_control_connection(const Event_t *event, Context_t *context);
int event_request_server(const Event_t *event, Context_t *context);
int event_send_message(const Event_t *event, Context_t *context);
int event_send_initial_paramaeters(const Event_t *event, Context_t *context);
int event_finish_protocol(const Event_t *event, Context_t *context);
int event_send_ptt(const Event_t *event, Context_t *context);
int event_send_ip_address(const Event_t *event, Context_t *context);
int event_req_localtime(const Event_t *event, Context_t *context);

/* ********** RTT TEST *********************************************************/
int event_authorization_rtt_test(const Event_t *event, Context_t *context);
int event_start_rtt_test(const Event_t *event, Context_t *context);

/* ********** TCP TEST EVENTS ******************************** */
int event_request_tcp_test(const Event_t *event, Context_t *context);
int event_request_tcp_socket(const Event_t *event, Context_t *context);

/***download sequence ***/
int event_request_tcp_download_test(const Event_t *event, Context_t *context);
int event_start_tcp_download(const Event_t *event, Context_t *context);
/***upload sequence ***/
int event_request_tcp_upload_test(const Event_t *event, Context_t *context);
int event_start_tcp_upload(const Event_t *event, Context_t *context);

/* *********** UDP TEST EVENTS ******************************* */
int event_confirm_udp_rate(const Event_t *event, Context_t *context);
int event_create_udp_sockets(const Event_t *event, Context_t *context);

/*download*/
int event_request_udp_download_test(const Event_t *event, Context_t *context);
int event_udp_confirm_download_package_size(const Event_t *event, Context_t *context);
int event_udp_start_download_test(const Event_t *event, Context_t *context);
/*upload*/
int event_request_udp_upload_test(const Event_t *event, Context_t *context);
int event_udp_start_upload_test(const Event_t *event, Context_t *context);

/* *********** JITTER EVENTS  ************************** */
int event_confirm_jitterport(const Event_t *event, Context_t *context);
int event_store_jitter_test_result(const Event_t *event, Context_t *context);
int event_jitter_send_start_message(const Event_t *event, Context_t *context);
int event_create_jitter_context(const Event_t *event, Context_t *context);

/*jitter upload*/
int event_request_jitter_upload_test(const Event_t *event, Context_t *context);
int event_start_jitter_upload(const Event_t *event, Context_t *context);
/*jitter download*/
int event_request_jitter_download_test(const Event_t *event, Context_t *context);
int event_start_jitter_download(const Event_t *event, Context_t *context);

/********* DIRECTIONAL LATENCY ************************************ */
int event_request_latency_upload(const Event_t *event, Context_t *context);
int event_start_latency_upload(const Event_t *event, Context_t *context);

int event_request_latency_download(const Event_t *event, Context_t *context);
int event_start_latency_download(const Event_t *event, Context_t *context);

#endif /* PROTOCOL_EVENT_H_ */
