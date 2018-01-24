# initial information about the project
AC_PREREQ([2.63])
AC_INIT([simet-tools],[1],[michel@nic.br])
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

# ------------------------------------------------------------------------
# SSL Code
# ------------------------------------------------------------------------

# Check for ssl includes (taken from the stunnel project)
checksslincldir() { :
    if test -f "$1/openssl/ssl.h"; then
        sslincldir="$1"
        return 0
    fi
    return 1
}

# Check for ssl libraries 
checkssllibdirdynamic() { :
    if test "(" -f "$1/libcrypto.so" -o -f "$1/libcrypto.dylib" ")"  -a \
            "(" -f "$1/libssl.so"    -o -f "$1/libssl.dylib" ")" ; then
        ssllibdir="$1"
        return 0
    fi
    return 1
}

checkssllibdirstatic() { :
    if test "(" -f "$1/libcrypto.a" ")"  -a \
            "(" -f "$1/libssl.a" ")" ; then
        ssllibdir="$1"
        return 0
    fi
    return 1
}

# Check if we want to have SSL

AC_MSG_CHECKING([for static SSL support])

AC_ARG_WITH(ssl-static,
    [  --with-ssl-static=DIR       location of SSL installation],
    [
        dnl Check the specified location only
        for dir in "$withval" "$withval/include"; do
            checksslincldir "$dir"
        done
        for dir in "$withval" "$withval/lib"; do
            checkssllibdirstatic "$dir" && break 2
        done
        use_sslstatic=1
        LDFLAGS="`echo $LDFLAGS | sed -e 's/-rdynamic/-ldl/g'`"
        AC_MSG_RESULT([enabled])
        AC_DEFINE([HAVE_OPENSSL], 1, [Define to 1 if you have openssl.])
        AC_SUBST(sslincldir)
        AC_SUBST(ssllibdir)
#        CFLAGS="$CFLAGS -I$sslincldir"
        LIBS="$LIBS $ssllibdir/libssl.a $ssllibdir/libcrypto.a -ljson-c -lsimetconfig -lpthread"
#        LIBS="$LIBS $ssllibdir/libssl.a -ljson-c -lsimetconfig"
    ],
    [
        use_sslstatic=0
        AC_MSG_RESULT([disabled])
    ]
)

if test "$use_sslstatic" = "0"
then
    AC_MSG_CHECKING([for SSL support])


    # Check for SSL directory (taken from the stunnel project)
    AC_ARG_WITH(ssl-dir,
                [  --with-ssl-dir=DIR       location of SSL installation],
                [
                 dnl Check the specified location only
                 for dir in "$withval" "$withval/include"; do
                     checksslincldir "$dir"
                 done
                 for dir in "$withval" "$withval/lib"; do
                     checkssllibdirdynamic "$dir" && break 2
                 done
                 ]
                )

    AC_MSG_CHECKING([for SSL include directory])
    AC_ARG_WITH(ssl-incl-dir,
                [  --with-ssl-incl-dir=DIR  location of installed SSL include files],
                [
                 dnl Check the specified location only
                 checksslincldir "$withval"
                 ],
                 [
                  if test -z "$sslincldir"; then
                      dnl Search default locations of SSL includes
                      for maindir in /usr /usr/local /usr/lib /usr/pkg /var /opt /usr/sfw; do
                          for dir in "$maindir/include"\
                              "$maindir/include/openssl"\ 
                          "$maindir/include/ssl"\
                              "$maindir/ssl/include"; do
                          checksslincldir $dir && break 2
                      done
                  done
              fi
              ]
                 )
    if test -z "$sslincldir"; then
        AC_MSG_RESULT([Not found])
        echo
        echo "Couldn't find your SSL header files."
        echo "Use --with-ssl-incl-dir option to fix this problem or disable"
        echo "the SSL support with --without-ssl"
        echo
        exit 1
    fi
    AC_MSG_RESULT([$sslincldir])

    AC_MSG_CHECKING([for SSL library directory])
    AC_ARG_WITH(ssl-lib-dir,
                [  --with-ssl-lib-dir=DIR   location of installed SSL library files],
                [
                 dnl Check the specified location only
                 checkssllibdirdynamic "$withval"
                 ],
                 [
                  if test -z "$ssllibdir"; then
                      dnl Search default locations of SSL libraries
                      for maindir in /usr \
                          /usr/local \
                          /usr/pkg \
                          /var /opt \
                          /usr/sfw; do
                      for dir in $maindir \
                          $maindir/openssl \
                          $maindir/ssl \
                          $maindir/lib \
                          $maindir/lib/openssl \
                          $maindir/lib/ssl \
                          $maindir/ssl/lib \
                          $maindir/lib/64 \
                          $maindir/lib/64/openssl \
                          $maindir/lib/64/ssl \
                          $maindir/ssl/lib/64 \
                          $maindir/lib64 \
                          $maindir/lib64/openssl \
                          $maindir/lib64/ssl \
                          $maindir/lib/x86_64-linux-gnu \
                          $maindir/ssl/lib64; do
                      checkssllibdirdynamic $dir && break 2
                  done
              done
          fi
          ]
                 )
    if test -z "$ssllibdir"; then
        AC_MSG_RESULT([Not found])
        echo
        echo "Couldn't find your SSL library files."
        echo "Use --with-ssl-lib-dir option to fix this problem or disable the"
        echo "SSL support with --without-ssl"
        echo
        exit 1
    fi
    AC_MSG_RESULT([$ssllibdir])

    AC_DEFINE([HAVE_OPENSSL], 1, [Define to 1 if you have openssl.])
    AC_SUBST(sslincldir)
    AC_SUBST(ssllibdir)
    # Add SSL includes and libraries
    if test "$sslincldir" -a "$ssllibdir"
    then
        if test "x$ARCH" = "xDARWIN"; then
         # Darwin already knows about ssldirs
         LIBS="$LIBS -lssl -lcrypto"
#         LIBS="$LIBS -lssl"
        elif test -f "/usr/kerberos/include/krb5.h"; then
         # Redhat 9 compilation fix:
         CFLAGS="$CFLAGS -I$sslincldir -I/usr/kerberos/include"
         LIBS="$LIBS -L$ssllibdir -lssl -lcrypto"
#         LIBS="$LIBS -L$ssllibdir -lssl"
        else
         #CFLAGS="$CFLAGS -I$sslincldir"
         LIBS="$LIBS -L$ssllibdir -lssl -lcrypto"
#         LIBS="$LIBS -L$ssllibdir -lssl"
        fi
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

# generate the final Makefile etc.
AC_OUTPUT
echo "                 SSL support: enabled"
echo "       SSL include directory: ${sslincldir}"
echo "       SSL library directory: ${ssllibdir}"
echo "              Compiler flags: ${CFLAGS}"
echo "                Linker flags: ${LIBS}"

