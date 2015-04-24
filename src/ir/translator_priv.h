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
 * AST to IR translator private interface
 */

#ifndef _TRANSLATOR_PRIV_H
#define _TRANSLATOR_PRIV_H

#include "ir/ir.h"

typedef struct trans_state_t {
    typetab_t *typetab;
    ir_trans_unit_t *tunit;
    ir_gdecl_t *func;
    ir_label_t *break_target;
    ir_label_t *continue_target;
} trans_state_t;

#define TRANS_STATE_LIT { NULL, NULL, NULL, NULL, NULL }

ir_label_t *trans_label_create(trans_state_t *ts, char *str);

ir_label_t *trans_numlabel_create(trans_state_t *ts);

ir_expr_t *trans_temp_create(trans_state_t *ts, ir_type_t *type);

ir_trans_unit_t *trans_trans_unit(trans_state_t *ts, trans_unit_t *ast);

void trans_gdecl(trans_state_t *ts, gdecl_t *gdecl, slist_t *ir_gdecls);

void trans_stmt(trans_state_t *ts, stmt_t *stmt, slist_t *ir_stmts);

ir_expr_t *trans_expr(trans_state_t *ts, bool addrof, expr_t *expr,
                      slist_t *ir_stmts);

ir_expr_t *trans_expr_bool(trans_state_t *ts, ir_expr_t *expr,
                           slist_t *ir_stmts);

ir_expr_t *trans_binop(trans_state_t *ts, expr_t *left, expr_t *right,
                       oper_t op, type_t *type, slist_t *ir_stmts,
                       ir_expr_t **left_loc);

ir_expr_t *trans_unaryop(trans_state_t *ts, expr_t *expr, slist_t *ir_stmts);


ir_expr_t *trans_type_conversion(trans_state_t *ts, type_t *dest, type_t *src,
                                 ir_expr_t *src_expr, slist_t *ir_stmts);

ir_expr_t *trans_assign(trans_state_t *ts, expr_t *dest, ir_expr_t *src,
                        slist_t *ir_stmts);

void trans_decl_node(trans_state_t *ts, decl_node_t *node, slist_t *ir_stmts);

ir_type_t *trans_type(trans_state_t *ts, type_t *type);

ir_oper_t trans_op(oper_t op);

#endif /* _TRANSLATOR_PRIV_H */
