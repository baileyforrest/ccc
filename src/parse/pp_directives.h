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
} pp_directive_t;


/**
 * Registers preprocessor directives in a preprocessor
 *
 * @param pp preprocessor to operate on
 */
status_t pp_directives_init(preprocessor_t *pp);

/**
 * Destroys preprocessor directive structures in a preprocessor
 *
 * @param pp preprocessor to operate on
 */
void pp_directives_destroy(preprocessor_t *pp);

/**
 * Directive for #define
 * @param pp The preprocessor to define for
 */
status_t pp_directive_define(preprocessor_t *pp);

/**
 * Directive for #include
 *
 * Warning: Current version uses static memory so is not reentrant
 *
 * @param pp The preprocessor to include for
 */
status_t pp_directive_include(preprocessor_t *pp);

/**
 * Directive for #ifndef
 * @param pp The preprocessor act on
 */
status_t pp_directive_ifndef(preprocessor_t *pp);

/**
 * Directive for #endif
 * @param pp The preprocessor act on
 */
status_t pp_directive_endif(preprocessor_t *pp);

#endif /* _PP_DIRECTIVE_H_ */
