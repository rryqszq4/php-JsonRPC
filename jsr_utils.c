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

#include "jsr_utils.h"


static char* jsr_return_zval_type(zend_uchar type)
{
  switch (type) {
    case IS_NULL:
      return "IS_NULL";
      break;
    case IS_BOOL:
      return "IS_BOOL";
      break;
    case IS_LONG:
      return "IS_LONG";
      break;
    case IS_DOUBLE:
      return "IS_DOUBLE";
      break;
    case IS_STRING:
      return "IS_STRING";
      break;
    case IS_ARRAY:
      return "IS_ARRAY";
      break;
    case IS_OBJECT:
      return "IS_OBJECT";
      break;
    case IS_RESOURCE:
      return "IS_RESOURCE";
      break;
    default :
      return "unknown";
  }
}

void jsr_dump_zval(zval *data)
{
  php_printf("zval<%p> {\n", data);
  php_printf("  refcount__gc -> %d\n", data->refcount__gc);
  php_printf("  is_ref__gc -> %d\n", data->is_ref__gc);
  php_printf("  type -> %s\n", jsr_return_zval_type(data->type));
  php_printf("  zand_value<%p> {\n", data->value);

  php_printf("    lval -> %d\n", data->value.lval);
  php_printf("    dval -> %e\n", data->value.dval);
  if (Z_TYPE_P(data) == IS_STRING){
    php_printf("    str -> %s\n", Z_STRVAL_P(data));
  }
  php_printf("    *ht<%p> {\n", data->value.ht);
  php_printf("    }\n");
  php_printf("    obj<%p> {\n", data->value.obj);
  php_printf("    }\n");
  php_printf("  }\n");

  php_printf("}\n");
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */