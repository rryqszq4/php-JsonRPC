dnl $Id$
dnl config.m4 for extension jsonrpc

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(jsonrpc, for jsonrpc support,
dnl Make sure that the comment is aligned:
dnl [  --with-jsonrpc             Include jsonrpc support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(jsonrpc, whether to enable jsonrpc support,
dnl Make sure that the comment is aligned:
[  --enable-jsonrpc           Enable jsonrpc support])

PHP_ARG_WITH(curl, for curl protocol support,
[  --with-curl[=DIR]       Include curl protocol support])

if test "$PHP_JSONRPC" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-jsonrpc -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/jsonrpc.h"  # you most likely want to change this
  dnl if test -r $PHP_JSONRPC/$SEARCH_FOR; then # path given as parameter
  dnl   JSONRPC_DIR=$PHP_JSONRPC
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for jsonrpc files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       JSONRPC_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$JSONRPC_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the jsonrpc distribution])
  dnl fi

  dnl # --with-jsonrpc -> add include path
  dnl PHP_ADD_INCLUDE($JSONRPC_DIR/include)

  dnl # --with-jsonrpc -> check for lib and symbol presence
  dnl LIBNAME=jsonrpc # you may want to change this
  dnl LIBSYMBOL=jsonrpc # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $JSONRPC_DIR/lib, JSONRPC_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_JSONRPCLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong jsonrpc lib version or lib not found])
  dnl ],[
  dnl   -L$JSONRPC_DIR/lib -lm
  dnl ])
  dnl
  dnl PHP_SUBST(JSONRPC_SHARED_LIBADD)

  if test -r $PHP_CURL/include/curl/easy.h; then
    CURL_DIR=$PHP_CURL
  else
    AC_MSG_CHECKING(for cURL in default path)
    for i in /usr/local /usr; do
      if test -r $i/include/curl/easy.h; then
        CURL_DIR=$i
        AC_MSG_RESULT(found in $i)
        break
      fi
    done
  fi

  if test -z "$CURL_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please reinstall the libcurl distribution -
    easy.h should be in <curl-dir>/include/curl/)
  fi

  PHP_ADD_INCLUDE($CURL_DIR/include)
  PHP_EVAL_LIBLINE($CURL_LIBS, YAR_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_DIR/$PHP_LIBDIR, YAR_SHARED_LIBADD)

  PHP_NEW_EXTENSION(jsonrpc, jsonrpc.c jsr_server.c jsr_client.c jsr_curl.c jsr_list.c jsr_epoll.c jsr_utils.c, $ext_shared)
fi
