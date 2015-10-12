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

typedef struct _jsr_curl_t jsr_curl_t;

struct _jsr_curl_t {
      CURL      *curl_handle;
      CURLM     *multi_handle;
      CURLcode  res; 

      FILE *fp;
      struct curl_slist *headers; 
};

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

void *jsr_curl_post();

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */