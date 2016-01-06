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
#include "ext/standard/php_smart_str.h"
#include "SAPI.h"
#include "ext/json/php_json.h"
#include "ext/standard/file.h"
#include "php_jsonrpc.h"

#include "jsr_client.h"
#include "jsr_curl.h"
#include "jsr_epoll.h"
#include "jsr_utils.h"
#include "jsr_yajl.h"

int le_jsr_epoll_persist;
int le_jsr_curlm_persist;

/** {{{ ARG_INFO
 *  */
ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_construct_arginfo, 0, 0, 0)
  ZEND_ARG_INFO(0, persist)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_destruct_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_call_arginfo, 0, 0, 3)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, procedure)
    ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_execute_arginfo, 0, 0, 0)
  ZEND_ARG_INFO(0, response_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_connect_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client___call_arginfo, 0, 0, 2)
  ZEND_ARG_INFO(0, method)
  ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_dorequest_arginfo, 0, 0, 1)
  ZEND_ARG_INFO(0, payload)
ZEND_END_ARG_INFO()
/* }}} */

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

static zval* 
_jsr_client_prepare_request(zval *procedure, zval *params, zval *custom_id TSRMLS_DC)
{
  zval *payload;
  long number;
  int nb_params;

  MAKE_STD_ZVAL(payload);
  array_init(payload);

  add_assoc_string(payload, "jsonrpc", "2.0", 0);
  add_assoc_string(payload, "method", Z_STRVAL_P(procedure), 0);

  if (Z_TYPE_P(custom_id) == IS_NULL){
    zval *id;
    MAKE_STD_ZVAL(id);

    if (!BG(mt_rand_is_seeded)) {
      php_mt_srand(GENERATE_SEED() TSRMLS_CC);
    }
    
    number = (long) (php_mt_rand(TSRMLS_C) >> 1);
    ZVAL_LONG(id, number);
    add_assoc_long(payload, "id", Z_LVAL_P(id));
    //add_assoc_long(payload, "id", 123456);
  }else {
    add_assoc_zval(payload, "id", custom_id);
  }
  
  nb_params = _php_count_recursive(params, 0 TSRMLS_CC);
  if (nb_params > 0)
  {
    add_assoc_zval(payload, "params", params);
  }

  return payload;
}

static int
_socket_callback(CURL *easy, curl_socket_t fd, int action, void *u, void *s)
{
  //php_printf(">>> %s: adding fd=%d action=%d\n", __func__, fd, action);
  php_jsr_reuqest_object *request = (php_jsr_reuqest_object *) u;
  jsr_epoll_t *jsr_epoll = request->context->epoll;
  jsr_curl_sockinfo_t *jsr_sock = (jsr_curl_sockinfo_t *) s;

  if (action == CURL_POLL_REMOVE)
  {
    if (jsr_sock){
      jsr_epoll_del_fd(jsr_epoll, jsr_sock->sockfd);
      free(jsr_sock);
      jsr_sock = NULL;
    }
  }else {
    if (!jsr_sock){
      jsr_sock = calloc(sizeof(jsr_curl_sockinfo_t), 1);
      jsr_sock->sockfd = fd;
      jsr_sock->easy = easy;
      jsr_epoll_add_fd(jsr_epoll, jsr_sock->sockfd);
      curl_multi_assign(request->curlm->multi_handle, jsr_sock->sockfd, jsr_sock);
    }
    if (action == CURL_POLL_IN || action == CURL_POLL_INOUT)
    {
      jsr_epoll_set_in(jsr_epoll, jsr_sock->sockfd);
    }

    if (action == CURL_POLL_OUT || action == CURL_POLL_INOUT)
    {
      jsr_epoll_set_out(jsr_epoll, jsr_sock->sockfd);
    }
  }

  return 0;
}

static int 
_timer_callback(CURLM *multi, long timeout_ms, void *u)
{
  //php_printf(">>> %s: timeout: %ld ms\n", __func__, timeout_ms);
  //php_jsr_reuqest_object *request = (php_jsr_reuqest_object *) u;
  //request->curlm->timeout = timeout_ms;
  return 0;
}

static size_t
_read_callback(void *ptr, size_t size, size_t nmemb, void *ctx)
{
  return 0;
}

static size_t
_write_callback(char *ptr, size_t size, size_t nmemb, void *ctx)
{
  TSRMLS_FETCH();
  
  jsr_curl_item_t * item = (jsr_curl_item_t *)ctx;

  zval *response_tmp;
  zval *error;

  char *buffer;

  CURLcode code;
  long response_code;

  size_t length = size * nmemb;
  item->write_length = item->write_length + length;

  code = curl_easy_getinfo(item->curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

  if (response_code == 200){
    if (!item->write_data){
      item->write_data = malloc(item->write_length + 1);
      memset(item->write_data, 0, item->write_length);
      strncpy(item->write_data, ptr, item->write_length+1);
    }else {
      item->write_data = realloc(item->write_data, item->write_length + 1);
      strncat(item->write_data, ptr, length);
    }
    return length;
  }

  switch (response_code)
  {
      case 100:
        response_tmp = _php_jsr_response_error(-32100, "Continue", &item->payload_id);
        break;
      case 101:
        response_tmp = _php_jsr_response_error(-32101, "Switching Protocols", &item->payload_id);
        break;
      case 102:
        response_tmp = _php_jsr_response_error(-32102, "Processing", &item->payload_id);
        break;

      case 201:
        response_tmp = _php_jsr_response_error(-32201, "Created", &item->payload_id);
        break;
      case 202:
        response_tmp = _php_jsr_response_error(-32202, "Accepted", &item->payload_id);
        break;
      case 203:
        response_tmp = _php_jsr_response_error(-32203, "Non-Authoritative Information", &item->payload_id);
        break;
      case 204:
        response_tmp = _php_jsr_response_error(-32204, "No Content", &item->payload_id);
        break;
      case 205:
        response_tmp = _php_jsr_response_error(-32205, "Reset Content", &item->payload_id);
        break;
      case 206:
        response_tmp = _php_jsr_response_error(-32206, "Partial Content", &item->payload_id);
        break;
      case 207:
        response_tmp = _php_jsr_response_error(-32207, "Multi-Status", &item->payload_id);
        break;

      case 300:
        response_tmp = _php_jsr_response_error(-32300, "Multiple Choices", &item->payload_id);
        break;
      case 301:
        response_tmp = _php_jsr_response_error(-32301, "Moved Permanently", &item->payload_id);
        break;
      case 302:
        response_tmp = _php_jsr_response_error(-32302, "Move temporarily", &item->payload_id);
        break;
      case 303:
        response_tmp = _php_jsr_response_error(-32303, "See Other", &item->payload_id);
        break;
      case 304:
        response_tmp = _php_jsr_response_error(-32304, "Not Modified", &item->payload_id);
        break;
      case 305:
        response_tmp = _php_jsr_response_error(-32305, "Use Proxy", &item->payload_id);
        break;
      case 306:
        response_tmp = _php_jsr_response_error(-32306, "Switch Proxy", &item->payload_id);
        break;
      case 307:
        response_tmp = _php_jsr_response_error(-32307, "Temporary Redirect", &item->payload_id);
        break;


      case 400:
        response_tmp = _php_jsr_response_error(-32400, "Bad Request", &item->payload_id);
        break;
      case 401:
        response_tmp = _php_jsr_response_error(-32401, "Unauthorized", &item->payload_id);
        break;
      case 402:
        response_tmp = _php_jsr_response_error(-32402, "Payment Required", &item->payload_id);
        break;
      case 403:
        response_tmp = _php_jsr_response_error(-32403, "Forbidden", &item->payload_id);
        break;
      case 404:
        response_tmp = _php_jsr_response_error(-32404, "Not Found", &item->payload_id);
        break;
      case 405:
        response_tmp = _php_jsr_response_error(-32405, "Method Not Allowed", &item->payload_id);
        break;
      case 406:
        response_tmp = _php_jsr_response_error(-32406, "Not Acceptable", &item->payload_id);
        break;
      case 407:
        response_tmp = _php_jsr_response_error(-32407, "Proxy Authentication Required", &item->payload_id);
        break;
      case 408:
        response_tmp = _php_jsr_response_error(-32408, "Request Timeout", &item->payload_id);
        break;
      case 409:
        response_tmp = _php_jsr_response_error(-32409, "Conflict", &item->payload_id);
        break;
      case 410:
        response_tmp = _php_jsr_response_error(-32410, "Gone", &item->payload_id);
        break;
      case 411:
        response_tmp = _php_jsr_response_error(-32411, "Length Required", &item->payload_id);
        break;
      case 412:
        response_tmp = _php_jsr_response_error(-32412, "Precondition Failed", &item->payload_id);
        break;
      case 413:
        response_tmp = _php_jsr_response_error(-32413, "Request Entity Too Large", &item->payload_id);
        break;
      case 414:
        response_tmp = _php_jsr_response_error(-32414, "Request URI Too Long", &item->payload_id);
        break;
      case 415:
        response_tmp = _php_jsr_response_error(-32415, "Unsupported Media Type", &item->payload_id);
        break;
      case 416:
        response_tmp = _php_jsr_response_error(-32416, "Requested Range Not Satisfiable", &item->payload_id);
        break;
      case 417:
        response_tmp = _php_jsr_response_error(-32417, "Expectation Failed", &item->payload_id);
        break;
      case 418:
        response_tmp = _php_jsr_response_error(-32418, "I'm a teapot", &item->payload_id);
        break;
      case 421:
        response_tmp = _php_jsr_response_error(-32421, "Misdirected Request", &item->payload_id);
        break;
      case 422:
        response_tmp = _php_jsr_response_error(-32422, "Unprocessable Entity", &item->payload_id);
        break;
      case 423:
        response_tmp = _php_jsr_response_error(-32423, "Locked", &item->payload_id);
        break;
      case 424:
        response_tmp = _php_jsr_response_error(-32424, "Failed Dependency", &item->payload_id);
        break;
      case 426:
        response_tmp = _php_jsr_response_error(-32426, "Upgrade Required", &item->payload_id);
        break;
      case 428:
        response_tmp = _php_jsr_response_error(-32428, "Precondition Required", &item->payload_id);
        break;
      case 429:
        response_tmp = _php_jsr_response_error(-32429, "Too Many Requests", &item->payload_id);
        break;
      case 431:
        response_tmp = _php_jsr_response_error(-32431, "Request Header Fields Too Large", &item->payload_id);
        break;

      case 500:
        response_tmp = _php_jsr_response_error(-32500, "Internal Server Error", &item->payload_id);
        break;
      case 501:
        response_tmp = _php_jsr_response_error(-32501, "Not Implemented", &item->payload_id);
        break;
      case 502:
        response_tmp = _php_jsr_response_error(-32502, "Bad Gateway", &item->payload_id);
        break;
      case 503:
        response_tmp = _php_jsr_response_error(-32503, "Service Unavailable", &item->payload_id);
        break;
      case 504:
        response_tmp = _php_jsr_response_error(-32504, "Gateway Timeout", &item->payload_id);
        break;
      case 505:
        response_tmp = _php_jsr_response_error(-32505, "HTTP Version Not Supported", &item->payload_id);
        break;
      case 506:
        response_tmp = _php_jsr_response_error(-32506, "Variant Also Negotiates", &item->payload_id);
        break;
      case 507:
        response_tmp = _php_jsr_response_error(-32507, "Insufficient Storage", &item->payload_id);
        break;
      case 509:
        response_tmp = _php_jsr_response_error(-32509, "Bandwidth Limit Exceeded", &item->payload_id);
        break;
      case 510:
        response_tmp = _php_jsr_response_error(-32510, "Not Extended", &item->payload_id);
        break;
      default:
        response_tmp = _php_jsr_response_error(-32599, "HTTP Unknow", &item->payload_id);
        break;
  }


  smart_str buf = {0};
  php_json_encode(&buf, response_tmp, 0 TSRMLS_CC);

  item->write_length = buf.len;
  item->write_data = malloc(item->write_length + 1);
  memset(item->write_data, 0, item->write_length);
  strncpy(item->write_data, buf.c, item->write_length+1);

  smart_str_free(&buf);
  zval_ptr_dtor(&response_tmp);

  return length;
}

static zval* 
_php_jsr_response_error(long code, char *message, jsr_payload_id *payload_id)
{
  zval *response_tmp;
  zval *error;

  MAKE_STD_ZVAL(response_tmp);
  array_init(response_tmp);
  MAKE_STD_ZVAL(error);
  array_init(error);

  add_assoc_string(response_tmp, "jsonrpc", "2.0", 1);

  if (payload_id){
    if (payload_id->type == IS_LONG){
      add_assoc_long(response_tmp, "id", payload_id->long_id);
    }else if (payload_id->type == IS_STRING){
      add_assoc_string(response_tmp, "id", payload_id->char_id, 1);
    }else {
      add_assoc_null(response_tmp, "id");
    }
  }else {
    add_assoc_null(response_tmp, "id");
  }

  add_assoc_long(error, "code", code);
  add_assoc_string(error, "message", message, 1);
  
  add_assoc_zval(response_tmp, "error", error);

  return response_tmp;
}

static void
_php_jsr_request_object_free_storage(void *object TSRMLS_DC)
{

  php_jsr_reuqest_object *jsr_request = (php_jsr_reuqest_object *) object;

  if (!jsr_request->context->is_persistent){
    jsr_epoll_destroy(&(jsr_request->context->epoll));
  }

  jsr_curl_item_t *item;
  size_t size;


  while ((size = jsr_list_size(jsr_request->curlm->list)) > 0){
    item = jsr_list_pop(jsr_request->curlm->list);
    if (!jsr_request->executed){
      //curl_easy_getinfo(item->curl_handle, CURLINFO_PRIVATE, &list);
      curl_multi_remove_handle(jsr_request->curlm->multi_handle, item->curl_handle);
      curl_easy_cleanup(item->curl_handle);
      curl_slist_free_all(item->slist);
    }

    jsr_curl_item_destroy(&item);

  }

  if (!jsr_request->is_persistent){
    jsr_curlm_destroy(&jsr_request->curlm);
  }

  jsr_curl_global_destroy();

  zend_object_std_dtor(&jsr_request->zo TSRMLS_CC);

  efree(jsr_request);

}

static zend_object_value
_php_jsr_request_object_new(zend_class_entry *class_type TSRMLS_DC)
{
  zend_object_value result;
  php_jsr_reuqest_object *jsr_request;
#if PHP_VERSION_ID < 50399
  zval *tmp;
#endif

  jsr_request = (php_jsr_reuqest_object *)emalloc(sizeof(php_jsr_reuqest_object));
  memset(&jsr_request->zo, 0, sizeof(zend_object));

  jsr_request->context = NULL;
  jsr_request->curlm = NULL;
  jsr_request->executed = 0;
  jsr_request->is_persistent = 0;

  zend_object_std_init(&jsr_request->zo, class_type TSRMLS_CC);
#if PHP_VERSION_ID < 50399
  zend_hash_copy(
    jsr_request->zo.properties, 
    &class_type->default_properties,
    (copy_ctor_func_t) zval_property_ctor,
    (void *)&tmp,
    sizeof(zval *)
    );
#else
  object_properties_init( &jsr_request->zo, class_type );
#endif

  result.handle = zend_objects_store_put(
      jsr_request,
      (zend_objects_store_dtor_t)zend_objects_destroy_object,
      (zend_objects_free_object_storage_t) _php_jsr_request_object_free_storage,
      NULL TSRMLS_CC
    );
  result.handlers = &jsr_request_object_handlers;

  return result;
}

static php_jsr_epoll_context *
_php_jsr_epoll_new(zend_bool is_persistent TSRMLS_DC)
{
  php_jsr_epoll_context *context;
  
  context = pemalloc(sizeof(php_jsr_epoll_context), 1);
  context->epoll = jsr_epoll_init();

  if (!context->epoll){
    pefree(context, is_persistent);
    return NULL;
  }

  context->is_persistent = is_persistent;

  return context;
}

static php_jsr_epoll_context *
_php_jsr_epoll_get(zend_bool is_persistent TSRMLS_DC)
{
  php_jsr_epoll_context *context;

  char plist_key[48];
  int plist_key_len;
  zend_rsrc_list_entry le, *le_p;

  if (is_persistent){
    plist_key_len = snprintf(plist_key, 48, "jsonrpc_client_epoll_1");
    plist_key_len += 1;

    if (zend_hash_find(&EG(persistent_list), plist_key, plist_key_len, (void *)&le_p) == SUCCESS)
    {
      if (le_p->type == le_jsr_epoll_persist){
        return (php_jsr_epoll_context *) le_p->ptr;
      }
    }
  }

  context = _php_jsr_epoll_new(is_persistent TSRMLS_CC);

  if (!context){
    return NULL;
  }

  if (is_persistent){
    le.type = le_jsr_epoll_persist;
    le.ptr = context;

    if (zend_hash_update(&EG(persistent_list), plist_key, plist_key_len, &le, sizeof(zend_rsrc_list_entry), NULL) == FAILURE)
    {
      php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not register persistent entry for epoll");
    }
  }

  return context;

}

static void
_php_jsr_epoll_destroy(php_jsr_epoll_context *context)
{
  jsr_epoll_destroy(&context->epoll);
  
  pefree(context, context->is_persistent);

}

static php_jsr_curlm_conn *
_php_jsr_curlm_new(zend_bool is_persistent TSRMLS_DC)
{
  php_jsr_curlm_conn *conn;
  
  conn = pemalloc(sizeof(php_jsr_curlm_conn), 1);
  conn->curlm = jsr_curlm_new();

  //php_printf("new curlm : %p\n", conn->curlm->multi_handle);

  if (!conn->curlm){
    pefree(conn, is_persistent);
    return NULL;
  }

  conn->is_persistent = is_persistent;

  return conn;
}

static php_jsr_curlm_conn *
_php_jsr_curlm_get(zend_bool is_persistent TSRMLS_DC)
{
  php_jsr_curlm_conn *conn;

  char plist_key[48];
  int plist_key_len;
  zend_rsrc_list_entry le, *le_p;

  if (is_persistent){
    plist_key_len = snprintf(plist_key, 48, "jsonrpc_client_curlm_1");
    plist_key_len += 1;

    if (zend_hash_find(&EG(persistent_list), plist_key, plist_key_len, (void *)&le_p) == SUCCESS)
    {
      if (le_p->type == le_jsr_curlm_persist){
        return (php_jsr_curlm_conn *) le_p->ptr;
      }
    }
  }

  conn = _php_jsr_curlm_new(is_persistent TSRMLS_CC);

  if (!conn){
    return NULL;
  }

  if (is_persistent){
    le.type = le_jsr_curlm_persist;
    le.ptr = conn;

    if (zend_hash_update(&EG(persistent_list), plist_key, plist_key_len, &le, sizeof(zend_rsrc_list_entry), NULL) == FAILURE)
    {
      php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not register persistent entry for curlm");
    }
  }

  return conn;
}

static void 
_php_jsr_curlm_destroy(php_jsr_curlm_conn *conn)
{
  jsr_curlm_destroy(&conn->curlm);
  
  pefree(conn, conn->is_persistent);
}

ZEND_RSRC_DTOR_FUNC(_php_jsr_epoll_dtor)
{
  if (rsrc->ptr){
    php_jsr_epoll_context *context = (php_jsr_epoll_context *)rsrc->ptr;
    _php_jsr_epoll_destroy(context);
    rsrc->ptr = NULL;
  }
}

ZEND_RSRC_DTOR_FUNC(_php_jsr_curlm_dtor)
{
  if (rsrc->ptr){
    php_jsr_curlm_conn *conn = (php_jsr_curlm_conn *) rsrc->ptr;
    _php_jsr_curlm_destroy(conn);
    rsrc->ptr = NULL;
  }
}

PHP_METHOD(jsonrpc_client, __construct)
{
  zval *object;
  zval *request_obj;

  zend_bool persist = 0;

  php_jsr_epoll_context *context;
  php_jsr_curlm_conn *conn;

  php_jsr_reuqest_object *request;
  jsr_epoll_t *epoll;
  jsr_curlm_t *curlm;

  object = getThis();
  
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b",
    &persist) == FAILURE)
  {
    return ;
  }

  jsr_curl_global_new();

  context = _php_jsr_epoll_get(persist TSRMLS_CC);
  conn = _php_jsr_curlm_get(persist TSRMLS_CC);

  MAKE_STD_ZVAL(request_obj);
  object_init_ex(request_obj, php_jsonrpc_client_request_entry);
  request = (php_jsr_reuqest_object *)zend_object_store_get_object(request_obj TSRMLS_CC);

  request->context = context;
  request->curlm = conn->curlm;
  request->is_persistent = persist;


  curl_multi_setopt(request->curlm->multi_handle, CURLMOPT_SOCKETFUNCTION, _socket_callback);
  curl_multi_setopt(request->curlm->multi_handle, CURLMOPT_SOCKETDATA, request);
  curl_multi_setopt(request->curlm->multi_handle, CURLMOPT_TIMERFUNCTION, _timer_callback);
  curl_multi_setopt(request->curlm->multi_handle, CURLMOPT_TIMERDATA, request);

  zend_update_property(php_jsonrpc_client_entry,
      object, "request", sizeof("request")-1, request_obj TSRMLS_CC
    );
  zval_ptr_dtor(&request_obj);

  zend_update_property_long(php_jsonrpc_client_entry,
      object, "response_total", sizeof("response_total")-1, 0 TSRMLS_CC
    );
  
}

PHP_METHOD(jsonrpc_client, __destruct)
{
  php_jsr_reuqest_object *request;

  zval *object;
  zval *request_obj;

  object = getThis();

}

PHP_METHOD(jsonrpc_client, call)
{
  php_jsr_reuqest_object *request;
  jsr_curl_item_t *jsr_curl_item;
  jsr_epoll_t **epoll_p;

  char *url;
  long url_len;

  char *procedure;
  long procedure_len;
  zval *params;
  zval *id = NULL;
  zval *payload;

  zval **payload_id;

  zval *item;
  zval *object;
  zval *request_obj;
  zval *response_total;
  zval *coroutine_start;

  object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssz|z", &url, &url_len, &procedure, &procedure_len, &params, &id) == FAILURE)
  {
    return ;
  }


  request_obj = zend_read_property(
      php_jsonrpc_client_entry, object, "request", sizeof("request")-1, 0 TSRMLS_CC
    );

  response_total = zend_read_property(
      php_jsonrpc_client_entry, object, "response_total", sizeof("response_total")-1, 0 TSRMLS_CC
    );

  request = (php_jsr_reuqest_object *)zend_object_store_get_object(request_obj TSRMLS_CC);


  MAKE_STD_ZVAL(payload);
  //payload = _jsr_client_prepare_request(procedure, params, id TSRMLS_CC);
  array_init(payload);
  add_assoc_string(payload, "jsonrpc", "2.0", 1);
  add_assoc_stringl(payload, "method", procedure, procedure_len, 1);
  if (!id){
    if (!BG(mt_rand_is_seeded)) {
      php_mt_srand(GENERATE_SEED() TSRMLS_CC);
    }
    
    long number;
    number = (long) (php_mt_rand(TSRMLS_C) >> 1);

    MAKE_STD_ZVAL(id);
    ZVAL_LONG(id, number);
    add_assoc_long(payload, "id", Z_LVAL_P(id));
  }else {
    add_assoc_zval(payload, "id", id);
  }

  int nb_params;
  nb_params = _php_count_recursive(params, 0 TSRMLS_CC);
  if (nb_params > 0)
  {
    add_assoc_zval(payload, "params", params);
  }


  smart_str buf = {0};
  php_json_encode(&buf, payload, 0 TSRMLS_CC);
  ZVAL_STRINGL(payload, buf.c, buf.len, 1);
  smart_str_free(&buf);

  //jsr_dump_zval(payload);

  convert_to_string_ex(&payload);

  jsr_curl_item = jsr_curl_item_new(
      url,
      url_len,
      Z_STRVAL_P(payload),
      Z_STRLEN_P(payload),
      Z_LVAL_P(response_total),
      id
    );

  zval_ptr_dtor(&payload);

  //jsr_curl_item->read_callback = _read_callback;
  jsr_curl_item->write_callback = _write_callback;
  
  jsr_curl_item_setopt(jsr_curl_item);
  curl_multi_add_handle(request->curlm->multi_handle, jsr_curl_item->curl_handle);
  jsr_curlm_list_push(request->curlm, jsr_curl_item);

  //jsr_curlm_list_append(request->curlm, jsr_curl_item);

  zend_update_property_long(php_jsonrpc_client_entry, 
    object, "response_total", sizeof("response_total")-1, Z_LVAL_P(response_total) + 1 TSRMLS_CC
  );
}

PHP_METHOD(jsonrpc_client, execute)
{
  zval *procedure;
  zval *params;
  zval *payload;
  zval *response;

  php_jsr_reuqest_object *request;

  zval *object;
  zval *func;
  zval **func_params;

  zval *request_obj;

  zend_bool response_type=0;

  object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b",
    &response_type) == FAILURE)
  {
    return ;
  }

  MAKE_STD_ZVAL(func);

  request_obj = zend_read_property(
        php_jsonrpc_client_entry, object, "request", sizeof("request")-1, 0 TSRMLS_CC
      );
  request = (php_jsr_reuqest_object *)zend_object_store_get_object(request_obj TSRMLS_CC);
  request->executed = 1;

  MAKE_STD_ZVAL(response);
  array_init(response);

  request->curlm->running_handles = 1;
  request->context->epoll->loop_total = 0;
  int action;
  int start_mcode = CURLM_CALL_MULTI_PERFORM;
  CURLMcode mcode;


  while (request->curlm->running_handles > 0)
  {

    request->context->epoll->loop_total = jsr_epoll_loop(request->context->epoll , request->curlm->timeout);
    //php_printf("loop_total: %d\n", request->context->epoll->loop_total);
    if (request->context->epoll->loop_total == 0){
      
      if (start_mcode == CURLM_CALL_MULTI_PERFORM){
        start_mcode = curl_multi_socket_action(request->curlm->multi_handle, CURL_SOCKET_TIMEOUT, 0, &(request->curlm->running_handles));
        //php_printf("start_mcode : %d\n", start_mcode);
      }else if (start_mcode == CURLM_OK) {
          
        start_mcode = curl_multi_socket_action(request->curlm->multi_handle, CURL_SOCKET_TIMEOUT, 0, &(request->curlm->running_handles));
        //php_printf("running: %d\n",request->curlm->running_handles);
        
        request->curlm->timeout = (request->curlm->timeout) * 2;

      }else {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "call curl_multi_socket_action error : %d", start_mcode);
        break;
      }
      
    }else if (request->context->epoll->loop_total > 0){
      int i;
      for (i = 0; i < request->context->epoll->loop_total; i++){
        //php_printf("action(.events) : %d\n" ,request->context->epoll->events[i].events);
        if (request->context->epoll->events[i].events & EPOLLIN){
          action = CURL_CSELECT_IN;
          //php_printf("action(.events) : %d\n" ,action);
          mcode = curl_multi_socket_action(request->curlm->multi_handle, 
          request->context->epoll->events[i].data.fd, action, &(request->curlm->running_handles));
          //php_printf("action-mcode : %d\n", mcode);
        }else if (request->context->epoll->events[i].events & EPOLLOUT){
          action = CURL_CSELECT_OUT;
          //php_printf("action(.events) : %d\n" ,action);
          mcode = curl_multi_socket_action(request->curlm->multi_handle, 
          request->context->epoll->events[i].data.fd, action, &(request->curlm->running_handles));
          //php_printf("action-mcode : %d\n", mcode);
        }
          
      }
    }else {
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "epoll_wait error '%s'", strerror(errno));
      break;
    }

  }

  request->curlm->timeout = 1;


  CURLMsg *msg;
  int msgs_left;
  CURL *easy;
  CURLcode error_code;

  jsr_curl_item_t *item;
  size_t size;

  zval *response_tmp;

  while ((msg = curl_multi_info_read(request->curlm->multi_handle, &msgs_left)))
  {
    if (msg->msg == CURLMSG_DONE){
      easy = msg->easy_handle;
      error_code = msg->data.result;

      //php_printf("error_code : %d\n", error_code);

      curl_easy_getinfo(easy, CURLINFO_PRIVATE, &item);

      if (error_code != CURLE_OK){
        switch (error_code)
        {
          case CURLE_UNSUPPORTED_PROTOCOL:
            response_tmp = _php_jsr_response_error(-32001, "Curl Unsupported Protocol", &item->payload_id);
            break;
          case CURLE_FAILED_INIT:
            response_tmp = _php_jsr_response_error(-32002, "Curl Failed Init", &item->payload_id);
            break;
          case CURLE_URL_MALFORMAT:
            response_tmp = _php_jsr_response_error(-32003, "Curl Url Malformat", &item->payload_id);
            break;
          case 4:
            response_tmp = _php_jsr_response_error(-32004, "Curl Not Built In", &item->payload_id);
            break;
          case CURLE_COULDNT_RESOLVE_PROXY:
            response_tmp = _php_jsr_response_error(-32005, "Curl Couldnt Resolve Proxy", &item->payload_id);
            break;
          case CURLE_COULDNT_RESOLVE_HOST:
            response_tmp = _php_jsr_response_error(-32006, "Curl Couldnt Resolve Host", &item->payload_id);
            break;
          case CURLE_COULDNT_CONNECT:
            response_tmp = _php_jsr_response_error(-32007, "Curl Couldnt Connect", &item->payload_id);
            break;
          case CURLE_OUT_OF_MEMORY:
            response_tmp = _php_jsr_response_error(-32027, "Curl Out Of Memory", &item->payload_id);
            break;
          case CURLE_OPERATION_TIMEDOUT:
            response_tmp = _php_jsr_response_error(-32028, "Curl Operaiton Timeout", &item->payload_id);
            break;
          case CURLE_RANGE_ERROR:
            response_tmp = _php_jsr_response_error(-32033, "Curl Range Error", &item->payload_id);
            break;
          case CURLE_HTTP_POST_ERROR:
            response_tmp = _php_jsr_response_error(-32034, "Curl Http Post Error", &item->payload_id);
            break;
          case CURLE_SEND_ERROR:
            response_tmp = _php_jsr_response_error(-32055, "Curl Send Error", &item->payload_id);
            break;
          case CURLE_RECV_ERROR:
            response_tmp = _php_jsr_response_error(-32056, "Curl Recv Error", &item->payload_id);
            break;
          default:
            response_tmp = _php_jsr_response_error(-32099, "Curl Error Unknow", &item->payload_id);
            break;
        }
        
        if (!response_type){
          add_index_zval(response, item->response_id, response_tmp);
        }else {
          smart_str buf = {0};
          php_json_encode(&buf, response_tmp, 0 TSRMLS_CC);
          add_index_stringl(response, item->response_id, buf.c, buf.len, 1);
          zval_dtor(response_tmp);
        }
        //zval_ptr_dtor(&response_tmp);

      }

      if (item->write_data){
        if (!response_type){
          zval *tmp;
          MAKE_STD_ZVAL(tmp);

          ZVAL_STRINGL(tmp, item->write_data, item->write_length, 1);
          func_params = emalloc(sizeof(zval *) * 1);
          func_params[0] = tmp;

          ZVAL_STRINGL(func, "Jsonrpc_Yajl::parse", sizeof("Jsonrpc_Yajl::parse") - 1, 0);
          if (call_user_function(EG(function_table), NULL, func, tmp, 1, func_params TSRMLS_CC) == FAILURE)
          {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Yajl::parse()");
            return ;
          }

          add_index_zval(response, item->response_id, tmp);

          efree(func_params);

          /*zval *tmp;
          MAKE_STD_ZVAL(tmp);
          //array_init(tmp);
          //php_printf("%s\n", item->write_data);
          php_json_decode(tmp, item->write_data, item->write_length, 1, 512 TSRMLS_CC);
          add_index_zval(response, item->response_id, tmp);
          //zval_ptr_dtor(&tmp);*/
        }else {
          add_index_stringl(response, item->response_id, item->write_data, item->write_length, 1);
        }
        
        free(item->write_data);
        item->write_data = NULL;
      }

      curl_multi_remove_handle(request->curlm->multi_handle, easy);
      curl_easy_cleanup(easy);
      curl_slist_free_all(item->slist);
    }
  }


  RETVAL_ZVAL(response, 1, 0);
  zval_dtor(response);

}

PHP_METHOD(jsonrpc_client, connect)
{
  char *url;
  long url_len;

  zval *object;
  object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &url, &url_len) == FAILURE)
  {
    return ;
  }

  zend_update_property_stringl(php_jsonrpc_client_entry,
      object, "request_url", sizeof("request_url")-1, url, url_len TSRMLS_CC
    );
}

PHP_METHOD(jsonrpc_client, __call)
{
  char *method_s;
  long method_len;

  zval *method;
  zval *params;
  zval *request_url;
  zval *func;
  zval **exec_params;
  zval retval;

  zval *object;
  object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &method_s, &method_len, &params) == FAILURE)
  {
    return ;
  }
  //jsr_dump_zval(params);

  INIT_ZVAL(retval);

  request_url = zend_read_property(
      php_jsonrpc_client_entry, object, "request_url", sizeof("request_url")-1, 0 TSRMLS_CC
    );
  //jsr_dump_zval(request_url);

  MAKE_STD_ZVAL(method);
  ZVAL_STRINGL(method, method_s, method_len, 0);

  exec_params = emalloc(sizeof(zval *) * 3);
  exec_params[0] = request_url;
  exec_params[1] = method;
  exec_params[2] = params;

  MAKE_STD_ZVAL(func);
  ZVAL_STRINGL(func, "call", sizeof("call") - 1, 0);

  if (call_user_function(NULL, &object, func, &retval, 3, exec_params TSRMLS_CC) == FAILURE){
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed calling Jsonrpc_Client::call()");
    return ;
  }

  //zval_ptr_dtor(&func);
  //zval_ptr_dtor(&method);
  efree(exec_params);
}

static const zend_function_entry jsonrpc_client_class_functions[] = {
  PHP_ME(jsonrpc_client, __construct, jsonrpc_client_construct_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
  PHP_ME(jsonrpc_client, __destruct,  jsonrpc_client_destruct_arginfo,  ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
  PHP_ME(jsonrpc_client, connect,     jsonrpc_client_connect_arginfo,   ZEND_ACC_PUBLIC)
  PHP_ME(jsonrpc_client, __call,      jsonrpc_client___call_arginfo,    ZEND_ACC_PUBLIC)
  PHP_ME(jsonrpc_client, call,        jsonrpc_client_call_arginfo,      ZEND_ACC_PUBLIC)
  PHP_ME(jsonrpc_client, execute,     jsonrpc_client_execute_arginfo,   ZEND_ACC_PUBLIC)
  {NULL, NULL, NULL}
};

void 
jsonrpc_client_init(int module_number TSRMLS_DC)
{

  le_jsr_epoll_persist = zend_register_list_destructors_ex(NULL, _php_jsr_epoll_dtor, "jsonrpc persistent epoll", module_number);
  le_jsr_curlm_persist = zend_register_list_destructors_ex(NULL, _php_jsr_curlm_dtor, "jsonrpc persisten curlm", module_number);

  memcpy(&jsr_request_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

  zend_class_entry jsonrpc_client_class_entry;
  INIT_CLASS_ENTRY(jsonrpc_client_class_entry, "Jsonrpc_Client", jsonrpc_client_class_functions);
  php_jsonrpc_client_entry = zend_register_internal_class(&jsonrpc_client_class_entry TSRMLS_CC);
  //zend_declare_property_null(php_jsonrpc_client_entry, "response", sizeof("response")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_null(php_jsonrpc_client_entry, "request",  sizeof("request")-1,  ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_long(php_jsonrpc_client_entry, "response_total",  sizeof("response_total")-1, 0, ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_stringl(php_jsonrpc_client_entry, "request_url", sizeof("request_url")-1, "", 0, ZEND_ACC_PUBLIC TSRMLS_CC);


  zend_class_entry jsonrpc_client_request_class_entry;
  INIT_CLASS_ENTRY(jsonrpc_client_request_class_entry, "Jsonrpc_Client_Request", NULL);
  jsonrpc_client_request_class_entry.create_object = _php_jsr_request_object_new;
  php_jsonrpc_client_request_entry = zend_register_internal_class(&jsonrpc_client_request_class_entry TSRMLS_CC);

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */