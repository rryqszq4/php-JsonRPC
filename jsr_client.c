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

extern zend_class_entry *php_jsonrpc_client_entry;

/** {{{ ARG_INFO
 *  */
ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_construct_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, timeout)
    ZEND_ARG_INFO(0, headers)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_destruct_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_execute_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, procedure)
    ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(jsonrpc_client_dorequest_arginfo, 0, 0, 1)
  ZEND_ARG_INFO(0, payload)
ZEND_END_ARG_INFO()
/* }}} */

PHP_METHOD(jsonrpc_client, __construct)
{
  CURL *cp;
  php_curl  *ch;
  zval    *clone;
  char    *url = NULL;
  int   url_len = 0;
  char *cainfo;

  zval *_url;
  zval *timeout_c;
  zval *headers;
  zval *object;
  zval *_headers;
  zval *_ch;

  object = getThis();

  MAKE_STD_ZVAL(timeout_c);
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

  MAKE_STD_ZVAL(_ch);
  ZEND_REGISTER_RESOURCE(_ch, ch, le_curl);
  ch->id = Z_LVAL_P(_ch);
  add_property_resource(object, "ch", ch->id);

}

PHP_METHOD(jsonrpc_client, __destruct)
{
  zval    *zid;
  php_curl  *ch;
  zval *object;

  object = getThis();

  zid = zend_read_property(
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

PHP_METHOD(jsonrpc_client, execute)
{
  zval *procedure;
  zval *params;
  zval *request;
  zval *response;

  zval *object;
  zval *func;
  zval **func_params;

  object = getThis();

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz",
    &procedure, &params) == FAILURE)
  {

  }

  MAKE_STD_ZVAL(response);
  MAKE_STD_ZVAL(request);
  request = jr_client_prepare_request(procedure, params);

  func_params = emalloc(sizeof(zval *) * 1);
  func_params[0] = request;

  MAKE_STD_ZVAL(func);
  ZVAL_STRINGL(func, "dorequest", sizeof("dorequest") - 1, 0);

  if (call_user_function(NULL, &object, func, response, 1, func_params TSRMLS_CC) == FAILURE)
  {

  }
  efree(func_params);

  RETVAL_ZVAL(response, 1, 0);

}

PHP_METHOD(jsonrpc_client, dorequest)
{
  zval *payload;
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

  /* curl_exec */
  zval *http_body;
  zval *response;
  CURLcode  error;

  MAKE_STD_ZVAL(http_body);
  MAKE_STD_ZVAL(response);

  _php_curl_verify_handlers(ch, 1 TSRMLS_CC);

  _php_curl_cleanup_handle(ch);

  error = curl_easy_perform(ch->cp);
  SAVE_CURL_ERROR(ch, error);
  /* CURLE_PARTIAL_FILE is returned by HEAD requests */
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

  /* flush the file handle, so any remaining data is synched to disk */
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

}

zend_function_entry jsonrpc_client_class_functions[] = {
  PHP_ME(jsonrpc_client, __construct, jsonrpc_client_construct_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
  PHP_ME(jsonrpc_client, __destruct,  jsonrpc_client_destruct_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR);
  PHP_ME(jsonrpc_client, execute,     jsonrpc_client_execute_arginfo, ZEND_ACC_PUBLIC);
  PHP_ME(jsonrpc_client, dorequest,   jsonrpc_client_dorequest_arginfo, ZEND_ACC_PUBLIC);
  {NULL, NULL, NULL}
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */