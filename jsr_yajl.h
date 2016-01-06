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
#ifndef JSR_YAJL_H
#define JSR_YAJL_H

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"

#include "yajl/api/yajl_version.h"
#include "yajl/api/yajl_gen.h"
#include "yajl/api/yajl_parse.h"
#include "yajl/yajl_parser.h"

#define PHP_YAJL_VERSION "0.0.1"

#define PHP_JSON_OUTPUT_ARRAY 0
#define PHP_JSON_OUTPUT_OBJECT 1

#define STATUS_CONTINUE 1
#define STATUS_ABORT    0

#define RETURN_ERROR(ctx,retval,...) {                                  \
        if ((ctx)->errbuf != NULL)                                      \
            snprintf ((ctx)->errbuf, (ctx)->errbuf_size, __VA_ARGS__);  \
        return (retval);                                                \
    }

struct stack_elem_s;
typedef struct stack_elem_s stack_elem_t;
struct stack_elem_s
{
    zval *key;
    zval *value;
    stack_elem_t *next;
    int type; // 0: map;   1: array
};

struct context_s
{
    stack_elem_t *stack;
    zval *root;
    char *errbuf;
    size_t errbuf_size;
};
typedef struct context_s context_t;

typedef struct _php_yajl_t {

    yajl_gen gen;

    yajl_handle handle;
    yajl_status status;
    context_t *ctx;    

} php_yajl_t;

static zend_class_entry *php_jsonrpc_yajl_class_entry;

static int json_determine_array_type(zval **val TSRMLS_DC);
static void php_yajl_generate_array(yajl_gen gen, zval *val TSRMLS_DC);
void php_yajl_generate(yajl_gen gen, zval *val TSRMLS_DC);

static int object_add_keyval(context_t *ctx, zval *obj, zval *key, zval *value);
static int array_add_value (context_t *ctx, zval *array, zval *value);
static int context_push(context_t *ctx, zval *v, int type);
static zval* context_pop(context_t *ctx);
static int context_add_value (context_t *ctx, zval *v);
static int handle_string (void *ctx, const unsigned char *string, size_t string_length);
static int handle_number (void *ctx, const char *string, size_t string_length);
static int handle_integer(void *ctx, long long int value);
static int handle_double(void *ctx, double value);
static int handle_start_map (void *ctx);
static int handle_end_map (void *ctx);
static int handle_start_array (void *ctx);
static int handle_end_array (void *ctx);
static int handle_boolean (void *ctx, int boolean_value);
static int handle_null (void *ctx);
zval* yajl_zval_parse (const char *input, char *error_buffer, size_t error_buffer_size);
void yajl_zval_free (zval *v);

static void php_yajl_rsrc_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
static zval* php_yajl_new();

PHP_METHOD(jsonrpc_yajl, generate);
PHP_METHOD(jsonrpc_yajl, parse);

void jsonrpc_yajl_init(int module_number TSRMLS_DC);

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */