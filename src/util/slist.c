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

#include "util.h"

// Extern declarations for the inline functions
extern inline void *sl_head(slist_t *slist);
extern inline void *sl_tail(slist_t *slist);

void sl_init(slist_t *slist, size_t head_offset) {
    slist->head = NULL;
    slist->tail = NULL;
    slist->head_offset = head_offset;
}

void sl_destroy(slist_t *slist) {
    slist->head = NULL;
    slist->tail = NULL;
    slist->head_offset = 0;
}

void sl_append(slist_t *slist, sl_link_t *link) {
    if (slist->tail == NULL) {
        slist->head = link;
        slist->tail = link;
        link->next = NULL;
        return;
    }

    slist->tail->next = link;
    link->next = NULL;
    slist->tail = link;
}

void sl_prepend(slist_t *slist, sl_link_t *link) {
    if (slist->tail == NULL) {
        slist->head = link;
        slist->tail = link;
        link->next = NULL;
        return;
    }

    link->next = slist->head;
    slist->head = link;
}

void *sl_pop_front(slist_t *slist) {
    sl_link_t *head = slist->head;

    // Removing head is O(1)
    if (!sl_remove(slist, head)) {
        return NULL;
    }

    return GET_ELEM(slist, head);
}

bool sl_remove(slist_t *slist, sl_link_t *link) {
    if (link == NULL) {
        return false;
    }

    for (sl_link_t **cur = &slist->head; *cur != NULL; cur = &(*cur)->next) {
        if (*cur == link) {
            *cur = link->next;
            if (slist->head == NULL) {
                slist->tail = NULL;
            }
            return true;
        }
    }

    return false;
}

void sl_concat_front(slist_t *list1, slist_t *list2) {
    list2->tail->next = list1->head;
    list1->head = list2->head;

    list2->head = NULL;
    list2->tail = NULL;
}
