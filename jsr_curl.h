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

#ifndef JSR_CURL_H
#define JSR_CURL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>

#include <sys/time.h>
#include <unistd.h>

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"

#include "jsr_list.h"
#include "jsr_utils.h"

typedef struct _jsr_curl_t jsr_curl_t;
typedef struct _jsr_curlm_t jsr_curlm_t;
typedef struct _jsr_payload_id jsr_payload_id;
typedef struct _jsr_curl_item_t jsr_curl_item_t;
typedef struct _jsr_curl_sockinfo_t jsr_curl_sockinfo_t;

struct _jsr_curl_t {
      CURL      *curl_handle;
      CURLM     *multi_handle;
      CURLcode  res; 

      FILE *fp;
      struct curl_slist *headers;
};

struct _jsr_curlm_t
{
    CURLM *multi_handle;
    jsr_list_t *list;

    int running_handles;

    int timeout;
};

struct _jsr_payload_id {
  int type;
  
  long long_id;
  char char_id[64];
};

struct _jsr_curl_item_t {
    CURL    *curl_handle;
    char    url[256];
    int     timeout;
    int     verbose;

    struct curl_slist *slist;
    char    post_field[512];
    size_t  post_field_size;
    //FILE    *fp;

    size_t (*write_callback)(char *ptr, size_t size, size_t nmemb, void *ctx);
    size_t (*read_callback)(void *ptr, size_t size, size_t nmemb, void *ctx);
    //char write_data[8192];
    size_t write_length;

    zval *object;
    int   response_id;

    jsr_payload_id payload_id;

};

struct _jsr_curl_sockinfo_t {
  curl_socket_t sockfd;
  CURL *easy;
  int action;
  long timeout;
};

typedef char bool;
#define TRUE 1

#define jsr_curl_setopt curl_easy_set_opt;

#define JSR_CURL_GLOBAL_ALL       CURL_GLOBAL_ALL;
#define JSR_CURL_GLOBAL_SSL       CURL_GLOBAL_SSL;
#define JSR_CURL_GLOBAL_WIN32     CURL_GLOBAL_WIN32;
#define JSR_CURL_GLOBAL_NOTHING   CURL_GLOBAL_NOTHING;
#define JSR_CURL_GLOBAL_DEFAULT   CURL_GLOBAL_DEFAULT

void *jsr_curl_global_new(void);
void *jsr_curl_global_destroy(void);

jsr_curl_t *jsr_curl_new(void);
void *jsr_curl_destroy(jsr_curl_t **self_p);

jsr_curlm_t *jsr_curlm_new(void);
void *jsr_curlm_destroy(jsr_curlm_t **self_p);
int jsr_curlm_list_append(jsr_curlm_t *self, jsr_curl_item_t *item);
int jsr_curlm_list_remove(jsr_curlm_t *self, jsr_curl_item_t *item);
void *jsr_curlm_add_post(jsr_curlm_t *self);

jsr_curl_item_t *jsr_curl_item_new(zval *object, char *url, size_t url_size, char *field, size_t field_size, int response_id, zval *payload_id);
void *jsr_curl_item_destroy(jsr_curl_item_t **self_p);
void *jsr_curl_item_setopt(jsr_curl_item_t *self);

static void _jsr_curl_dump(const char *text, FILE *stream, unsigned char *ptr, size_t size, bool nohex);
static int _jsr_curl_trace(CURL *handle, curl_infotype type, unsigned char *data, size_t size, void *userp);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */