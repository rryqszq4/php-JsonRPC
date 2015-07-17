/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_JSONRPC_H
#define PHP_JSONRPC_H

#include "php.h"
#include "ext/standard/php_smart_str.h"

#include <curl/curl.h>
#include <curl/multi.h>

extern zend_module_entry jsonrpc_module_entry;
#define phpext_jsonrpc_ptr &jsonrpc_module_entry

#ifdef PHP_WIN32
#	define PHP_JSONRPC_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_JSONRPC_API __attribute__ ((visibility("default")))
#else
#	define PHP_JSONRPC_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(jsonrpc);
PHP_MSHUTDOWN_FUNCTION(jsonrpc);
PHP_RINIT_FUNCTION(jsonrpc);
PHP_RSHUTDOWN_FUNCTION(jsonrpc);
PHP_MINFO_FUNCTION(jsonrpc);

PHP_FUNCTION(confirm_jsonrpc_compiled);	/* For testing, remove later. */

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     

ZEND_BEGIN_MODULE_GLOBALS(jsonrpc)
	long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(jsonrpc)
*/

#define PHP_CURL_STDOUT 0
#define PHP_CURL_FILE   1
#define PHP_CURL_USER   2
#define PHP_CURL_DIRECT 3
#define PHP_CURL_RETURN 4
#define PHP_CURL_ASCII  5
#define PHP_CURL_BINARY 6
#define PHP_CURL_IGNORE 7

typedef struct {
  zval            *func_name;
  zend_fcall_info_cache fci_cache;
  FILE            *fp;
  smart_str       buf;
  int             method;
  int             type;
  zval    *stream;
} php_curl_write;

typedef struct {
  zval            *func_name;
  zend_fcall_info_cache fci_cache;
  FILE            *fp;
  long            fd;
  int             method;
  zval    *stream;
} php_curl_read;

typedef struct {
  zval    *func_name;
  zend_fcall_info_cache fci_cache;
  int             method;
} php_curl_progress;

typedef struct {
  php_curl_write *write;
  php_curl_write *write_header;
  php_curl_read  *read;
  zval           *passwd;
  zval           *std_err;
  php_curl_progress *progress;
} php_curl_handlers;

struct _php_curl_error  {
  char str[CURL_ERROR_SIZE + 1];
  int  no;
};

struct _php_curl_send_headers {
  char *str;
  size_t str_len;
};

struct _php_curl_free {
  zend_llist str;
  zend_llist post;
  zend_llist slist;
};

typedef struct {
  struct _php_curl_error   err;
  struct _php_curl_free    *to_free;
  struct _php_curl_send_headers header;
  void ***thread_ctx;
  CURL                    *cp;
  php_curl_handlers       *handlers;
  long                     id;
  unsigned int             uses;
  zend_bool                in_callback;
  zval                     *clone;
} php_curl;

typedef struct {
  int    still_running;
  CURLM *multi;
  zend_llist easyh;
} php_curlm;

/* In every utility function you add that needs to use variables 
   in php_jsonrpc_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as JSONRPC_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define JSONRPC_G(v) TSRMG(jsonrpc_globals_id, zend_jsonrpc_globals *, v)
#else
#define JSONRPC_G(v) (jsonrpc_globals.v)
#endif

#endif	/* PHP_JSONRPC_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
