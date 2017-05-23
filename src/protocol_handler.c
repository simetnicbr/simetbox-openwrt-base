/** ----------------------------------------
 * @file   protocol_handler.c
 * @brief
 * @author  Rafael Lopes <rafael@nic.br>
 * @date 28 de outubro de 2011
 *------------------------------------------*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "debug.h"
#include "unp.h"
#include "protocol.h"
#include "protocol_event.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "tcp_test.h"
#include "udp_test.h"
#include "messages.h"
#include "results.h"
#include "protocol_handler.h"

static const char *SIMET_COMMONS_RESULT_TCP_U = "SIMET_COMMONS_RESULT_TCP=U";
static const char *SIMET_COMMONS_RESULT_UDP_U = "SIMET_COMMONS_RESULT_UDP=U";

static int cmp_event(message_t message, const char *buffer);
static int64_t number_content(char *message);
static char *string_content(char *message);

static message_t M_TCPDOWNLOADTIME = {23, "TCPDOWNLOADTIME", TCP_DOWNLOAD_TIME_STRING};

/* #####    Event Functions   #################################################### */

static int64_t number_content(char *message) {

    int64_t number = 0;
    char *end;
    char *aux = strrchr(message, '=');

    TRACE_PRINT("number_content(%s)", message);

    number = strtoll(aux + 1, &end, 10);

    TRACE_PRINT("number = %"PRI64, number);
    assert(strcmp("\r\n", end) == 0);
    return number;
}

static char *string_content(char *message) {
    char *result = 0;
    char *aux = strrchr(message, '=');
    result = strdup(aux + 1);
    return result;
}

static int handle_latency_messages(Context_t * context, char *message) {
    int handled = -1;
    Event_t *e;
    if (cmp_event(M_LATENCYUPLOAD_OK, message) == 0){
         e = create_event(event_start_latency_upload, DISPATCH, NULL);
         register_event(context, e);
         handled = M_LATENCYUPLOAD_OK.type;
     }
    return handled;

}


static int handle_udp_messages(Context_t * context, char *message) {
	int handled = -1;
	Event_t *e;
	TRACE_PRINT("handle_udp");
	if (cmp_event(M_UDPDOWNLOADTIME_OK, message) == 0) {
		e = create_event(event_send_message, DISPATCH, &M_UDPDOWNLOADINTERVAL);
		register_event(context, e);
		handled = M_UDPDOWNLOADINTERVAL.type;
	} else if (cmp_event(M_UDPUPLOADTIME_OK, message) == 0) {
		e = create_event(event_send_message, DISPATCH, &M_UDPUPLOADINTERVAL);
		register_event(context, e);
		handled = M_UDPUPLOADTIME_OK.type;
	} else if (cmp_event(M_UDPDOWNLOADINTERVAL_OK, message) == 0) {
		e = create_event(event_create_udp_sockets, DISPATCH, NULL);
		register_event(context, e);
		handled = M_UDPDOWNLOADINTERVAL_OK.type;
	} else if (cmp_event(M_UDPUPLOADINTERVAL_OK, message) == 0) {
		e = create_event(event_create_udp_sockets, DISPATCH, NULL);
		register_event(context, e);
		handled = M_UDPUPLOADINTERVAL_OK.type;
	} else if (cmp_event(M_MAXRATE, message) == 0) {
		e = create_event(event_confirm_udp_rate, DISPATCH, NULL);
		e->data = malloc(sizeof(int64_t));
		*((int64_t *) e->data) = number_content(message);
		register_event(context, e);
		handled = M_MAXRATE.type;
	} else if (cmp_event(M_UDPDOWNLOADPACKAGESIZE_OK, message) == 0) {
		e = create_event(event_udp_confirm_download_package_size, DISPATCH,
				NULL);
		register_event(context, e);
		handled = M_UDPDOWNLOADPACKAGESIZE_OK.type;
	} else if (cmp_event(M_UDPUPLOADPACKAGESIZE_OK, message) == 0) {
		e = create_event(event_send_message, DISPATCH, &M_UDPUPLOAD_START);
		register_event(context, e);
		handled = M_UDPUPLOADPACKAGESIZE_OK.type;
	} else if (cmp_event(M_UDPUPLOAD_OK, message) == 0) {
		e = create_event(event_udp_start_upload_test, DISPATCH, NULL);
		register_event(context, e);
		handled = M_UDPUPLOAD_OK.type;
	} else if (cmp_event(M_UDPDOWNLOAD_OK, message) == 0) {
		e = create_event(event_udp_start_download_test, DISPATCH, NULL);
		register_event(context, e);
		handled = M_UDPDOWNLOAD_OK.type;
	} else if (cmp_event(M_UDPDOWNLOADSENDRATE_DONE, message) == 0) {
		finalize_test_and_start_next(context);
		handled = M_UDPDOWNLOADSENDRATE_DONE.type;
	} else if (cmp_event(M_UDPPORT_OK, message) == 0) {
		INFO_PRINT("%s", message);
		handled = M_UDPPORT_OK.type;
	} else if (cmp_event(M_UDPDOWNLOADSENDRATE, message) == 0) {
		INFO_PRINT("%s", message);
		handled = M_UDPDOWNLOADSENDRATE.type;
	}else if (cmp_event(M_UDPRESULT, message) == 0){
            store_result_recvmsg(context->output, message);
            handled = M_UDPRESULT.type;
	}else if (cmp_event(M_UDPUPLOADRESULT_DONE, message) == 0){
        finalize_test_and_start_next(context);
        handled = M_UDPUPLOADRESULT_DONE.type;
	}
	TRACE_PRINT("%s", "saiu do udp handler");
	return handled;
}
static int handle_jitter_messages(Context_t * context, char *message) {
    int handled = -1;
    Event_t *e;
    TRACE_PRINT("handle_jitter_messages");
    if (cmp_event(M_JITTERUPLOADTIME_OK, message) == 0){
        e = create_event(event_send_message, DISPATCH, &M_JITTERUPLOADINTERVAL);
        register_event(context, e);
        handled = M_JITTERUPLOADTIME_OK.type;
    } else if (cmp_event(M_JITTERDOWNLOADTIME_OK, message) == 0){


        e = create_event(event_send_message, DISPATCH, &M_JITTERDOWNLOADINTERVAL);
        register_event(context, e);
        handled = M_JITTERDOWNLOADTIME_OK.type;


    } else if (cmp_event(M_JITTERUPLOADINTERVAL_OK, message) == 0){


    	register_event(context,create_event(event_create_jitter_context, DISPATCH, (void*)UPLOAD));
    	register_event(context,create_event(event_confirm_jitterport, DISPATCH, NULL));
        handled = M_JITTERUPLOADINTERVAL_OK.type;


    } else if (cmp_event(M_JITTERDOWNLOADINTERVAL_OK, message) == 0){
    	//TODO: enviar daqui o inicio do teste de jitter download

    	register_event(context,create_event(event_create_jitter_context, DISPATCH, (void*)DOWNLOAD));
        register_event(context,create_event(event_jitter_send_start_message, DISPATCH, (void*)DOWNLOAD));
        register_event(context, create_event(event_confirm_jitterport, DISPATCH, NULL));
        handled = M_JITTERDOWNLOADINTERVAL_OK.type;

    } else if (cmp_event(M_JITTERPORT_OK, message) == 0){
        e = create_event(event_jitter_send_start_message, DISPATCH, NULL);
        register_event(context, e);
        handled = M_JITTERPORT_OK.type;
    } else if (cmp_event(M_JITTERUPLOAD_OK, message) == 0){
        e = create_event(event_start_jitter_upload, DISPATCH, NULL);
        register_event(context, e);
        handled = M_JITTERUPLOAD_OK.type;
    } else if (cmp_event(M_JITTERDOWNLOAD_OK, message) == 0){
        e = create_event(event_start_jitter_download, DISPATCH, NULL);
        register_event(context, e);
        handled = M_JITTERDOWNLOAD_OK.type;
    } else if (cmp_event(M_JITTERRESULT, message) == 0){
        e = create_event(event_store_jitter_test_result, DISPATCH, strdup(message));
        register_event(context, e);
        handled = M_JITTERRESULT.type;
    } else if (cmp_event(M_JITTERDOWNLOADRESULT_OK, message) == 0){
        finalize_test_and_start_next(context);
        handled = M_JITTERDOWNLOADRESULT_OK.type;
    } else if (cmp_event(M_JITTERUPLOADDYNAMIC, message) == 0){
        INFO_PRINT("%s", message);
        handled = M_JITTERUPLOADDYNAMIC.type;
    } else if (cmp_event(M_JITTERUPLOADRESULT_DONE, message) == 0){
        finalize_test_and_start_next(context);
        handled = M_JITTERUPLOADRESULT_DONE.type;
    }
    if (handled == -1){
        TRACE_PRINT("nao consegui processar \"%s\"", message);
    }
    return handled;
}
static int handle_tcp_messages(Context_t * context, char *message) {
    int handled = -1;
    struct tcp_context_st *tcp_context;
    Event_t *e;
    if (cmp_event(M_TCPUPLOADTIME_OK, message) == 0){
        e = create_event(event_send_message, DISPATCH, &M_TCPUPLOADINTERVAL);
        register_event(context, e);
        handled = M_TCPUPLOADTIME_OK.type;
    } else if (cmp_event(M_TCPUPLOADINTERVAL_OK, message) == 0){
        e = create_event(event_send_message, DISPATCH, &M_TCPUPLOADPACKAGESIZE);
        register_event(context, e);
        handled = M_TCPUPLOADINTERVAL_OK.type;
    } else if (cmp_event(M_TCPUPLOADPACKAGESIZE_OK, message) == 0){
        e = create_event(event_send_message, DISPATCH, &M_TCPDOWNLOADTIME);
        register_event(context, e);
        handled = M_TCPUPLOADPACKAGESIZE_OK.type;
    } else if (cmp_event(M_TCPDOWNLOADTIME_OK, message) == 0){
        e = create_event(event_send_message, DISPATCH, &M_TCPDOWNLOADINTERVAL);
        register_event(context, e);
        handled = M_TCPDOWNLOADTIME_OK.type;
    } else if (cmp_event(M_TCPDOWNLOADINTERVAL_OK, message) == 0){
        e = create_event(event_send_message, DISPATCH, &M_TCPDOWNLOADPACKAGESIZE);
        register_event(context, e);
        handled = M_TCPDOWNLOADINTERVAL_OK.type;
    } else if (cmp_event(M_TCPDOWNLOADPACKAGESIZE_OK, message) == 0){
        e = create_event(event_request_tcp_socket, DISPATCH, context->data);
        register_event(context, e);
        handled = M_TCPDOWNLOADPACKAGESIZE_OK.type;
    } else if (cmp_event(M_TCPSOCKET_OK, message) == 0){
        tcp_context = context->data;
        tcp_context->sockets_created++;
        if (tcp_context->sockets_created < TCP_NUMBER_OF_SOCKETS){
            e = create_event(event_request_tcp_socket, DISPATCH, context->data);
            register_event(context, e);
        }else{
        	finalize_test_and_start_next(context);
        }
        handled = M_TCPSOCKET_OK.type;
    } else if (cmp_event(M_TCPDOWNLOAD_OK, message) == 0){
        e = create_event(event_start_tcp_download, DISPATCH, NULL);
        register_event(context, e);
        handled = M_TCPDOWNLOAD_OK.type;
    } else if (cmp_event(M_TCPDOWNLOADRESULT_OK, message) == 0){
        finalize_test_and_start_next(context);
        handled = M_TCPDOWNLOADRESULT_OK.type;
    } else if (cmp_event(M_TCP_UPLOAD_OK, message) == 0){
        e = create_event(event_start_tcp_upload, DISPATCH, NULL);
        register_event(context, e);
        handled = M_TCP_UPLOAD_OK.type;
    } else if (strncmp(SIMET_COMMONS_RESULT_TCP_U, message, strlen(SIMET_COMMONS_RESULT_TCP_U)) == 0){
        int64_t temp1, temp2, temp3, temp4, vazao;
        store_result_recvmsg(context->output, message);
        sscanf (message + strlen(SIMET_COMMONS_RESULT_TCP_U), "%"PRI64" %"PRI64" %"PRI64" %"PRI64" %"PRI64, &temp1, &temp2, &temp3, &temp4, &vazao);
        context->bytes_tcp_upload_test += vazao;
        handled = 1;
    }else if (cmp_event(M_TCPUPLOADRESULT_DONE, message) == 0){
        finalize_test_and_start_next(context);
        handled = M_TCPUPLOADRESULT_DONE.type;
	}
    return handled;
}

static int handle_serverlocal_time(Context_t * context, char *message) {
    Event_t *e;
    struct timeval tv;
    int64_t t;

    context->serverlocaltime = number_content(message);
    e = create_event(event_send_ip_address, DISPATCH, NULL);
    register_event(context, e);

    gettimeofday(&tv, NULL);
    t = TIMEVAL_MICROSECONDS(tv);
    context->serverinfo->clockoffset_in_usec = t - (context->serverlocaltime * 1000);
    TRACE_PRINT("local(us)=%" PRI64 "\tserver(ms)=%"PRI64"\toffset(us)=%"PRI64, t, context->serverlocaltime, context->serverinfo->clockoffset_in_usec);

    return M_SERVERTIME.type;
}

void finalize_test_and_start_next(Context_t * context) {
    Event_t *e;
    TRACE_PRINT("finalize_test_and_start_next");
    enum test_type type = context->tests_to_do[context->next_test];
    if (type == RTT_TEST_TYPE){
        e = create_event(event_authorization_rtt_test, DISPATCH, NULL);
        register_event(context, e);
    } else if (type == TCP_TESTS_TYPE){
        e = create_event(event_request_tcp_test, DISPATCH, NULL);
        register_event(context, e);
    } else if (type == TCP_DOWNLOAD_TESTS_TYPE){
        e = create_event(event_request_tcp_download_test, DISPATCH, NULL);
        register_event(context, e);
    } else if (type == TCP_UPLOAD_TESTS_TYPE){
        e = create_event(event_request_tcp_upload_test, DISPATCH, NULL);
        register_event(context, e);
    } else if (type == UDP_DOWNLOAD_TESTS_TYPE){
        e = create_event(event_request_udp_download_test, DISPATCH, NULL);
        register_event(context, e);
    } else if (type == UDP_UPLOAD_TESTS_TYPE){
        e = create_event(event_request_udp_upload_test, DISPATCH, NULL);
        register_event(context, e);
    } else if (type == JITTER_UPLOAD_TESTS_TYPE){
        e = create_event(event_request_jitter_upload_test, DISPATCH, NULL);
        register_event(context, e);
    } else if (type == JITTER_DOWNLOAD_TESTS_TYPE){
        e = create_event(event_request_jitter_download_test, DISPATCH, NULL);
        register_event(context, e);
    }else if (type == LATENCY_UPLOAD_TESTS_TYPE){
//        e = create_event(event_request_latency_upload, DISPATCH, NULL);
//        register_event(context, e);
    }
    else if (type == LATENCY_DOWNLOAD_TESTS_TYPE){
//            e = create_event(event_request_latency_download, DISPATCH, NULL);
//            register_event(context, e);
    }
    /*END LOOP*/
    else{
        e = create_event(event_finish_protocol, END_LOOP, NULL);
        register_event(context, e);
    }
    context->next_test++;
}

