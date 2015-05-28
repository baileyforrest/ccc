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

#include "trans_intrinsic.h"

#include "trans_expr.h"
#include "trans_type.h"

#define LLVM_MEMCPY "llvm.memcpy.p0i8.p0i8.i64"
#define LLVM_VA_START "llvm.va_start"

ir_symtab_entry_t *trans_intrinsic_register(trans_state_t *ts,
                                            ir_type_t *func_type,
                                            char *func_name) {
    ir_expr_t *var_expr = ir_expr_create(ts->tunit, IR_EXPR_VAR);
    var_expr->var.type = func_type;
    var_expr->var.name = func_name;
    var_expr->var.local = false;

    ir_symtab_entry_t *func =
        ir_symtab_entry_create(IR_SYMTAB_ENTRY_VAR, var_expr->var.name);

    func->var.expr = var_expr;
    func->var.access = var_expr;

    status_t status = ir_symtab_insert(&ts->tunit->globals, func);
    assert(status == CCC_OK);

    // Add the declaration
    ir_gdecl_t *ir_gdecl = ir_gdecl_create(IR_GDECL_FUNC_DECL);
    ir_gdecl->func_decl.type = func_type;
    ir_gdecl->func_decl.name = func_name;
    sl_append(&ts->tunit->decls, &ir_gdecl->link);

    return func;
}

ir_expr_t *trans_intrinsic_call(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                                ir_symtab_entry_t *func) {
    ir_expr_t *func_expr = func->var.access;

    ir_expr_t *call = ir_expr_create(ts->tunit, IR_EXPR_CALL);
    call->call.func_sig = ir_expr_type(func_expr);
    call->call.func_ptr = func_expr;

    ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_EXPR);
    stmt->expr = call;
    trans_add_stmt(ts, ir_stmts, stmt);

    return call;
}

void trans_memcpy(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                  ir_expr_t *dest, ir_expr_t *src, size_t len,
                  size_t align, bool isvolatile) {
    ir_expr_t *dest_ptr = trans_ir_type_conversion(ts, &ir_type_i8_ptr, false,
                                                   ir_expr_type(dest), false,
                                                   dest, ir_stmts);
    ir_expr_t *src_ptr = trans_ir_type_conversion(ts, &ir_type_i8_ptr, false,
                                                  ir_expr_type(src), false,
                                                  src, ir_stmts);

    ir_expr_t *len_expr = ir_int_const(ts->tunit, &ir_type_i64, len);
    ir_expr_t *align_expr = ir_int_const(ts->tunit, &ir_type_i32, align);

    ir_expr_t *volatile_expr = ir_int_const(ts->tunit, &ir_type_i1, isvolatile);

    char *func_name = LLVM_MEMCPY;
    ir_symtab_entry_t *func = ir_symtab_lookup(&ts->tunit->globals, func_name);

    // Lazily create the function declaration and object
    if (func == NULL) {
        ir_type_t *func_type = ir_type_create(ts->tunit, IR_TYPE_FUNC);
        func_type->func.type = &ir_type_void;
        func_type->func.varargs = false;
        vec_push_back(&func_type->func.params, &ir_type_i8_ptr);
        vec_push_back(&func_type->func.params, &ir_type_i8_ptr);
        vec_push_back(&func_type->func.params, &ir_type_i64);
        vec_push_back(&func_type->func.params, &ir_type_i32);
        vec_push_back(&func_type->func.params, &ir_type_i1);

        func = trans_intrinsic_register(ts, func_type, func_name);
    }
    assert(func->type == IR_SYMTAB_ENTRY_VAR);

    ir_expr_t *call = trans_intrinsic_call(ts, ir_stmts, func);

    sl_append(&call->call.arglist, &dest_ptr->link);
    sl_append(&call->call.arglist, &src_ptr->link);
    sl_append(&call->call.arglist, &len_expr->link);
    sl_append(&call->call.arglist, &align_expr->link);
    sl_append(&call->call.arglist, &volatile_expr->link);
}

void trans_va_start(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                    expr_t *va_list) {
    ir_expr_t *ir_expr = trans_expr(ts, true, va_list, ir_stmts);
    ir_expr = trans_ir_type_conversion(ts, &ir_type_i8_ptr, false,
                                       ir_expr_type(ir_expr), false,
                                       ir_expr, ir_stmts);
    char *func_name = LLVM_VA_START;

    // Lazily create the function declaration and object
    ir_symtab_entry_t *func = ir_symtab_lookup(&ts->tunit->globals, func_name);
    if (func == NULL) {
        ir_type_t *func_type = ir_type_create(ts->tunit, IR_TYPE_FUNC);
        func_type->func.type = &ir_type_void;
        func_type->func.varargs = false;
        vec_push_back(&func_type->func.params, &ir_type_i8_ptr);

        func = trans_intrinsic_register(ts, func_type, func_name);
    }
    assert(func->type == IR_SYMTAB_ENTRY_VAR);

    ir_expr_t *call = trans_intrinsic_call(ts, ir_stmts, func);

    sl_append(&call->call.arglist, &ir_expr->link);
}
