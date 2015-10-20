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

void *
jsr_curl_global_new()
{
    curl_global_init(JSR_CURL_GLOBAL_DEFAULT);
}

void *
jsr_curl_global_destroy()
{
    curl_global_cleanup();
}

jsr_curl_t *
jsr_curl_new()
{
    jsr_curl_t *jsr_curl = (jsr_curl_t *)malloc(sizeof(jsr_curl_t));
    if (!jsr_curl)
        return NULL;

    jsr_curl->curl_handle = curl_easy_init();
    jsr_curl->multi_handle = curl_multi_init();

    return jsr_curl;
}

void *
jsr_curl_destroy(jsr_curl_t **self_p)
{
    if (*self_p){
        jsr_curl_t *self = *self_p;
        curl_multi_cleanup(&self->multi_handle);
        curl_easy_cleanup(&self->curl_handle);
        free(self);
        *self_p = NULL;
    }
}

jsr_curlm_t *
jsr_curlm_new(void)
{
    jsr_curlm_t *jsr_curlm = (jsr_curlm_t *)malloc(sizeof(jsr_curlm_t));
    if (!curlm)
        return NULL;

    jsr_curlm->multi_handle = curl_multi_init();
    jsr_curlm->list = jsr_list_new();

    return jsr_curlm;

}

void *
jsr_curlm_destroy(jsr_curlm_t **self_p)
{
    if (*self_p){
        jsr_curlm_t *self = *self_p;
        curl_multi_cleanup(&self->multi_handle);
        jsr_list_destroy(&self->list);
        free(self);
        *self_p = NULL;
    }
}

int 
jsr_curlm_list_append(jsr_curlm_t *self, jsr_curl_item_t *item)
{
    if (!self || !item)
        return -1;

    if (jsr_list_append(self->list, item)
        return -1;

    return 0;
}

jsr_curl_item_t *
jsr_curl_item_new(char *url, char *field, size_t field_size)
{
    jsr_curl_item_t *item = (jsr_curl_item_t *)malloc(sizeof(jsr_curl_item_t));
    if (!item)
        return NULL;

    item->curl_handle = curl_easy_init();
    item->url = url;
    item->timeout = 5;
    item->post_field = field;
    item->post_field_size = field_size;

    return item;
}

void *
jsr_curl_item_destroy(jsr_curl_item_t **self_p)
{

}

void *
jsr_curl_item_setopt(jsr_curl_item_t *self)
{
    curl_easy_setopt(ch->cp, CURLOPT_CONNECTTIMEOUT, self->timeout);
    curl_easy_setopt(ch->cp, CURLOPT_USERAGENT, "JSON-RPC PHP Client");
    curl_easy_setopt(ch->cp, CURLOPT_HTTPHEADER, self->slist);
    curl_easy_setopt(ch->cp, CURLOPT_FOLLOWLOCATION, 0);
    curl_easy_setopt(ch->cp, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(ch->cp, CURLOPT_SSL_VERIFYPEER, 1);

    curl_easy_setopt(ch->cp, CURLOPT_POSTFIELDS, self->post_field);
    curl_easy_setopt(ch->cp, CURLOPT_POSTFIELDSIZE, self->post_field_size);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */