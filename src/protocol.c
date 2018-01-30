/** ----------------------------------------
* @file   protocol.c
* @brief
* @author  Rafael de O. Lopes Gonçalves
* @date 26 de outubro de 2011
*------------------------------------------*/

/*
* Changelog:
* Created: 26 de outubro de 2011
* Last Change: 23 de February de 2012
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
#include "local_simet_config.h"
#include "netsimet.h"
#include "messages.h"
#include "protocol_handler.h"
#include "utils.h"

static const int CONTROL_MESSAGE_TIMEOUT = 30;
/*tempo em ms entre uma passada e outro do loop em microsegundos.*/
const uint SLEEP_TIME = 50000;

/* ####################################### Messages          ##### */
/*send*/

struct event_dispatcher_args_st {
	Event_t *e;
	Context_t *c;
};


struct receiver_args_st {
	Context_t *c;
};

static pthread_mutex_t finish_context_mutex = PTHREAD_MUTEX_INITIALIZER;

Event_t WAIT_EVENT = {NULL, WAITING, NULL};

/*internal functions*/
static void *recieve_and_handle_message(void *args);

int send_control_message(Simet_server_info_t * si, const message_t message) {
	char buf[BUFSIZ];
	int64_t len;
	int rc, rc_err, escreve_mais, rc_sel;
	struct timeval tv_timeo;
	fd_set writefds;

	if (si->socket_control_fd < 0) {
		ERROR_PRINT("trying to send_control_message on an illegal fd");
		saida(1);
	}

	len = snprintf(buf, BUFSIZ, "%s=%s\r\n", message.field, message.content);

//#ifdef  NDEBUG
	char *dbg = strdup(buf);
	dbg[strlen(dbg) - 2] = '\0';
	TRACE_PRINT(">>>SEND>>>\t\'%s\'", dbg);
	free(dbg);
//#endif
	rc = 0;
	do {
		escreve_mais = 0;
		rc = SSL_write(si->ssl, buf, len);
		if (rc < 0) {
			rc_err = SSL_get_error (si->ssl, rc);
			switch (rc_err) {
				case SSL_ERROR_WANT_WRITE:
//					INFO_PRINT ("SEND WANT_WRITE\n");
					FD_ZERO(&writefds);
					FD_SET(si->socket_control_fd, &writefds);
					tv_timeo.tv_sec = TIMEOUT;
					tv_timeo.tv_usec = 0;
					if ((rc_sel = select(si->socket_control_fd + 1, NULL, &writefds, NULL, &tv_timeo)) > 0)
						escreve_mais = 1;
				case SSL_ERROR_ZERO_RETURN:
					ERROR_PRINT ("SSL_ERROR_ZERO_RETURN on control socket\n");
					saida (1);
					break;
				case SSL_ERROR_SYSCALL:
					ERROR_PRINT ("SSL_ERROR_SYSCALL on control socket\n");
					saida (1);
					break;
				case SSL_ERROR_SSL:
					ERROR_PRINT ("SSL_ERROR_SSL on control socket\n");
					saida (1);
					break;
			}
		}
	} while ((rc != len) && (escreve_mais));

	return rc;
}

static int readable_timeo(int fd, int sec) {
	fd_set rset;
	struct timeval tv;
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	tv.tv_sec = sec;
	tv.tv_usec = 0;
	return select(fd + 1, &rset, NULL, NULL, &tv);
}

int receive_control_message(Simet_server_info_t * si, char * buffer, int64_t max_len, char *file, int linha) {
	int status = 0;

	if (si->socket_control_fd < 0) {
		ERROR_PRINT("trying to receive_control_message on an illegal fd");
		return -1; /* failsafe */
	}

	if (readable_timeo(si->socket_control_fd, CONTROL_MESSAGE_TIMEOUT)){
		status = SSL_read(si->ssl, buffer, max_len);
	} else{
		TRACE_PRINT("readable_timeo:timeout");
		ERROR_PRINT("timeout on control socket %s:%d", file, linha);
	}

	if (status > 0){ /*SUCCESS*/
		return status;
	} else if (status < 0){ /*ERROR:internal */
		return 0;
	} else{ /*SERVER IS DOWN*/
		return -1;
	}
}

static void *dipatch_events(void *args_pt) {
	struct event_dispatcher_args_st *args = (struct event_dispatcher_args_st *) args_pt;
	int status;

	args->c->queue_head++;
	status = args->e->dispatcher(args->e, args->c);
	if (status < 0) {
		ERROR_PRINT("protocol event handler returned error status %d", status);
		/* FIXME: what now?! */
		saida(1);
	} else {
		TRACE_PRINT("protocol event handler returned status %d", status);
	}

	free(args->e);
	args->e = NULL;

	return args_pt;
}

Event_t *create_event(event_dispatcher_t dispatcher, enum event_action action, void *data) {
	Event_t *event;
	event = malloc(sizeof(Event_t));

	event->action = action;
	event->dispatcher = dispatcher;
	event->data = data;

	return event;
}

void do_events_loop(Context_t * context) {
	struct event_dispatcher_args_st dispathcer_args;
	struct receiver_args_st receiver_args;
	pthread_t reciever_thread;


	dispathcer_args.c = context;
	receiver_args.c = context;

	while (1){
		if (context->queue_head < context->queue_tail){
			dispathcer_args.e = context->queue[context->queue_head];
			if (dispathcer_args.e->action == DISPATCH) {
				dipatch_events(&dispathcer_args);
			} else if (dispathcer_args.e->action == END_LOOP){
				TRACE_PRINT("e->action:%s", "END_LOOP");
				dipatch_events(&dispathcer_args);
				break;
			}
		}

		if (pthread_mutex_lock(&(context->mutex)) == 0){
			meu_pthread_create(&reciever_thread, 0, recieve_and_handle_message, &receiver_args);
			pthread_detach(reciever_thread);
		}

		usleep(SLEEP_TIME);

	}
}

int create_context(Context_t ** context) {
	int i;
	*context = malloc(sizeof(Context_t));

	if (*context == NULL){
		return -1;
	}

	memset (*context, 0, sizeof(Context_t));
	for (i = 0; i < TEST_TYPES_SIZE; i++){
		(*context)->tests_to_do[i] = -1;
	}


	pthread_mutex_init(&((*context)->mutex),NULL);
	return 1;
}

static int finish_server_info(Simet_server_info_t * serverinfo) {
	TRACE_PRINT("finish_server_info");
	if (serverinfo != NULL){
		if (SSL_shutdown(serverinfo->ssl) != 1) {
			SSL_shutdown(serverinfo->ssl);
		}
		SSL_free(serverinfo->ssl);
		TRACE_PRINT("finish_server_info close_tcp");
		close_tcp(serverinfo->socket_control_fd);
		free(serverinfo->address_text);
		free(serverinfo->location);
		free(serverinfo);
	}
	return 1;
}


int finish_context(Context_t ** context_pt) {
	pthread_mutex_lock(&finish_context_mutex);
	if(*context_pt!= NULL) {
		TRACE_PRINT("finish_context");
		Context_t *context = *context_pt;
		pthread_mutex_lock(&(context->mutex));
		TRACE_PRINT("finish_context");
		if (context->external_ipaddress != NULL){
			free(context->external_ipaddress);
			context->external_ipaddress = NULL;
		}
		if (context->my_city != NULL){
			free(context->my_city);
			context->my_city = NULL;
		}
		if (context->ptt_location != NULL){
			free(context->ptt_location);
			context->ptt_location = NULL;
		}
		if (context->hash_measure != NULL){
			free(context->hash_measure);
			context->hash_measure = NULL;
		}
		if (context->id_cookie != NULL){
			free(context->id_cookie);
			context->id_cookie = NULL;
		}
		if (context->tipo_conexao != NULL){
			free(context->tipo_conexao);
			context->tipo_conexao = NULL;
		}
		while (context->queue_head < context->queue_tail){
			free(context->queue[context->queue_head]);
			context->queue_head++;
		}
//		if (context->output != stdout){
//			fclose(context->output);
//		}
		finish_server_info(context->serverinfo);
		//SSL_CTX_free(context->ssl_ctx);
		pthread_mutex_unlock(&(context->mutex));
		pthread_mutex_destroy(&(context->mutex));
		free(context);
		(*context_pt) = NULL;
	}
	//pthread_mutex_unlock(&finish_context_mutex);
	return 1;
}




int register_event(Context_t * context, Event_t * event) {
	if (context->queue_tail >= CONTEXT_QUEUE_MAX_SIZE){
		ERROR_PRINT("context_queue reached max size %"PRISIZ, context->queue_tail);
		return -1;
	}
	context->queue[context->queue_tail] = event;
	context->queue_tail++;
	return context->queue_tail;
}

static int process_and_handle_if_needed(Context_t *context, char * buffer, int64_t * total_readed) {
	int status;
	int msg_begin = 0;
	int i;
	char aux[BUFSIZ];

	for (i = 0; i < *total_readed; i++){
		if (buffer[i] == '\n'){
			strncpy(aux, &(buffer[msg_begin]), i-msg_begin+1);
			aux[msg_begin + i + 1] = '\0';
			status = handle_control_message(context, aux);
			TRACE_PRINT("<<<RECV<<<\t%s", aux);
			msg_begin = i+1;
			if (status <= 0){
				ERROR_PRINT("could not handle message:'%s'", buffer);
			}
		}
	}
	for(i=0; i+msg_begin < *total_readed; i++) {
		buffer[i] = buffer[i+msg_begin];
	}
	*total_readed = i;
	return 1;
}


static void *recieve_and_handle_message(void *argspt) {
	static char buffer[BUFSIZ];
	static int64_t readed = 0;
	int status;
	struct receiver_args_st *args = argspt;
	Context_t *context = args->c;
	if (!((context->queue_head == 10) && (context->queue_tail == 12))) {
		status = receive_control_message(context->serverinfo, &(buffer[readed]), BUFSIZ - readed, __FILE__, __LINE__);

		if (status == 0){
			ERROR_PRINT("disconnection from control socket");
			saida (1);
		} else if (status > 0){
			readed += status;
			process_and_handle_if_needed(context, buffer,&readed);
		}/* else{
			handle_fatal_error(context);
		}*/
	}
	pthread_mutex_unlock(&(context->mutex));
	return context;
}
