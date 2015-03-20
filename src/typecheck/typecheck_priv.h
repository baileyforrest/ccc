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

#ifndef _TYPECHECK_PRIV_H_
#define _TYPECHECK_PRIV_H_

#include "typecheck.h"

/**
 * Typechecks a trans_unit_t.
 *
 * @param tras_unit Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_trans_unit(trans_unit_t *trans_unit);

/**
 * Typechecks a gdecl_t.
 *
 * @param gdecl Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_gdecl(gdecl_t *gdecl);

/**
 * Typechecks a stmt_t.
 *
 * @param stmt Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_stmt(stmt_t *stmt);

/**
 * Typechecks a decl_t.
 *
 * @param decl Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_decl(decl_t *decl);

/**
 * Typechecks a decl_node_t.
 *
 * @param decl_node Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_decl_node(decl_node_t *decl_node);

/**
 * Typechecks a expr_t.
 *
 * @param expr Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_expr(expr_t *expr);


/**
 * Typechecks a type_t that is not protected.
 *
 * @param type Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_type(type_t *type);

#endif /* _TYPECHECK_PRIV_H_ */
