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
 * Type checker interface
 */

#ifndef _TYPECHECKER_H_
#define _TYPECHECKER_H_

#include <stdbool.h>

#include "parse/ast.h"

/**
 * Typecheck an ast. Error and warnings will be sent to the logger.
 *
 * @param ast The AST to typecheck
 * @return true if the tranlation unit typechecks, false otherwise
 */
bool typecheck_ast(trans_unit_t *ast);

/**
 * Typecheck an expression and evaluate it as a constant expression. Error and
 * warnings will be sent to the logger.
 *
 * @param ast The AST to typecheck
 * @param result Location to store the result
 * @param If true, undefined variables are ignored
 * @return true if the tranlation unit typechecks, false otherwise
 */
bool typecheck_const_expr(expr_t *expr, long long *result, bool ignore_undef);

/**
 * Evaluates a given constant expression expression.
 *
 * Assumes that the expression is typechecked
 *
 * @param tcs Type checker context
 * @param expr The expression to evaluate
 * @param result Location to store the result
 */
void typecheck_const_expr_eval(typetab_t *typetab, expr_t *expr,
                               long long *result);

/**
 * Returns true if t1 is equivalent to t2
 *
 * @param t1 Type 1
 * @param t2 Type 2
 * @return true if the types are equal, false otherwise
 */
bool typecheck_type_equal(type_t *t1, type_t *t2);

/**
 * Verifies that t1 and t2 are compatible, and the returns the "higher" type of
 * the two.
 *
 * @param mark Location of usage. NULL if none, no errors will be reported
 * @param t1 Type 1
 * @param t2 Type 2
 * @param result The wider of t1 and t2 otherwise.
 *     NULL if they are not compatible
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_type_max(trans_unit_t *tunit, fmark_t *mark, type_t *t1,
                        type_t *t2, type_t **result);

#endif /* _TYPECHECKER_H_ */
