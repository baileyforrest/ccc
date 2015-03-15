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
 * Hashtable Interface
 *
 * Some interface ideas from here: http://lwn.net/Articles/612100/
 */

#ifndef _HTABLE_H_
#define _HTABLE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "util/status.h"
#include "util/slist.h"

/** Pointer to element represented by head */
#define GET_HT_ELEM(ht, head) \
    ((void *)(head) - (ht)->params.head_offset)

/** Pointer to key of element represented by head */
#define GET_HT_KEY(ht, head) \
    (GET_HT_ELEM((ht), (head)) + (ht)->params.key_offset)

typedef uint32_t (*ht_hashfunc)(const void *key);
typedef bool (*ht_cmpfunc)(const void *key1, const void *key2);

/**
 * Paramaters for initializing a hashtable
 * TODO: change to ht_params_t
 */
typedef struct ht_params {
    size_t nelems;        /**< Hint for number of elements, 0 for unused */
    size_t key_offset;    /**< Offset of the key in the struct */
    size_t head_offset;   /**< Offset of the sl_link_t into the struct */
    ht_hashfunc hashfunc; /**< Hash function to use */
    ht_cmpfunc cmpfunc;   /**< Comparison function to use */
} ht_params;

/**
 * The hash table structure. Basic chained buckets.
 */
typedef struct htable_t {
    sl_link_t **buckets; /**< The bucket array */
    size_t nbuckets;     /**< Number of buckets */
    ht_params params;    /**< Paramaters used to initialize the hashtable */
} htable_t;

/**
 * Initialize given hashtable with params
 *
 * @param ht The hashtable to initialize
 * @param params paramaters
 * @return CCC_OK on success, relevant error code on failure
 */
status_t ht_init(htable_t *ht, const ht_params *params);

/**
 * Destroys an hash table
 * Does not free ht itself. Does not free elements, only backing store
 *
 * @param ht The hashtable to destroy
 * @param do_free DOFREE (true) if elements should be freed. NOFREE otherwise
 */
void ht_destroy(htable_t *ht);

/**
 * Destroys an hash table. Does not free ht itself.
 *
 * Calls a destructor on each of the elements, which may free the the elements
 *
 * @param ht The hashtable to destroy
 * @param do_free DOFREE (true) if elements should be freed. NOFREE otherwise
 */
#define HT_DESTROY_FUNC(ht, func)                       \
    do {                                                \
        for (size_t i = 0; i < (ht)->nbuckets; ++i) {   \
            sl_link_t *cur = (ht)->buckets[i];          \
            sl_link_t *next = NULL;                     \
                                                        \
            while (cur != NULL) {                       \
                next = cur->next;                       \
                (func)(GET_HT_ELEM((ht), cur));         \
                cur = next;                             \
            }                                           \
        }                                               \
        ht_destroy(ht);                                 \
    } while (0)

/**
 * Insert element with specified link into hashtable. Frees the old element if
 * it exists.
 *
 * @param ht The hashtable to insert into
 * @param elem The element to insert
 * @return CCC_OK on success, error code on failure. Returns CCC_DUPLICATE if
 * key with same value already exists
 */
status_t ht_insert(htable_t *ht, sl_link_t *elem);

/**
 * Remove specified element from hashtable. Frees the element
 *
 * @param ht The hashtable to remove from
 * @param key Key of element to remove
 * @return A pointer to the element removed, or NULL if it doesn't exist
 */
void *ht_remove(htable_t *ht, const void *key);

/**
 * Lookup specified element in hashtable
 *
 * @param ht The hashtable to retrieve from
 * @param key Key of element to retrieve
 * @return A pointer to the element in the hashtable. NULL otherwises
 */
void *ht_lookup(const htable_t *ht, const void *key);

#define HT_FOREACH(link, ht)                                            \
    for (size_t i = 0; i < (ht)->nbuckets; ++i)                         \
        for (link = (ht)->buckets[i]; link != NULL; link = link->next)  \

#endif /* _HTABLE_H_ */
