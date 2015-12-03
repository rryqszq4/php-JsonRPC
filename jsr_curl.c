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
        curl_multi_cleanup(self->multi_handle);
        curl_easy_cleanup(self->curl_handle);
        free(self);
        *self_p = NULL;
    }
}

jsr_curlm_t *
jsr_curlm_new(void)
{
    jsr_curlm_t *jsr_curlm = (jsr_curlm_t *)malloc(sizeof(jsr_curlm_t));
    if (!jsr_curlm)
        return NULL;

    jsr_curlm->multi_handle = curl_multi_init();
    jsr_curlm->list = jsr_list_new();
    jsr_curlm->running_handles = 1;
    jsr_curlm->timeout = 1;

    return jsr_curlm;

}

void *
jsr_curlm_destroy(jsr_curlm_t **self_p)
{
    jsr_node_t *node;
    jsr_curl_item_t *item;

    if (*self_p){
        jsr_curlm_t *self = *self_p;
        /*for (node = jsr_list_first(self->list) ; node != NULL; node = jsr_list_next(self->list))
        {
            item = jsr_list_item(self->list);
            jsr_curl_item_destroy(&item);
        }*/
        curl_multi_cleanup(self->multi_handle);
        jsr_list_free(&self->list);
        free(self);
        *self_p = NULL;
    }
}

int 
jsr_curlm_list_append(jsr_curlm_t *self, jsr_curl_item_t *item)
{
    if (!self || !item)
        return -1;

    if (jsr_list_append(self->list, item))
        return -1;

    return 0;
}

int
jsr_curlm_list_push(jsr_curlm_t *self, jsr_curl_item_t *item)
{
    if (!self || !item)
        return -1;

    if (jsr_list_push(self->list, item))
        return -1;

    return 0;
}

jsr_curl_item_t *
jsr_curlm_list_pop(jsr_curlm_t *self)
{
  if (!self)
    return NULL;

  return jsr_list_pop(self->list);

}

jsr_curl_item_t *
jsr_curl_item_new(zval *object, char *url, size_t url_size, char *field, size_t field_size, int response_id, zval *payload_id)
{
    jsr_curl_item_t *item = (jsr_curl_item_t *)malloc(sizeof(jsr_curl_item_t));
    if (!item)
        return NULL;

    item->curl_handle = curl_easy_init();

    memset(item->url, 0, 256);
    strcpy(item->url, url);

    memset(item->post_field, 0, 512);
    strcpy(item->post_field, field);
    
    item->post_field_size = field_size;

    item->timeout = 5;

    //memset(item->write_data, 0, 8192);

    item->object = object;
    //item->fp = fopen("curl_data.txt", "ab+");

    item->verbose = 0;

    item->slist = NULL;
    item->slist = curl_slist_append(item->slist, "Content-Type: application/json"); 
    item->slist = curl_slist_append(item->slist, "Accept: application/json"); 

    item->response_id = response_id;

    if (Z_TYPE_P(payload_id) == IS_LONG){
      item->payload_id.type = IS_LONG;
      item->payload_id.long_id = Z_LVAL_P(payload_id);
    }else if(Z_TYPE_P(payload_id) == IS_STRING){
      item->payload_id.type = IS_STRING;
      memset(item->payload_id.char_id, 0, 64);
      strcpy(item->payload_id.char_id, Z_STRVAL_P(payload_id));
    }else {
      item->payload_id.type = IS_NULL;
    }
    
    return item;
}

void *
jsr_curl_item_destroy(jsr_curl_item_t **self_p)
{
    if (*self_p)
    {
        jsr_curl_item_t *self = *self_p;
        //if (self->curl_handle)
          //curl_easy_cleanup(self->curl_handle);
        //fclose(self->fp);
        free(self);
        *self_p = NULL;
    }
}

void *
jsr_curl_item_setopt(jsr_curl_item_t *self)
{
    curl_easy_setopt(self->curl_handle, CURLOPT_URL, self->url);
    curl_easy_setopt(self->curl_handle, CURLOPT_CONNECTTIMEOUT, self->timeout);
    curl_easy_setopt(self->curl_handle, CURLOPT_USERAGENT, "JSON-RPC PHP Client");
    curl_easy_setopt(self->curl_handle, CURLOPT_HTTPHEADER, self->slist);
    curl_easy_setopt(self->curl_handle, CURLOPT_FOLLOWLOCATION, 0);
    curl_easy_setopt(self->curl_handle, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(self->curl_handle, CURLOPT_SSL_VERIFYPEER, 1);

    curl_easy_setopt(self->curl_handle, CURLOPT_NOPROGRESS,        1);

    curl_easy_setopt(self->curl_handle, CURLOPT_PRIVATE, self);

    //curl_easy_setopt(self->curl_handle, CURLOPT_POSTFIELDS, "{\"jsonrpc\":\"2.0\",\"method\":\"addition\",\"id\":1793433748,\"params\":[3,5]}");
    curl_easy_setopt(self->curl_handle, CURLOPT_POSTFIELDS, self->post_field);
    curl_easy_setopt(self->curl_handle, CURLOPT_POSTFIELDSIZE, self->post_field_size);

    curl_easy_setopt(self->curl_handle, CURLOPT_WRITEFUNCTION, self->write_callback);
    curl_easy_setopt(self->curl_handle, CURLOPT_WRITEDATA, self);

    //curl_easy_setopt(self->curl_handle, CURLOPT_READFUNCTION, self->read_callback);
    //curl_easy_setopt(self->curl_handle, CURLOPT_READDATA, self);

    //curl_easy_setopt(self->curl_handle, CURLOPT_DEBUGFUNCTION, _jsr_curl_trace);
    curl_easy_setopt(self->curl_handle, CURLOPT_VERBOSE, self->verbose);

    //curl_easy_setopt(self->curl_handle, CURLOPT_WRITEDATA, self->fp);

}

void *
jsr_curlm_add_post(jsr_curlm_t *self)
{
    jsr_curl_item_t *item;
    int still_running;

    size_t size;

    size = jsr_list_size(self->list);

    while (size > 0){
        item = jsr_list_next(self->list);
        //php_printf("%d, %d\n", self->multi_handle, item->curl_handle);
        curl_multi_add_handle(self->multi_handle, item->curl_handle);
        size--;
    }

    self->list->cursor = NULL;

    //curl_multi_perform(self->multi_handle, &still_running);
    
}

static void 
_jsr_curl_dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size,
          bool nohex)
{
  size_t i;
  size_t c;
 
  unsigned int width=0x10;
 
  if(nohex)
    /* without the hex output, we can fit more on screen */ 
    width = 0x40;
 
  fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
          text, (long)size, (long)size);
 
  for(i=0; i<size; i+= width) {
 
    fprintf(stream, "%4.4lx: ", (long)i);
 
    if(!nohex) {
      /* hex not disabled, show it */ 
      for(c = 0; c < width; c++)
        if(i+c < size)
          fprintf(stream, "%02x ", ptr[i+c]);
        else
          fputs("   ", stream);
    }
 
    for(c = 0; (c < width) && (i+c < size); c++) {
      /* check for 0D0A; if found, skip past and start a new line of output */ 
      if (nohex && (i+c+1 < size) && ptr[i+c]==0x0D && ptr[i+c+1]==0x0A) {
        i+=(c+2-width);
        break;
      }
      fprintf(stream, "%c",
              (ptr[i+c]>=0x20) && (ptr[i+c]<0x80)?ptr[i+c]:'.');
      /* check again for 0D0A, to avoid an extra \n if it's at width */ 
      if (nohex && (i+c+2 < size) && ptr[i+c+1]==0x0D && ptr[i+c+2]==0x0A) {
        i+=(c+3-width);
        break;
      }
    }
    fputc('\n', stream); /* newline */ 
  }
  fflush(stream);
}
 
static int 
_jsr_curl_trace(CURL *handle, curl_infotype type,
             unsigned char *data, size_t size,
             void *userp)
{
  const char *text;
 
  (void)userp;
  (void)handle; /* prevent compiler warning */ 
 
  switch (type) {
  case CURLINFO_TEXT:
    fprintf(stderr, "== Info: %s", data);
  default: /* in case a new one is introduced to shock us */ 
    return 0;
 
  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  }
 
  _jsr_curl_dump(text, stderr, data, size, TRUE);
  return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */