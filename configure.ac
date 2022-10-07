# initial information about the project
AC_PREREQ([2.63])
AC_INIT([simet-tools],[106][medicoes@simet.nic.br])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([src/config.h])

LT_INIT


# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT8_T

# Checks for library functions.
#AC_FUNC_MALLOC
AC_CHECK_FUNCS([strchr])

AC_CONFIG_FILES([Makefile
                 src/Makefile])

CFLAGS="$OLD_CFLAGS -Wall -I/usr/include/json-c"
LIBS="-ljson-c -lsimetconfig -Wl,--gc-sections"

# Checks for header files.
# -Wmissing-field-initializers \
WARNINGCFLAGS=" -Wall -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations \
 -Wwrite-strings -Wclobbered  -Wempty-body \
 -Wold-style-declaration  -Wmissing-parameter-type -Wunused-but-set-parameter \
 -Waddress -Warray-bounds -Wchar-subscripts -Wenum-compare -Wimplicit-function-declaration -Wmissing-braces \
 -Wpointer-sign -Wreturn-type -Wsequence-point -Wcomment -Wformat -Wmain \
 -Wstrict-aliasing -Wstrict-overflow=1 -Wswitch -Wtrigraphs -Wstack-usage=16384 \
 -Wunknown-pragmas -Wunused-function -Wunused-label -Wunused-value -Wvolatile-register-var -Wuninitialized -Waddress"
AC_ARG_ENABLE(debug, [--enable-debug turn -g on and off], CFLAGS="$CFLAGS -ggdb3 -O0 -fstack-usage", CFLAGS="$CFLAGS -DNDEBUG -Os -ffunction-sections -fdata-sections")
#AC_ARG_ENABLE(debug, [--enable-debug turn -g on and off], CFLAGS="$CFLAGS -ggdb3 -O0", CFLAGS="$CFLAGS -DNDEBUG -O0 -ggdb3")
AC_ARG_ENABLE(trace, [--enable-trace turn trace prints off], CFLAGS="$CFLAGS", CFLAGS="$CFLAGS -DNTRACE")
AC_ARG_ENABLE(morewarnings, [--enable-morewarnings turn compilation warnings on], CFLAGS="$CFLAGS $WARNINGCFLAGS", CFLAGS="$CFLAGS")



# Checks for libraries.
AC_CHECK_FUNCS(clock_gettime, [], [AC_CHECK_LIB(rt, clock_gettime)])
AC_CHECK_LIB(simetconfig, config_simet)
if test "$ac_cv_lib_simetconfig_config_simet" = no ; then
   LDFLAGS="$LDFLAGS -L/usr/local/lib"
fi

AC_CHECK_LIB(pthread, pthread_create)
if test "$ac_cv_lib_pthread_pthread_create" = yes ; then
   CFLAGS="$CFLAGS -pthread"
else
   AC_CHECK_LIB(pthreads, pthread_create)
   if test "$ac_cv_lib_pthreads_pthread_create" = yes ; then
      CFLAGS="$CFLAGS -pthread"
   fi
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h netdb.h stdlib.h string.h sys/socket.h sys/time.h unistd.h sys/select.h bits/select.h simet_config.h])
AC_CHECK_HEADERS(sys/types.h sys/socket.h sys/time.h time.h netinet/in.h arpa/inet.h errno.h fcntl.h netdb.h signal.h stdio.h stdlib.h string.h sys/stat.h sys/uio.h unistd.h sys/wait.h sys/un.h sys/select.h poll.h strings.h sys/ioctl.h sys/filio.h sys/sockio.h pthread.h sys/sysctl.h poll.h netconfig.h netdir.h stropts.h sys/select.h bits/select.h)
AC_HEADER_TIME
AC_CHECK_FUNCS([strrchr])
# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T


# Checks for library functions.
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero gettimeofday memset socket strdup])


AC_CHECK_FUNCS(bzero)
AC_CHECK_FUNCS(getaddrinfo)
AC_CHECK_FUNCS(gethostname)
AC_CHECK_FUNCS(gethostbyname2)
AC_CHECK_FUNCS(gethostbyname_r)
AC_CHECK_FUNCS(getnameinfo)
AC_CHECK_FUNCS(hstrerror)
AC_CHECK_FUNCS(if_nametoindex)
AC_CHECK_FUNCS(inet_aton)
AC_CHECK_FUNCS(inet_pton)
AC_CHECK_FUNCS(isfdtype)
AC_CHECK_FUNCS([poll])
AC_CHECK_FUNCS([pselect])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([snprintf])
AC_CHECK_FUNCS([sockatmark])
AC_CHECK_FUNCS([vsnprintf])
AC_CHECK_FUNCS([inet_ntoa])

PKG_CHECK_MODULES(
  [OPENSSL], [openssl > 0.9],
  [
    AC_DEFINE([HAVE_OPENSSL], 1, [Define to 1 if you have openssl.])
    CFLAGS="$CFLAGS $OPENSSL_CFLAGS"
    LIBS="$LIBS $OPENSSL_LIBS"
  ],
  [
    AC_MSG_ERROR([openssl not found])
  ]
)
# generate the final Makefile etc.
AC_OUTPUT
echo "                 SSL support: enabled"
echo "              Compiler flags: ${CFLAGS}"
echo "                Linker flags: ${LIBS}"

