/* include unph */
/* Our own header.  Tabs are set for 4 spaces, not 8 */

#ifndef	__unp_h
#define	__unp_h


/* If anything changes in the following list of #includes, must change
 acsite.m4 also, for configure's tests. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include	<sys/types.h>   /* basic system data types */
#include	<sys/socket.h>  /* basic socket definitions */
#include	<sys/time.h>    /* timeval{} for select() */
#include	<time.h>        /* timespec{} for pselect() */
#include	<netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include	<netinet/tcp.h> /* TCP socket options */
#include	<arpa/inet.h>   /* inet(3) functions */
#include	<errno.h>
#include	<fcntl.h>       /* for nonblocking */
#include	<netdb.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/stat.h>    /* for S_xxx file mode constants */
#include	<sys/uio.h>     /* for iovec{} and readv/writev */
#include	<unistd.h>
#include	<sys/wait.h>
#include	<sys/un.h>      /* for Unix domain sockets */
#include <stdint.h>

#ifdef	HAVE_SYS_SELECT_H
# include	<sys/select.h>  /* for convenience */
#endif

#ifdef	HAVE_POLL_H
# include	<poll.h>        /* for convenience */
#endif

#ifdef	HAVE_STRINGS_H
# include	<strings.h>     /* for convenience */
#endif

/* Three headers are normally needed for socket/file ioctl's:
 * <sys/ioctl.h>, <sys/filio.h>, and <sys/sockio.h>.
 */
#ifdef	HAVE_SYS_IOCTL_H
# include	<sys/ioctl.h>
#endif
#ifdef	HAVE_SYS_FILIO_H
# include	<sys/filio.h>
#endif
#ifdef	HAVE_SYS_SOCKIO_H
# include	<sys/sockio.h>
#endif

#ifdef	HAVE_PTHREAD_H
# include	<pthread.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

/* Older resolvers do not have gethostbyname2() */
#ifndef	HAVE_GETHOSTBYNAME2
#define	gethostbyname2(host,family)		gethostbyname((host))
#endif

/* Posix.1g requires that an #include of <poll.h> DefinE INFTIM, but many
   systems still DefinE it in <sys/stropts.h>.  We don't want to include
   all the streams stuff if it's not needed, so we just DefinE INFTIM here.
   This is the standard value, but there's no guarantee it is -1. */
#ifndef INFTIM
#define INFTIM          (-1)    /* infinite poll timeout */
#endif

/* utils######################################################################### */
#ifndef MIN
#define	MIN(a,b)	((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define	MAX(a,b)	((a) > (b) ? (a) : (b))
#endif


#define    TIMEVAL_MICROSECONDS(tv)  ((tv.tv_sec * 1e6L) + tv.tv_usec)
#define    TIMESPEC_NANOSECONDS(ts)  ((ts.tv_sec * 1e9L) + ts.tv_nsec)


//#if OSBIT == 64
# if __WORDSIZE == 64
#define PRI64 "ld"
#define PRISIZ "ld"
#define PRUI64 "lu"
#define PRUISIZ "lu"

#else
#define PRI64 "lld"
#define PRISIZ "lld"
#define PRUI64 "llu"
#define PRUISIZ "llu"
#endif


#endif                          /* __unp_h */
