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

#include "util/file_directory.h"
#include "util/htable.h"
#include "util/slist.h"
#include "util/text_stream.h"

#define PP_BUF_SIZE 1024

/**
 * Object for preprocessor
 */
typedef struct preprocessor_t {
    slist_t file_insts;   /**< Stack of instances of open files */
    slist_t macro_insts;  /**< Stack of paramaters and strings mappings */
    slist_t param_insts;  /**< Stack of macro parameters */
    slist_t search_path;  /**< #include search path */
    htable_t macros;      /**< Macro table */
    htable_t directives;  /**< Preprocessor directives */

    fmark_t last_mark;    /**< Mark of last returned token */

    // Paramaters for reading preprocessor commands
    bool pp_if;           /**< if true, we're processing a preprocessor if */
    bool block_comment;   /**< true if in a block comment */
    bool line_comment;    /**< true if in a line comment */
    bool string;          /**< true if in string */
    bool char_line;       /**< true if non whitespace on current line */
    bool ignore;          /**< Conditional compilation - ignore output */
    bool in_directive;    /**< if true, we're processing a directive */

    char macro_buf[PP_BUF_SIZE]; /**< Buffer for built in macros (e.g. FILE) */
} preprocessor_t;

/**
 * Types of macros
 */
typedef enum pp_macro_type_t {
    MACRO_BASIC,   /**< Regular macro */
    MACRO_FILE,    /**< __FILE__ */
    MACRO_LINE,    /**< __LINE__ */
    MACRO_DATE,    /**< __DATE__ */
    MACRO_TIME,    /**< __TIME__ */
    MACRO_DEFINED, /**< defined operator */
    MACRO_PRAGMA,  /**< _Pragma operator */
    MACRO_CLI_OPT, /**< Command line option */
} pp_macro_type_t;


/**
 * Struct for macro definition
 */
typedef struct pp_macro_t {
    sl_link_t link;         /**< List link */
    len_str_t name;         /**< Macro name, hashtable key */
    const tstream_t stream; /**< Text stream template */
    slist_t params;         /**< Macro paramaters, list of len_str_node_t */
    int num_params;         /**< Number of paramaters -1 for non func style */
    pp_macro_type_t type;   /**< Type of macro */
} pp_macro_t;

/**
 * Initializes preprocessor
 *
 * @param pp The preprocessor to initalize
 * @param macros If non NULL, manager's preprocessor is initialized with
 *     given macros. It is then used under the assumption that macros will not
 *     change during use, and that manager will not change macros
 * @return CCC_OK on success, error code on error.
 */
status_t pp_init(preprocessor_t *pp, htable_t *macros);

/**
 * Destroys preprocessor. Closes file if open
 *
 * @param pp The preprocessor to destroy
 */
void pp_destroy(preprocessor_t *pp);

/**
 * Destroys a macro definition. Does free macro
 *
 * @param macro definition to destroy
 */
void pp_macro_destroy(pp_macro_t *macro);

/**
 * Initializes preprocessor for reading specified file
 *
 * @param pp The preprocessor to open file with
 * @param filename The name of the file to process
 * @return CCC_OK on success, error code on error.
 */
status_t pp_open(preprocessor_t *pp, const char *filename);

/**
 * Closes preprocessor to let it process another file
 *
 * @param pp The preprocessor to close
 * @param filename The name of the file to process
 */
void pp_close(preprocessor_t *pp);

/**
 * Get the mark of the last successfully fetched character
 *
 * @param pp The preprocessor to get the mark from from
 * @param mark Pointer to mark to populate
 */
void pp_last_mark(preprocessor_t *pp, fmark_t *mark);


#define PP_EOF 0

/**
 * Fetch next character from preprocessor
 *
 * @param pp The preprocessor to get characters from
 * @return the next character. PP_EOF on end of input. Negative value on error.
 *     It is a negated status_t
 */
int pp_nextchar(preprocessor_t *pp);

#endif /* _PREPROCESSOR_H_ */
