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

#include "util/file_directory.h"

typedef struct tstream_t {
    char *cur;
    char *end;
    fmark_t mark;
} tstream_t;

/**
 * Text stream literal
 *
 * @param str String to create stream from
 * @param file Filename
 * @param line_start Pointer to start of current line
 * @param last Last file mark on stack
 * @param line Current line number
 * @param col Current column number
 */
#define TSTREAM_LIT(word, last, file, line_start, line, col)    \
    { word, word + (sizeof(word) - 1),                          \
            FMARK_LIT(last, file, line_start, line, col) }

/**
 * Initializes a text stream
 *
 * @param ts Text stream to initalize
 * @param start Starting character of the stream
 * @param end Ending character of the stream
 * @param file Filename of the file the stream is processing
 * @param line_start Pointer to start of current line
 * @param last Last file mark on stack
 * @param line Current line number
 * @param col Current column number
 */
void ts_init(tstream_t *ts, char *start, char *end,
             len_str_t *file, char *line_start, fmark_t *last,
             int line, int col);

#define TS_COPY_DEEP true
#define TS_COPY_SHALLOW false

/**
 * Creates a deep copy of the src stream. (All file marks in the chain are
 * allocated)
 *
 * @param dest Destination
 * @param src Source
 * @return CCC_OK on success, error code on error
 */
status_t ts_copy(tstream_t *dest, const tstream_t *src, bool deep);

/**
 * Destroys a text stream. This function should only be called on a deep copy
 * created with ts_copy.
 *
 * @param ts Text stream to destroy
 */
void ts_destroy(tstream_t *ts);

/**
 * Returns the a pointer to the current location in the text stream
 *
 * @param ts Text stream to use
 * @return the current character
 */
inline char *ts_location(tstream_t *ts) {
    return ts->cur;
}

/**
 * Returns the current character in the text stream
 *
 * @param ts Text stream to use
 * @return The current character
 */
inline int ts_cur(tstream_t *ts) {
    if (ts->cur == ts->end) {
        return EOF;
    }
    return *ts->cur;
}

/**
 * Returns the next character in the text stream
 *
 * @param ts Text stream to use
 * @return The next character
 */
inline int ts_next(tstream_t *ts) {
    if (ts->cur == ts->end || ts->cur + 1 == ts->end) {
        return EOF;
    }
    return *(ts->cur + 1);
}

/**
 * Tells whether a text stream is at the end
 *
 * @param ts Text stream to use
 * @return true if text stream is at end, false otherwise
 */
inline bool ts_end(tstream_t *ts) {
    return ts->cur == ts->end;
}

/**
 * Advance an identifier forward one character.
 *
 * @param ts Text stream to use
 * @return the character in the LAST position
 */
int ts_advance(tstream_t *ts);

/**
 * Skip whitespace characters and block comments, or stop if the end is reached
 *
 * @param ts Text stream to use
 * @return The number of characters skipped
 */
size_t ts_skip_ws_and_comment(tstream_t *ts);

/**
 * Advance ts past an identifier
 *
 * @param ts Text stream to use
 * @return The length of the identifier
 */
size_t ts_advance_identifier(tstream_t *ts);

/**
 * Skip until the start of the next line.
 *
 * @param ts Text stream to use
 * @return The number of characters skipped
 */
size_t ts_skip_line(tstream_t *ts);

#endif /* _TEXT_STREAM_H_ */
