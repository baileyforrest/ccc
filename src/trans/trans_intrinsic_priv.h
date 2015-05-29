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

#ifndef _TRANS_INTRINSIC_PRIV_H_
#define _TRANS_INTRINSIC_PRIV_H_

#include "trans_intrinsic.h"

#define LLVM_MEMCPY "llvm.memcpy.p0i8.p0i8.i64"
#define LLVM_VA_START "llvm.va_start"
#define LLVM_VA_END "llvm.va_end"
#define LLVM_VA_COPY "llvm.va_copy"

ir_symtab_entry_t *trans_intrinsic_register(trans_state_t *ts,
                                            ir_type_t *func_type,
                                            char *func_name);

ir_expr_t *trans_intrinsic_call(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                                ir_symtab_entry_t *func);

void trans_va_start_end_helper(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                               ir_expr_t *va_list, char *func_name);

#endif /* _TRANS_INTRINSIC_PRIV_H_ */
