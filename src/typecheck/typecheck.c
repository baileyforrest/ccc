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
 * Type checker implementation
 */

#include "typecheck.h"
#include "typecheck_priv.h"

bool typecheck_ast(trans_unit_t *tunit) {
    // TODO: Implement this
    (void)tunit;
    return false;
}

bool typecheck_trans_unit(trans_unit_t *trans_unit);

bool typecheck_gdecl(gdecl_t *gdecl);

bool typecheck_stmt(stmt_t *stmt);

bool typecheck_decl(decl_t *decl);

bool typecheck_decl_node(decl_node_t *decl_node);

bool typecheck_expr(expr_t *expr);

bool typecheck_type(type_t *type);
