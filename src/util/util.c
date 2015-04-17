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

#include <stdlib.h>

#include "util/logger.h"

extern uint32_t strhash(const void *vstr);
extern uint32_t ind_strhash(const void *vstr);
extern bool vstrcmp(const void *vstr1, const void *vstr2);
extern bool ind_vstrcmp(const void *vstr1, const void *vstr2);

static const char *const mem_err = "out of memory, giving up";

void *emalloc(size_t size) {
    void *result = malloc(size);
    if (result == NULL) {
        logger_log(NULL, LOG_ERR, mem_err);
        exit(EXIT_FAILURE);
    }
    return result;
}

void *ecalloc(size_t nmemb, size_t size) {
    void *result = calloc(nmemb, size);
    if (result == NULL) {
        logger_log(NULL, LOG_ERR, mem_err);
        exit(EXIT_FAILURE);
    }
    return result;
}

void *erealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    if (result == NULL) {
        logger_log(NULL, LOG_ERR, mem_err);
        exit(EXIT_FAILURE);
    }
    return result;
}