int handle_control_message(Context_t * context, char *message) {
    int handled = -1;
    Event_t *e;

    if (cmp_event(M_TILDE, message) == 0){
        e = create_event(event_request_server, DISPATCH, NULL);
        register_event(context, e);
        handled = M_TILDE.type;
    } else if (cmp_event(M_SIMET_SERVER_ACK_OK, message) == 0){
        e = create_event(event_send_initial_paramaeters, DISPATCH, NULL);
        register_event(context, e);
        handled = M_SIMET_SERVER_ACK_OK.type;
    } else if (cmp_event(M_SERVERTIME, message) == 0){
        context->servertime = number_content(message);
        e = create_event(event_req_localtime, DISPATCH, NULL);
        register_event(context, e);
        handled = M_SERVERTIME.type;
    } else if (cmp_event(M_SERVERLOCALTIME, message) == 0){
        handled = handle_serverlocal_time(context, message);
    } else if (cmp_event(M_CLIENTIPADDRESS, message) == 0){
        context->external_ipaddress = string_content(message);
        e = create_event(event_send_ptt, DISPATCH, NULL);
        register_event(context, e);
        handled = M_CLIENTIPADDRESS.type;
    } else if (cmp_event(M_PTTLOCATION, message) == 0){
        context->ptt_location = string_content(message);
        finalize_test_and_start_next(context);
        handled = M_PTTLOCATION.type;
    } else if (cmp_event(M_YOURCITYSTATE, message) == 0){
        context->my_city = string_content(message);
        handled = M_YOURCITYSTATE.type;
    } else if (cmp_event(M_RTT_OK, message) == 0){
        e = create_event(event_start_rtt_test, DISPATCH, NULL);
        register_event(context, e);
        handled = M_RTT_OK.type;
    } else if (cmp_event(M_UPLOADBANDWIDTH, message) == 0){
    	if (context->max_rate_in_bps == 0) {
    		context->max_rate_in_bps = number_content(message);
//    		DEBUG_PRINT("=================== UPLOAD BANDWIDTH DO UDP = %"PRI64, context->max_rate_in_bps);
    	}
        handled = M_UPLOADBANDWIDTH.type;
    } else if (strncmp("TCP", message, 3) == 0
            || strncmp(SIMET_COMMONS_RESULT_TCP_U, message, strlen(SIMET_COMMONS_RESULT_TCP_U)) == 0){
        handled = handle_tcp_messages(context, message);
    } else if (strncmp("UDP", message, 3) == 0 || strncmp("MAXRATE", message, 7) == 0
    		|| strncmp(SIMET_COMMONS_RESULT_UDP_U, message, strlen(SIMET_COMMONS_RESULT_UDP_U)) == 0){
        handled = handle_udp_messages(context, message);
    } else if (strncmp("JITTER", message, 6) == 0 || cmp_event(M_JITTERRESULT, message) == 0){
        handled = handle_jitter_messages(context, message);
    } else if (strncmp("LATENCY", message, 7) == 0 ){
        handled = handle_latency_messages(context, message);
    }
    else if (strcmp("ERROR=UNKNOWNMESSAGE", message) == 0){
        ERROR_PRINT("Error: wrong message sended");
    }

    TRACE_PRINT("handled message %d", handled);
    return handled;
}

int handle_fatal_error(Context_t * context) {
    TRACE_PRINT("handle fatal error");
    Event_t * e = create_event(event_finish_protocol, END_LOOP, NULL);
    register_event(context, e);
    return 1;
}

static int cmp_event(message_t message, const char *buffer) {
    char mesbuf[BUFSIZ];
    if (message.content != NULL){
        snprintf(mesbuf, sizeof mesbuf, "%s=%s", message.field, message.content);
    } else{
        snprintf(mesbuf, sizeof mesbuf, "%s=", message.field);
    }

    return strncmp(mesbuf, buffer, strlen(mesbuf));
}
