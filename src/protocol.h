/**
* @file   protocol.h
* @brief  
* @author  Rafael de O. Lopes Gonçalves
* @date 26 de outubro de 2011
*/

/*
* Changelog:
* Created: 26 de outubro de 2011
* Last Change: 24 de fevereiro de 2012
*/

#ifndef PROTOCOL_H
#define PROTOCOL_H


#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "debug.h"
#include "unp.h"
#include "messages.h"
#include "simet_tools.h"
#include "local_simet_config.h"

#include <openssl/ssl.h>

/* ######################################################################## */

/**
* @brief Information of a simet server.
* All of its structures have its initialization and (de)allocation done by
* get_simet_servers and free_server_infos.
*
*/

//#define GETSERVERSINFOS_OLD

struct simet_server_info_st {

	char *address_text; /**< server ip numerical address */
	int priority; /**< Priority of this simet server. Smaller has more priority*/
	char *description; /**< */
#ifdef GETSERVERSINFOS_OLD
	char *symbol;               /*< server simbol */
	char *asn_origin; /**< */
	char *asn_participant; /**< */
#else
	int id_pool_server;
	char *location;
#endif
	uint64_t rtt; /**< Time to reach and awnser by ICMP echo */
	int64_t clockoffset_in_usec; /**< Clock offset from this computer for the server in microseconds*/
	int socket_control_fd;
	SSL * ssl;
	struct simet_server_info_st *next; /**< Linked list next element*/

};
typedef struct simet_server_info_st Simet_server_info_t;

struct event_st;
#define CONTEXT_QUEUE_MAX_SIZE  BUFSIZ
#define CONTEXT_QUEUE_MAX_TESTS 16

enum test_type {
	RTT_TEST_TYPE = 1,
	TCP_TESTS_TYPE,
	TCP_DOWNLOAD_TESTS_TYPE,
	TCP_UPLOAD_TESTS_TYPE,
	UDP_UPLOAD_TESTS_TYPE,
	UDP_DOWNLOAD_TESTS_TYPE,
	JITTER_UPLOAD_TESTS_TYPE,
	JITTER_DOWNLOAD_TESTS_TYPE,
	LATENCY_DOWNLOAD_TESTS_TYPE,
	LATENCY_UPLOAD_TESTS_TYPE,
	TEST_TYPES_SIZE
};

enum test_direction {
	DOWNLOAD = 'D',
	UPLOAD  = 'U'
};

struct context_st {
	struct event_st *queue[CONTEXT_QUEUE_MAX_SIZE];
	enum test_type tests_to_do[CONTEXT_QUEUE_MAX_TESTS];

	int64_t queue_head; /**< indice do primeiro elemento da fila.*/
	int64_t queue_tail; /**< indice da proxima posicao vazia.*/
	Simet_server_info_t *serverinfo; /**<informacoes do servidor */
	int64_t next_test;
	int64_t servertime; /**<tempo no servidor */
	int64_t serverlocaltime; /**<tempo no servidor */
	FILE *output;
	char * port;
	int family;
	void *data;  /**< extra space used by various tests*/
	char *external_ipaddress;
	char *my_city;
	char *ptt_location;
	char * id_cookie;
	char * hash_measure;
	char * tipo_conexao;
	SSL_CTX * ssl_ctx;
	pthread_mutex_t  mutex;
	int64_t max_rate_in_bps;
	uint64_t bytes_tcp_upload_test;
	CONFIG_SIMET *config;
};
typedef struct context_st Context_t;

enum event_action {
	DISPATCH, WAITING, END_LOOP
};
typedef int (*event_dispatcher_t) (const struct event_st * event, Context_t * context);
struct event_st {
	event_dispatcher_t dispatcher;
	enum event_action action;
	const void *data;
};
typedef struct event_st Event_t;

/* ############################################ functions ######### */

int init_simet_server_address(Simet_server_info_t * serverinfo);

/* ####################################### events management ##### */
/**
* @brief Fills the initial data needed for context also allocate memory for context.
* @param[out] context a pointer to a pointer where the new address will be allocated.
*
* @return  -1 if in case of error. A number > 0 otherwise.
*/
int create_context(Context_t ** context);

/**
* @brief Register the event into the context queue.
* @param event
*
* @return  -1 if in case of error. A number > 0 otherwise.
*/
int register_event(Context_t * context, Event_t * event);

/**
* @brief starts a loop over this context.
*
*/
void do_events_loop(Context_t * context);

Event_t *create_event(event_dispatcher_t dispatcher, enum event_action action, void *data);

int finish_context(Context_t ** context_pt);

/* ####################################### deal with messages ##### */

int send_control_message(Simet_server_info_t * si, const message_t message);
int receive_control_message(Simet_server_info_t * si, char * buffer, int64_t max_len, char *file, int linha);


#endif
