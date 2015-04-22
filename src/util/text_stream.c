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
#include <stdio.h>

extern inline char *ts_location(tstream_t *ts);
extern inline int ts_cur(tstream_t *ts);
extern inline bool ts_end(tstream_t *ts);
extern inline int ts_next(tstream_t *ts);
extern inline int ts_last(tstream_t *ts);

void ts_init(tstream_t *ts, char *start, char *end, char *file,
             char *line_start, fmark_t *last, int line, int col) {
    assert(ts != NULL);
    ts->cur = start;
    ts->end = end;
    ts->last = 0;
    ts->mark.filename = file;
    ts->mark.line_start = line_start;
    ts->mark.last = last;
    ts->mark.line = line;
    ts->mark.col = col;
}

int ts_advance(tstream_t *ts) {
    ts->last = ts_cur(ts);
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

size_t ts_skip_ws_and_comment(tstream_t *ts) {
    size_t num_chars = 0;
    bool done = false;
    bool comment = false;
    while (!done && !ts_end(ts)) {
        num_chars++;

        if (comment) {
            if (ts_advance(ts) != '*') {
                continue;
            }
            /* Found '*' */

            if (ts_end(ts)) {
                continue;
            }

            if (ts_advance(ts) == '/') {
                comment = false;
            }
            continue;
        }
        switch (ts_cur(ts)) {
        case ' ':
        case '\t':
            // Skip white space
            ts_advance(ts);
            break;
        case '/':
            ts_advance(ts);
            if (ts_end(ts)) {
                break;
            }
            if (ts_cur(ts) == '*') {
                comment = true;
            }
            break;
        case '\\':
            ts_advance(ts);
            if (ts_end(ts)) {
                break;
            }

            /* Skip escaped newlines */
            if (ts_cur(ts) == '\n') {
                ts_advance(ts);
            }
            break;
        default:
            done = true;
        }
    }

    // Subtract one because terminator was counted
    return num_chars - 1;
}

size_t ts_skip_string(tstream_t *ts) {
    int terminator;
    size_t num_chars = 0;
    switch (ts_cur(ts)) {
    case '"':
    case '\'':
        terminator = ts_cur(ts);
        break;
    default:
        return num_chars;
    }
    ts_advance(ts);
    ++num_chars;

    bool ignore_escape = false;
    while (!ts_end(ts)) {
        ++num_chars;
        if (ts_cur(ts) == terminator &&
            (ts_last(ts) != '\\' && !ignore_escape)) {
            ts_advance(ts);
            break;
        }
        if (ts_cur(ts) == '\\' && ts_last(ts) == '\\') {
            ignore_escape = true;
        } else {
            ignore_escape = false;
        }
        ts_advance(ts);
    }
    return num_chars;
}

size_t ts_advance_identifier(tstream_t *ts) {
    size_t num_chars = 0;
    bool done = false;
    bool first = true;
    while (!done && !ts_end(ts)) {

        /* Charaters allowed to be in idenitifer */
        switch (ts_cur(ts)) {
        case ASCII_DIGIT:
            if (first) {
                done = true;
                break;
            }
        case ASCII_LOWER:
        case ASCII_UPPER:
        case '_':
            num_chars++;
            ts_advance(ts);
            break;
        default: /* Found ts->end */
            done = true;
        }
        first = false;
    }

    return num_chars;
}

size_t ts_skip_line(tstream_t *ts, bool *in_comment) {
    size_t num_chars = 0;
    int last = -1;
    bool comment = in_comment == NULL ? false : *in_comment;
    while (!ts_end(ts)) {
        if (ts_cur(ts) == '/' && ts_next(ts) == '*') {
            comment = true;
        } else if (last == '*' && ts_cur(ts) == '/') {
            comment = false;
        } else if (ts_cur(ts) == '\n' && last != '\\') {
            break;
        }
        num_chars++;
        last = ts_advance(ts);
    }

    if (in_comment != NULL) {
        *in_comment = comment;
    }
    return num_chars;
}
