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

#include "jsr_curl.h"

jsr_curl_t *
jsr_curl_new()
{
    jsr_curl_t *jsr_curl = (jsr_curl_t *)malloc(sizeof(jsr_curl_t));
    if (!jsr_curl)
        return NULL;

    jsr_curl->curl_handle = curl_easy_init();

    return jsr_curl;
}

void *
jsr_curl_destroy(jsr_curl_t **self_p)
{
    if (*self_p){
        jsr_curl_t *self = *self_p;
        curl_easy_clean(&self->curl_handle);
        free(self);
        *self_p = NULL;
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */