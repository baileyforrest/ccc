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
 * Interface for preprocessor/file reader
 */

#ifndef _PREPROCESSOR_H_
#define _PREPROCESSOR_H_

#include <stdbool.h>

#include "util/htable.h"
#include "util/slist.h"


/**
 * Object for preprocessor
 */
typedef struct preprocessor_t {
    slist_t file_insts;        /**< Stack of instances of open files */
    slist_t macro_insts;       /**< Stack of paramaters and strings mappings */
    slist_t search_path;       /**< #include search path */
    htable_t macros;           /**< Macro table */
    htable_t directives;       /**< Preprocessor directives */

    const char *cur_param;
    const char *param_end;

    // Paramaters for reading preprocessor commands
    bool block_comment;        /**< true if in a block comment */
    bool line_comment;         /**< true if in a line comment */
    bool string;               /**< true if in string */
    bool char_line;            /**< true if non whitespace on current line */
} preprocessor_t;


/**
 * Initializes preprocessor
 *
 * @param pp The preprocessor to initalize
 */
status_t pp_init(preprocessor_t *pp);

/**
 * Destroys preprocessor. Closes file if open
 *
 * @param pp The preprocessor to destroy
 */
void pp_destroy(preprocessor_t *pp);

/**
 * Initializes preprocessor for reading specified file
 *
 * @param pp The preprocessor to open file with
 * @param filename The name of the file to process
 */
status_t pp_open(preprocessor_t *pp, const char *filename);

/**
 * Closes preprocessor to let it process another file
 *
 * @param pp The preprocessor to close
 * @param filename The name of the file to process
 */
void pp_close(preprocessor_t *pp);

#define PP_EOF (-1)

/**
 * Fetch next character from preprocessor
 *
 * @param  pp The preprocessor to get characters from
 * @return the next character. PP_EOF on EOF
 */
int pp_nextchar(preprocessor_t *pp);

#endif /* _PREPROCESSOR_H_ */
