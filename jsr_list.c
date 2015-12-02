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

#include "jsr_list.h"

jsr_list_t *
jsr_list_new(void)
{
    jsr_list_t *self = (jsr_list_t *)malloc(sizeof(jsr_list_t));
    self->tail = NULL;
    self->size = 0;
    return self;
}

void 
jsr_list_free(jsr_list_t **self_p)
{
    if (*self_p){
        jsr_list_t *self = *self_p;
        free(self);
        *self_p = NULL;
    }
}

void 
jsr_list_destroy(jsr_list_t **self_p)
{
    if (*self_p){
        jsr_list_t *self = *self_p;
        jsr_list_purge(self);
        free(self);
        *self_p = NULL;
    }
}

void *
jsr_list_first(jsr_list_t *self)
{
    self->cursor = self->head;
    if (self->cursor)
        return self->cursor->item;
    else
        return NULL;
}

void *
jsr_list_next(jsr_list_t *self)
{
    if (self->cursor)
        self->cursor = self->cursor->next;
    else
        self->cursor = self->head;

    if (self->cursor)
        return self->cursor->item;
    else
        return NULL;
}

void *
jsr_list_last(jsr_list_t *self)
{
    self->cursor = self->tail;
    if (self->cursor)
        return self->cursor->item;
    else
        return NULL;
}

void *
jsr_list_head(jsr_list_t *self)
{
    return self->head ? self->head->item : NULL;
}

void *
jsr_list_tail(jsr_list_t *self)
{
    return self->tail ? self->tail->item : NULL;
}

void *
jsr_list_item(jsr_list_t *self)
{
    if (self->cursor)
        return self->cursor->item;
    else
        return NULL;
}

int 
jsr_list_append(jsr_list_t *self, void *item)
{
    if (!item)
        return -1;

    jsr_node_t *node;
    node = (jsr_node_t *)malloc(sizeof(jsr_node_t));

    if (!node)
        return -1;

    //if (self->autofree)
    //    item = strdup((char *)item);

    node->item = item;
    if (self->tail)
        self->tail->next = node;
    else
        self->head = node;

    self->tail = node;
    node->next = NULL;

    self->size++;
    self->cursor = NULL;

    return 0;
}

int 
jsr_list_push(jsr_list_t *self, void *item)
{
    jsr_node_t *node;
    node = (jsr_node_t *)malloc(sizeof(jsr_node_t));

    if (!node)
        return -1;

    //if (self->autofree)
    //    item = strdup((char *) item);

    node->item = item;
    node->next = self->head;
    self->head = node;

    if (self->tail == NULL)
        self->tail = node;

    self->size++;
    self->cursor = NULL;

    return 0;
}

void *
jsr_list_pop(jsr_list_t *self)
{
    jsr_node_t *node = self->head;
    void *item = NULL;
    if (node){
        item = node->item;
        self->head = node->next;
        if (self->tail == node)
            self->tail = NULL;
        free(node);
        self->size--;
    }
    self->cursor = NULL;
    return item;
}

void jsr_list_remove(jsr_list_t *self, void *item)
{
    jsr_node_t *node, *prev = NULL;

    //  First off, we need to find the list node
    for (node = self->head; node != NULL; node = node->next) {
        if (self->compare_fn) {
            if ((*self->compare_fn)(node->item, item) == 0)
               break;
        }
        else {
            if (node->item == item)
                break;
        }
        prev = node;
    }
    if (node) {
        if (prev)
            prev->next = node->next;
        else
            self->head = node->next;

        if (node->next == NULL)
            self->tail = prev;
        if (self->cursor == node)
            self->cursor = prev;

        //if (node->free_fn)
        //    (node->free_fn)(node->item);

        free (node);
        self->size--;
    }
}

int 
jsr_list_exists(jsr_list_t *self, void *item)
{
    jsr_node_t *node = self->head;

    while (node){
        if (self->compare_fn){
            if ((*self->compare_fn)(node->item, item) == 0)
                return 1;
        }
        else{
            if (node->item = item){
                return 1;
            }
        }
        node = node->next;
    }

    return 0;
}

//jsr_list_t *jsr_list_dup(jsr_list_t *self);

void 
jsr_list_purge(jsr_list_t *self)
{
    jsr_node_t *node = self->head;
    while (node){
        jsr_node_t *next = node->next;
        if (self->autofree){
            free(node->item);
        }
        else {
            if (node->free_fn)
                (node->free_fn)(node->item);
        }

        free(node);
        node = next;
    }
    self->head = NULL;
    self->tail = NULL;
    self->cursor = NULL;
    self->size = 0;
}

size_t 
jsr_list_size(jsr_list_t *self)
{
    return self->size;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */