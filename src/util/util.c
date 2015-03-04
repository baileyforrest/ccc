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
 * Implementations of misc utilities
 */

#include "util.h"

#include <string.h>

uint32_t strhash(const void *vstr) {
    const len_str_t *len_str = (const len_str_t *)vstr;
    const char *str = len_str->str;
    size_t len = len_str->len;
    uint32_t hash = 5381;
    int c;

    while (len-- > 0 && (c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

bool vstrcmp(const void *vstr1, const void *vstr2) {
    const len_str_t *str1 = (const len_str_t *)vstr1;
    const len_str_t *str2 = (const len_str_t *)vstr2;

    if (str1->len != str2->len) {
        return false;
    }

    return strncmp(str1->str, str2->str, str1->len) == 0;
}
