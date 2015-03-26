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
 * Private Interface for preprocessor/file reader
 */

#ifndef _PREPROCESSOR_PRIV_H_
#define _PREPROCESSOR_PRIV_H_

#include "util/slist.h"
#include "util/util.h"
#include "util/logger.h"

typedef struct pp_cond_inst_t {
    sl_link_t link;     /**< List link */
    bool if_taken;      /**< Branch taken on current if */
} pp_cond_inst_t;

/**
 * An instance of an open file on the preprocessor
 */
typedef struct pp_file_t {
    sl_link_t link;     /**< List link */
    slist_t cond_insts; /**< Instances of conditional preprocessor directive */
    tstream_t stream;   /**< Text stream */
    int start_if_count; /**< Instances of if at start of current conditional */
    int if_count;       /**< Instances of if at start of current conditional */
    bool owns_file;
} pp_file_t;

typedef enum pp_macro_type_t {
    MACRO_BASIC,   /**< Regular macro */
    MACRO_FILE,    /**< __FILE__ */
    MACRO_LINE,    /**< __LINE__ */
    MACRO_DATE,    /**< __DATE__ */
    MACRO_TIME,    /**< __TIME__ */
    MACRO_DEFINED, /**< defined operator */
    MACRO_PRAGMA,  /**< _Pragma operator */
} pp_macro_type_t;

/**
 * Struct for macro definition
 */
typedef struct pp_macro_t {
    sl_link_t link;         /**< List link */
    len_str_t name;         /**< Macro name, hashtable key */
    const tstream_t stream; /**< Text stream template */
    slist_t params;         /**< Macro paramaters, list of len_str */
    int num_params;         /**< Number of paramaters */
    pp_macro_type_t type;   /**< Type of macro */
} pp_macro_t;

/**
 * Mapping from macro paramater to value
 */
typedef struct pp_param_map_elem_t {
    sl_link_t link; /**< List link */
    len_str_t key;  /**< Macro paramater being mappend */
    len_str_t val;  /**< Macro paramater value */
} pp_param_map_elem_t;

/**
 * Represents the macro invocation
 */
typedef struct pp_macro_inst_t {
    sl_link_t link;     /**< List link */
    htable_t param_map; /**< Mapping of strings to paramater values */
    tstream_t stream;   /**< Text stream of macro instance */
} pp_macro_inst_t;

/**
 * Maps the specified file. Gives result as a pp_file_t
 *
 * @param filename Filename to open.
 * @param len Length of filename
 * @param result Location to store result. NULL if failed
 * @param last_file The file which included one being mapped. NULL if none.
 * @return CCC_OK on success, error code otherwise
 */
status_t pp_map_file(const char *filename, size_t len, pp_file_t *last_file,
                     pp_file_t **result);

/**
 * Maps a stream into a preprocessor as if it were a file, placing it on top of
 * the file stack
 *
 * @param pp The preprocessor to map a stream for
 * @param stream The stream to map
 * @return CCC_OK on success, error code on error.
 */
status_t pp_map_stream(preprocessor_t *pp, tstream_t *stream);

/**
 * Unmaps and given pp_file_t. Does free pp_file.
 *
 * @param filename Filename to open.
 * @param result Location to store result. NULL if failed
 */
void pp_file_destroy(pp_file_t *pp_file);

/**
 * Creates a macro
 *
 * @param macro Points to new macro on success
 * @return CCC_OK on success, error code otherwise
 */
status_t pp_macro_create(char *name, size_t len, pp_macro_t **result);

/**
 * Destroys a macro definition. Does free macro
 *
 * @param macro definition to destroy
 */
void pp_macro_destroy(pp_macro_t *macro);

/**
 * Creates a macro instance
 *
 * @param macro to create instace of
 * @param result Location to store result. NULL if failed
 * @return CCC_OK on success, error code otherwise
 */
status_t pp_macro_inst_create(pp_macro_t *macro, pp_macro_inst_t **result);

/**
 * Destroys a macro instance. Does free macro_inst.
 *
 * @param macro intance to destroy
 */
void pp_macro_inst_destroy(pp_macro_inst_t *macro_inst);

/**
 * Fetches the next unfinished stream from a preprocessor
 *
 * @param pp The preprocessor to fetch characters from
 * @return The next stream in the preprocessor, NULL if none
 */
tstream_t *pp_get_stream(preprocessor_t *pp);

/**
 * Helper function to fetch characters with macro substitution
 *
 * @param pp The preprocessor to fetch characters from
 * @return Returns the return value for pp_nextchar
 */
int pp_nextchar_helper(preprocessor_t *pp);

/**
 * Handle special macros (e.g. __FILE__)
 *
 * @param pp The preprocessor to handle special macros for
 * @param stream The stream at the current special macro
 * @param type The type of special macro
 * @return Returns the return value for pp_nextchar
 */
int pp_handle_special_macro(preprocessor_t *pp, tstream_t *stream,
                            pp_macro_type_t type);
/**
 * Handle the defined operator
 *
 * @param pp The preprocessor to handle special macros for
 * @param lookahead The lookahead stream being used
 * @param stream The stream at the current special macro
 * @return Returns the return value for pp_nextchar
 */
status_t pp_handle_defined(preprocessor_t *pp, tstream_t *lookahead,
                           tstream_t *stream);

#endif /* _PREPROCESSOR_PRIV_H_ */
