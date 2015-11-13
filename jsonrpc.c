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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/basic_functions.h"
#include "ext/standard/php_rand.h"
#include "SAPI.h"
#include "ext/json/php_json.h"
#include "ext/standard/file.h"
#include "php_jsonrpc.h"

#include "jsr_client.h"
#include "jsr_server.h"

/* If you declare any globals in php_jsonrpc.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(jsonrpc)
*/

/* True global resources - no need for thread safety here */


/* {{{ jsonrpc_functions[]
 *
 * Every user visible function must have an entry in jsonrpc_functions[].
 */
const zend_function_entry jsonrpc_functions[] = {
	PHP_FE(confirm_jsonrpc_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in jsonrpc_functions[] */
};
/* }}} */

/* {{{ jsonrpc_module_entry
 */
zend_module_entry jsonrpc_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"jsonrpc",
	jsonrpc_functions,
	PHP_MINIT(jsonrpc),
	PHP_MSHUTDOWN(jsonrpc),
	PHP_RINIT(jsonrpc),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(jsonrpc),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(jsonrpc),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_JSONRPC
ZEND_GET_MODULE(jsonrpc)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("jsonrpc.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_jsonrpc_globals, jsonrpc_globals)
    STD_PHP_INI_ENTRY("jsonrpc.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_jsonrpc_globals, jsonrpc_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_jsonrpc_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_jsonrpc_init_globals(zend_jsonrpc_globals *jsonrpc_globals)
{
	jsonrpc_globals->global_value = 0;
	jsonrpc_globals->global_string = NULL;
}
*/
/* }}} */









/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(jsonrpc)
{
	//le_curl = zend_register_list_destructors_ex(_php_curl_close, NULL, "curl", module_number);
	
	//le_curl_multi_handle = zend_register_list_destructors_ex(NULL, _php_curlm_close_persistent, "curl_multi", module_number);
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/

	/*zend_class_entry jsonrpc_server_class_entry;
	INIT_CLASS_ENTRY(jsonrpc_server_class_entry, "Jsonrpc_Server", jsonrpc_server_class_functions);
	php_jsonrpc_server_entry = zend_register_internal_class(&jsonrpc_server_class_entry TSRMLS_CC);
*/
	/*zend_class_entry jsonrpc_client_class_entry;
	INIT_CLASS_ENTRY(jsonrpc_client_class_entry, "Jsonrpc_Client", jsonrpc_client_class_functions);
	php_jsonrpc_client_entry = zend_register_internal_class(&jsonrpc_client_class_entry TSRMLS_CC);
*/
	jsonrpc_client_init(module_number TSRMLS_CC);

	jsonrpc_server_init(module_number TSRMLS_CC);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(jsonrpc)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(jsonrpc)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(jsonrpc)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(jsonrpc)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "php jsonrpc support", "enabled");
	php_info_print_table_row(2, "php jsonrpc version", PHP_JSONRPC_VERSION);
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_jsonrpc_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_jsonrpc_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "jsonrpc", arg);
	RETURN_STRINGL(strg, len, 0);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
