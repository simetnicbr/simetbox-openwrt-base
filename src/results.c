/** ----------------------------------------
* @file  results.c
* @brief
* @author  Rafael de O. Lopes Goncalves
* @date Nov 1, 2011
*------------------------------------------*/

#include "config.h"

#include "results.h"
#include <stdio.h>
#include <stdlib.h>
#include "unp.h"



char *result_string_rtt(struct result_st r)
{
	char *s;
	s = malloc(sizeof(char) * 300);
	snprintf(s, 300, "%c %"PRI64" %"PRI64" %"PRISIZ" %"PRISIZ" %"PRISIZ" %"PRISIZ" %"PRISIZ" %"PRISIZ" %"PRI64"",
			r.direction,
			r.event_start_time_in_us / 1000,
			r.event_end_time_in_us / 1000,
			r.number_of_packages,
			r.package_size_in_bytes,
			r.number_of_bytes,
			r.lost_packages, r.outoforder_packages, r.send_rate_in_bps, r.time_in_us);
	return s;
}

char *result_string_udp(struct result_st r)
{
	char *s;
	s = malloc(sizeof(char) * 300);
/*	INFO_PRINT ("event_start_time_in_us: %"PRI64" event_end_time_in_us: %"PRI64" number_of_packages: %"PRISIZ" package_size_in_bytes: %"PRISIZ" number_of_bytes: %"PRISIZ" send_rate_in_bps: %"PRISIZ"\n\n",
			r.event_start_time_in_us / 1000,
			r.event_end_time_in_us / 1000,
			r.number_of_packages,
			r.package_size_in_bytes,
			r.number_of_bytes,
			r.send_rate_in_bps);
*/
	snprintf(s, 300, "%c %"PRI64" %"PRI64" %"PRISIZ" %"PRISIZ" %"PRISIZ" 0 0 %"PRISIZ"",
			r.direction,
			r.event_start_time_in_us / 1000,
			r.event_end_time_in_us / 1000,
			r.number_of_packages,
			r.package_size_in_bytes,
			r.number_of_bytes,
			r.send_rate_in_bps);
	return s;
}


char *result_string_jitter(struct result_st r)
{
	TRACE_PRINT("result_string_rtt");
	char *s;
	s = malloc(sizeof(char) * 300);
	snprintf(s, 300, "%c %"PRI64" %"PRI64" %"PRISIZ" %"PRI64" %"PRI64" %"PRI64" %"PRI64" %ld %"PRI64,
			r.direction,
			r.event_start_time_in_us / 1000,
			r.event_end_time_in_us / 1000,
			r.number_of_packages,
			r.package_size_in_bytes,
			r.number_of_bytes,
			r.lost_packages, r.outoforder_packages, 0L, r.time_in_us);
	return s;
}






char *result_string_tcp(struct result_st r)
{
	char *s;
	s = malloc(sizeof(char) * 300);
		snprintf(s, 300, "%c %"PRI64" %"PRI64" %"PRISIZ" %"PRISIZ" %"PRISIZ"",
			r.direction,
			r.event_start_time_in_us / 1000,
			r.event_end_time_in_us / 1000,
			r.number_of_packages, r.package_size_in_bytes, r.number_of_bytes);
	return s;
}

int store_results(FILE * file, const char *type, const char *result_string)
{
	return fprintf(file, "%s=%s\n", type, result_string);
}
int store_result_recvmsg(FILE * file, const char *result_string)
{
	return fprintf(file, "%s\n",result_string);
}

