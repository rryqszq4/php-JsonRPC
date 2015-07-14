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
#include "SAPI.h"
#include "ext/json/php_json.h"
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
	zval *closure, *val;
	zval *name;
	zval *callbacks;
	zend_function *fptr;

	zval *object = getThis();


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", 
		&name, &closure) == SUCCESS)
	{
		fptr = (zend_function*)zend_get_closure_method_def(closure TSRMLS_CC);
		Z_ADDREF_P(closure);
	}


	callbacks = zend_read_property(
			php_jsonrpc_server_entry, getThis(), "callbacks", sizeof("callbacks")-1, 0 TSRMLS_CC
		);


	MAKE_STD_ZVAL(val);
	zend_create_closure(val, fptr TSRMLS_CC);

	add_assoc_zval(callbacks, Z_STRVAL_P(name), val);


	RETURN_ZVAL(object,1,0);

}

PHP_FUNCTION(jsonrpc_server_execute)
{
	zval *object;
	zval *func;
	zval retval;
	zval **func_params;
	zval *data, *payload, *error, *id, *response;

	object = getThis();
	
	MAKE_STD_ZVAL(error);
	MAKE_STD_ZVAL(id);
	MAKE_STD_ZVAL(data);
	array_init(error);
	array_init(id);
	array_init(data);
	
	//add_assoc_zval(response, "id", id);

	INIT_ZVAL(retval);
	MAKE_STD_ZVAL(func);
	ZVAL_STRINGL(func, "jsonformat", sizeof("jsonformat") - 1, 0);
	if (call_user_function(NULL, &object, func, &retval, 0, NULL TSRMLS_CC) == FAILURE){

	}
	if (!Z_BVAL(retval)){
		add_assoc_long(error, "code", -32767);
		add_assoc_string(error, "message", "Parse error", 0);
		add_assoc_null(id,"id");
		add_assoc_zval(data, "error", error);
		goto getresponse;
	}
	

	ZVAL_STRINGL(func, "rpcformat", sizeof("rpcformat") - 1, 0);
	if (call_user_function(NULL, &object, func, &retval, 0, NULL TSRMLS_CC) == FAILURE){

	}
	if (!Z_BVAL(retval)){
		add_assoc_long(error, "code", -32600);
		add_assoc_string(error, "message", "Invalid Request", 0);
		add_assoc_null(id,"id");
		add_assoc_zval(data, "error", error);
		goto getresponse;
	}


getresponse:
	func_params = emalloc(sizeof(zval *) * 2);
	func_params[0] = data;
	func_params[1] = id;
	
	ZVAL_STRINGL(func, "getresponse", sizeof("getresponse") - 1, 0);
	if (call_user_function(NULL, &object, func, &retval, 2, func_params TSRMLS_CC) == FAILURE)
	{

	}

	efree(func_params);
	ZVAL_STRINGL(return_value, Z_STRVAL(retval), Z_STRLEN(retval), 1);
	return ;

}

PHP_FUNCTION(jsonrpc_server_jformat)
{
	zval *payload, *val;
	zval *object = getThis();
	
	zend_bool use_include_path = 0;
	php_stream *stream;
	php_stream_context *context = NULL;
	int len;
	long maxlen = PHP_STREAM_COPY_ALL;
	char *contents;

	payload = zend_read_property(
			php_jsonrpc_server_entry, object, "payload", sizeof("payload")-1, 0 TSRMLS_CC
		);

	if (Z_TYPE_P(payload) == IS_NULL){
		context = php_stream_context_alloc(TSRMLS_CC);
		stream = php_stream_open_wrapper_ex("php://input", "rb",
				(use_include_path ? USE_PATH : 0) | ENFORCE_SAFE_MODE | REPORT_ERRORS,
				NULL, context);

		if ((len = php_stream_copy_to_mem(stream, &contents, maxlen, 0)) > 0) {

			if (PG(magic_quotes_runtime)) {
				contents = php_addslashes(contents, len, &len, 1 TSRMLS_CC); /* 1 = free source string */
			}

			ZVAL_STRINGL(payload, contents, len, 1);
		} else if (len == 0) {
			ZVAL_STRING(payload, "", 1);
		} else {
			ZVAL_NULL(payload);
		}
		php_stream_close(stream);
		zend_update_property(php_jsonrpc_server_entry, object, "payload", sizeof(payload)-1, payload TSRMLS_CC);
	}
	if (Z_TYPE_P(payload) == IS_STRING){
		php_json_decode(payload, Z_STRVAL_P(payload), Z_STRLEN_P(payload), 1, 512 TSRMLS_CC);
		zend_update_property(php_jsonrpc_server_entry, object, "payload", sizeof(payload)-1, payload TSRMLS_CC);
	}
	if (Z_TYPE_P(payload) != IS_ARRAY){
		RETVAL_FALSE;
	}else {
		RETVAL_TRUE;
	}
}

PHP_FUNCTION(jsonrpc_server_rformat)
{
	zval *payload;
	zval *object;
	zval **method = NULL;
	zval **jsonrpc = NULL;
	zval **params = NULL;

	object = getThis();

	payload = zend_read_property(
			php_jsonrpc_server_entry, object, "payload", sizeof("payload")-1, 0 TSRMLS_CC
		);

	if (Z_TYPE_P(payload) != IS_ARRAY)
	{
		RETVAL_FALSE;
		return ;
	}

	if (!zend_symtable_exists(Z_ARRVAL_P(payload), "jsonrpc", strlen("jsonrpc")+1))
	{
		RETVAL_FALSE;
		return ;
	}

	if (!zend_symtable_exists(Z_ARRVAL_P(payload), "method", strlen("method")+1))
	{
		RETVAL_FALSE;
		return ;
	}

	//MAKE_STD_ZVAL(&method);
	if (zend_hash_find(Z_ARRVAL_P(payload), "method", strlen("method")+1, (void **)&method) == FAILURE)
	{
		RETVAL_FALSE;
		return ;
	}
	if (Z_TYPE_PP(method) != IS_STRING)
	{
		RETVAL_FALSE;
		return ;
	}

	//MAKE_STD_ZVAL(&jsonrpc);
	if (zend_hash_find(Z_ARRVAL_P(payload), "jsonrpc", strlen("jsonrpc")+1, (void **)&jsonrpc) == FAILURE)
	{
		RETVAL_FALSE;
		return ;
	}
	if (Z_STRVAL_PP(jsonrpc) != "2.0")
	{
		RETVAL_FALSE;
		return ;
	}

	//MAKE_STD_ZVAL(&params);
	if (zend_hash_find(Z_ARRVAL_P(payload), "params", strlen("params")+1, (void **)&params) == FAILURE)
	{
		RETVAL_FALSE;
		return ;
	}
	if (Z_TYPE_PP(params) != IS_ARRAY)
	{
		RETVAL_FALSE;
		return ;
	}


}

PHP_FUNCTION(jsonrpc_server_getresponse)
{
	zval *data;
	zval *key;
	HashTable *payload;
	zval *response;
	zval **id = NULL;
	smart_str buf = {0};
	sapi_header_line ctr = {0};

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "aH", 
		&data, &payload) == FAILURE)
	{	
		return ;
	}

	MAKE_STD_ZVAL(key);
	ZVAL_STRING(key, "id", 0);
	if (!zend_symtable_exists(payload, Z_STRVAL_P(key), Z_STRLEN_P(key) + 1))
	{
		RETVAL_STRING("", 0);
		return ;
	}

	MAKE_STD_ZVAL(response);
	array_init(response);
	add_assoc_string(response, "jsonrpc", "2.0", 0);
	
	if (zend_hash_find(payload, "id", 3, (void **)&id ) == FAILURE)
	{
		RETVAL_STRING("", 0);
		return ;
	}

	if (Z_TYPE_PP(id) == IS_NULL){
		add_assoc_null(response, "id");
	}else if(Z_TYPE_PP(id) == IS_STRING){
		convert_to_string(*id);
		add_assoc_string(response, "id", Z_STRVAL_PP(id), 0);
	}

	zend_hash_merge(Z_ARRVAL_P(response), Z_ARRVAL_P(data), NULL, NULL, sizeof(zval *), 1);

	ctr.line = "Content-Type: application/json";
	ctr.line_len = strlen(ctr.line);
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);

	php_json_encode(&buf, response, 0 TSRMLS_CC);
	ZVAL_STRINGL(return_value, buf.c, buf.len, 1);
	smart_str_free(&buf);

}

static zend_function_entry jsonrpc_server_class_functions[] = {
	PHP_FALIAS(__construct, jsonrpc_server_new, NULL)
	PHP_FALIAS(register, jsonrpc_server_register, NULL)
	PHP_FALIAS(execute, jsonrpc_server_execute, NULL)
	PHP_FALIAS(jsonformat, jsonrpc_server_jformat, NULL)
	PHP_FALIAS(rpcformat, jsonrpc_server_rformat, NULL)
	PHP_FALIAS(getresponse, jsonrpc_server_getresponse, NULL)
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
