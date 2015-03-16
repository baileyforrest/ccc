/*
 * Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>
 *
 * This file is part of CCC.
 *
 * CCC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CCC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CCC.  If not, see <http://www.gnu.org/licenses/>.
 */
/**
 * AST private interface
 */

#ifndef _AST_PRIV_H_
#define _AST_PRIV_H_

void ast_trans_unit_print(trans_unit_t *tras_unit);
void ast_gdecl_print(gdecl_t *gdecl);
void ast_stmt_print(stmt_t *stmt, int indent);
void ast_decl_print(decl_t *decl);
void ast_decl_node_print(decl_node_t *decl_node, type_t *type);
void ast_expr_print(expr_t *expr);
void ast_oper_print(oper_t op);
void ast_type_print(type_t *type);
void ast_type_mod_print(type_mod_t type_mod);
void ast_enum_id_print(enum_id_t *enum_id);
void ast_struct_decl_print(struct_decl_t *struct_decl);

#endif /* _AST_PRIV_H_ */
