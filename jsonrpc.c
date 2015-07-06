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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_jsonrpc.h"

/* If you declare any globals in php_jsonrpc.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(jsonrpc)
*/

/* True global resources - no need for thread safety here */
static int le_jsonrpc;
static zend_class_entry *php_jsonrpc_server_entry;

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

PHP_FUNCTION(jsonrpc_server_new)
{
	zval *payload, *callbacks, *classes;
	zval *object = getThis();


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", 
		&payload, &callbacks, &classes) == FAILURE)
	{
		RETURN_NULL();
	}

	if (Z_TYPE_P(callbacks) == IS_ARRAY){
		add_property_zval(object, "callbacks", callbacks);
	}else {
		array_init(callbacks);
		add_property_zval(object, "callbacks", callbacks);
	}

	add_property_zval(object, "payload", payload);
	add_property_zval(object, "classes", classes);

	payload = NULL;
	payload = zend_read_property(
			php_jsonrpc_server_entry, getThis(), "payload", sizeof("payload")-1, 0 TSRMLS_CC
		);
	//php_var_dump(&payload, 1 TSRMLS_CC);

	php_printf("jsonrpc_server new\n");
}

PHP_FUNCTION(jsonrpc_server_register)
{
	zval *name, *callback;
	zval *callbacks;
	zval *object = getThis();
	zval *a;

	//MAKE_STD_ZVAL(callback);

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", 
		&name, &callback) == FAILURE)
	{
		RETURN_NULL();
	}

	add_property_zval(object, "callback", callback);

	callback = NULL;
	callback = zend_read_property(
			php_jsonrpc_server_entry, getThis(), "callback", sizeof("callback")-1, 0 TSRMLS_CC
		);

	php_var_dump(&callback, 1 TSRMLS_CC);

	callbacks = zend_read_property(
			php_jsonrpc_server_entry, getThis(), "callbacks", sizeof("callbacks")-1, 0 TSRMLS_CC
		);

	add_assoc_zval(callbacks, Z_STRVAL_P(name), callback);

	//add_index_zval(callbacks, 1, callback);

	RETURN_ZVAL(getThis(),1,0);

}

PHP_FUNCTION(jsonrpc_server_execute)
{

}

static zend_function_entry jsonrpc_server_class_functions[] = {
	PHP_FALIAS(__construct, jsonrpc_server_new, NULL)
	PHP_FALIAS(register, jsonrpc_server_register, NULL)
	PHP_FALIAS(execute, jsonrpc_server_execute, NULL)
	{NULL, NULL, NULL}
};

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(jsonrpc)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/

	zend_class_entry jsonrpc_server_class_entry;
	INIT_CLASS_ENTRY(jsonrpc_server_class_entry, "Jsonrpc_Server", jsonrpc_server_class_functions);
	php_jsonrpc_server_entry = zend_register_internal_class(&jsonrpc_server_class_entry TSRMLS_CC);

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
	php_info_print_table_header(2, "jsonrpc support", "enabled");
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
