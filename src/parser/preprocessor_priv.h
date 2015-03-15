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

/**
 * Given a pointer and an end, skip whitespace characters and block
 * comments, or stop if the end is reached
 *
 * @param lookahead - char * to advance
 * @param end - max char *
 */
#define SKIP_WS_AND_COMMENT(lookahead, end)     \
    do {                                        \
    bool done = false;                          \
    bool comment = false;                       \
    while (!done && lookahead != end) {         \
        if (comment) {                          \
            if ('*' != *(lookahead++)) {        \
                continue;                       \
            }                                   \
                                                \
            /* Found '*' */                     \
                                                \
            if (lookahead == end) {             \
                continue;                       \
            }                                   \
                                                \
            if ('/' == *(lookahead++)) {        \
                comment = false;                \
            }                                   \
            continue;                           \
        }                                       \
        switch (*lookahead) {                   \
        case ' ':                               \
        case '\t':                              \
            lookahead++;                        \
            break;                              \
        case '/':                               \
            lookahead++;                        \
            if (lookahead == end) {             \
                break;                          \
            }                                   \
            if ('*' == *lookahead) {            \
                comment = true;                 \
            }                                   \
            break;                              \
        case '\\':                              \
            lookahead++;                        \
            if (lookahead == end) {             \
                break;                          \
            }                                   \
            /* Skip escaped newlines */         \
            if ('\n' == *lookahead) {           \
                lookahead++;                    \
            }                                   \
            break;                              \
        default:                                \
            done = true;                        \
        }                                       \
    }                                           \
    } while(0)

/**
 * Given a pointer and an end, advance the the pointer until 1 past the end of
 * an identifier, or stop if the end is reaced
 *
 * @param lookahead - char * to advance
 * @param end - max char *
 */
#define ADVANCE_IDENTIFIER(lookahead, end)              \
    do {                                                \
        bool done = false;                              \
        while (!done && lookahead != end) {             \
            /* Charaters allowed to be in idenitifer */ \
            switch (*lookahead) {                       \
            case ASCII_LOWER:                           \
            case ASCII_UPPER:                           \
            case ASCII_DIGIT:                           \
            case '_':                                   \
            case '-':                                   \
                lookahead++;                            \
                break;                                  \
            default: /* Found end */                    \
                done = true;                            \
            }                                           \
        }                                               \
    } while(0)

/**
 * Skip util past newline
 *
 * @param lookahead - char * to advance
 * @param end - max char *
 */
#define SKIP_LINE(lookahead, end)                                       \
    do {                                                                \
    bool done = false;                                                  \
    char last = -1;                                                     \
    while (!done && lookahead != end) {                                 \
        /* Skip until we reach an unescaped newline */                  \
        if ('\n' == *lookahead && '\\' != last) {                       \
            done = true;                                                \
        }                                                               \
        last = *(lookahead++);                                          \
    }                                                                   \
    } while(0)

/**
 * Log an error
 *
 * @param preprocessor_t The preprocessor the error is in
 * @param char * message The message
 * @param log_type_t type Type of log message
 */
#define LOG_ERROR(pp, message, type)                        \
    do {                                                    \
        pp_file_t *pp_file = sl_head(&(pp)->file_insts);    \
        assert(NULL != pp_file);                            \
        fmark_t mark = {                                    \
            (pp_file)->filename,                            \
            (pp_file)->line_num,                            \
            (pp_file)->col_num                              \
        };                                                  \
        logger_log(&mark, (message), (type));               \
    } while(0)

/**
 * An instance of an open file on the preprocessor
 */
typedef struct pp_file_t {
    sl_link_t link;      /**< List link */
    len_str_t *filename; /**< Filename. Not freed/alloced with pp_file_t */
    char *buf;           /**< mmap'd buffer */
    char *cur;           /**< Current buffer location */
    char *end;           /**< Max location */
    int fd;              /**< File descriptor of open file */
    int if_count;        /**< Number of instances of active if directive */
    int line_num;        /**< Current line number in file */
    int col_num;         /**< Current column number in file */
} pp_file_t;

/**
 * Struct for macro definition
 */
typedef struct pp_macro_t {
    sl_link_t link; /**< List link */
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
 * Helper function to fetch characters with macro substitution
 *
 * @param pp The preprocessor te fetch characters from
 * @param ignore_directive if true, directives are not processed
 */
int pp_nextchar_helper(preprocessor_t *pp, bool ignore_directive);

/**
 * Maps the specified file. Gives result as a pp_file_t
 *
 * @param filename Filename to open.
 * @param len Length of filename
 * @param result Location to store result. NULL if failed
 * @return CCC_OK on success, error code otherwise
 */
status_t pp_file_map(const char *filename, size_t len, pp_file_t **result);

/**
 * Unmaps and given pp_file_t. Does free pp_file.
 *
 * @param filename Filename to open.
 * @param result Location to store result. NULL if failed
 * @return CCC_OK on success, error code otherwise
 */
status_t pp_file_destroy(pp_file_t *pp_file);

/**
 * Initializes a macro
 *
 * @param macro to initalize
 * @return CCC_OK on success, error code otherwise
 */
status_t pp_macro_init(pp_macro_t *macro);

/**
 * Destroys a macro definition. Does not free macro
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

#endif /* _PREPROCESSOR_PRIV_H_ */
