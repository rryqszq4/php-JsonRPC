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

    return jsr_curlm;

}

void *
jsr_curlm_destroy(jsr_curlm_t **self_p)
{
    jsr_node_t *node;
    jsr_curl_item_t *item;

    if (*self_p){
        jsr_curlm_t *self = *self_p;
        for (node = jsr_list_first(self->list) ; node != NULL; node = jsr_list_next(self->list))
        {
            item = jsr_list_item(self->list);
            jsr_curl_item_destroy(&item);
        }
        curl_multi_cleanup(self->multi_handle);
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

    if (jsr_list_append(self->list, item))
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
    item->fp = fopen("curl_data.txt", "ab+");

    return item;
}

void *
jsr_curl_item_destroy(jsr_curl_item_t **self_p)
{
    if (*self_p)
    {
        jsr_curl_item_t *self = *self_p;
        curl_easy_cleanup(self->curl_handle);
        fclose(self->fp);
        free(self);
        *self_p = NULL;
    }
}

static
void dump(const char *text,
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
 
static
int my_trace(CURL *handle, curl_infotype type,
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
 
  dump(text, stderr, data, size, TRUE);
  return 0;
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

    curl_easy_setopt(self->curl_handle, CURLOPT_POSTFIELDS, self->post_field);
    curl_easy_setopt(self->curl_handle, CURLOPT_POSTFIELDSIZE, self->post_field_size);

    //curl_easy_setopt(self->curl_handle, CURLOPT_DEBUGFUNCTION, my_trace);
    //curl_easy_setopt(self->curl_handle, CURLOPT_VERBOSE, 1L);

    //curl_easy_setopt(self->curl_handle, CURLOPT_WRITEDATA, self->fp);
}

void *
jsr_curlm_post(jsr_curlm_t *self)
{
    jsr_node_t *node;
    jsr_curl_item_t *item;
    int still_running;

    for (node = jsr_list_first(self->list) ; node != NULL; node = jsr_list_next(self->list))
    {

        item = jsr_list_item(self->list);
        // multi stack
        curl_multi_add_handle(self->multi_handle, item->curl_handle);
    }

    //curl_multi_perform(self->multi_handle, &still_running);
    
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */