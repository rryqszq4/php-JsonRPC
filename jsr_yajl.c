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
#include "jsr_yajl.h"

static int le_yajl;

ZEND_BEGIN_ARG_INFO_EX(arginfo_jsonrpc_yajl_construct, 0, 0, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_jsonrpc_yajl_generate, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_jsonrpc_yajl_parse, 0, 0, 1)
    ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()


static int json_determine_array_type(zval **val TSRMLS_DC) /* {{{ */
{
  int i;
  HashTable *myht = HASH_OF(*val);

  i = myht ? zend_hash_num_elements(myht) : 0;
  if (i > 0) {
    char *key;
    ulong index, idx;
    uint key_len;
    HashPosition pos;

    zend_hash_internal_pointer_reset_ex(myht, &pos);
    idx = 0;
    for (;; zend_hash_move_forward_ex(myht, &pos)) {
      i = zend_hash_get_current_key_ex(myht, &key, &key_len, &index, 0, &pos);
      if (i == HASH_KEY_NON_EXISTANT) {
        break;
      }

      if (i == HASH_KEY_IS_STRING) {
        return 1;
      } else {
        if (index != idx) {
          return 1;
        }
      }
      idx++;
    }
  }

  return PHP_JSON_OUTPUT_ARRAY;
}
/* }}} */

static void php_yajl_generate_array(yajl_gen gen, zval *val TSRMLS_DC)
{
  HashTable *hash_table;
  int i, r;

  if (Z_TYPE_P(val) == IS_ARRAY)
  {
    hash_table = HASH_OF(val);
    r = json_determine_array_type(&val TSRMLS_CC);
    if (r == PHP_JSON_OUTPUT_ARRAY){
      yajl_gen_array_open(gen);
    }else {
      yajl_gen_map_open(gen);
    }
  }else {
    hash_table = Z_OBJPROP_P(val);
    r = PHP_JSON_OUTPUT_OBJECT;
    yajl_gen_map_open(gen);
  }

  if (hash_table && hash_table->nApplyCount > 1) {
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "recursion detected");
    yajl_gen_null(gen);
    return;
  }

  i = hash_table ? zend_hash_num_elements(hash_table) : 0;

  if (i > 0)
  {
    HashPosition pos;
    char *key;
    ulong index;
    uint key_len;
    zval **data;
    HashTable *tmp_ht;
    char key_index[10];

    zend_hash_internal_pointer_reset_ex(hash_table, &pos);
    for (;; zend_hash_move_forward_ex(hash_table, &pos))
    {
      i = zend_hash_get_current_key_ex(hash_table, &key, &key_len, &index, 0, &pos);
      if (i == HASH_KEY_NON_EXISTANT)
        break;

      if (zend_hash_get_current_data_ex(hash_table, (void **)&data, &pos) == SUCCESS)
      {
        tmp_ht = HASH_OF(*data);
        if (tmp_ht){
          tmp_ht->nApplyCount++;
        }

        if (r == PHP_JSON_OUTPUT_ARRAY){
          php_yajl_generate(gen, *data TSRMLS_CC);
        }else if (r == PHP_JSON_OUTPUT_OBJECT){
          if (i == HASH_KEY_IS_STRING){
            yajl_gen_string(gen, key, key_len-1);
          }else {
            sprintf(key_index, "%d", index);
            yajl_gen_string(gen, key_index, strlen(key_index));
          }
          php_yajl_generate(gen, *data TSRMLS_CC);
        }

        if (tmp_ht) {
          tmp_ht->nApplyCount--;
        }
      }
    }
  }

  if (r == PHP_JSON_OUTPUT_ARRAY){
    yajl_gen_array_close(gen);
  }else {
    yajl_gen_map_close(gen);
  }

}

void php_yajl_generate(yajl_gen gen, zval *val TSRMLS_DC)
{
  switch(Z_TYPE_P(val))
  {
    case IS_NULL:
      yajl_gen_null(gen);
      break;

    case IS_BOOL:
      if (Z_BVAL_P(val)){
        yajl_gen_bool(gen, 1);
      }else {
        yajl_gen_bool(gen, 0);
      }
      break;

    case IS_LONG:
      yajl_gen_integer(gen, Z_LVAL_P(val));
      break;

    case IS_DOUBLE:
      yajl_gen_double(gen, Z_DVAL_P(val));
      break;

    case IS_STRING:
      yajl_gen_string(gen, Z_STRVAL_P(val), Z_STRLEN_P(val));
      break;

    case IS_ARRAY:
    case IS_OBJECT:
      php_yajl_generate_array(gen, val TSRMLS_CC);
      break;

    default:
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "type is unsupported, generate as null");
      yajl_gen_null(gen);
      break;
  }

  return ;
}

static int object_add_keyval(context_t *ctx,
                             zval *obj, zval *key, zval *value)
{

    /* We're checking for NULL in "context_add_value" or its callers. */
    assert (ctx != NULL);
    assert (obj != NULL);
    assert (key != NULL);
    assert (value != NULL);

    /* We're assuring that "obj" is an object in "context_add_value". */
    assert(Z_TYPE_P(obj) == IS_OBJECT);

    add_assoc_zval(obj, Z_STRVAL_P(key), value);

    return (0);
}

static int array_add_value (context_t *ctx,
                            zval *array, zval *value)
{

    /* We're checking for NULL pointers in "context_add_value" or its
     * callers. */
    assert (ctx != NULL);
    assert (array != NULL);
    assert (value != NULL);

    /* "context_add_value" will only call us with array values. */
    assert(YAJL_IS_ARRAY(array));

  add_next_index_zval(array, value);

    return 0;
}

static int context_push(context_t *ctx, zval *v, int type)
{
    stack_elem_t *stack;

    stack = emalloc (sizeof (*stack));
    if (stack == NULL)
        RETURN_ERROR (ctx, ENOMEM, "Out of memory");
    memset (stack, 0, sizeof (*stack));

    assert ((ctx->stack == NULL)
            || Z_TYPE_P(v) == IS_OBJECT
            || Z_TYPE_P(v) == IS_ARRAY);

    stack->type = type;
    stack->key = NULL;
    stack->value = v;
    stack->next = ctx->stack;
    ctx->stack = stack;

    return (0);
}

static zval* context_pop(context_t *ctx)
{
    stack_elem_t *stack;
    zval *v;

    if (ctx->stack == NULL)
        RETURN_ERROR (ctx, NULL, "context_pop: "
                      "Bottom of stack reached prematurely");

    stack = ctx->stack;
    ctx->stack = stack->next;

    v = stack->value;

    efree (stack);

    return (v);
}

static int context_add_value (context_t *ctx, zval *v)
{
    assert (ctx != NULL);
    assert (v != NULL);

    if (ctx->stack == NULL)
    {
        assert (ctx->root == NULL);
        ctx->root = v;
        return (0);
    }
    else if (Z_TYPE_P(ctx->stack->value) == IS_ARRAY)
    {
        if (ctx->stack->type == 0){
            if (ctx->stack->key == NULL)
            {
                if (Z_TYPE_P(v) != IS_STRING)
                    RETURN_ERROR (ctx, EINVAL, "context_add_value: "
                                  "Object key is not a string (%#04x)",
                                  Z_TYPE_P(v));

                ctx->stack->key = v;

                return (0);
            }
            else /* if (ctx->key != NULL) */
            {
                zval * key;

                key = ctx->stack->key;
                ctx->stack->key = NULL;
                return (object_add_keyval (ctx, ctx->stack->value, key, v));
            }
        }else if (ctx->stack->type == 1){
            return (array_add_value (ctx, ctx->stack->value, v));
        }
    }
    /*else if (Z_TYPE_P(ctx->stack->value) == IS_ARRAY)
    {

        return (array_add_value (ctx, ctx->stack->value, v));
    }*/
    else
    {
        RETURN_ERROR (ctx, EINVAL, "context_add_value: Cannot add value to "
                      "a value of type %#04x (not a composite type)",
                      Z_TYPE_P(ctx->stack->value));
    }
}

static int handle_string (void *ctx,
                          const unsigned char *string, size_t string_length)
{
    zval *v;
    MAKE_STD_ZVAL(v);

    ZVAL_STRINGL(v, string, string_length, 1);

    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_number (void *ctx, const char *string, size_t string_length)
{
    zval *v;
    MAKE_STD_ZVAL(v);

    ZVAL_STRINGL(v, string, string_length, 1);

    return ((context_add_value(ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_integer(void *ctx, long long int value)
{
    zval *v;
    MAKE_STD_ZVAL(v);

    ZVAL_LONG(v, value);

    return ((context_add_value(ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_double(void *ctx, double value)
{
    zval *v;
    MAKE_STD_ZVAL(v);

    ZVAL_DOUBLE(v, value);

    return ((context_add_value(ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_start_map (void *ctx)
{
    int type = 0;
    zval *v;
    MAKE_STD_ZVAL(v);

    array_init(v);

    return ((context_push (ctx, v, type) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_end_map (void *ctx)
{
  zval *v;

    v = context_pop (ctx);
    if (v == NULL)
        return (STATUS_ABORT);
    
    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_start_array (void *ctx)
{
    int type = 1;
    zval *v;
    MAKE_STD_ZVAL(v);

    array_init(v);

    return ((context_push (ctx, v, type) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_end_array (void *ctx)
{
  zval *v;

    v = context_pop (ctx);
    if (v == NULL)
        return (STATUS_ABORT);

    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_boolean (void *ctx, int boolean_value)
{
    zval *v;
    MAKE_STD_ZVAL(v);

    ZVAL_BOOL(v, boolean_value);

    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_null (void *ctx)
{
    zval *v;
    MAKE_STD_ZVAL(v);

    ZVAL_NULL(v);

    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}


zval* yajl_zval_parse (const char *input,
                          char *error_buffer, size_t error_buffer_size)
{
    static const yajl_callbacks callbacks =
        {
            /* null        = */ handle_null,
            /* boolean     = */ handle_boolean,
            /* integer     = */ handle_integer,
            /* double      = */ handle_double,
            /* number      = */ NULL,//handle_number,
            /* string      = */ handle_string,
            /* start map   = */ handle_start_map,
            /* map key     = */ handle_string,
            /* end map     = */ handle_end_map,
            /* start array = */ handle_start_array,
            /* end array   = */ handle_end_array
        };

    yajl_handle handle;
    yajl_status status;
    char * internal_err_str;
    context_t ctx = { NULL, NULL, NULL, 0};

    ctx.errbuf = error_buffer;
    ctx.errbuf_size = error_buffer_size;

    if (error_buffer != NULL)
        memset (error_buffer, 0, error_buffer_size);

    handle = yajl_alloc (&callbacks, NULL, &ctx);
    yajl_config(handle, yajl_allow_comments, 1);

    status = yajl_parse(handle,
                        (unsigned char *) input,
                        strlen (input));
    status = yajl_complete_parse (handle);
    if (status != yajl_status_ok) {
        if (error_buffer != NULL && error_buffer_size > 0) {
               internal_err_str = (char *) yajl_get_error(handle, 1,
                     (const unsigned char *) input,
                     strlen(input));
             snprintf(error_buffer, error_buffer_size, "%s", internal_err_str);
             //YA_FREE(&(handle->alloc), internal_err_str);
        }
        yajl_free (handle);
        return NULL;
    }

    yajl_free (handle);
    return (ctx.root);
}

void yajl_zval_free (zval *v)
{
    if (v == NULL) return;

    zval_ptr_dtor(&v);
}

static void php_yajl_rsrc_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    php_yajl_t *yajl = (php_yajl_t *)rsrc->ptr;

    if (yajl->gen){
      yajl_gen_clear(yajl->gen);
      yajl_gen_free(yajl->gen);
  }

  if (yajl->ctx){
      efree(yajl->ctx);
  }

  if (yajl->handle){
      yajl_free(yajl->handle);
  }

  if (yajl){
      efree(yajl);
  }
}

static zval* php_yajl_new()
{
  TSRMLS_FETCH();

  php_yajl_t *yajl;
    zval *return_value = NULL;
    char errbuf[1024];

    static const yajl_callbacks callbacks =
        {
            /* null        = */ handle_null,
            /* boolean     = */ handle_boolean,
            /* integer     = */ handle_integer,
            /* double      = */ handle_double,
            /* number      = */ NULL,//handle_number,
            /* string      = */ handle_string,
            /* start map   = */ handle_start_map,
            /* map key     = */ handle_string,
            /* end map     = */ handle_end_map,
            /* start array = */ handle_start_array,
            /* end array   = */ handle_end_array
        };

    yajl = emalloc(sizeof(php_yajl_t));

    yajl->gen = yajl_gen_alloc(NULL);
    yajl_gen_config(yajl->gen, yajl_gen_beautify, 1);
    yajl_gen_config(yajl->gen, yajl_gen_validate_utf8, 1);
    yajl_gen_config(yajl->gen, yajl_gen_escape_solidus, 1);

    
  yajl->ctx = ecalloc(1, sizeof(context_t *));

    yajl->handle = yajl_alloc (&callbacks, NULL, yajl->ctx);
    yajl_config(yajl->handle, yajl_allow_comments, 1);

    MAKE_STD_ZVAL(return_value);
    ZEND_REGISTER_RESOURCE(return_value, yajl, le_yajl);

    return return_value;
}

PHP_METHOD(jsonrpc_yajl, generate)
{
  zval *instance;
  php_yajl_t *yajl;

  zval *param;
    const unsigned char * buf;
    size_t len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &param) == FAILURE)
  {
    return ;
  }

  instance = zend_read_static_property(php_jsonrpc_yajl_class_entry, "instance", sizeof("instance")-1 , 1 TSRMLS_CC);
  if (Z_TYPE_P(instance) == IS_RESOURCE){

  }else {
    instance = php_yajl_new();
    zend_update_static_property(php_jsonrpc_yajl_class_entry, "instance", sizeof("instance")-1, instance TSRMLS_CC);
  }

  ZEND_FETCH_RESOURCE(yajl, php_yajl_t *, &instance, -1, "php yajl", le_yajl);

  php_yajl_generate(yajl->gen, param TSRMLS_CC);

  yajl_gen_get_buf(yajl->gen, &buf, &len);

  ZVAL_STRINGL(return_value, buf, len, 1);

    return;

}

PHP_METHOD(jsonrpc_yajl, parse)
{
  zval *instance;
  php_yajl_t *yajl;

  char *str;
  int str_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE)
  {
    return ;
  }

  instance = zend_read_static_property(php_jsonrpc_yajl_class_entry, "instance", sizeof("instance")-1 , 1 TSRMLS_CC);
  if (Z_TYPE_P(instance) == IS_RESOURCE){

  }else {
    instance = php_yajl_new();
    zend_update_static_property(php_jsonrpc_yajl_class_entry, "instance", sizeof("instance")-1, instance TSRMLS_CC);
  }

  ZEND_FETCH_RESOURCE(yajl, php_yajl_t *, &instance, -1, "php yajl", le_yajl);

  yajl->status = yajl_parse(yajl->handle,
                        (unsigned char *) str,
                        (size_t) str_len);

    yajl->status = yajl_complete_parse (yajl->handle);
    
    if (yajl->status != yajl_status_ok) {
        return ;
    }

  RETVAL_ZVAL(yajl->ctx->root, 1, 0);

  yajl_bs_free(yajl->handle->stateStack);
    yajl_bs_init(yajl->handle->stateStack, &(yajl->handle->alloc));
    yajl_bs_push(yajl->handle->stateStack, 0);
    //yajl_bs_set((yajl->handle)->stateStack, 0);

    zval_ptr_dtor(&yajl->ctx->root);
  return ;

}

static const zend_function_entry php_yajl_class_functions[] = {
    //PHP_ME(yajl, __construct, arginfo_php_yajl_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(jsonrpc_yajl, generate,    arginfo_jsonrpc_yajl_generate,  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(jsonrpc_yajl, parse,       arginfo_jsonrpc_yajl_parse,     ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    {NULL, NULL, NULL}
};

void 
jsonrpc_yajl_init(int module_number TSRMLS_DC)
{
  le_yajl = zend_register_list_destructors_ex(php_yajl_rsrc_dtor, NULL, "php jsonrpc yajl", module_number);

  zend_class_entry yajl_class_entry;
  INIT_CLASS_ENTRY(yajl_class_entry, "Jsonrpc_Yajl", php_yajl_class_functions);
  php_jsonrpc_yajl_class_entry = zend_register_internal_class(&yajl_class_entry TSRMLS_CC);

  zend_declare_property_null(php_jsonrpc_yajl_class_entry, "instance", sizeof("instance")-1, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC TSRMLS_CC);
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */