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
 * Interface for compliation manager
 */

#ifndef _MANAGER_H_
#define _MANAGER_H_

#include "parse/ast.h"
#include "parse/lexer.h"
#include "parse/symtab.h"

#include "util/status.h"

/**
 * Compilation manager structure. Manages data structures necessary for compiles
 */
typedef struct manager_t {
    preprocessor_t pp;
    symtab_t symtab;
    symtab_t string_tab;
    lexer_t lexer;
} manager_t;

/**
 * Initialize a compilation mananger
 *
 * @param manager The compilation mananger to initialize
 * @param macros If non NULL, manager's preprocessor is initialized with
 *     given macros. It is then used under the assumption that macros will not
 *     change during use, and that manager will not change macros
 * @return CCC_OK on success, error code on error.
 */
status_t man_init(manager_t *manager, htable_t *macros);

/**
 * Destroy a compilation mananger
 *
 * @param manager The compilation mananger to destroy
 */
void man_destroy(manager_t *manager);

/**
 * Parse a translation unit from a compilation manager.
 *
 * The manager's preprocessor must be set up first
 *
 * @param manager The compilation mananger to parse
 * @param ast The parsed ast
 * @return CCC_OK on success, error code on error.
 */
status_t man_parse(manager_t *manager, trans_unit_t **ast);

/**
 * Parse an expression from a compilation manager.
 *
 * The manager's preprocessor must be set up first
 *
 * @param manager The compilation mananger to parse
 * @param expr The parsed expression
 * @return CCC_OK on success, error code on error.
 */
status_t man_parse_expr(manager_t *manager, expr_t **expr);

#endif /* _MANAGER_H_ */