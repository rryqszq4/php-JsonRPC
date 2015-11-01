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

#include "jsr_server.h"
#include "jsr_utils.h"


/** {{{ ARG_INFO
 *  */
ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_construct_arginfo, 0, 0, 0)
    ZEND_ARG_INFO(0, payload)
    ZEND_ARG_INFO(0, callbacks)
    ZEND_ARG_INFO(0, classes)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_register_arginfo, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_bind_arginfo, 0, 0, 3)
    ZEND_ARG_INFO(0, procedure)
    ZEND_ARG_INFO(0, class)
    ZEND_ARG_INFO(0, method)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_execute_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_jsonformat_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_rpcformat_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_executeprocedure_arginfo, 0, 0, 2)
    ZEND_ARG_INFO(0, procedure)
    ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_executecallback_arginfo, 0, 0, 2)
    ZEND_ARG_INFO(0, callback)
    ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_getresponse_arginfo, 0, 0, 2)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, payload)
ZEND_END_ARG_INFO()
/* }}} */

static zval* jr_file_get_contents()
{
  zval *payload;

  MAKE_STD_ZVAL(payload);

  zend_bool use_include_path = 0;
  php_stream *stream;
  int len;
  long offset = -1;
  long maxlen = PHP_STREAM_COPY_ALL;
  zval *zcontext = NULL;
  php_stream_context *context = NULL;

  char *contents;

  

  context = php_stream_context_from_zval(zcontext, 0);

  stream = php_stream_open_wrapper_ex("php://input", "rb",
        (use_include_path ? USE_PATH : 0) | ENFORCE_SAFE_MODE | REPORT_ERRORS,
        NULL, context);

  
  if (!stream) {
    ZVAL_NULL(payload);
    php_stream_close(stream);
    return payload;
  }

  if (offset > 0 && php_stream_seek(stream, offset, SEEK_SET) < 0) {
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to seek to position %ld in the stream", offset);
    php_stream_close(stream);
    ZVAL_NULL(payload);
    return payload;
  }

  if ((len = php_stream_copy_to_mem(stream, &contents, maxlen, 0)) > 0) {

    if (PG(magic_quotes_runtime)) {
      contents = php_addslashes(contents, len, &len, 1 TSRMLS_CC); 
    }

    ZVAL_STRINGL(payload, contents, len, 1);
    php_stream_close(stream);
    return payload;
  } else if (len == 0) {
    ZVAL_STRING(payload, "", 1);
    php_stream_close(stream);
    return payload;
  } else {
    ZVAL_NULL(payload);
    php_stream_close(stream);
    return payload;
  }
  
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

PHP_METHOD(jsonrpc_server, __construct)
{
  zval *payload, *callbacks, *classes;
  zval *object = getThis();

  ALLOC_INIT_ZVAL(payload);
  MAKE_STD_ZVAL(callbacks);
  MAKE_STD_ZVAL(classes);

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zzz", 
    &payload, &callbacks, &classes) == FAILURE)
  {
    RETURN_NULL();
  }

  if (Z_TYPE_P(payload) == IS_STRING){
    add_property_zval(object, "payload", payload);
  }else {
    ZVAL_NULL(payload);
    add_property_zval(object, "payload", payload);
  }

  if (Z_TYPE_P(callbacks) == IS_ARRAY){
    add_property_zval(object, "callbacks", callbacks);
  }else {
    array_init(callbacks);
    add_property_zval(object, "callbacks", callbacks);
  }
  
  if (Z_TYPE_P(classes) == IS_ARRAY){
    add_property_zval(object, "classes", classes);
  }else {
    array_init(classes);
    add_property_zval(object, "classes", classes);
  }

  //payload = NULL;
  payload = zend_read_property(
      php_jsonrpc_server_entry, getThis(), "payload", sizeof("payload")-1, 0 TSRMLS_CC
    );
  //php_var_dump(&payload, 1 TSRMLS_CC);

  //php_printf("jsonrpc_server new\n");
}

PHP_METHOD(jsonrpc_server, register)
{
  zval *closure=NULL;
  zval *val;
  zval *name;
  zval *callbacks;
  char *lcname;
  zend_function *fptr;
  char *name_str;
  int name_len;

  zval *object = getThis();


  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", 
    &name, &closure) == FAILURE)
  {
    
  }

  if (Z_TYPE_P(closure) == IS_OBJECT){
    fptr = (zend_function*)zend_get_closure_method_def(closure TSRMLS_CC);
    Z_ADDREF_P(closure);
  }else if (Z_TYPE_P(closure) == IS_STRING){
    char *nsname;
    name_str = Z_STRVAL_P(closure);
    name_len = Z_STRLEN_P(closure);


    lcname = zend_str_tolower_dup(name_str, name_len);

    /* Ignore leading "\" */
    nsname = lcname;
    if (lcname[0] == '\\') {
      nsname = &lcname[1];
      name_len--;
    }
    
    if (zend_hash_find(EG(function_table), nsname, name_len + 1, (void **)&fptr) == FAILURE) {
      efree(lcname);
      php_error_docref(NULL TSRMLS_CC, E_WARNING, 
        "Function %s() does not exist", name_str);
      return;
    }
    efree(lcname);
  }

  callbacks = zend_read_property(
    php_jsonrpc_server_entry, object, "callbacks", sizeof("callbacks")-1, 0 TSRMLS_CC
  );

  MAKE_STD_ZVAL(val);

  zend_create_closure(val, fptr TSRMLS_CC);
  //shurrik_dump_zval(val);
  add_assoc_zval(callbacks, Z_STRVAL_P(name), val);

  zend_update_property(php_jsonrpc_server_entry, object, "callbacks", sizeof(callbacks)-1, callbacks TSRMLS_CC);

  RETURN_ZVAL(object,1,0);

}

PHP_METHOD(jsonrpc_server, bind)
{
  zval *procedure, *classes, *method;
  zval *val;
  zval *object;

  object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", 
    &procedure, &classes, &method) == FAILURE)
  {
    
  }

  classes = zend_read_property(
    php_jsonrpc_server_entry, object, "classes", sizeof("classes")-1, 0 TSRMLS_CC
  );

  array_init(val);
  add_assoc_string(val, "class", Z_STRVAL_P(classes), 0);
  add_assoc_string(val, "method", Z_STRVAL_P(method), 0);

  add_assoc_zval(classes, Z_STRVAL_P(procedure), val);

  zend_update_property(php_jsonrpc_server_entry, object, "classes", sizeof(classes)-1, classes TSRMLS_CC);

  RETURN_ZVAL(object, 1, 0);
}

PHP_METHOD(jsonrpc_server, execute)
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
  ZVAL_STRINGL(func, "jsonformat", sizeof("jsonformat") - 1, 0);
  if (call_user_function(NULL, &object, func, &retval, 0, NULL TSRMLS_CC) == FAILURE){

  }
  if (!Z_BVAL(retval)){
    add_assoc_long(error, "code", -32700);
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

PHP_METHOD(jsonrpc_server, jsonformat)
{
  zval *payload, *val;
  zval *object = getThis();

  payload = zend_read_property(
      php_jsonrpc_server_entry, object, "payload", sizeof("payload")-1, 0 TSRMLS_CC
    );

  //shurrik_dump_zval(payload);


  if (Z_TYPE_P(payload) == IS_NULL){
    /*context = php_stream_context_alloc(TSRMLS_CC);
    stream = php_stream_open_wrapper_ex("php://input", "rb",
        (use_include_path ? USE_PATH : 0) | ENFORCE_SAFE_MODE | REPORT_ERRORS,
        NULL, context);

    if ((len = php_stream_copy_to_mem(stream, &contents, maxlen, 0)) > 0) {

      if (PG(magic_quotes_runtime)) {
        contents = php_addslashes(contents, len, &len, 1 TSRMLS_CC); 
      }

      ZVAL_STRINGL(payload, contents, len, 1);
    } else if (len == 0) {
      ZVAL_STRING(payload, "", 1);
    } else {
      ZVAL_NULL(payload);
    }
    php_stream_close(stream);
    zend_update_property(php_jsonrpc_server_entry, object, "payload", sizeof(payload)-1, payload TSRMLS_CC);
    */
    
    payload = jr_file_get_contents();
    
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

PHP_METHOD(jsonrpc_server, rpcformat)
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

PHP_METHOD(jsonrpc_server, executeprocedure)
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

PHP_METHOD(jsonrpc_server, executecallback)
{
  zval *closure = NULL;
  zval *params;
  zval *arguments;
  zval *robj;
  zval *func;
  zval *reflection;
  
  int func_params_num;
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
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "closeure is not expected to be a valid callback");
    RETVAL_FALSE;
    return;
  }

  MAKE_STD_ZVAL(closure_result_ptr);

  func_params_num = php_count_recursive(params, 0 TSRMLS_CC);
  func_params = emalloc(sizeof(zval *) * func_params_num);

  zval              **current;
  HashTable          *ph;
  int i = 0;

  ph = HASH_OF(params);
  if (!ph) {
  }

  for (zend_hash_internal_pointer_reset(ph);
     zend_hash_get_current_data(ph, (void **) &current) == SUCCESS;
     zend_hash_move_forward(ph)
  ) {
    SEPARATE_ZVAL(current);
    func_params[i] = *current;

    i++;
  }

  //shurrik_dump_zval(closure);
  if (call_user_function(EG(function_table), NULL, closure, closure_result_ptr, func_params_num, func_params TSRMLS_CC) == FAILURE)
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

PHP_METHOD(jsonrpc_server, getresponse)
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

static const zend_function_entry jsonrpc_server_class_functions[] = {
    PHP_ME(jsonrpc_server,    __construct,        jsonrpc_server_construct_arginfo,           ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(jsonrpc_server,    register,           jsonrpc_server_register_arginfo,            ZEND_ACC_PUBLIC)
    PHP_ME(jsonrpc_server,    bind,               jsonrpc_server_bind_arginfo,                ZEND_ACC_PUBLIC)
    PHP_ME(jsonrpc_server,    execute,            jsonrpc_server_execute_arginfo,             ZEND_ACC_PUBLIC)
    PHP_ME(jsonrpc_server,    jsonformat,         jsonrpc_server_jsonformat_arginfo,          ZEND_ACC_PUBLIC)
    PHP_ME(jsonrpc_server,    rpcformat,          jsonrpc_server_rpcformat_arginfo,           ZEND_ACC_PUBLIC)
    PHP_ME(jsonrpc_server,    executeprocedure,   jsonrpc_server_executeprocedure_arginfo,    ZEND_ACC_PUBLIC)
    PHP_ME(jsonrpc_server,    executecallback,    jsonrpc_server_executecallback_arginfo,     ZEND_ACC_PUBLIC)
    PHP_ME(jsonrpc_server,    getresponse,        jsonrpc_server_getresponse_arginfo,         ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

void
jsonrpc_server_init()
{
  zend_class_entry jsonrpc_server_class_entry;
  INIT_CLASS_ENTRY(jsonrpc_server_class_entry, "Jsonrpc_Server", jsonrpc_server_class_functions);
  php_jsonrpc_server_entry = zend_register_internal_class(&jsonrpc_server_class_entry TSRMLS_CC);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */