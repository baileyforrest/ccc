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

typedef uint32_t (*ht_hashfunc)(const void *key, size_t len);
typedef bool (*ht_cmpfunc)(const void *key1, const void *key2, size_t len);

/**
 * Single Linked Link hash table elements.
 *
 * This should be a member of the values stored in the hashtable. This is
 * preferred over the link pointing to the object stored to avoid an extra
 * pointer indirection.
 */
typedef struct ht_link_t {
    struct ht_link_t *next; /**< The next element */
} ht_link_t;

/**
 * Paramaters for initializing a hashtable
 */
typedef struct ht_params {
    size_t nelems;        /**< Hint for number of elements, 0 for unused */
    size_t key_len;       /**< Length of a key. 0 for unused */
    size_t key_offset;    /**< Offset of the key in the struct */
    size_t head_offset;   /**< Offset of the ht_link_t into the struct */
    ht_hashfunc hashfunc; /**< Hash function to use */
    ht_cmpfunc cmpfunc;   /**< Comparison function to use */
} ht_params;

/**
 * The hash table structure. Basic chained buckets.
 */
typedef struct htable_t {
    ht_link_t **buckets; /**< The bucket array */
    size_t nbuckets;
    ht_params params; /**< Paramaters used to initialize the hashtable */
} htable_t;

/**
 * Initialize given hashtable with params
 *
 * @param ht The hashtable to initialize
 * @param params paramaters
 * @return CCC_OK on success, relevant error code on failure
 */
status_t ht_init(htable_t *ht, ht_params *params);

/**
 * Does not free ht itself. Frees all of the contained elements and bucketlist.
 *
 * @param ht The hashtable to destroy
 */
void ht_destroy(htable_t *ht);

/**
 * Insert element with specified link into hashtable. Frees the old element if
 * it exists.
 *
 * @param ht The hashtable to insert into
 * @param elem The element to insert
 * @return CCC_OK on success, error code on failure
 */
status_t ht_insert(htable_t *ht, ht_link_t *elem);

/**
 * Remove specified element from hashtable. Frees the element
 *
 * @param ht The hashtable to remove from
 * @param key Key of element to remove
 * @return true if removed, false otherwise
 */
bool ht_remove(htable_t *ht, const void *key);

/**
 * Lookup specified element in hashtable
 *
 * @param ht The hashtable to retrieve from
 * @param key Key of element to retrieve
 * @return A pointer to the element in the hashtable. NULL otherwises
 */
void *ht_lookup(const htable_t *ht, const void *key);

#endif /* _HTABLE_H_ */
