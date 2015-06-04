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

#define PRINT_BUF_SIZE 4096

/**
 * Destroys a type_t that is not on a decl_node
 *
 * @param type Object to destroy
 */
void ast_type_destroy(type_t *type);

/**
 * Destroys a expr_t.
 *
 * @param expr Object to destroy
 */
void ast_expr_destroy(expr_t *expr);

/**
 * Destroys a decl_node_t.
 *
 * @param decl_node Object to destroy
 */
void ast_decl_node_destroy(decl_node_t *decl_node);

/**
 * Destroys a decl_t.
 *
 * @param decl Object to destroy
 */
void ast_decl_destroy(decl_t *decl);

/**
 * Destroys a stmt_t.
 *
 * @param stmt Object to destroy
 */
void ast_stmt_destroy(stmt_t *stmt);

/**
 * Destroys a gdecl_t.
 *
 * @param gdecl Object to destroy
 */
void ast_gdecl_destroy(gdecl_t *gdecl);

void ast_directed_print(char **buf, size_t *remain, const char *fmt, ...);

/**
 * Prints a trans_unit_t
 *
 * @param trans_unit trans_unit_t to print
 */
void ast_trans_unit_print(trans_unit_t *tras_unit);

/**
 * Prints a gdecl_t
 *
 * @param gdecl gdecl_t to print
 */
void ast_gdecl_print(gdecl_t *gdecl);

/**
 * Prints a stmt_t
 *
 * @param stmt stmt_t to print
 * @param indent Indentation to print this statement at
 */
void ast_stmt_print(stmt_t *stmt, int indent);

/**
 * Prints a decl_t
 *
 * @param decl decl_t to print
 * @param type if TYPE_STRUCT or TYPE_UNION will print decl_node expression as
 *     bit fields
 */
void ast_decl_print(decl_t *decl, type_type_t type, int indent, char **dest,
                    size_t *remain);

/**
 * Prints a decl_node_t
 *
 * @param decl_node decl_node_t to print
 */
void ast_decl_node_print(decl_node_t *decl_node, type_t *type, char **dest,
                         size_t *remain);

/**
 * Prints a expr_t
 *
 * @param expr expr_t to print
 */
void ast_expr_print(expr_t *expr, int indent, char **dest, size_t *remain);

void ast_designator_list_print(designator_list_t *list, bool nodot, char **dest,
                               size_t *remain);

/**
 * Prints a oper_t
 *
 * @param op oper_t to print
 */
void ast_oper_print(oper_t op, char **dest, size_t *remain);

/**
 * Prints a type_t
 *
 * @param type type_t to print
 */
void ast_type_print(type_t *type, int indent, char **dest, size_t *remain);

/**
 * Prints a type modifier mask
 *
 * @param type_mod Typemodifer mask to print
 */
void ast_type_mod_print(type_t *type, char **dest, size_t *remain);

#endif /* _AST_PRIV_H_ */
