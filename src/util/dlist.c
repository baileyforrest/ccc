/*
 * Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>
 *
 * This file is part of CCC.
 *
 * CCC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CCC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CCC.  If not, see <http://www.gnu.org/licenses/>.
 */
/**
 * Doubly linked list implementation
 */

#include "dlist.h"

#include <assert.h>

extern void *dl_head(dlist_t *dlist);
extern void *dl_tail(dlist_t *dlist);

void dl_init(dlist_t *dlist, size_t head_offset) {
    dlist->head = NULL;
    dlist->tail = NULL;
    dlist->head_offset = head_offset;
}

void dl_destroy(dlist_t *dlist) {
    dl_init(dlist, 0);
}

void dl_append(dlist_t *dlist, dl_link_t *link) {
    if (dlist->head == NULL) {
        assert(dlist->tail == NULL);
        dlist->head = link;
        dlist->tail = link;
        link->next = NULL;
        link->prev = NULL;
        return;
    }

    link->next = NULL;
    link->prev = dlist->tail;
    dlist->tail->next = link;
    dlist->tail = link;
}

void dl_prepend(dlist_t *dlist, dl_link_t *link) {
    if (dlist->head == NULL) {
        assert(dlist->tail == NULL);
        dlist->head = link;
        dlist->tail = link;
        link->next = NULL;
        link->prev = NULL;
        return;
    }

    link->prev = NULL;
    link->next = dlist->head;
    dlist->head->prev = link;
    dlist->head = link;
}

void dl_remove(dlist_t *dlist, dl_link_t *link) {
    if (link->prev != NULL) {
        link->prev->next = link->next;
    } else {
        dlist->head = link->next;
    }
    if (link->next != NULL) {
        link->next->prev = link->prev;
    } else {
        dlist->tail = link->prev;
    }
}
