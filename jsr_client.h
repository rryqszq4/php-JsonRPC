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

#ifndef PHP_JSR_CLIENT_H
#define PHP_JSR_CLIENT_H

#include "jsr_epoll.h"
#include "jsr_curl.h" 

#define COUNT_RECURSIVE     1

typedef struct _php_jsr_epoll_context {

  jsr_epoll_t *epoll;

  zend_bool is_persistent;

  //zend_bool is_global;

} php_jsr_epoll_context;


typedef struct _php_jsr_curlm_conn {

  jsr_curlm_t *curlm;

  zend_bool is_persistent;

} php_jsr_curlm_conn;


typedef struct _php_jsr_request_object {
    zend_object zo;

    //jsr_epoll_t *epoll;
    php_jsr_epoll_context *context;

    jsr_curlm_t *curlm;

    zend_bool executed;

    zend_bool is_persistent;

} php_jsr_reuqest_object;

#endif


static zend_class_entry *php_jsonrpc_client_entry;
static zend_class_entry *php_jsonrpc_client_request_entry;

static zend_object_handlers jsr_request_object_handlers;

static int _php_count_recursive(zval *array, long mode TSRMLS_DC);

static zval* _jsr_client_prepare_request(zval *procedure, zval *params, zval *custom_id TSRMLS_DC);

static zval* _php_jsr_response_error(long code, char *message, jsr_payload_id *payload_id);

static int _socket_callback(CURL *easy, curl_socket_t fd, int action, void *u, void *s);
static int _timer_callback(CURLM *multi, long timeout_ms, void *u);

static zend_object_value _php_jsr_request_object_new(zend_class_entry *class_type TSRMLS_DC);
static void _php_jsr_request_object_free_storage(void *object TSRMLS_DC);

static size_t _write_callback(char *ptr, size_t size, size_t nmemb, void *ctx);

static php_jsr_epoll_context *_php_jsr_epoll_new(zend_bool is_persistent TSRMLS_DC);
static php_jsr_epoll_context *_php_jsr_epoll_get(zend_bool is_persistent TSRMLS_DC);
static void _php_jsr_epoll_destroy(php_jsr_epoll_context *context);
ZEND_RSRC_DTOR_FUNC(_php_jsr_epoll_dtor);

static php_jsr_curlm_conn *_php_jsr_curlm_new(zend_bool is_persistent TSRMLS_DC);
static php_jsr_curlm_conn *_php_jsr_curlm_get(zend_bool is_persistent TSRMLS_DC);
static void _php_jsr_curlm_destroy(php_jsr_curlm_conn *conn);
ZEND_RSRC_DTOR_FUNC(_php_jsr_curlm_dtor);

PHP_METHOD(jsonrpc_client, __construct);
PHP_METHOD(jsonrpc_client, __destruct);
PHP_METHOD(jsonrpc_client, call);
PHP_METHOD(jsonrpc_client, execute);
//PHP_METHOD(jsonrpc_client, dorequest);

void jsonrpc_client_init(int module_number TSRMLS_DC);
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */