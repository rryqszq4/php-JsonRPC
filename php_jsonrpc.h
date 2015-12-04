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
  | Author: rryqszq4                                                     |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_JSONRPC_H
#define PHP_JSONRPC_H

#include "php.h"
//#include "ext/standard/php_smart_str.h"

#define PHP_JSONRPC_VERSION "0.0.8"

#define PHP_JSONRPC_DEBUG   0

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

#if PHP_VERSION_ID < 70000
#define JSR_ZVAL_STRINGL(zval, name, len, dup){ \
    ZVAL_STRINGL(zval, name, len, dup);\
  }
#else
#define JSR_ZVAL_STRINGL(zval, name, len, dup){ \
    ZVAL_STRINGL(zval, name, len);\
  }
#endif

#if PHP_VERSION_ID < 70000
#define JSR_ZVAL_STRING(zval, name, dup){ \
    ZVAL_STRING(zval, name, dup);\
  }
#else
#define JSR_ZVAL_STRING(zval, name, dup){ \
    ZVAL_STRINGL(zval, name);\
  }
#endif

#if PHP_VERSION_ID < 70000
#define JSR_STRING char
#else
#define JSR_STRING zend_string
#endif

extern int le_jsr_epoll_persist;
extern int le_jsr_curlm_persist;

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
