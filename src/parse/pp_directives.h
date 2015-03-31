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
 * Interface to preprocessor directives
 */

#ifndef _PP_DIRECTIVE_H_
#define _PP_DIRECTIVE_H_

#include "parse/preprocessor.h"
#include "parse/preprocessor_priv.h"

#include "util/htable.h"
#include "util/slist.h"
#include "util/status.h"
#include "util/util.h"


/**
 * Preprocessor directive callback type
 */
typedef status_t (*pp_action_t)(preprocessor_t *pp);

/**
 * Prepcoressor directive object
 */
typedef struct pp_directive_t {
    sl_link_t link;     /**< Hash table link */
    len_str_t key;      /**< Directive name */
    pp_action_t action; /**< Preprocessor action */
    bool skip_line;     /**< If true skip the line after the directive */
} pp_directive_t;


/**
 * Registers preprocessor directives in a preprocessor
 *
 * @param pp preprocessor to operate on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directives_init(preprocessor_t *pp);

/**
 * Destroys preprocessor directive structures in a preprocessor
 *
 * @param pp preprocessor to operate on
 */
void pp_directives_destroy(preprocessor_t *pp);

/**
 * Directive for #include
 *
 * @param pp The preprocessor to include for
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_include(preprocessor_t *pp);

/**
 * Directive for #include_next
 *
 * @param pp The preprocessor to include for
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_include_next(preprocessor_t *pp);

/**
 * Helper function for include and include next
 *
 * @param pp The preprocessor to include for
 * @param next. If true, treats the call as include next
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_include_helper(preprocessor_t *pp, bool next);

/**
 * Directive for #define
 *
 * @param pp The preprocessor to define for
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_define(preprocessor_t *pp);

/**
 * Helper function to create a macro
 *
 * @param stream Stream to use
 * @param new_macro Location to store new macro
 * @param is_cli_param If true, its a macro defined with the -D flag
 */
status_t pp_directive_define_helper(tstream_t *stream, pp_macro_t **new_macro,
                                    bool is_cli_param, bool *in_comment);

/**
 * Directive for #undef
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_undef(preprocessor_t *pp);

/**
 * Directive for #ifdef
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_ifdef(preprocessor_t *pp);

/**
 * Directive for #ifndef
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_ifndef(preprocessor_t *pp);

/**
 * Helper function for #ifndef and #ifdef
 *
 * @param pp The preprocessor act on
 * @param directive Name of the directive
 * @param ifdef. If true, process as ifdef, else processess as ifndef
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_ifdef_helper(preprocessor_t *pp, const char *directive,
                                   bool ifdef);

/**
 * Directive for #if
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_if(preprocessor_t *pp);

/**
 * Directive for #elif
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_elif(preprocessor_t *pp);

/**
 * Helper function for #elif and #if
 *
 * @param pp The preprocessor act on
 * @param directive Name of the directive
 * @param is_if true if #if directive, false if #elif directive
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_if_helper(preprocessor_t *pp, const char *directive,
                                bool is_if);

/**
 * Directive for #else
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_else(preprocessor_t *pp);

/**
 * Directive for #endif
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_endif(preprocessor_t *pp);

/**
 * Directive for #error
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_error(preprocessor_t *pp);

/**
 * Directive for #warning
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_warning(preprocessor_t *pp);

/**
 * Helper function for error and warning
 *
 * @param pp The preprocessor act on
 * @param is_err if true, is error, else warning
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_error_helper(preprocessor_t *pp, bool is_err);

/**
 * Directive for #pragma
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_pragma(preprocessor_t *pp);

#define PRAGMA_POUND 0 /* #pragma */
#define PRAGMA_UNDER 1 /* _Pragma */

/**
 * Helper function for pragmas
 *
 * @param pp The preprocessor act on
 * @param pragma type, the type of pragma to process.
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_pragma_helper(preprocessor_t *pp, int pragma_type);

/**
 * Directive for #line
 *
 * @param pp The preprocessor act on
 * @return CCC_OK on success, error code on error
 */
status_t pp_directive_line(preprocessor_t *pp);


#endif /* _PP_DIRECTIVE_H_ */
