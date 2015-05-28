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
 * Type translation functions
 */

#ifndef _TRANS_TYPE_H_
#define _TRANS_TYPE_H_

#include "trans_priv.h"

ir_expr_t *trans_type_conversion(trans_state_t *ts, type_t *dest, type_t *src,
                                 ir_expr_t *src_expr,
                                 ir_inst_stream_t *ir_stmts);

ir_expr_t *trans_ir_type_conversion(trans_state_t *ts, ir_type_t *dest_type,
                                    bool dest_signed, ir_type_t *src_type,
                                    bool src_signed, ir_expr_t *src_expr,
                                    ir_inst_stream_t *ir_stmts);

ir_type_t *trans_type(trans_state_t *ts, type_t *type);

#endif /* _TRANS_TYPE_H_ */
