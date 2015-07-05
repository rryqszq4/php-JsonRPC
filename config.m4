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

  PHP_NEW_EXTENSION(jsonrpc, jsonrpc.c, $ext_shared)
fi
