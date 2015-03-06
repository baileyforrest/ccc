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
 * Singly linked list interface
 */
#ifndef _SLIST_H_
#define _SLIST_H_

#include <stdbool.h>
#include <stdlib.h>

#include "util/status.h"

/** Pointer to element represented by head */
#define GET_ELEM(list, head) \
    ((void *)(head) - (list)->head_offset)

/**
 * Single Linked Link list node.
 *
 * This should be a member of the values stored in the list. This is
 * preferred over the link pointing to the object stored to avoid an extra
 * pointer indirection.
 */
typedef struct sl_link_t {
    struct sl_link_t *next; /**< The next element */
} sl_link_t;

/**
 * Singly linked list wrapper structure
 */
typedef struct slist_t {
    sl_link_t *head;    /**< Head of the list */
    sl_link_t *tail;    /**< Tail of the list for fast append */
    size_t head_offset; /**< Offset of head into element structure */
} slist_t;

/**
 * Initalizes a singly linked list head
 *
 * @param slist List head to initalize
 * @param head_offset offset of head into list element structs
 */
status_t sl_init(slist_t *slist, size_t head_offset);


/**
 * Does not free slist. Destroys a singly linked list. Optionally frees elements
 *
 * @param slist List head to destroy
 * @param do_free if SL_FREE, frees the elements
 */
void sl_destroy(slist_t *slist, bool do_free);

/**
 * Return slist head
 *
 * @param slist List to get head of
 */
inline void *sl_head(slist_t *slist) {
    return slist->head == NULL ? NULL : GET_ELEM(slist, slist->head);
}

/**
 * Return slist tail
 *
 * @param slist List to get tail of
 */
inline void *sl_tail(slist_t *slist) {
    return slist->head == NULL ? NULL : GET_ELEM(slist, slist->tail);
}

/**
 * Appends an element to the list
 *
 * @param slist List to append to
 * @param link Link of element to append
 */
void sl_append(slist_t *slist, sl_link_t *link);

/**
 * Prepends an element to the list
 *
 * @param slist List to append to
 * @param link Link of element to append
 */
void sl_prepend(slist_t *slist, sl_link_t *link);

/**
 * Pops of the front element
 *
 * @param slist List to pop off of
 * @return The element to return. NULL if none
 */
void *sl_pop_front(slist_t *slist);

/**
 * Removes an element from the singly linked list
 *
 * Warning: This is O(n) because singly linked list.
 *
 * @param slist List to remove element from
 * @param link The link to remove.
 * @return true if removed, false otherwise
 */
bool sl_remove(slist_t *slist, sl_link_t *link);

/**
 * Calls callback function on each of the elements
 *
 * @param slist List to perform operations on
 * @param func The function to call on each element
 */
void sl_foreach(slist_t *slist, void (*func)(void *));

/**
 * Run code on each element
 *
 * The elements themselves must be accessed with GET_ELEM
 *
 * @param CURRENT_ELEMENT pp_link_t pointer to store current element
 * @param SL_HEAD Head of the list
 */
#define SL_FOREACH(CURRENT_ELEM, SL_HEAD) \
    for (CURRENT_ELEM = (SL_HEAD)->head;                                       \
         CURRENT_ELEM != NULL;                                                 \
         CURRENT_ELEM = CURRENT_ELEM->next)


#endif /* _SLIST_H_ */
