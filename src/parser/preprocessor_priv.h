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

/**
 * An instance of an open file on the preprocessor
 */
typedef struct pp_file_t {
    sl_link_t link; /**< List link */
    char *buf;      /**< mmap'd buffer */
    char *cur;      /**< Current buffer location */
    char *end;      /**< Max location */
    int fd;         /**< File descriptor of open file */
} pp_file_t;

/**
 * Struct for macro definition
 */
typedef struct pp_macro_t {
    len_str_t name; /**< Macro name, hashtable key */
    char *start;    /**< Start of macro text */
    char *end;      /**< End of macro text */
    slist_t params;  /**< Macro paramaters, list of len_str */
    int num_params;  /**< Number of paramaters */
} pp_macro_t;

/**
 * Mapping from macro paramater to value
 */
typedef struct pp_param_map_elem_t {
    sl_link_t link; /**< List link */
    len_str_t key; /**< Macro paramater being mappend */
    len_str_t val; /**< Macro paramater value */
} pp_param_map_elem_t;

/**
 * Represents the macro invocation
 */
typedef struct pp_macro_inst_t {
    sl_link_t link; /**< List link */
    htable_t param_map; /**< Mapping of strings to paramater values */
    char *cur; /**< Current location in macro */
    char *end; /**< End of macro */
} pp_macro_inst_t;

/**
 * Maps the specified file. Gives result as a pp_file_t
 *
 * @param filename Filename to open.
 * @param result Location to store result. NULL if failed
 * @return CCC_OK on success, error code otherwise
 */
status_t pp_file_map(const char *filename, pp_file_t **result);

/**
 * Does not free pp_file. Unmaps and given pp_file_t
 *
 * @param filename Filename to open.
 * @param result Location to store result. NULL if failed
 * @return CCC_OK on success, error code otherwise
 */
void pp_file_destroy(pp_file_t *pp_file);

/**
 * Creates a macro instance
 *
 * @param macro to create instace of
 * @param result Location to store result. NULL if failed
 * @return CCC_OK on success, error code otherwise
 */
status_t pp_macro_inst_create(pp_macro_t *macro, pp_macro_inst_t **result);

/**
 * Destroys a macro instance
 *
 * @param macro intance to destroy
 */
void pp_macro_inst_destroy(pp_macro_inst_t *macro_inst);

#endif /* _PREPROCESSOR_PRIV_H_ */
