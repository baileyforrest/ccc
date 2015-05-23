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

#define MIN_CAPACITY 16

// 1.5 growth factor
#define NEW_SIZE(size) ((size) + ((size) >> 1))


void fmark_man_init(fmark_man_t *man) {
    man->capacity = MIN_CAPACITY;
    man->size = 0;
    man->marks = emalloc(sizeof(fmark_t) * man->capacity);
}

void fmark_man_destroy(fmark_man_t *man) {
    free(man->marks);
    man->capacity = 0;
    man->size = 0;
    man->marks = NULL;
}

fmark_t *fmark_man_insert(fmark_man_t *man, fmark_t *copy_from) {
    if (man->size == man->capacity) {
        man->capacity = NEW_SIZE(man->capacity);
        man->marks = realloc(man->marks, man->capacity * sizeof(fmark_t));
    }

    memcpy(&man->marks[man->size], copy_from, sizeof(fmark_t));

    return &man->marks[man->size++];
}
