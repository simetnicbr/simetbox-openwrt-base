/**
 * @file   debug.h
 * @brief  Include debug macros
 * @author  Rafael de O. Lopes Goncalves
 * @date 20 de novembro de 2010
 */

#ifndef DEBUG_H
#define DEBUG_H



#include <features.h>
#include <stdio.h>
#include <string.h>

#if defined __cplusplus && __GNUC_PREREQ (2,95)
# define __DEBUG_VOID_CAST static_cast<void>
#else
# define __DEBUG_VOID_CAST (void)
#endif

#ifdef NDEBUG
#define DEBUG_PRINT(...)  (__DEBUG_VOID_CAST (0))
#else
#define DEBUG_PRINT(...) do { fprintf(stderr,"\nDEBUG\t"); fprintf(stderr, __VA_ARGS__);} while (0)
#endif


//#undef NTRACE
#ifdef NTRACE
#define TRACE_PRINT(fmt, ...)  (__DEBUG_VOID_CAST (0))
#else
#define TRACE_PRINT(...) do {fprintf(stderr,"\n%s:%3.d(%lld)\tTRACE  \t", __FILE__, __LINE__,(long long) time(NULL)); fprintf(stderr, __VA_ARGS__); } while (0)
#endif


#define INFO_PRINT(...) do { fprintf(stderr,"\nINFO\t"); fprintf(stderr, __VA_ARGS__);} while (0)
#define ERROR_PRINT(...) do {fprintf(stderr,"\n%s:%3.d\tERROR\t", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__);} while (0)
#define ERRNO_PRINT(...) do {fprintf(stderr,"\n%s:%3.d\tERRNO %s\t", __FILE__, __LINE__, strerror(errno)); fprintf(stderr, __VA_ARGS__);} while (0)
#endif                          /* DEBUG_H */

