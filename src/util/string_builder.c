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
 * String builder implementation
 */

#include "string_builder.h"

#include <stdarg.h>

#include "util/util.h"

#define GROWTH_FUNC(size) ((size) < 16 ? 16 : (((size) >> 1) + (size)))

extern char *sb_buf(string_builder_t *sb);
extern size_t sb_len(string_builder_t *sb);

void sb_init(string_builder_t *sb, size_t capacity) {
    sb->capacity = capacity;
    sb->len = 0;
    sb->buf = emalloc(capacity + 1);
    sb->buf[0] = '\0';
}

void sb_destroy(string_builder_t *sb) {
    free(sb->buf);
    sb->len = 0;
    sb->capacity = 0;
}

void sb_compact(string_builder_t *sb) {
    sb->buf = erealloc(sb->buf, sb->len + 1);
    sb->capacity = sb->len;
}

void sb_reserve(string_builder_t *sb, size_t capacity) {
    if (capacity < sb->capacity) {
        return;
    }

    sb->buf = erealloc(sb->buf, capacity + 1);
    sb->capacity = capacity;
}

void sb_clear(string_builder_t *sb) {
    sb->len = 0;
    sb->buf[0] = '\0';
}

void sb_append_char(string_builder_t *sb, char val) {
    if (sb->len == sb->capacity) {
        sb->capacity = GROWTH_FUNC(sb->capacity);
        sb->buf = erealloc(sb->buf, sb->capacity + 1);
    }

    sb->buf[sb->len++] = val;
    sb->buf[sb->len] = '\0';
}

void sb_append_printf(string_builder_t *sb, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    sb_append_vprintf(sb, fmt, ap);
    va_end(ap);
}

void sb_append_vprintf(string_builder_t *sb, char *fmt, va_list ap) {
    va_list copy;
    va_copy(copy, ap);
    int size = vsnprintf(NULL, 0, fmt, ap);

    sb_reserve(sb, sb_len(sb) + size);

    vsnprintf(sb_buf(sb) + sb_len(sb), size + 1, fmt, copy);

    sb->len = sb->len + size;
    sb->buf[sb->len] = '\0';
    va_end(copy);
}
