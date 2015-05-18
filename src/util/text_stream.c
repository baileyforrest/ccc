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
 * Text stream implementation
 */

#include "text_stream.h"

#include <assert.h>

void ts_init(tstream_t *ts, char *start, char *end, char *file, fmark_t *last) {
    assert(ts != NULL);
    ts->cur = start;
    ts->end = end;
    ts->mark.filename = file;
    ts->mark.line_start = start;
    ts->mark.last = last;
    ts->mark.line = 1;
    ts->mark.col = 1;
    ts->last = EOF;
}

int ts_getc(tstream_t *ts) {
    if (ts->last != EOF) {
        int retval = ts->last;
        ts->last = EOF;
        return retval;
    }

    if (ts->cur == ts->end) {
        return EOF;
    }

    if (*ts->cur == '\n') {
        ts->mark.line++;
        ts->mark.col = 1;
        ts->mark.line_start = ts->cur + 1;
    } else {
        ts->mark.col++;
    }

    return *(ts->cur++);
}

void ts_ungetc(int c, tstream_t *ts) {
    ts->last = c;
}
