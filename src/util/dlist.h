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
 * Doubly linked list interface
 */

#ifndef _DLIST_H_
#define _DLIST_H_

#include <stddef.h>

typedef struct dl_link_t dl_link_t;
struct dl_link_t {
    dl_link_t *next;
    dl_link_t *prev;
};

typedef struct dlist_t {
    dl_link_t *head;    /**< Head of the list */
    dl_link_t *tail;    /**< Tail of the list for fast append */
    size_t head_offset; /**< Offset of head into element structure */
} dlist_t;

/**
 * Pointer to element represented by head
 *
 * @param list List to get element from
 * @param head The sl_link_t * representing the element
 */
#define DL_GET_ELEM(list, head) ((void *)(head) - (list)->head_offset)

/**
 * Initalizes a doubly linked list head
 *
 * @param dlist List head to initalize
 * @param head_offset offset of head into list element structs
 */
void dl_init(dlist_t *dlist, size_t head_offset);


/**
 * Destroys a doubly linked list. Does not free dlist or elements
 *
 * @param dlist List head to destroy
 */
void dl_destroy(dlist_t *dlist);

/**
 * Destroys a doubly linked list. Calls func on each element, which may free
 * the element
 *
 * @param dlist List head to destroy
 * @param func Function to call on each element
 */
#define DL_DESTROY_FUNC(dlist, func)                                \
    do {                                                            \
        for (dl_link_t *cur = (dlist)->head, *next; cur != NULL;    \
             cur = next) {                                          \
            next = cur->next;                                       \
            (func)(GET_ELEM((dlist), cur));                         \
        }                                                           \
        dl_destroy(dlist);                                          \
    } while (0)

/**
 * Return dlist head
 *
 * @param dlist List to get head of
 */
inline void *dl_head(dlist_t *dlist) {
    return dlist->head == NULL ? NULL : DL_GET_ELEM(dlist, dlist->head);
}

/**
 * Return dlist tail
 *
 * @param dlist List to get tail of
 */
inline void *dl_tail(dlist_t *dlist) {
    return dlist->head == NULL ? NULL : DL_GET_ELEM(dlist, dlist->tail);
}

/**
 * Appends an element to the list
 *
 * @param dlist List to append to
 * @param link Link of element to append
 */
void dl_append(dlist_t *dlist, dl_link_t *link);

/**
 * Prepends an element to the list
 *
 * @param dlist List to append to
 * @param link Link of element to append
 */
void dl_prepend(dlist_t *dlist, dl_link_t *link);

/**
 * Removes an element from the doubly linked list
 *
 * @param dlist List to remove element from
 * @param link The link to remove.
 */
void dl_remove(dlist_t *dlist, dl_link_t *link);

/**
 * Run code on each element
 *
 * The elements themselves must be accessed with GET_ELEM
 *
 * @param CURRENT_ELEMENT pp_link_t pointer to store current element
 * @param DL_HEAD Head of the list
 */
#define DL_FOREACH(cur_elem, dl_head)                               \
    for (dl_link_t *cur_elem = (dl_head)->head;                     \
         cur_elem != NULL;                                          \
         cur_elem = cur_elem->next)

#endif /* _DLIST_H_ */
