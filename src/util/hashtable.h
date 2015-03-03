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

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef uint32_t (*ht_hashfunc)(const void *key, size_t len);
typedef bool (*ht_cmpfunc)(const void *key1, size_t len1, const void *key2,
                           size_t len2);

typedef struct ht_link {
    struct ht_link *next;
} ht_link;

typedef struct ht_params {
    size_t nelem_hint;
    size_t key_len;
    size_t key_offset;
    size_t head_offset;
    ht_hashfunc hashfunc;
    ht_cmpfunc cmpfunc;
} ht_params;

typedef struct ht_table {
    ht_params params;
    ht_link *buckets;
    size_t n_buckets;
} ht_table;

int ht_init(ht_table *ht, ht_params params);
void ht_destroy(ht_table *ht);
int ht_insert(ht_table *ht, ht_link *elem);
int ht_remove(ht_table *ht, ht_link *elem);

void *ht_lookup(const ht_table, const void *key);

#endif /* _HASHTABLE_H_ */
