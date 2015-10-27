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
#include "jsr_curl.h"
#include "jsr_epoll.h"


/** {{{ ARG_INFO
 *  */
ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_construct_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, timeout)
    ZEND_ARG_INFO(0, headers)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_destruct_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_call_arginfo, 0, 0, 3)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, procedure)
    ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_execute_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, procedure)
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
_jsr_client_prepare_request(zval *procedure, zval *params)
{
  zval *payload;
  long number;
  int nb_params;

  MAKE_STD_ZVAL(payload);
  array_init(payload);

  add_assoc_string(payload, "jsonrpc", "2.0", 0);
  add_assoc_string(payload, "method", Z_STRVAL_P(procedure), 0);

  if (!BG(mt_rand_is_seeded)) {
    php_mt_srand(GENERATE_SEED() TSRMLS_CC);
  }
  number = (long) (php_mt_rand(TSRMLS_C) >> 1);
  add_assoc_long(payload, "id", number);

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
  printf(">>> %s: adding fd=%d action=%d\n", __func__, fd, action);
  jsr_epoll_t *jsr_epoll = (jsr_epoll_t *) u;

  if (action == CURL_POLL_REMOVE)
  {
    jsr_epoll_del_fd(jsr_epoll, fd);
  }

  if (action == CURL_POLL_IN || action == CURL_POLL_INOUT)
  {
    printf("in\n");
    jsr_epoll_add_fd(jsr_epoll, fd);
    jsr_epoll_set_in(jsr_epoll, fd);
  }

  if (action == CURL_POLL_OUT || action == CURL_POLL_INOUT)
  {
    printf("out\n");
    jsr_epoll_add_fd(jsr_epoll, fd);
    jsr_epoll_set_out(jsr_epoll, fd);
  }

  return 0;
}

static int 
_timer_callback(CURLM *multi, long timeout_ms, void *u)
{
  printf(">>> %s: timeout: %ld ms\n", __func__, timeout_ms);
    return 0;
}

PHP_METHOD(jsonrpc_client, __construct)
{
  zval *_url;
  zval *timeout_c;
  zval *headers;
  zval *object;
  zval *_headers;

  php_jsr_reuqest_object *request;

  zval *request_obj;

  object = getThis();

  /*MAKE_STD_ZVAL(timeout_c);
  ZVAL_LONG(timeout_c, 5);

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zz",
    &_url, &timeout_c, &headers) == FAILURE)
  {

  }

  MAKE_STD_ZVAL(_headers);
  array_init(_headers);
  add_index_string(_headers, 0, estrdup("Content-Type: application/json"), 0);
  add_next_index_string(_headers, estrdup("Accept: application/json"), 0);

  add_property_zval(object, "url", _url);
  
  if (Z_TYPE_P(timeout_c) == IS_LONG){
    add_property_zval(object, "timeout", timeout_c);
  }else {
    ZVAL_LONG(timeout_c, 5);
    add_property_zval(object, "timeout", timeout_c);
  }

  if (Z_TYPE_P(headers) == IS_ARRAY){
    zend_hash_merge(Z_ARRVAL_P(_headers), Z_ARRVAL_P(headers), NULL, NULL, sizeof(zval *), 1);
    add_property_zval(object, "headers", _headers);
  }else {
    add_property_zval(object, "headers", _headers);
  }
  */
  jsr_curl_global_new();

  //epoll = jsr_epoll_init();
  //curlm = jsr_curlm_new();

  MAKE_STD_ZVAL(request_obj);
  object_init_ex(request_obj, php_jsonrpc_client_request_entry);
  request = (php_jsr_reuqest_object *)zend_object_store_get_object(request_obj TSRMLS_CC);

  request->epoll->epoll_fd = epoll_create(1024);

  request->curlm->multi_handle = curl_multi_init();
  request->curlm->list = jsr_list_new();
  request->curlm->running_handles = 1;


  curl_multi_setopt(request->curlm->multi_handle, CURLMOPT_SOCKETFUNCTION, _socket_callback);
  curl_multi_setopt(request->curlm->multi_handle, CURLMOPT_SOCKETDATA, request->epoll);
  curl_multi_setopt(request->curlm->multi_handle, CURLMOPT_TIMERFUNCTION, _timer_callback);

  add_property_zval(object, "request", request_obj);

  //MAKE_STD_ZVAL(request->item_array);
  //array_init(request->item_array);

}

PHP_METHOD(jsonrpc_client, __destruct)
{
  php_jsr_reuqest_object *request;

  zval *object;
  zval *request_obj;

  object = getThis();

  request_obj = zend_read_property(
      php_jsonrpc_client_entry, object, "request", sizeof("request")-1, 0 TSRMLS_CC
    );

  request = (php_jsr_reuqest_object *)zend_object_store_get_object(request_obj TSRMLS_CC);

  close(request->epoll->epoll_fd);

  jsr_node_t *node;
  jsr_curl_item_t *item;
  for (node = jsr_list_first(request->curlm->list) ; node != NULL; node = jsr_list_next(request->curlm->list))
  {
      item = jsr_list_item(request->curlm->list);
      jsr_curl_item_destroy(&item);
  }
  curl_multi_cleanup(request->curlm->multi_handle);
  //jsr_list_purge(request->curlm->list);


  jsr_curl_global_destroy();
}

PHP_METHOD(jsonrpc_client, call)
{
  php_jsr_reuqest_object *request;
  jsr_curl_item_t *jsr_curl_item;
  jsr_epoll_t **epoll_p;

  zval *url;
  zval *procedure;
  zval *params;
  zval *item;
  zval *object;
  zval *request_obj;

  object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", &url, &procedure, &params) == FAILURE)
  {
    return ;
  }

  //jsr_dump_zval(object);

  request_obj = zend_read_property(
      php_jsonrpc_client_entry, object, "request", sizeof("request")-1, 0 TSRMLS_CC
    );

  request = (php_jsr_reuqest_object *)zend_object_store_get_object(request_obj TSRMLS_CC);

  /*MAKE_STD_ZVAL(item);
  array_init(item);
  add_assoc_string(item, "url", Z_STRVAL_P(url), 0);
  add_assoc_string(item, "procedure", Z_STRVAL_P(procedure), 0);
  add_assoc_zval(item, "params", params);

  add_next_index_zval(request->item_array, item);

*/
  jsr_curl_item = jsr_curl_item_new(
      Z_STRVAL_P(url),
      "key=value",
      9
    );
  jsr_curl_item_setopt(jsr_curl_item);
  jsr_curlm_list_append(request->curlm, jsr_curl_item);

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

  object = getThis();

  /*if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz",
    &procedure, &params) == FAILURE)
  {

  }*/

  request_obj = zend_read_property(
        php_jsonrpc_client_entry, object, "request", sizeof("request")-1, 0 TSRMLS_CC
      );
  request = (php_jsr_reuqest_object *)zend_object_store_get_object(request_obj TSRMLS_CC);


  /*MAKE_STD_ZVAL(response);
  MAKE_STD_ZVAL(payload);
  payload = _jsr_client_prepare_request(procedure, params);

  func_params = emalloc(sizeof(zval *) * 1);
  func_params[0] = payload;

  MAKE_STD_ZVAL(func);
  ZVAL_STRINGL(func, "dorequest", sizeof("dorequest") - 1, 0);

  if (call_user_function(NULL, &object, func, response, 1, func_params TSRMLS_CC) == FAILURE)
  {

  }
  efree(func_params);
*/
/*####################*/

  jsr_curlm_post(request->curlm);

  request->curlm->running_handles = 1;
  request->epoll->loop_total = 0;
  while (request->curlm->running_handles > 0)
  {
    printf(">>> calling epoll_wait\n");
    request->epoll->loop_total = jsr_epoll_loop(request->epoll , 1000);
    printf("%d\n", request->epoll->loop_total);

    if (request->epoll->loop_total == 0){
      curl_multi_socket_action(request->curlm->multi_handle, CURL_SOCKET_TIMEOUT, 0, &(request->curlm->running_handles));
    }
    else 
    {
      int i = 0;
      for (i = 0; i < request->epoll->loop_total; i++){
        curl_multi_socket_action(request->curlm->multi_handle, request->epoll->events[i].data.fd, 0, &(request->curlm->running_handles));
      }
    }

  }
/*#####################*/
  //RETVAL_ZVAL(response, 1, 0);

}

PHP_METHOD(jsonrpc_client, dorequest)
{
  /*zval *payload;
  zval    *zid;
  php_curl  *ch;

  zval *url, *timeout, *headers;

  zval *object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", 
    &payload) == FAILURE)
  {
  }

  zid = zend_read_property(
      php_jsonrpc_server_entry, object, "ch", sizeof("ch")-1, 0 TSRMLS_CC
    );
  url = zend_read_property(
      php_jsonrpc_server_entry, object, "url", sizeof("url")-1, 0 TSRMLS_CC
    );
  timeout = zend_read_property(
      php_jsonrpc_server_entry, object, "timeout", sizeof("timeout")-1, 0 TSRMLS_CC
    );
  headers = zend_read_property(
      php_jsonrpc_server_entry, object, "headers", sizeof("headers")-1, 0 TSRMLS_CC
    );

  zval              **current;
  HashTable          *ph;
  struct curl_slist  *slist = NULL;

  ph = HASH_OF(headers);
  if (!ph) {
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "You must pass either an object or an array with headers");
    RETVAL_FALSE;
    return ;
  }
  
  for (zend_hash_internal_pointer_reset(ph);
     zend_hash_get_current_data(ph, (void **) &current) == SUCCESS;
     zend_hash_move_forward(ph)
  ) {
    SEPARATE_ZVAL(current);
    convert_to_string_ex(current);

    slist = curl_slist_append(slist, Z_STRVAL_PP(current));
    if (!slist) {
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not build curl_slist");
      RETVAL_FALSE;
      return ;
    }
  }
  //zend_llist_add_element(&ch->to_free->slist, &slist);

  smart_str buf = {0};

  php_json_encode(&buf, payload, 0 TSRMLS_CC);
  ZVAL_STRINGL(payload, buf.c, buf.len, 1);
  smart_str_free(&buf);


  ZEND_FETCH_RESOURCE(ch, php_curl *, &zid, -1, le_curl_name, le_curl);

  //curl_easy_setopt(ch->cp, CURLOPT_URL, Z_STRVAL_P(url));
  if (!php_curl_option_url(ch, Z_STRVAL_P(url), Z_STRLEN_P(url)) ) {
      _php_curl_close_ex(ch TSRMLS_CC);
      RETVAL_FALSE;
      return ;
  }
  curl_easy_setopt(ch->cp, CURLOPT_HEADER, 0);
  //curl_easy_setopt(ch->cp, CURLOPT_RETURNTRANSFER, 1);
  ch->handlers->write->method = PHP_CURL_RETURN;

  convert_to_long_ex(&timeout);
  curl_easy_setopt(ch->cp, CURLOPT_CONNECTTIMEOUT, Z_LVAL_P(timeout));
  curl_easy_setopt(ch->cp, CURLOPT_USERAGENT, "JSON-RPC PHP Client");
  curl_easy_setopt(ch->cp, CURLOPT_HTTPHEADER, slist);
  curl_easy_setopt(ch->cp, CURLOPT_FOLLOWLOCATION, 0);
  curl_easy_setopt(ch->cp, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(ch->cp, CURLOPT_SSL_VERIFYPEER, 1);

  convert_to_string_ex(&payload);
  curl_easy_setopt(ch->cp, CURLOPT_POSTFIELDS, Z_STRVAL_P(payload));
  curl_easy_setopt(ch->cp, CURLOPT_POSTFIELDSIZE, Z_STRLEN_P(payload));

  // curl_exec 
  zval *http_body;
  zval *response;
  CURLcode  error;

  MAKE_STD_ZVAL(http_body);
  MAKE_STD_ZVAL(response);

  _php_curl_verify_handlers(ch, 1 TSRMLS_CC);

  _php_curl_cleanup_handle(ch);

  error = curl_easy_perform(ch->cp);
  SAVE_CURL_ERROR(ch, error);
  // CURLE_PARTIAL_FILE is returned by HEAD requests 
  if (error != CURLE_OK && error != CURLE_PARTIAL_FILE) {
    if (ch->handlers->write->buf.len > 0) {
      smart_str_free(&ch->handlers->write->buf);
    }
    //RETURN_FALSE;
    RETVAL_FALSE;
    return ;
  }

  if (ch->handlers->std_err) {
    php_stream  *stream;
    stream = (php_stream*)zend_fetch_resource(&ch->handlers->std_err TSRMLS_CC, -1, NULL, NULL, 2, php_file_le_stream(), php_file_le_pstream());
    if (stream) {
      php_stream_flush(stream);
    }
  }

  if (ch->handlers->write->method == PHP_CURL_RETURN && ch->handlers->write->buf.len > 0) {
    smart_str_0(&ch->handlers->write->buf);
    ZVAL_STRINGL(http_body, ch->handlers->write->buf.c, ch->handlers->write->buf.len, 1);
    goto doresponse;
  }

  // flush the file handle, so any remaining data is synched to disk 
  if (ch->handlers->write->method == PHP_CURL_FILE && ch->handlers->write->fp) {
    fflush(ch->handlers->write->fp);
  }
  if (ch->handlers->write_header->method == PHP_CURL_FILE && ch->handlers->write_header->fp) {
    fflush(ch->handlers->write_header->fp);
  }

  if (ch->handlers->write->method == PHP_CURL_RETURN) {
    ZVAL_STRING(http_body, "", 1);
    goto doresponse;
  } else {
    //RETURN_TRUE;
    RETVAL_TRUE;
    return ;
  }

doresponse:
  array_init(response);
  php_json_decode(response, Z_STRVAL_P(http_body), Z_STRLEN_P(http_body), 1, 512 TSRMLS_CC);

  if (Z_TYPE_P(response) == IS_ARRAY)
  {
    RETVAL_ZVAL(response, 1, 0);
    return ;
  } else {
    RETVAL_ZVAL(response, 1, 0);
    return ;
  }
*/
}

static const zend_function_entry jsonrpc_client_class_functions[] = {
  PHP_ME(jsonrpc_client, __construct, jsonrpc_client_construct_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
  PHP_ME(jsonrpc_client, __destruct,  jsonrpc_client_destruct_arginfo,  ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
  PHP_ME(jsonrpc_client, call,        jsonrpc_client_call_arginfo,      ZEND_ACC_PUBLIC)
  PHP_ME(jsonrpc_client, execute,     jsonrpc_client_execute_arginfo,   ZEND_ACC_PUBLIC)
  PHP_ME(jsonrpc_client, dorequest,   jsonrpc_client_dorequest_arginfo, ZEND_ACC_PUBLIC)
  {NULL, NULL, NULL}
};

void 
jsonrpc_client_init()
{
  zend_class_entry jsonrpc_client_class_entry;
  INIT_CLASS_ENTRY(jsonrpc_client_class_entry, "Jsonrpc_Client", jsonrpc_client_class_functions);
  php_jsonrpc_client_entry = zend_register_internal_class(&jsonrpc_client_class_entry TSRMLS_CC);

  zend_class_entry jsonrpc_client_request_class_entry;
  INIT_CLASS_ENTRY(jsonrpc_client_request_class_entry, "Jsonrpc_Client_Request", NULL);
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