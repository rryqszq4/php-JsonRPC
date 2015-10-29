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

typedef struct _php_jsr_request_object {
    zend_object zo;

    jsr_epoll_t *epoll;

    jsr_curlm_t *curlm;

    zval *item_array;

} php_jsr_reuqest_object;

#endif

static zend_class_entry *php_jsonrpc_client_entry;
static zend_class_entry *php_jsonrpc_client_request_entry;

static zend_object_handlers jsr_request_object_handlers;

static int _php_count_recursive(zval *array, long mode TSRMLS_DC);
static zval* _jsr_client_prepare_request(zval *procedure, zval *params);
static int _socket_callback(CURL *easy, curl_socket_t fd, int action, void *u, void *s);
static int _timer_callback(CURLM *multi, long timeout_ms, void *u);

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */