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
 * Text stream interface
 *
 * Text stream provideds a uniform interface to retrieving characters from a
 * character array
 */

#ifndef _TEXT_STREAM_H_
#define _TEXT_STREAM_H_

#include <stdbool.h>
#include <stdio.h>

#include "util/file_mark.h"

/**
 * Text stream. Represents a string of characters which automatically updates
 * its location in a file.
 */
typedef struct tstream_t {
    char *cur;
    char *end;
    fmark_t mark; /**< Mark of current location in the stream */
    int last;
} tstream_t;

/**
 * Initializes a text stream
 *
 * @param ts Text stream to initalize
 */
void ts_init(tstream_t *ts, char *start, char *end, char *file, fmark_t *last);

int ts_peek(tstream_t *ts);

/**
 * Retrieves a character from the text stream
 *
 * @param ts Text stream to fetch from
 * @return the next character
 */
int ts_getc(tstream_t *ts);

/**
 * Returns a character to the text stream
 *
 * @param ts Text stream to fetch from
 * @return the next character
 */
void ts_ungetc(int c, tstream_t *ts);

#endif /* _TEXT_STREAM_H_ */
