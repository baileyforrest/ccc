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
 * LLVM intrinsic translator implementations
 */

#ifndef _TRANS_INTRINSIC_H_
#define _TRANS_INTRINSIC_H_

#include "trans/trans_priv.h"

ir_symtab_entry_t *trans_intrinsic_register(trans_state_t *ts,
                                            ir_type_t *func_type,
                                            char *func_name);

ir_expr_t *trans_intrinsic_call(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                                ir_symtab_entry_t *func);

void trans_memcpy(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                  ir_expr_t *dest, ir_expr_t *src, size_t len,
                  size_t align, bool isvolatile);

void trans_va_start(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                    expr_t *va_list);

#endif /* _TRANS_INTRINSIC_H_ */
