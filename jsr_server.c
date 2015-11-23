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
#include "zend_closures.h"
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

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_executemethod_arginfo, 0, 0, 3)
    ZEND_ARG_INFO(0, class)
    ZEND_ARG_INFO(0, method)
    ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_server_getresponse_arginfo, 0, 0, 2)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, payload)
ZEND_END_ARG_INFO()
/* }}} */

static zval* _jsr_file_get_contents()
{
  TSRMLS_FETCH();
  
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
#if PHP_API_VERSION < 20100412
    if (PG(magic_quotes_runtime)) {
      contents = php_addslashes(contents, len, &len, 1 TSRMLS_CC); 
    }
#endif
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

static int _php_count_recursive(zval *array, long mode TSRMLS_DC) /* {{{ */
{
  long cnt = 0;
  zval **element;

  if (Z_TYPE_P(array) == IS_ARRAY) {
    if (Z_ARRVAL_P(array)->nApplyCount > 1) {
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "recursion detected");
      return 0;
    }

    cnt = zend_hash_num_elements(Z_ARRVAL_P(array));
    if (mode == COUNT_RECURSIVE) {
      HashPosition pos;

      for (zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(array), &pos);
        zend_hash_get_current_data_ex(Z_ARRVAL_P(array), (void **) &element, &pos) == SUCCESS;
        zend_hash_move_forward_ex(Z_ARRVAL_P(array), &pos)
      ) {
        Z_ARRVAL_P(array)->nApplyCount++;
        cnt += _php_count_recursive(*element, COUNT_RECURSIVE TSRMLS_CC);
        Z_ARRVAL_P(array)->nApplyCount--;
      }
    }
  }

  return cnt;
}

static zval* _jsr_server_get_arguments(zval *request_params, zval *method_params,
  int nb_required_params, int nb_max_params TSRMLS_DC)
{
  zval *retval;
  int nb_params;

  MAKE_STD_ZVAL(retval);

  nb_params = _php_count_recursive(request_params, 0 TSRMLS_CC);

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
  zval *payload = NULL;
  zval *callbacks = NULL;
  zval *classes = NULL;
  zval *object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zzz", 
    &payload, &callbacks, &classes) == FAILURE)
  {
    RETURN_NULL();
  }

  if (payload){
    zend_update_property(php_jsonrpc_server_entry,
      object, "payload", sizeof("payload")-1, payload TSRMLS_CC
    );
  }

  if (callbacks){
    zend_update_property(php_jsonrpc_server_entry,
      object, "callbacks", sizeof("callbacks")-1, callbacks TSRMLS_CC
    );
  }else {
    MAKE_STD_ZVAL(callbacks);
    array_init(callbacks);
    zend_update_property(php_jsonrpc_server_entry,
      object, "callbacks", sizeof("callbacks")-1, callbacks TSRMLS_CC
    );
    //zval_ptr_dtor(&callbacks);
  }

  if (classes){
    zend_update_property(php_jsonrpc_server_entry,
      object, "classes", sizeof("classes")-1, classes TSRMLS_CC
    );
  }else {
    MAKE_STD_ZVAL(classes);
    array_init(classes);
    zend_update_property(php_jsonrpc_server_entry,
      object, "classes", sizeof("classes")-1, classes TSRMLS_CC
    );
    //zval_ptr_dtor(&classes);
  }


}

PHP_METHOD(jsonrpc_server, register)
{
  zval *closure=NULL;
  zval *val;
  zval *name;
  zval *callbacks;
  char *lcname;
  char *name_str, *tmp;
  int name_len, tmp_len;
  zval *classname;

  zend_class_entry **pce;
  zend_class_entry *ce;
  zend_function *fptr;

  zval *object = getThis();

  if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC, "zO", &name, &closure, zend_ce_closure) == SUCCESS) {
    fptr = (zend_function*)zend_get_closure_method_def(closure TSRMLS_CC);
    Z_ADDREF_P(closure);
  } else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zs", &name, &name_str, &name_len) == SUCCESS) { 
    if ((tmp = strstr(name_str, "::")) == NULL) {
      char *nsname;

      lcname = zend_str_tolower_dup(name_str, name_len);

      // Ignore leading "\" 
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
    }else {
      tmp_len = tmp - name_str;
      MAKE_STD_ZVAL(classname);
      ZVAL_STRINGL(classname, name_str, tmp_len, 1);
      name_len = name_len - (tmp_len + 2);
      name_str = tmp + 2;
      //php_printf("classname: %s, method: %s\n", Z_STRVAL_P(classname), name_str);
      if (zend_lookup_class(Z_STRVAL_P(classname), Z_STRLEN_P(classname), &pce TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
          "Class %s does exist", Z_STRVAL_P(classname));
        //zend_throw_exception_ex(reflection_exception_ptr, 0 TSRMLS_CC,
        //    "Class %s does not exist", Z_STRVAL_P(classname)); 
        zval_dtor(classname);
        return;
      }
      ce = *pce;

      lcname = zend_str_tolower_dup(name_str, name_len);

      if (zend_hash_find(&ce->function_table, lcname, name_len + 1, (void **) &fptr) == FAILURE) {
        efree(lcname);
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
          "Method %s::%s() does not exist", ce->name, name_str);
        //zend_throw_exception_ex(reflection_exception_ptr, 0 TSRMLS_CC, 
        //  "Method %s::%s() does not exist", ce->name, name_str);
        return;
      }
      efree(lcname);
    }
  }else {
    return ;
  }


  callbacks = zend_read_property(
    php_jsonrpc_server_entry, object, "callbacks", sizeof("callbacks")-1, 0 TSRMLS_CC
  );

  MAKE_STD_ZVAL(val);

#if PHP_VERSION_ID < 50399
  zend_create_closure(val, fptr TSRMLS_CC);
#else
  zend_create_closure(val, fptr, NULL, NULL TSRMLS_CC);
#endif


  add_assoc_zval(callbacks, Z_STRVAL_P(name), val);

  zend_update_property(php_jsonrpc_server_entry, object, "callbacks", sizeof("callbacks")-1, callbacks TSRMLS_CC);

  RETURN_ZVAL(object,1,0);

}

PHP_METHOD(jsonrpc_server, bind)
{
  zval *procedure, *classname, *method;
  zval *classes;
  zval *val;
  zval *object;

  object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", 
    &procedure, &classname, &method) == FAILURE)
  {
    return ;
  }

  classes = zend_read_property(
    php_jsonrpc_server_entry, object, "classes", sizeof("classes")-1, 0 TSRMLS_CC
  );

  MAKE_STD_ZVAL(val);
  array_init(val);
  if (Z_TYPE_P(classname) == IS_STRING){
    add_assoc_stringl(val, "class", Z_STRVAL_P(classname), Z_STRLEN_P(classname), 1);
  }else if (Z_TYPE_P(classname) == IS_OBJECT){
    add_assoc_zval(val, "class", classname);
  }else {
    return ;
  }
  add_assoc_stringl(val, "method", Z_STRVAL_P(method), Z_STRLEN_P(method), 1);

  add_assoc_zval(classes, Z_STRVAL_P(procedure), val);

  zend_update_property(php_jsonrpc_server_entry, object, "classes", sizeof("classes")-1, classes TSRMLS_CC);

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
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Server::jsonformat()");
    return ;
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
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Server::rpcformat()");
    return ;
  }

  if (Z_LVAL(retval) == -32600){
    add_assoc_long(error, "code", -32600);
    add_assoc_string(error, "message", "Invalid Request", 0);
    add_assoc_null(id,"id");
    add_assoc_zval(data, "error", error);
    goto getresponse;
  }else if (Z_LVAL(retval) == -32601){
    add_assoc_long(error, "code", -32601);
    add_assoc_string(error, "message", "Method not found", 0);
    add_assoc_null(id,"id");
    add_assoc_zval(data, "error", error);
    goto getresponse;
  }else if (Z_LVAL(retval) == -32602){
    add_assoc_long(error, "code", -32602);
    add_assoc_string(error, "message", "Invalid params", 0);
    add_assoc_null(id,"id");
    add_assoc_zval(data, "error", error);
    goto getresponse;
  }else if (Z_LVAL(retval) == -32603){
    add_assoc_long(error, "code", -32603);
    add_assoc_string(error, "message", "Internal error", 0);
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
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "method is not find");
    return ;
  }
  //jsr_dump_zval(*payload_method);
  exec_params[0] = *payload_method;

  if (zend_hash_find(Z_ARRVAL_P(payload), "params", strlen("params")+1, (void **)&payload_params) == FAILURE)
  {
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "params is not find");
    return ;
  }
  //shurrik_dump_zval(*payload_params);
  exec_params[1] = *payload_params;

  MAKE_STD_ZVAL(return_val);
  ZVAL_STRINGL(func, "executeprocedure", sizeof("executeprocedure") - 1, 0);
  if (call_user_function(NULL, &object, func, return_val, 2, exec_params TSRMLS_CC) == FAILURE){
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Server::executeprocedure()");
    return ;
  }

  if (Z_LVAL_P(return_val) == -32601){
    add_assoc_long(error, "code", -32601);
    add_assoc_string(error, "message", "Method not found", 0);
    add_assoc_null(id,"id");
    add_assoc_zval(data, "error", error);
    goto getresponse;
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
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Server::getresponse()");
    return ;
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

    payload = _jsr_file_get_contents();
    
    zend_update_property(php_jsonrpc_server_entry, object, "payload", sizeof(payload)-1, payload TSRMLS_CC);
  }
  if (Z_TYPE_P(payload) == IS_STRING){
    php_json_decode(payload, Z_STRVAL_P(payload), Z_STRLEN_P(payload), 1, 512 TSRMLS_CC);
  }
  if (Z_TYPE_P(payload) != IS_ARRAY){
    RETVAL_FALSE;
  }else {
    zend_update_property(php_jsonrpc_server_entry, object, "payload", sizeof(payload)-1, payload TSRMLS_CC);
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
    RETVAL_LONG(-32600);
    return ;
  }

  if (!zend_symtable_exists(Z_ARRVAL_P(payload), "jsonrpc", strlen("jsonrpc")+1))
  {
    RETVAL_LONG(-32600);
    return ;
  }

  if (!zend_symtable_exists(Z_ARRVAL_P(payload), "method", strlen("method")+1))
  {
    RETVAL_LONG(-32601);
    return ;
  }

  //MAKE_STD_ZVAL(&method);
  if (zend_hash_find(Z_ARRVAL_P(payload), "method", strlen("method")+1, (void **)&method) == FAILURE)
  {
    RETVAL_LONG(-32601);
    return ;
  }

  if (Z_TYPE_PP(method) != IS_STRING)
  {
    RETVAL_LONG(-32601);
    return ;
  }

  //MAKE_STD_ZVAL(&jsonrpc);
  if (zend_hash_find(Z_ARRVAL_P(payload), "jsonrpc", strlen("jsonrpc")+1, (void **)&jsonrpc) == FAILURE)
  {
    RETVAL_LONG(-32600);
    return ;
  }

  if (strcmp(Z_STRVAL_PP(jsonrpc),"2.0") != 0)
  {
    RETVAL_LONG(-32600);
    return ;
  }

  
  //MAKE_STD_ZVAL(&params);
  if (zend_hash_find(Z_ARRVAL_P(payload), "params", strlen("params")+1, (void **)&params) == FAILURE)
  {
    RETVAL_LONG(-32602);
    return ;
  }
  if (Z_TYPE_PP(params) != IS_ARRAY)
  {
    RETVAL_LONG(-32602);
    return ;
  }

  RETVAL_LONG(0);
  return ;

}

PHP_METHOD(jsonrpc_server, executeprocedure)
{
  zval *procedure, *params;
  zval *callbacks;
  zval *classes;
  zval *object;
  zval **func_params;
  zval *func;
  zval *retval;
  zval **procedure_params;

  object = getThis();


  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "za", 
    &procedure, &params) == FAILURE)
  { 
    return ;
  }


  MAKE_STD_ZVAL(retval);
    
  callbacks = zend_read_property(
      php_jsonrpc_server_entry, object, "callbacks", sizeof("callbacks")-1, 0 TSRMLS_CC
    );

  classes = zend_read_property(
      php_jsonrpc_server_entry, object, "classes", sizeof("classes")-1, 0 TSRMLS_CC
    );

  if (zend_hash_exists(Z_ARRVAL_P(callbacks), Z_STRVAL_P(procedure), Z_STRLEN_P(procedure) + 1))
  {
    if (zend_hash_find(Z_ARRVAL_P(callbacks), Z_STRVAL_P(procedure), Z_STRLEN_P(procedure)+1, (void **)&procedure_params) == FAILURE)
    {
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Server::executecallback()");
      ZVAL_LONG(retval, -32601);
      RETVAL_ZVAL(retval, 1, 0);
      return ;
    }
    MAKE_STD_ZVAL(func);
    ZVAL_STRINGL(func, "executecallback", sizeof("executecallback") - 1, 1);
    func_params = emalloc(sizeof(zval *) * 2);
    //jsr_dump_zval(*procedure_params);
    func_params[0] = *procedure_params;
    func_params[1] = params;
    if (call_user_function(NULL, &object, func, retval, 2, func_params TSRMLS_CC) == FAILURE){
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Server::executecallback()");
      ZVAL_LONG(retval, -32601);
      RETVAL_ZVAL(retval, 1, 0);
      return ;
    }

    efree(func_params);

    //
    //if (Z_TYPE_P(retval) == IS_BOOL){
    //  RETVAL_FALSE;
    //}else {
    //  RETVAL_NULL();
    //}
    //return ;
    
  }else if (zend_hash_exists(Z_ARRVAL_P(classes), Z_STRVAL_P(procedure), Z_STRLEN_P(procedure) + 1)){
    if (zend_hash_find(Z_ARRVAL_P(classes), Z_STRVAL_P(procedure), Z_STRLEN_P(procedure)+1, (void **)&procedure_params) == FAILURE)
    {
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Server::executemethod()");
      ZVAL_LONG(retval, -32601);
      RETVAL_ZVAL(retval, 1, 0);
      return ;
    }
    MAKE_STD_ZVAL(func);
    ZVAL_STRINGL(func, "executemethod", sizeof("executemethod") - 1, 1);
    func_params = emalloc(sizeof(zval *) * 3);
    //jsr_dump_zval(*procedure_params);
    zval **class, **method;
    zend_hash_find(Z_ARRVAL_PP(procedure_params), "class", sizeof("class"), (void **)&class);
    zend_hash_find(Z_ARRVAL_PP(procedure_params), "method", sizeof("method"), (void **)&method);
    func_params[0] = *class;
    func_params[1] = *method;
    func_params[2] = params;
    if (call_user_function(NULL, &object, func, retval, 3, func_params TSRMLS_CC) == FAILURE){
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Server::executecallback()");
      ZVAL_LONG(retval, -32601);
      RETVAL_ZVAL(retval, 1, 0);
      return ;
    }

    efree(func_params);

  }else {
    ZVAL_LONG(retval, -32601);
  }

  RETVAL_ZVAL(retval, 1, 0);
  return ;
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
    &closure, &params) == FAILURE)
  {
    //fptr = (zend_function*)zend_get_closure_method_def(closure TSRMLS_CC);
    //Z_ADDREF_P(closure);
    return ;
  }


  if (!closure || !zend_is_callable(closure, 0, NULL TSRMLS_CC))
  {
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "closeure is not expected to be a valid callback");
    RETVAL_FALSE;
    return;
  }

  MAKE_STD_ZVAL(closure_result_ptr);

  func_params_num = _php_count_recursive(params, 0 TSRMLS_CC);
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
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling closure");
    return ;
  }
  efree(func_params);

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

PHP_METHOD(jsonrpc_server, executemethod)
{
  zval *class, *method, *params;
  zval *zcallable = NULL;
  char *func_name;

  int func_params_num;
  zval **func_params;

  zval *result_ptr = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", 
    &class, &method, &params) == FAILURE)
  {
    return ;
  }

  MAKE_STD_ZVAL(zcallable);
  array_init(zcallable);
  add_next_index_zval(zcallable, class);
  add_next_index_zval(zcallable, method);

  if (!zcallable || !zend_is_callable(zcallable, 0, &func_name TSRMLS_CC)){
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s is not exist", func_name);
    RETVAL_FALSE;
    return;
  }

  MAKE_STD_ZVAL(result_ptr);

  func_params_num = _php_count_recursive(params, 0 TSRMLS_CC);
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
  if (call_user_function(EG(function_table), NULL, zcallable, result_ptr, func_params_num, func_params TSRMLS_CC) == FAILURE)
  {
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling closure");
    return ;
  }
  efree(func_params);

  RETVAL_ZVAL(result_ptr, 1, 0);
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

  MAKE_STD_ZVAL(response);
  array_init(response);
  add_assoc_string(response, "jsonrpc", "2.0", 0);

  MAKE_STD_ZVAL(key);
  ZVAL_STRING(key, "id", 0);
  if (!zend_symtable_exists(payload, Z_STRVAL_P(key), Z_STRLEN_P(key) + 1))
  {
  }else {
    
    if (zend_hash_find(payload, "id", 3, (void **)&id ) == FAILURE)
    {
      //php_error_docref(NULL TSRMLS_CC, E_WARNING, "closeure is not expected to be a valid callback");
      //return ;
    }

    if (Z_TYPE_PP(id) == IS_NULL){
      add_assoc_null(response, "id");
    }else if(Z_TYPE_PP(id) == IS_STRING){
      convert_to_string(*id);
      add_assoc_string(response, "id", Z_STRVAL_PP(id), 0);
    }else if (Z_TYPE_PP(id) == IS_LONG){
      convert_to_long(*id);
      add_assoc_long(response, "id", Z_LVAL_PP(id));
    }
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
    PHP_ME(jsonrpc_server,    executemethod,      jsonrpc_server_executemethod_arginfo,       ZEND_ACC_PUBLIC)
    PHP_ME(jsonrpc_server,    getresponse,        jsonrpc_server_getresponse_arginfo,         ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

void
jsonrpc_server_init(int module_number TSRMLS_DC)
{

  zend_class_entry jsonrpc_server_class_entry;
  INIT_CLASS_ENTRY(jsonrpc_server_class_entry, "Jsonrpc_Server", jsonrpc_server_class_functions);
  php_jsonrpc_server_entry = zend_register_internal_class(&jsonrpc_server_class_entry TSRMLS_CC);
  zend_declare_property_null(php_jsonrpc_server_entry, "payload",   sizeof("payload")-1,   ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_null(php_jsonrpc_server_entry, "callbacks", sizeof("callbacks")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_null(php_jsonrpc_server_entry, "classes",   sizeof("classes")-1,   ZEND_ACC_PUBLIC TSRMLS_CC);

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */