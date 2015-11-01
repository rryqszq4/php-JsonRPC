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

#ifndef PHP_JSR_SERVER_H
#define PHP_JSR_SERVER_H

static zend_class_entry *php_jsonrpc_server_entry;

static zval* jr_file_get_contents();
static zval* jr_server_get_arguments(zval *request_params, zval *method_params,
  int nb_required_params, int nb_max_params);

PHP_METHOD(jsonrpc_server, __construct);
PHP_METHOD(jsonrpc_server, register);
PHP_METHOD(jsonrpc_server, bind);
PHP_METHOD(jsonrpc_server, execute);
PHP_METHOD(jsonrpc_server, jsonformat);
PHP_METHOD(jsonrpc_server, rpcformat);
PHP_METHOD(jsonrpc_server, executeprocedure);
PHP_METHOD(jsonrpc_server, executecallback);
PHP_METHOD(jsonrpc_server, getresponse);

void jsonrpc_server_init();

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */