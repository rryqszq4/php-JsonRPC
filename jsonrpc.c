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

#include <curl/curl.h>
#include <curl/easy.h>

/* If you declare any globals in php_jsonrpc.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(jsonrpc)
*/

/* True global resources - no need for thread safety here */
static int le_jsonrpc;
static int le_curl;
#define le_curl_name "cURL handle"
static zend_class_entry *php_jsonrpc_server_entry;
static zend_class_entry *php_jsonrpc_client_entry;

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

static zval* jr_server_get_arguments(zval *request_params, zval *method_params,
	int nb_required_params, int nb_max_params)
{
	zval *retval;
	int nb_params;

	MAKE_STD_ZVAL(retval);

	nb_params = php_count_recursive(request_params, 0 TSRMLS_CC);

	if (nb_params < nb_required_params){
		ZVAL_FALSE(retval);
		return retval;
	}

	if (nb_params > nb_max_params){
		ZVAL_FALSE(retval);
		return retval;
	}

	ZVAL_TRUE(retval);
	return retval;
}


/* {{{ curl_read
 */
static size_t curl_read(char *data, size_t size, size_t nmemb, void *ctx)
{
	php_curl       *ch = (php_curl *) ctx;
	php_curl_read  *t  = ch->handlers->read;
	int             length = 0;

	switch (t->method) {
		case PHP_CURL_DIRECT:
			if (t->fp) {
				length = fread(data, size, nmemb, t->fp);
			}
			break;
		case PHP_CURL_USER: {
			zval **argv[3];
			zval  *handle = NULL;
			zval  *zfd = NULL;
			zval  *zlength = NULL;
			zval  *retval_ptr;
			int   error;
			zend_fcall_info fci;
			TSRMLS_FETCH_FROM_CTX(ch->thread_ctx);

			MAKE_STD_ZVAL(handle);
			MAKE_STD_ZVAL(zfd);
			MAKE_STD_ZVAL(zlength);

			ZVAL_RESOURCE(handle, ch->id);
			zend_list_addref(ch->id);
			ZVAL_RESOURCE(zfd, t->fd);
			zend_list_addref(t->fd);
			ZVAL_LONG(zlength, (int) size * nmemb);

			argv[0] = &handle;
			argv[1] = &zfd;
			argv[2] = &zlength;

			fci.size = sizeof(fci);
			fci.function_table = EG(function_table);
			fci.function_name = t->func_name;
			fci.object_ptr = NULL;
			fci.retval_ptr_ptr = &retval_ptr;
			fci.param_count = 3;
			fci.params = argv;
			fci.no_separation = 0;
			fci.symbol_table = NULL;

			ch->in_callback = 1;
			error = zend_call_function(&fci, &t->fci_cache TSRMLS_CC);
			ch->in_callback = 0;
			if (error == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cannot call the CURLOPT_READFUNCTION");
#if LIBCURL_VERSION_NUM >= 0x070c01 /* 7.12.1 */
				length = CURL_READFUNC_ABORT;
#endif
			} else if (retval_ptr) {
				if (Z_TYPE_P(retval_ptr) == IS_STRING) {
					length = MIN((int) (size * nmemb), Z_STRLEN_P(retval_ptr));
					memcpy(data, Z_STRVAL_P(retval_ptr), length);
				}
				zval_ptr_dtor(&retval_ptr);
			}

			zval_ptr_dtor(argv[0]);
			zval_ptr_dtor(argv[1]);
			zval_ptr_dtor(argv[2]);
			break;
		}
	}

	return length;
}
/* }}} */

/* {{{ curl_write
 */
static size_t curl_write(char *data, size_t size, size_t nmemb, void *ctx)
{
	php_curl       *ch     = (php_curl *) ctx;
	php_curl_write *t      = ch->handlers->write;
	size_t          length = size * nmemb;
	TSRMLS_FETCH_FROM_CTX(ch->thread_ctx);

#if PHP_CURL_DEBUG
	fprintf(stderr, "curl_write() called\n");
	fprintf(stderr, "data = %s, size = %d, nmemb = %d, ctx = %x\n", data, size, nmemb, ctx);
#endif

	switch (t->method) {
		case PHP_CURL_STDOUT:
			PHPWRITE(data, length);
			break;
		case PHP_CURL_FILE:
			return fwrite(data, size, nmemb, t->fp);
		case PHP_CURL_RETURN:
			if (length > 0) {
				smart_str_appendl(&t->buf, data, (int) length);
			}
			break;
		case PHP_CURL_USER: {
			zval **argv[2];
			zval *retval_ptr = NULL;
			zval *handle = NULL;
			zval *zdata = NULL;
			int   error;
			zend_fcall_info fci;

			MAKE_STD_ZVAL(handle);
			ZVAL_RESOURCE(handle, ch->id);
			zend_list_addref(ch->id);
			argv[0] = &handle;

			MAKE_STD_ZVAL(zdata);
			ZVAL_STRINGL(zdata, data, length, 1);
			argv[1] = &zdata;

			fci.size = sizeof(fci);
			fci.function_table = EG(function_table);
			fci.object_ptr = NULL;
			fci.function_name = t->func_name;
			fci.retval_ptr_ptr = &retval_ptr;
			fci.param_count = 2;
			fci.params = argv;
			fci.no_separation = 0;
			fci.symbol_table = NULL;

			ch->in_callback = 1;
			error = zend_call_function(&fci, &t->fci_cache TSRMLS_CC);
			ch->in_callback = 0;
			if (error == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not call the CURLOPT_WRITEFUNCTION");
				length = -1;
			} else if (retval_ptr) {
				if (Z_TYPE_P(retval_ptr) != IS_LONG) {
					convert_to_long_ex(&retval_ptr);
				}
				length = Z_LVAL_P(retval_ptr);
				zval_ptr_dtor(&retval_ptr);
			}

			zval_ptr_dtor(argv[0]);
			zval_ptr_dtor(argv[1]);
			break;
		}
	}

	return length;
}
/* }}} */

/* {{{ curl_write_header
 */
static size_t curl_write_header(char *data, size_t size, size_t nmemb, void *ctx)
{
	php_curl       *ch  = (php_curl *) ctx;
	php_curl_write *t   = ch->handlers->write_header;
	size_t          length = size * nmemb;
	TSRMLS_FETCH_FROM_CTX(ch->thread_ctx);

	switch (t->method) {
		case PHP_CURL_STDOUT:
			/* Handle special case write when we're returning the entire transfer
			 */
			if (ch->handlers->write->method == PHP_CURL_RETURN && length > 0) {
				smart_str_appendl(&ch->handlers->write->buf, data, (int) length);
			} else {
				PHPWRITE(data, length);
			}
			break;
		case PHP_CURL_FILE:
			return fwrite(data, size, nmemb, t->fp);
		case PHP_CURL_USER: {
			zval **argv[2];
			zval  *handle = NULL;
			zval  *zdata = NULL;
			zval  *retval_ptr;
			int   error;
			zend_fcall_info fci;

			MAKE_STD_ZVAL(handle);
			MAKE_STD_ZVAL(zdata);

			ZVAL_RESOURCE(handle, ch->id);
			zend_list_addref(ch->id);
			ZVAL_STRINGL(zdata, data, length, 1);

			argv[0] = &handle;
			argv[1] = &zdata;

			fci.size = sizeof(fci);
			fci.function_table = EG(function_table);
			fci.function_name = t->func_name;
			fci.symbol_table = NULL;
			fci.object_ptr = NULL;
			fci.retval_ptr_ptr = &retval_ptr;
			fci.param_count = 2;
			fci.params = argv;
			fci.no_separation = 0;

			ch->in_callback = 1;
			error = zend_call_function(&fci, &t->fci_cache TSRMLS_CC);
			ch->in_callback = 0;
			if (error == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not call the CURLOPT_HEADERFUNCTION");
				length = -1;
			} else if (retval_ptr) {
				if (Z_TYPE_P(retval_ptr) != IS_LONG) {
					convert_to_long_ex(&retval_ptr);
				}
				length = Z_LVAL_P(retval_ptr);
				zval_ptr_dtor(&retval_ptr);
			}
			zval_ptr_dtor(argv[0]);
			zval_ptr_dtor(argv[1]);
			break;
		}

		case PHP_CURL_IGNORE:
			return length;

		default:
			return -1;
	}

	return length;
}
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
	zval **func_params, **exec_params;
	zval *data, *payload, *error, *id, *response;
	zval **payload_method, **payload_params;
	zval *return_val;

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
	/*ZVAL_STRINGL(func, "jsonformat", sizeof("jsonformat") - 1, 0);
	if (call_user_function(NULL, &object, func, &retval, 0, NULL TSRMLS_CC) == FAILURE){

	}
	if (!Z_BVAL(retval)){
		add_assoc_long(error, "code", -32767);
		add_assoc_string(error, "message", "Parse error", 0);
		add_assoc_null(id,"id");
		add_assoc_zval(data, "error", error);
		goto getresponse;
	}
*/

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

	exec_params = emalloc(sizeof(zval *) * 2);
	payload = zend_read_property(
			php_jsonrpc_server_entry, getThis(), "payload", sizeof("payload")-1, 0 TSRMLS_CC
		);
	if (zend_hash_find(Z_ARRVAL_P(payload), "method", strlen("method")+1, (void **)&payload_method) == FAILURE)
	{
	}
	//shurrik_dump_zval(*payload_method);
	exec_params[0] = *payload_method;

	if (zend_hash_find(Z_ARRVAL_P(payload), "params", strlen("params")+1, (void **)&payload_params) == FAILURE)
	{
	}
	//shurrik_dump_zval(*payload_params);
	exec_params[1] = *payload_params;

	MAKE_STD_ZVAL(return_val);
	ZVAL_STRINGL(func, "executeprocedure", sizeof("executeprocedure") - 1, 0);
	if (call_user_function(NULL, &object, func, return_val, 2, exec_params TSRMLS_CC) == FAILURE){

	}

	RETVAL_ZVAL(return_val, 1, 0);
	add_assoc_zval(data, "result", return_val);
	id = payload;
	efree(exec_params);
	goto getresponse;

	return ;

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

	if (strcmp(Z_STRVAL_PP(jsonrpc),"2.0") != 0)
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

	RETVAL_TRUE;
	return ;

}

PHP_FUNCTION(jsonrpc_server_execute_procedure)
{
	zval *procedure, *params;
	zval *callbacks;
	zval *object;
	zval **func_params;
	zval *func;
	zval *retval;
	zval **procedure_params;

	object = getThis();


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "za", 
		&procedure, &params) == FAILURE)
	{	
		
	}

	callbacks = zend_read_property(
			php_jsonrpc_server_entry, object, "callbacks", sizeof("callbacks")-1, 0 TSRMLS_CC
		);

	if (zend_symtable_exists(Z_ARRVAL_P(callbacks), Z_STRVAL_P(procedure), Z_STRLEN_P(procedure) + 1))
	{
		if (zend_hash_find(Z_ARRVAL_P(callbacks), Z_STRVAL_P(procedure), Z_STRLEN_P(procedure)+1, (void **)&procedure_params) == FAILURE)
		{

		}

		MAKE_STD_ZVAL(retval);
		MAKE_STD_ZVAL(func);
		ZVAL_STRINGL(func, "executecallback", sizeof("executecallback") - 1, 0);
		func_params = emalloc(sizeof(zval *) * 2);
		//shurrik_dump_zval(*procedure_params);
		func_params[0] = *procedure_params;
		func_params[1] = params;
		if (call_user_function(NULL, &object, func, retval, 2, func_params TSRMLS_CC) == FAILURE){
		}

		efree(func_params);

		RETVAL_ZVAL(retval, 1, 0);
		/*if (Z_TYPE_P(retval) == IS_BOOL){
			RETVAL_FALSE;
		}else {
			RETVAL_NULL();
		}*/
		return ;
	}
	
}

PHP_FUNCTION(jsonrpc_server_execute_callback)
{
	zval *closure = NULL;
	zval *params;
	zval *arguments;
	zval *robj;
	zval *func;
	zval *reflection;
	zval **func_params;
	zval *method_params;
	zval *nb_required_params;
	zval *nb_max_params;

	zval *closure_result_ptr = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", 
		&closure, &params) == SUCCESS)
	{
		//fptr = (zend_function*)zend_get_closure_method_def(closure TSRMLS_CC);
		//Z_ADDREF_P(closure);
	}

	if (!closure || !zend_is_callable(closure, 0, NULL TSRMLS_CC))
	{
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "closeure is expected to be a valid callback");
		RETVAL_FALSE;
		return;
	}

	MAKE_STD_ZVAL(closure_result_ptr);
	//shurrik_dump_zval(closure);
	if (call_user_function(EG(function_table), NULL, closure, closure_result_ptr, 0, NULL TSRMLS_CC) == FAILURE)
	{
	}

	RETVAL_ZVAL(closure_result_ptr, 1, 0);


	/*MAKE_STD_ZVAL(robj)
	object_init_ex(robj,reflection_function_ptr);

	MAKE_STD_ZVAL(func);
	ZVAL_STRINGL(func, "__construct", sizeof("__construct") - 1, 0);

	func_params = emalloc(sizeof(zval *) * 2);
	func_params[0] = closure;

	if (call_user_function(NULL, &robj, func, reflection, 1, func_params TSRMLS_CC) == FAILURE){

	}

	ZVAL_STRINGL(func, "getParameters", sizeof("getParameters") - 1, 0);
	if (call_user_function(NULL, &reflection, func, method_params, 0, NULL TSRMLS_CC) == FAILURE)
	{

	}

	ZVAL_STRINGL(func, "getNumberOfRequiredParameters", sizeof("getNumberOfRequiredParameters") - 1, 0);
	if (call_user_function(NULL, &reflection, func, nb_required_params, 0, NULL TSRMLS_CC) == FAILURE)
	{

	}

	ZVAL_STRINGL(func, "getNumberOfParameters", sizeof("getNumberOfParameters") - 1, 0);
	if (call_user_function(NULL, &reflection, func, nb_max_params, 0, NULL TSRMLS_CC) == FAILURE)
	{

	}

	MAKE_STD_ZVAL(arguments);
	arguments = jr_server_get_arguments(
			params,
			method_params,
			Z_LVAL_p(nb_required_params),
			Z_LVAL_p(nb_max_params)
		);

	if (Z_TYPE_P(arguments) == IS_BOOL)
	{
		RETVAL_FALSE;
	}

	efree(func_params);
	zval_ptr_dtor(&robj);
	*/
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

PHP_FUNCTION(jsonrpc_client_new)
{
	CURL *cp;
	php_curl	*ch;
	zval		*clone;
	char		*url = NULL;
	int		url_len = 0;
	char *cainfo;

	zval *_url;
	zval *timeout;
	zval *headers;
	zval *object;
	zval *_headers;
	zval *_ch;

	object = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz",
		&_url, &timeout, &headers) == FAILURE)
	{

	}

	MAKE_STD_ZVAL(_headers);
	array_init(_headers);
	add_index_string(_headers, 0, estrdup("Content-Type: application/json"), 0);
	add_next_index_string(_headers, estrdup("Accept: application/json"), 0);

	add_property_zval(object, "url", _url);
	add_property_zval(object, "timeout", timeout);

	zend_hash_merge(Z_ARRVAL_P(_headers), Z_ARRVAL_P(headers), NULL, NULL, sizeof(zval *), 1);
	add_property_zval(object, "header", _headers);
	
	cp = curl_easy_init();
	if (!cp){
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not initialize a new cURL handle");
		RETURN_FALSE;
	}

	alloc_curl_handle(&ch);
	TSRMLS_SET_CTX(ch->thread_ctx);

	ch->cp = cp;

	ch->handlers->write->method = PHP_CURL_STDOUT;
	ch->handlers->write->type   = PHP_CURL_ASCII;
	ch->handlers->read->method  = PHP_CURL_DIRECT;
	ch->handlers->write_header->method = PHP_CURL_IGNORE;

	ch->uses = 0;

	MAKE_STD_ZVAL(clone);
	ch->clone = clone;

	curl_easy_setopt(ch->cp, CURLOPT_NOPROGRESS,        1);
	curl_easy_setopt(ch->cp, CURLOPT_VERBOSE,           0);
	curl_easy_setopt(ch->cp, CURLOPT_ERRORBUFFER,       ch->err.str);
	curl_easy_setopt(ch->cp, CURLOPT_WRITEFUNCTION,     curl_write);
	curl_easy_setopt(ch->cp, CURLOPT_FILE,              (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_READFUNCTION,      curl_read);
	curl_easy_setopt(ch->cp, CURLOPT_INFILE,            (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_HEADERFUNCTION,    curl_write_header);
	curl_easy_setopt(ch->cp, CURLOPT_WRITEHEADER,       (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_DNS_USE_GLOBAL_CACHE, 1);
	curl_easy_setopt(ch->cp, CURLOPT_DNS_CACHE_TIMEOUT, 120);
	curl_easy_setopt(ch->cp, CURLOPT_MAXREDIRS, 20); /* prevent infinite redirects */

	cainfo = INI_STR("curl.cainfo");
	if (cainfo && strlen(cainfo) > 0) {
		curl_easy_setopt(ch->cp, CURLOPT_CAINFO, cainfo);
	}

#if defined(ZTS)
	curl_easy_setopt(ch->cp, CURLOPT_NOSIGNAL, 1);
#endif

	if (url) {
		if (!php_curl_option_url(ch, url, url_len)) {
			_php_curl_close_ex(ch TSRMLS_CC);
			RETURN_FALSE;
		}
	}

	ZEND_REGISTER_RESOURCE(_ch, ch, le_curl);
	add_property_resource(object, "ch", _ch);
}

PHP_FUNCTION(jsonrpc_client_del)
{
	zval		*zid;
	php_curl	*ch;

	zid = callbacks = zend_read_property(
			php_jsonrpc_server_entry, object, "ch", sizeof("ch")-1, 0 TSRMLS_CC
		);

	ZEND_FETCH_RESOURCE(ch, php_curl *, &zid, -1, le_curl_name, le_curl);

	if (ch->in_callback) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Attempt to close cURL handle from a callback");
		return;
	}

	if (ch->uses) {
		ch->uses--;
	} else {
		zend_list_delete(Z_LVAL_P(zid));
	}
}



static zend_function_entry jsonrpc_server_class_functions[] = {
	PHP_FALIAS(__construct, jsonrpc_server_new, NULL)
	PHP_FALIAS(register, jsonrpc_server_register, NULL)
	PHP_FALIAS(execute, jsonrpc_server_execute, NULL)
	PHP_FALIAS(jsonformat, jsonrpc_server_jformat, NULL)
	PHP_FALIAS(rpcformat, jsonrpc_server_rformat, NULL)
	PHP_FALIAS(executeprocedure, jsonrpc_server_execute_procedure, NULL)
	PHP_FALIAS(executecallback, jsonrpc_server_execute_callback, NULL)
	PHP_FALIAS(getresponse, jsonrpc_server_getresponse, NULL)
	{NULL, NULL, NULL}
};

static zend_function_entry jsonrpc_client_class_functions[] = {
	PHP_FALIAS(__construct, jsonrpc_client_new, NULL)
	PHP_FALIAS(__destruct, jsonrpc_client_del, NULL);
	{NULL, NULL, NULL}
};

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(jsonrpc)
{
	le_curl = zend_register_list_destructors_ex(_php_curl_close, NULL, "curl", module_number);

	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/

	zend_class_entry jsonrpc_server_class_entry;
	INIT_CLASS_ENTRY(jsonrpc_server_class_entry, "Jsonrpc_Server", jsonrpc_server_class_functions);
	php_jsonrpc_server_entry = zend_register_internal_class(&jsonrpc_server_class_entry TSRMLS_CC);

	zend_class_entry jsonrpc_client_class_entry;
	INIT_CLASS_ENTRY(jsonrpc_client_class_entry, "Jsonrpc_Client", jsonrpc_client_class_functions);
	php_jsonrpc_client_entry = zend_register_internal_class(&jsonrpc_client_class_entry TSRMLS_CC);

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
