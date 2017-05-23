/**
 * @file   server_finder.h
 * @brief  Find better servers from the simet location server.
 * @author  Rafael Lopes <rafael@nic.br>
 * @date 13 de outubro de 2011
 */

/*
 * Changelog:
 * Created: 13 de outubro de 2011
 * Last Change: 15 de fevereiro de 2012
 */

#ifndef LIBSERVER_FINDER_H
#define LIBSERVER_FINDER_H

#include <time.h>
#include <netdb.h>
#include "unp.h"
#include "protocol.h"
#define NUM_RTT_SELECT_SERVER 5
#define  DEFAULT_LOCATION_SERVER_PORT "52424"

/**
 * @brief Get a ordered linked list of simet servers.
 *
 * @param location_server address of location server. This will be resolved by  getaddrinfo_a
 * @param head result parameter 
 *
 * @return  -1 if in case of error. A number > 0 otherwise.
 */
#ifdef GETSERVERSINFOS_OLD
int get_simet_servers(const char *location_server, const char *desired_ptt, Simet_server_info_t ** head);
#else
int get_simet_servers(char *host, char *context_root, char *port, const char *location_server, Simet_server_info_t ** head, int family, SSL_CTX *ssl_ctx);
#endif
void free_server_info(Simet_server_info_t * node);


/**
 * @brief Free the memory of the linked list of server infos.
 *
 * @param[out] head head element.
 */
void free_server_infos_list(Simet_server_info_t * head);

/**
 * @brief Set the rtt field in server_info using the ICMP echo protocol.
 *
 * @param server_info  where the rtt will be stored.
 *
 * @return  -1 if in case of error. A number > 0 otherwise.
 */
int test_server_time(Simet_server_info_t * server_info, int family);

#endif
