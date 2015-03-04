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

uint32_t strhash(const void *vstr, size_t len) {
    const char *str = (char *)vstr;
    uint32_t hash = 5381;
    int c;

    len = len == 0 ? SIZE_MAX : len;

    while (len-- > 0 && (c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

bool vstrcmp(const void *str1, const void *str2, size_t len) {
    (void)len; // Ignore length
    return strcmp((const char *)str1, (const char *)str2) == 0;
}
