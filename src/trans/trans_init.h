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
 * Translator functions for complicated literals
 */

#ifndef _TRANS_INIT_H_
#define _TRANS_INIT_H_

#include "trans_priv.h"

// If val is NULL, then a "zero" value will be substituted instead
void trans_initializer(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                       type_t *ast_type, ir_type_t *ir_type, ir_expr_t *addr,
                       expr_t *val);

ir_expr_t *trans_string(trans_state_t *ts, char *str);

ir_expr_t *trans_array_init(trans_state_t *ts, expr_t *expr);

ir_expr_t *trans_union_init(trans_state_t *ts, type_t *type, expr_t *expr);

ir_expr_t *trans_struct_init(trans_state_t *ts, expr_t *expr);

void trans_struct_init_helper(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                              type_t *ast_type, ir_type_t *ir_type,
                              ir_expr_t *addr, expr_t *val, ir_type_t *ptr_type,
                              sl_link_t **cur_expr, size_t offset);

ir_expr_t *trans_compound_literal(trans_state_t *ts, bool addrof,
                                  ir_inst_stream_t *ir_stmts,
                                  expr_t *expr);

#endif /* _TRANS_INIT_H_ */
