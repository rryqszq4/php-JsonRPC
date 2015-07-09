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

static void shurrik_dump_zval(zval *data TSRMLS_DC);
static char* shurrik_return_zval_type(zend_uchar type TSRMLS_DC);

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

static char* shurrik_return_zval_type(zend_uchar type)
{
	switch (type) {
		case IS_NULL:
			return "IS_NULL";
			break;
		case IS_BOOL:
			return "IS_BOOL";
			break;
		case IS_LONG:
			return "IS_LONG";
			break;
		case IS_DOUBLE:
			return "IS_DOUBLE";
			break;
		case IS_STRING:
			return "IS_STRING";
			break;
		case IS_ARRAY:
			return "IS_ARRAY";
			break;
		case IS_OBJECT:
			return "IS_OBJECT";
			break;
		case IS_RESOURCE:
			return "IS_RESOURCE";
			break;
		default :
			return "unknown";
	}
}

static void shurrik_dump_zval(zval *data)
{
	php_printf("zval<%p> {\n", data);
	php_printf("	refcount__gc -> %d\n", data->refcount__gc);
	php_printf("	is_ref__gc -> %d\n", data->is_ref__gc);
	php_printf("	type -> %s\n", shurrik_return_zval_type(data->type));
	php_printf("	zand_value<%p> {\n", data->value);

	php_printf("		lval -> %d\n", data->value.lval);
	php_printf("		dval -> %e\n", data->value.dval);
	if (Z_TYPE_P(data) == IS_STRING){
		php_printf("		str -> %s\n", Z_STRVAL_P(data));
	}
	php_printf("		*ht<%p> {\n", data->value.ht);
	php_printf("		}\n");
	php_printf("		obj<%p> {\n", data->value.obj);
	php_printf("		}\n");
	php_printf("	}\n");

	php_printf("}\n");
}

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
	zval *name, *callback, *new;
	zval *callbacks;
	zval *object = getThis();

	//MAKE_STD_ZVAL(callback);

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", 
		&name, &callback) == FAILURE)
	{
		RETURN_NULL();
	}

	/*add_property_zval(object, "callback", callback);

	callback = NULL;
	MAKE_STD_ZVAL(callback);
	callback = zend_read_property(
			php_jsonrpc_server_entry, getThis(), "callback", sizeof("callback")-1, 0 TSRMLS_CC
		);

	php_var_dump(&callback, 1 TSRMLS_CC);
	*/


	callbacks = zend_read_property(
			php_jsonrpc_server_entry, getThis(), "callbacks", sizeof("callbacks")-1, 0 TSRMLS_CC
		);

	//add_assoc_zval(callbacks, Z_STRVAL_P(name), callback);

	shurrik_dump_zval(callback);

	//if (zend_hash_next_index_insert(Z_ARRVAL_P(callbacks), &callback, sizeof(zval *), NULL) == FAILURE){	
	//}

	//efree(callback);

	//add_index_zval(callbacks, 1, callback);

	RETURN_ZVAL(getThis(),1,0);

}

PHP_FUNCTION(jsonrpc_server_execute)
{

}

PHP_FUNCTION(jsonrpc_server_jformat)
{
	
}

static zend_function_entry jsonrpc_server_class_functions[] = {
	PHP_FALIAS(__construct, jsonrpc_server_new, NULL)
	PHP_FALIAS(register, jsonrpc_server_register, NULL)
	PHP_FALIAS(execute, jsonrpc_server_execute, NULL)
	PHP_FALIAS(jsonformat, jsonrpc_server_jformat, NULL)
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
