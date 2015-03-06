/*
  Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>

  This file is part of CCC.

  CCC is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  CCC is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with CCC.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * Directory for holding information about source files
 *
 * This is implemented as a singleton
 */

#include "file_directory.h"

#include <stddef.h>
#include <string.h>

#include "util/htable.h"
#include "util/util.h"

typedef struct fdir_t {
    htable_t table;
} fdir_t;

static fdir_t s_fdir;

status_t fdir_init() {
    static const ht_params s_params = {
        0,                              // No Size estimate
        offsetof(len_str_node_t, str),  // Offset of key
        offsetof(len_str_node_t, link), // Offset of ht link
        strhash,                        // Hash function
        vstrcmp,                        // void string compare
    };

    return ht_init(&s_fdir.table, &s_params);
}

void fdir_destroy() {
    ht_destroy(&s_fdir.table, DOFREE);
}

status_t fdir_insert(const char *filename, size_t len, len_str_t **result) {
    len_str_t lookup = { (char *)filename, len };
    len_str_node_t *current = ht_lookup(&s_fdir.table, &lookup);
    if (NULL != current) {
        *result = &current->str;
    }

    // Allocate the string and len_str structure in one region
    len_str_node_t *new_len_str = malloc(sizeof(len_str_t) + len + 1);
    if (NULL == new_len_str) {
        return CCC_NOMEM;
    }

    new_len_str->str.str = (char *)new_len_str + sizeof(len_str_t);
    new_len_str->str.len = len;

    strncpy(new_len_str->str.str, filename, len);

    ht_insert(&s_fdir.table, &new_len_str->link);

    *result = &new_len_str->str;
    return CCC_OK;
}

len_str_t *fdir_lookup(const char *filename, size_t len) {
    len_str_t lookup = { (char *)filename, len };
    len_str_node_t *result = ht_lookup(&s_fdir.table, &lookup);

    if (NULL == result) {
        return NULL;
    }

    return &result->str;
}
