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
typedef struct tc_state {
    typetab_t *typetab; /*< Type table on top of the stack */
} tc_state;

/**
 * Typechecks a trans_unit_t.
 *
 * @param tras_unit Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_trans_unit(tc_state *tcs, trans_unit_t *trans_unit);

/**
 * Typechecks a gdecl_t.
 *
 * @param gdecl Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_gdecl(tc_state *tcs, gdecl_t *gdecl);

/**
 * Typechecks a stmt_t.
 *
 * @param stmt Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_stmt(tc_state *tcs, stmt_t *stmt);

/**
 * Typechecks a decl_t.
 *
 * @param decl Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_decl(tc_state *tcs, decl_t *decl);

/**
 * Typechecks a decl_node_t.
 *
 * @param decl_node Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_decl_node(tc_state *tcs, decl_node_t *decl_node);

/**
 * Typechecks a expr_t.
 *
 * @param expr Object to typecheck
 * @param constant if TC_CONST, make sure the expression is constant.
 * @return true if the node type checks, false otherwise
 */
bool typecheck_expr(tc_state *tcs, expr_t *expr, bool constant);

/**
 * Typechecks a type_t that is not protected.
 *
 * @param type Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_type(tc_state *tcs, type_t *type);

#endif /* _TYPECHECKER_PRIV_H_ */
