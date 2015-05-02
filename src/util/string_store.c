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
 * Centralized string storage implementation
 */

#include "string_store.h"

#include <assert.h>

#include "util/htable.h"
#include "util/util.h"

typedef struct sstore_t {
    htable_t table;
} sstore_t;

typedef struct sstore_entry_t {
    sl_link_t link;
    char *str;
    bool free_string;
} sstore_entry_t;

sstore_t strings;

void sstore_init(void) {
    static const ht_params_t ht_params = {
        0,                              // Size estimate
        offsetof(sstore_entry_t, str),  // Offset of key
        offsetof(sstore_entry_t, link), // Offset of ht link
        ind_str_hash,                   // Hash function
        ind_str_eq,                     // void string compare
    };

    ht_init(&strings.table, &ht_params);
}

void sstore_entry_destroy(sstore_entry_t *entry) {
    if (entry->free_string) {
        free(entry->str);
    }
    free(entry);
}

void sstore_destroy(void) {
    HT_DESTROY_FUNC(&strings.table, sstore_entry_destroy);
}

char *sstore_lookup(const char *str) {
    sstore_entry_t *node = ht_lookup(&strings.table, &str);
    if (node != NULL) {
        return node->str;
    }

    node = emalloc(sizeof(*node) + strlen(str) + 1);
    node->str = (char *)node + sizeof(*node);
    node->free_string = false;
    strcpy(node->str, str);

    status_t status = ht_insert(&strings.table, &node->link);
    assert(status == CCC_OK);

    return node->str;
}

char *sstore_insert(char *str) {
    sstore_entry_t *node = ht_lookup(&strings.table, &str);
    if (node != NULL) {
        free(str);
        return node->str;
    }

    node = emalloc(sizeof(*node));
    node->str = str;
    node->free_string = true;

    status_t status = ht_insert(&strings.table, &node->link);
    assert(status == CCC_OK);

    return str;
}
