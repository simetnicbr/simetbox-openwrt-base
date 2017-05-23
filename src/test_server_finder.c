/** ----------------------------------------
 * @file   test_server_finder.c
 * @brief  
 * @author  Rafael Lopes <rafael@nic.br>
 * @date 13 de outubro de 2011
 *------------------------------------------*/

/*
 * Changelog:
 * Created: 13 de outubro de 2011
 * Last Change: 16 de fevereiro de 2012
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "server_finder.h"
#include "debug.h"

const char *DEFAULT_SERVER = "200.160.4.30";

int main(int argc, char **argv)
{
    Simet_server_info_t *server_list;
    Simet_server_info_t *next;
    char *location_server;

    if(argc < 2) {
        location_server = strdup(DEFAULT_SERVER);
    } else {
        location_server = argv[1];
    }
    fprintf(stderr, "*testing location server:\'%s\'\n\n", location_server);

    get_simet_servers(location_server, &server_list, -1);

    next = server_list;

    while(next != NULL) {
#ifdef GETSERVERSINFOS_OLD
		INFO_PRINT ("symbol: %s, address_text: %s, asn_origin: %s, asn_participant: %s, priority: %d, description: %s\n", next->symbol, next->address_text, next->asn_origin, next->asn_participant, next->priority, next->description);
#else
//		INFO_PRINT ("location: %s, address_text: %s, id_pool_server: %d, priority: %d, description: %s\n", next->location, next->address_text, next->id_pool_server, next->priority, next->description);
#endif
        next = next->next;
    }
    
    free_server_infos_list(server_list);
    exit(EXIT_SUCCESS);
}
