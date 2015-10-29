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
#ifndef JSR_EPOLL_H
#define JSR_EPOLL_H

#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#define JSR_MAX_EVENTS 32

typedef struct _jsr_epoll_t jsr_epoll_t;

struct _jsr_epoll_t
{
    int epoll_fd;
    struct epoll_event events [JSR_MAX_EVENTS];

    int loop_total;
};

int jsr_setnonblocking(int fd);

jsr_epoll_t *jsr_epoll_init();

int jsr_epoll_add_fd(jsr_epoll_t *self, int fd);

int jsr_epoll_del_fd(jsr_epoll_t *self, int fd);

int jsr_epoll_set_in(jsr_epoll_t *self, int fd);

int jsr_epoll_set_out(jsr_epoll_t *self, int fd);

int jsr_epoll_loop(jsr_epoll_t *self, int timeout);

void jsr_epoll_destroy(jsr_epoll_t **self_p);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */