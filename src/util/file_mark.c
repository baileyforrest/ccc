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
 * File mark implementation
 */

#include "file_mark.h"

#include "util/util.h"

fmark_t fmark_built_in =
    FMARK_LIT(NULL, BUILT_IN_FILENAME, BUILT_IN_FILENAME, 1, 1);

#define FMARK_NUM_NODES 256

typedef struct fmark_node_t {
    sl_link_t link;
    fmark_t marks[FMARK_NUM_NODES];
} fmark_node_t;

void fmark_man_init(fmark_man_t *man) {
    sl_init(&man->list, offsetof(fmark_node_t, link));
    man->offset = FMARK_NUM_NODES;
}

void fmark_man_destroy(fmark_man_t *man) {
    SL_DESTROY_FUNC(&man->list, free);
    man->offset = 0;
}

fmark_t *fmark_man_insert(fmark_man_t *man, fmark_t *copy_from) {
    if (man->offset == FMARK_NUM_NODES) {
        fmark_node_t *node = emalloc(sizeof(fmark_node_t));
        man->offset = 0;
        sl_append(&man->list, &node->link);
    }

    fmark_node_t *tail = sl_tail(&man->list);
    memcpy(&tail->marks[man->offset], copy_from, sizeof(fmark_t));

    return &tail->marks[man->offset++];
}
