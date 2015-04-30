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
 * Temp file interface
 */

#ifndef _TEMPFILE_H_
#define _TEMPFILE_H_

#include <stdio.h>

#include "util/slist.h"

typedef struct tempfile_t {
    sl_link_t link;
    char *tmp_path;
    FILE *stream;
    int fd;
} tempfile_t;

tempfile_t *tempfile_create(char *path, char *ext);

inline char *tempfile_path(tempfile_t *tf) {
    return tf->tmp_path;
}

inline FILE *tempfile_file(tempfile_t *tf) {
    return tf->stream;
}

void tempfile_close(tempfile_t *tf);
void tempfile_destroy(tempfile_t *tf);

#endif /* _TEMPFILE_H_ */
