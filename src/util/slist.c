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
 * Singly linked list implementation
 */

#include "slist.h"

#include <stdlib.h>
#include <string.h>

/** Pointer to element represented by head */
#define GET_ELEM(list, head) \
    ((void *)(head) - list->head_offset)

status_t sl_init(slist_t *slist, size_t head_offset) {
    slist->head = NULL;
    slist->tail = NULL;
    slist->head_offset = head_offset;

    return CCC_OK;
}

void sl_destroy(slist_t *slist, bool do_free) {
    if (!do_free) {
        memset(slist, 0, sizeof(*slist));
        return;
    }

    for (sl_link_t *cur = slist->head, *next; cur != NULL; cur = next) {
        next = cur->next;
        free(GET_ELEM(slist, cur));
    }
    memset(slist, 0, sizeof(*slist));
}

void sl_append(slist_t *slist, sl_link_t *link) {
    slist->tail->next = link;
    link->next = NULL;
    slist->tail = link;
}

void sl_remove(slist_t *slist, sl_link_t *link) {
    for (sl_link_t **cur = &slist->head; *cur != NULL; *cur = (*cur)->next) {
        if (*cur == link) {
            *cur = link->next;
            return;
        }
    }
}

void sl_foreach(slist_t *slist, void (*func)(void *)) {
    for (sl_link_t *cur = slist->head; cur != NULL; cur = cur->next) {
        func(GET_ELEM(slist, cur));
    }
}
