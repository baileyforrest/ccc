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

#ifndef _TRANS_PRIV_H
#define _TRANS_PRIV_H

#include "trans.h"

#include "ir/ir.h"

typedef struct trans_state_t {
    typetab_t *typetab;
    trans_unit_t *ast_tunit;
    ir_trans_unit_t *tunit;
    ir_type_t *va_type;
    ir_gdecl_t *func;
    ir_label_t *break_target;
    ir_label_t *continue_target;
    int break_count;
    bool in_switch;
    bool branch_next_labeled;
    bool ignore_until_label;
    bool cur_case_jumps;
} trans_state_t;

#define TRANS_STATE_LIT { NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, false, \
            false, false, false }

void trans_add_stmt(trans_state_t *ts, ir_inst_stream_t *stream,
                    ir_stmt_t *stmt);

ir_label_t *trans_label_create(trans_state_t *ts, char *str);

ir_label_t *trans_numlabel_create(trans_state_t *ts);

ir_expr_t *trans_temp_create(trans_state_t *ts, ir_type_t *type);

ir_expr_t *trans_assign_temp(trans_state_t *ts, ir_inst_stream_t *stream,
                             ir_expr_t *expr);

ir_expr_t *trans_load_temp(trans_state_t *ts, ir_inst_stream_t *stream,
                           ir_expr_t *expr);

ir_trans_unit_t *trans_trans_unit(trans_state_t *ts, trans_unit_t *ast);

void trans_gdecl_node(trans_state_t *ts, decl_node_t *node);

void trans_gdecl(trans_state_t *ts, gdecl_t *gdecl, slist_t *ir_gdecls);

// Returns true if the statement always jumps, false otherwise
bool trans_stmt(trans_state_t *ts, stmt_t *stmt, ir_inst_stream_t *ir_stmts);

ir_expr_t *trans_expr(trans_state_t *ts, bool addrof, expr_t *expr,
                      ir_inst_stream_t *ir_stmts);

ir_expr_t *trans_expr_bool(trans_state_t *ts, ir_expr_t *expr,
                           ir_inst_stream_t *ir_stmts);

ir_expr_t *trans_binop(trans_state_t *ts, expr_t *left, ir_expr_t *left_addr,
                       expr_t *right, oper_t op, type_t *type,
                       ir_inst_stream_t *ir_stmts, ir_expr_t **left_loc);

ir_expr_t *trans_unaryop(trans_state_t *ts, bool addrof, expr_t *expr,
                         ir_inst_stream_t *ir_stmts);


ir_expr_t *trans_type_conversion(trans_state_t *ts, type_t *dest, type_t *src,
                                 ir_expr_t *src_expr,
                                 ir_inst_stream_t *ir_stmts);

ir_expr_t *trans_ir_type_conversion(trans_state_t *ts, ir_type_t *dest_type,
                                    bool dest_signed, ir_type_t *src_type,
                                    bool src_signed, ir_expr_t *src_expr,
                                    ir_inst_stream_t *ir_stmts);


ir_expr_t *trans_assign(trans_state_t *ts, ir_expr_t *dest_ptr,
                        type_t *dest_type, ir_expr_t *src, type_t *src_type,
                        ir_inst_stream_t *ir_stmts);

char *trans_decl_node_name(ir_symtab_t *symtab, char *name);

typedef enum ir_decl_node_type_t {
    IR_DECL_NODE_GLOBAL,
    IR_DECL_NODE_LOCAL,
    IR_DECL_NODE_FDEFN,
    IR_DECL_NODE_FUNC_PARAM,
} ir_decl_node_type_t;

ir_type_t *trans_decl_node(trans_state_t *ts, decl_node_t *node,
                           ir_decl_node_type_t type, void *context);

// If val is NULL, then a "zero" value will be substituted instead
void trans_initializer(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                       type_t *ast_type, ir_type_t *ir_type, ir_expr_t *addr,
                       expr_t *val);

ir_type_t *trans_type(trans_state_t *ts, type_t *type);

ir_oper_t trans_op(oper_t op);

ir_expr_t *trans_create_anon_global(trans_state_t *ts, ir_type_t *type,
                                    ir_expr_t *init, size_t align,
                                    ir_linkage_t linkage,
                                    ir_gdata_flags_t flags);

ir_expr_t *trans_string(trans_state_t *ts, char *str);

ir_expr_t *trans_array_init(trans_state_t *ts, expr_t *expr);

ir_expr_t *trans_union_init(trans_state_t *ts, type_t *type, expr_t *expr);

ir_expr_t *trans_struct_init(trans_state_t *ts, expr_t *expr);

void trans_struct_init_helper(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                              type_t *ast_type, ir_type_t *ir_type,
                              ir_expr_t *addr, expr_t *val, ir_type_t *ptr_type,
                              sl_link_t **cur_expr, size_t offset);

bool trans_struct_mem_offset(trans_state_t *ts, type_t *type, char *mem_name,
                             slist_t *indexs);

ir_expr_t *trans_compound_literal(trans_state_t *ts, bool addrof,
                                  ir_inst_stream_t *ir_stmts,
                                  expr_t *expr);


#endif /* _TRANS_PRIV_H */