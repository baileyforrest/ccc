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

#define MAX_GLOBAL_NAME 128

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

ir_expr_t *trans_create_anon_global(trans_state_t *ts, ir_type_t *type,
                                    ir_expr_t *init, size_t align,
                                    ir_linkage_t linkage,
                                    ir_gdata_flags_t flags);

bool trans_struct_mem_offset(trans_state_t *ts, type_t *type, char *mem_name,
                             slist_t *indexs);



ir_trans_unit_t *trans_trans_unit(trans_state_t *ts, trans_unit_t *ast);

void trans_gdecl(trans_state_t *ts, gdecl_t *gdecl, slist_t *ir_gdecls);

// Returns true if the statement always jumps, false otherwise
bool trans_stmt(trans_state_t *ts, stmt_t *stmt, ir_inst_stream_t *ir_stmts);

#endif /* _TRANS_PRIV_H */
