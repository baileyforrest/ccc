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
    int start_if_count; /**< Instances of if at start of current conditional */
    bool if_taken;      /**< Branch taken on current if */
} pp_cond_inst_t;

/**
 * An instance of an open file on the preprocessor
 */
typedef struct pp_file_t {
    sl_link_t link;     /**< List link */
    slist_t cond_insts; /**< Instances of conditional preprocessor directive */
    tstream_t stream;   /**< Text stream */
    int if_count;       /**< Instances of if at start of current conditional */
} pp_file_t;

/**
 * Mapping from macro paramater to value
 */
// TODO1: Look into replacing expand_val and raw_val with streams
typedef struct pp_param_map_elem_t {
    sl_link_t link;       /**< List link */
    len_str_t key;        /**< Macro paramater being mappend */
    len_str_t expand_val; /**< Macro paramater with macros expanded */
    len_str_t raw_val;    /**< Macro paramater raw value */
} pp_param_map_elem_t;

/**
 * Represents the macro invocation
 */
typedef struct pp_macro_inst_t {
    sl_link_t link;      /**< List link */
    pp_macro_t *macro;   /**< The macro this is an instance of */
    slist_t param_insts; /**< Stack of macro parameters */
    htable_t param_map;  /**< Mapping of strings to paramater values */
    tstream_t stream;    /**< Text stream of macro instance */
} pp_macro_inst_t;

/**
 * An instance of a macro paramater
 */
typedef struct pp_param_inst_t {
    sl_link_t link;    /**< List link */
    tstream_t stream;  /**< Current macro parameter's stream */
    bool stringify;    /**< Whether or not we are stringifying */
} pp_param_inst_t;

/**
 * Creates a pp_file
 *
 * @return The resulting mapped file
 */
pp_file_t *pp_file_create(void);

/**
 * Unmaps and given pp_file_t. Does free pp_file.
 *
 * @param pp_file The pp_file_t to destroy
 */
void pp_file_destroy(pp_file_t *pp_file);

/**
 * Maps the specified file. Gives result as a pp_file_t
 *
 * @param filename Filename to open.
 * @param result Location to store result. NULL if failed
 * @param result The resulting mapped file
 * @return CCC_OK on success, error code otherwise
 */
status_t pp_map_file(const char *filename, pp_file_t **result);

/**
 * Maps a stream into a preprocessor as if it were a file, placing it on top of
 * the file stack
 *
 * @param pp The preprocessor to map a stream for
 * @param stream The stream to map
 */
void pp_map_stream(preprocessor_t *pp, tstream_t *stream);

/**
 * Creates a macro
 *
 * @param name The name of the macro
 * @param len The length of the macro's name
 * @return The new macro
 */
pp_macro_t *pp_macro_create(char *name, size_t len);

/**
 * Creates a macro instance
 *
 * @param macro to create instace of
 * @return Returns a newly created macro instance
 */
pp_macro_inst_t *pp_macro_inst_create(pp_macro_t *macro);

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
 * @param stringify Set to true if the closing quote of a stringification
 *     occured. Ignored if NULL.
 * @param macro_param Set to true if returned stream is a macro parameter.
 *     Ignored if NULL.
 * @return The next stream in the preprocessor, NULL if none
 */
tstream_t *pp_get_stream(preprocessor_t *pp, bool *stringify,
                         bool *macro_param);

/**
 * Lookups up a macro parameter in a preprocessor. Ignores mapped macros
 *
 * @param pp Preprocessor to use
 * @param lookup Param name to lookup
 */
pp_param_map_elem_t *pp_lookup_macro_param(preprocessor_t *pp,
                                           len_str_t *lookup);

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
 * @param macro The special macro
 * @return Returns the return value for pp_nextchar
 */
int pp_handle_special_macro(preprocessor_t *pp, tstream_t *stream,
                            pp_macro_t *macro);
/**
 * Handle the defined operator
 *
 * @param pp The preprocessor to handle special macros for
 * @param lookahead The lookahead stream being used
 * @param stream The stream at the current special macro
 * @return Returns the return value for pp_nextchar
 */
int pp_handle_defined(preprocessor_t *pp, tstream_t *lookahead,
                      tstream_t *stream);

#endif /* _PREPROCESSOR_PRIV_H_ */
