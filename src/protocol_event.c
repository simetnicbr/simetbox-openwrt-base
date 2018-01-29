/** ----------------------------------------
* @file   protocol_event.c
* @brief  
* @author  Rafael de O. Lopes Gonçalves
* @date 28 de outubro de 2011
*------------------------------------------*/

/*
* Changelog:
* Created: 28 de outubro de 2011
* Last Change: 01 de março de 2012
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <limits.h>

#include "debug.h"
#include "unp.h"
#include "protocol.h"
#include "protocol_handler.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "rtt_test.h"
#include "tcp_test.h"
#include "udp_test.h"
#include "jitter_test.h"
#include "latency_test.h"
#include "results.h"
#include "protocol_event.h"
#include "server_finder.h"
#include "utils.h"

const message_t M_TCP_UPLOAD_TIME = { 11, "TCPUPLOADTIME",
		TCP_UPLOAD_TIME_STRING };

const char * SIMET_DEFAULT_PORT = "16000";

/* GENERAL ******************************************/
int event_start_control_connection(const Event_t *event, Context_t *context) {
	Simet_server_info_t * node;
	int status_ok = 0, option = 1;

	do {
		if (context->serverinfo->description)
			INFO_PRINT("Conectando ao servidor: %s", context->serverinfo->description);

		if (connect_tcp(context->serverinfo->address_text, SIMET_DEFAULT_PORT, 0,
				&(context->serverinfo->socket_control_fd)) >= 0) {
			free(context->serverinfo->address_text);
			context->serverinfo->address_text = get_remote_ip(context->serverinfo->socket_control_fd);
		//	context->ssl_ctx = init_ssl_context();
			setsockopt(context->serverinfo->socket_control_fd, SOL_SOCKET, MSG_NOSIGNAL, (void *)&option, sizeof(int));
			context->serverinfo->ssl = connect_ssl_bio(context->serverinfo->socket_control_fd, context->ssl_ctx);
			/* socket will have been closed by connect_ssl_bio on failure */
			if (context->serverinfo->ssl) {
				status_ok = 1;
			} else {
				INFO_PRINT ("nao conseguiu ssl");
			}
		} else {
			INFO_PRINT ("nao conseguiu tcp connect");
		}

		if (!status_ok) {
			node = context->serverinfo->next;
			free_server_info(context->serverinfo);
			context->serverinfo = node;
			TRACE_PRINT(node ? "trying next node" : "no more nodes to try, failed.");
		}
	} while (!status_ok && context->serverinfo != NULL);

	return (status_ok)? 1 : -ENOTCONN;
}

int event_request_server(const Event_t *event, Context_t *context) {
	int status;
	status = send_control_message(context->serverinfo,
			M_SIMET_SERVER_REQUEST_NOW);
	return status;
}

int event_send_message(const Event_t *event, Context_t *context) {
	message_t *m = (message_t *) event->data;
	int status;
	status = send_control_message(context->serverinfo, *m);
	return status;
}

int event_send_initial_paramaeters(const Event_t *event, Context_t *context) {
	int status;

	status = send_control_message(context->serverinfo, M_REQUEST_SERVERTIME);
	if (status < 0) {
		return status;
	}

	return 1;
}

int event_send_ip_address(const Event_t *event, Context_t *context) {
	message_t aux_message;
	char * tempstring;
	char * tempstring2;
	int status;

	aux_message.field = tempstring = strdup("SIMET_COMMONS_RESULT_INTERNALIP");
	aux_message.content = tempstring2 = get_local_ip(
			context->serverinfo->socket_control_fd);
	status = send_control_message(context->serverinfo, aux_message);
	free(tempstring);
	free(tempstring2);
	if (status < 0) {
		return status;
	}

	status = send_control_message(context->serverinfo,
			M_REQUEST_CLIENT_IPADDRESS);
	return 1;
}

int event_req_localtime(const Event_t *event, Context_t *context) {
	int status;
	status = send_control_message(context->serverinfo,
			M_REQUEST_SERVERLOCALTIME);

	return status;

}

int event_send_ptt(const Event_t *event, Context_t *context) {
	message_t aux_message;
	int status;
	char * tempstring;

	aux_message.field = tempstring = strdup("ASNORIGIN");
#ifdef GETSERVERSINFOS_OLD
	aux_message.content = context->serverinfo->asn_origin;
#else
	aux_message.content = "0";
#endif
	status = send_control_message(context->serverinfo, aux_message);
	free(tempstring);
	if (status < 0) {
		return status;
	}

	aux_message.field = tempstring = strdup("ASNPARTICIPANT");
#ifdef GETSERVERSINFOS_OLD
	aux_message.content = context->serverinfo->asn_participant;
#else
	aux_message.content = "0";
#endif
	status = send_control_message(context->serverinfo, aux_message);
	free(tempstring);
	if (status < 0) {
		return status;
	}

	aux_message.field = tempstring = strdup("IDCOOKIE");
	aux_message.content = context->id_cookie;
	status = send_control_message(context->serverinfo, aux_message);
	free(tempstring);
	if (status < 0) {
		return status;
	}

	aux_message.field = tempstring = strdup("HASH_MEASURE");
	aux_message.content = context->hash_measure;
	status = send_control_message(context->serverinfo, aux_message);
	free(tempstring);
	if (status < 0) {
		return status;
	}
	aux_message.field = tempstring = strdup("TIPO_CONEXAO");
	aux_message.content = context->tipo_conexao;
	status = send_control_message(context->serverinfo, aux_message);
	free(tempstring);
	if (status < 0) {
		return status;
	}

	aux_message.field = tempstring = strdup("PTTUSED");
#ifdef GETSERVERSINFOS_OLD
	aux_message.content = context->serverinfo->symbol;
#else
	aux_message.content = context->serverinfo->location;
#endif
	status = send_control_message(context->serverinfo, aux_message);
	free(tempstring);
	if (status < 0) {
		return status;
	}

	status = send_control_message(context->serverinfo, M_DYNAMICRESULTS_YES);
	if (status < 0) {
		return status;
	}

	return 1;
}

int event_finish_protocol(const Event_t *event, Context_t *context) {
	TRACE_PRINT("event_finish_protocol");
	send_control_message(context->serverinfo, M_ENDTESTS_OK);
	finish_context(&context);
	return 1;
}

/* TCP ******************************************/

int event_request_tcp_socket(const Event_t *event, Context_t *context) {
	int status = request_socket(context);
	return status;

}

int event_start_tcp_download(const Event_t *event, Context_t *context) {
	TRACE_PRINT("event_start_tcp_download\n");
	int status;
	status = tcp_download_test(context);
	free(context->data);
	TRACE_PRINT("end event_start_tcp_download\n");
	return status;
}

int event_start_tcp_upload(const Event_t *event, Context_t *context) {
	tcp_upload_test(context);
	sleep(1);
	return 1;

}

static void create_tcp_context(Context_t *context) {
	struct tcp_context_st *tcp_context = malloc(sizeof(struct tcp_context_st));
	context->data = tcp_context;
	TRACE_PRINT("\n\n\n\nv^v^v^v^v^v^v^ cria o tcp_context %p", tcp_context);
	int i;
	for (i = 0; i < TCP_NUMBER_OF_SOCKETS; i++) {
		tcp_context->sockets[i] = -1;
	}
	tcp_context->sockets_created = 0;
	tcp_context->do_download = -1;
}

int event_request_tcp_test(const Event_t *event, Context_t *context) {
	int status;
	status = send_control_message(context->serverinfo, M_TCP_UPLOAD_TIME);
	create_tcp_context(context);
	return status;
}

int event_request_tcp_download_test(const Event_t *event, Context_t *context) {
	int status;
	struct tcp_context_st *tcp_context = context->data;
	tcp_context->do_download = 1;
	status = send_control_message(context->serverinfo, M_TCP_DOWNLOAD_START);

	return status;
}

int event_request_tcp_upload_test(const Event_t *event, Context_t *context) {
	int status;
	struct tcp_context_st *tcp_context = context->data;
	tcp_context->do_download = 0;
	status = send_control_message(context->serverinfo, M_TCP_UPLOAD_START);

	return status;
}
/* RTT ******************************************************************/
int event_authorization_rtt_test(const Event_t *event, Context_t *context) {
	int status;
	status = send_control_message(context->serverinfo, M_REQUEST_RTT_START);
	return status;
}

int event_start_rtt_test(const Event_t *event, Context_t *context) {
	int status = rtt_test(context->serverinfo, context->output, context->family);
	finalize_test_and_start_next(context);
	return status;
}

/* UDP *************************************************************************/

int event_request_udp_download_test(const Event_t *event, Context_t *context) {
	context->data = (void*) udp_initialize_context_download(DOWNLOAD, context->max_rate_in_bps, context);
	return send_control_message(context->serverinfo, M_UDPDOWNLOADTIME);
}

int event_create_udp_sockets(const Event_t *event, Context_t *context) {
	request_sockets_udp(context);
	return 1;
}
int event_confirm_udp_rate(const Event_t *event, Context_t *context) {
	int64_t * rate = (int64_t*) event->data;
	TRACE_PRINT("event_confirm_udp_rate");
	confirm_port_rate_udp_test(context, *rate);

	free(rate);
	return 1;
}
int event_udp_confirm_download_package_size(const Event_t *event,
		Context_t *context) {
	TRACE_PRINT("event_udp_confirm_download_package_size");
	return udp_confirm_download_package_size(context);
}
int event_udp_start_download_test(const Event_t *event, Context_t *context) {
	TRACE_PRINT("event_udp_start_download_test");
	return udp_download_test(context);
}

int event_request_udp_upload_test(const Event_t *event, Context_t *context) {
	context->data = udp_initialize_context(UPLOAD, context->max_rate_in_bps);
	return send_control_message(context->serverinfo, M_UDPUPLOADTIME);
}

int event_udp_start_upload_test(const Event_t *event, Context_t *context) {
	TRACE_PRINT("event_udp_start_upload");
	return udp_upload_test(context);
}

/** Jitter **********************************************************************/
int event_request_jitter_download_test(const Event_t *event, Context_t *context) {
	TRACE_PRINT("event_start_jitter_download");
	return send_control_message(context->serverinfo, M_JITTERDOWNLOADTIME);
}

int event_request_jitter_upload_test(const Event_t *event, Context_t *context) {
	TRACE_PRINT("event_request_jitter_upload_test");
	return send_control_message(context->serverinfo, M_JITTERUPLOADTIME);
}

int event_create_jitter_context(const Event_t *event, Context_t *context) {
	int i;
	TRACE_PRINT("confirma jitter port");
	i =  jitter_create_jitter_context(context, ((enum test_direction) event->data));
	TRACE_PRINT("confirma jitter port-2");
	return i;
}

int event_confirm_jitterport(const Event_t *event, Context_t *context) {
	TRACE_PRINT("confirma jitter port");
	return jitter_create_socket_confirmation(context);

}

static void * jitter_upload_test_wrapper(void * context_pt) {
	Context_t * ctx = context_pt;
	jitter_upload_test(ctx);
	return NULL;
}

static void * jitter_download_test_wrapper(void * context_pt) {
	Context_t * ctx = context_pt;
	jitter_download_test(ctx);
	return NULL;
}

int event_jitter_send_start_message(const Event_t *event, Context_t *context) {
	struct Jitter_context_st * jc = context->data;
	if (jc->direction == UPLOAD) {
		return send_control_message(context->serverinfo, M_JITTERUPLOAD_START);
	} else {
		return send_control_message(context->serverinfo, M_JITTERDOWNLOAD_START);
	}
}

int event_start_jitter_download(const Event_t *event, Context_t *context) {
	pthread_t test_thread;
	TRACE_PRINT("event_start_jitter_download");
	meu_pthread_create(&test_thread, 0, jitter_download_test_wrapper, context);
	pthread_detach(test_thread);
	return 1;
}

int event_start_jitter_upload(const Event_t *event, Context_t *context) {
	pthread_t test_thread;
	TRACE_PRINT("event_start_jitteruploadtest");
	meu_pthread_create(&test_thread, 0, jitter_upload_test_wrapper, context);
	pthread_detach(test_thread);
	return 1;
}

int event_store_jitter_test_result(const Event_t *event, Context_t *context) {
	store_result_recvmsg(context->output, event->data);
	free((void*) event->data);
	return 1;
}

/* Latency *****************************************************************/
int event_request_latency_upload(const Event_t *event, Context_t *context) {
	return send_control_message(context->serverinfo, M_LATENCYUPLOAD_START);
}
int event_start_latency_upload(const Event_t *event, Context_t *context) {
	TRACE_PRINT("event_start_latency_upload");
	latency_upload_test(context);
	return 1;
}

static void * latency_download_test_wrapper(void * context_pt) {
	Context_t * ctx = context_pt;
	latency_download_test(ctx);
	return NULL;
}

int event_request_latency_download(const Event_t *event, Context_t *context) {
	pthread_t test_thread;

	context->data = malloc(sizeof(struct latency_context_st));

	meu_pthread_create(&test_thread, 0, latency_download_test_wrapper, context);
	pthread_detach(test_thread);
	sleep(1);

	return send_control_message(context->serverinfo, M_LATENCYDOWNLOAD_START);
}

int event_start_latency_download(const Event_t *event, Context_t *context) {
	TRACE_PRINT("event_start_latency_download");
	return 1;
}

