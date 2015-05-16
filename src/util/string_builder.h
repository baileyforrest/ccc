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
 * String builder interface
 */

#ifndef _STRING_BUILDER_H_
#define _STRING_BUILDER_H_

#include <stddef.h>

typedef struct string_builder_t {
    char *buf;
    size_t len;
    size_t capacity;
} string_builder_t;

inline char *sb_buf(string_builder_t *sb) {
    return sb->buf;
}

inline size_t sb_len(string_builder_t *sb) {
    return sb->len;
}

void sb_init(string_builder_t *sb, size_t capacity);

void sb_destroy(string_builder_t *sb);

void sb_compact(string_builder_t *sb);

void sb_append_char(string_builder_t *sb, char val);

#endif /* _STRING_BUILDER_H_ */
