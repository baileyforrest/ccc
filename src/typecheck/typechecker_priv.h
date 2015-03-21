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
 * Type checker private interface
 */

#ifndef _TYPECHECKER_PRIV_H_
#define _TYPECHECKER_PRIV_H_

#include "typechecker.h"

#define TC_CONST true
#define TC_NOCONST false

/**
 * Container for type checking context
 */
typedef struct tc_state_t {
    typetab_t *typetab; /*< Type table on top of the stack */
    stmt_t *last_switch;
    stmt_t *last_loop;
    stmt_t *last_break;
    decl_t *last_func;
} tc_state_t;

/**
 * Returns true if type is integral, false otherwise
 *
 * @param type The type to check
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_type_integral(type_t *type);

/**
 * Returns true if type can be in a conditional, false otherwise
 *
 * @param type The type to check
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_type_conditional(type_t *type);

/**
 * Typechecks expression, and makes sure its an integral type
 *
 * @param tcs The typechecking state
 * @param expr The expression to check
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_expr_integral(tc_state_t *tcs, expr_t *expr);

/**
 * Typechecks expression, and makes sure its a conditional type
 *
 * @param tcs The typechecking state
 * @param expr The expression to check
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_expr_conditional(tc_state_t *tcs, expr_t *expr);

/**
 * Typechecks a trans_unit_t.
 *
 * @param tcs The typechecking state
 * @param tras_unit Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_trans_unit(tc_state_t *tcs, trans_unit_t *trans_unit);

/**
 * Typechecks a gdecl_t.
 *
 * @param tcs The typechecking state
 * @param gdecl Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_gdecl(tc_state_t *tcs, gdecl_t *gdecl);

/**
 * Typechecks a stmt_t.
 *
 * @param tcs The typechecking state
 * @param stmt Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_stmt(tc_state_t *tcs, stmt_t *stmt);

/**
 * Typechecks a decl_t.
 *
 * @param tcs The typechecking state
 * @param decl Object to typecheck
 * @param constant if TC_CONST, make sure the decl has constant expressions
 *     as assignments, and they are integers (for struct bit fields)
 * @return true if the node type checks, false otherwise
 */
bool typecheck_decl(tc_state_t *tcs, decl_t *decl, bool constant);

/**
 * Typechecks a decl_node_t.
 *
 * @param tcs The typechecking state
 * @param decl_node Object to typecheck
 * @param constant if TC_CONST, make sure the expression is constant and of type
 *     integer. This is for struct bitfields and enum values
 * @return true if the node type checks, false otherwise
 */
bool typecheck_decl_node(tc_state_t *tcs, decl_node_t *decl_node,
                         bool constant);

/**
 * Typechecks a expr_t.
 *
 * @param tcs The typechecking state
 * @param expr Object to typecheck
 * @param constant if TC_CONST, make sure the expression is constant.
 * @return true if the node type checks, false otherwise
 */
bool typecheck_expr(tc_state_t *tcs, expr_t *expr, bool constant);

/**
 * Typechecks a type_t that is not protected.
 *
 * @param tcs The typechecking state
 * @param type Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_type(tc_state_t *tcs, type_t *type);

#endif /* _TYPECHECKER_PRIV_H_ */
