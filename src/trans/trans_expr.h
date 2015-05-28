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
 * Expression translator functions
 */

#ifndef _TRANS_EXPR_H_
#define _TRANS_EXPR_H_

#include "trans_priv.h"

ir_expr_t *trans_expr(trans_state_t *ts, bool addrof, expr_t *expr,
                      ir_inst_stream_t *ir_stmts);

ir_expr_t *trans_assign(trans_state_t *ts, ir_expr_t *dest_ptr,
                        type_t *dest_type, ir_expr_t *src, type_t *src_type,
                        ir_inst_stream_t *ir_stmts);

ir_expr_t *trans_expr_bool(trans_state_t *ts, ir_expr_t *expr,
                           ir_inst_stream_t *ir_stmts);

ir_expr_t *trans_binop(trans_state_t *ts, expr_t *left, ir_expr_t *left_addr,
                       expr_t *right, oper_t op, type_t *type,
                       ir_inst_stream_t *ir_stmts, ir_expr_t **left_loc);

ir_expr_t *trans_unaryop(trans_state_t *ts, bool addrof, expr_t *expr,
                         ir_inst_stream_t *ir_stmts);

#endif /* _TRANS_EXPR_H_ */
