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

#include "trans_expr.h"

#include <limits.h>

#include "trans_decl.h"
#include "trans_intrinsic.h"
#include "trans_init.h"
#include "trans_type.h"

#include "typecheck/typecheck.h"

ir_expr_t *trans_expr(trans_state_t *ts, bool addrof, expr_t *expr,
                      ir_inst_stream_t *ir_stmts) {
    switch (expr->type) {
    case EXPR_VOID:
        return NULL;
    case EXPR_PAREN:
        return trans_expr(ts, addrof, expr->paren_base, ir_stmts);
    case EXPR_VAR: {
        typetab_t *tt = ts->typetab;
        typetab_entry_t *tt_ent;
        do {
            assert(tt != NULL);
            tt_ent = tt_lookup(tt, expr->var_id);
            assert(tt_ent != NULL);

            // Search through scopes until we find this expression's entry
            if ((tt_ent->entry_type == TT_VAR ||
                 tt_ent->entry_type == TT_ENUM_ID)) {
                if (tt_ent->type == expr->etype) {
                    break;
                }

                // Function expressions evaluate to function pointers
                if (expr->type == EXPR_VAR && tt_ent->type->type == TYPE_FUNC) {
                    assert(expr->etype->type == TYPE_PTR);
                    if (expr->etype->ptr.base == tt_ent->type ||
                        expr->etype == tt_implicit_func_ptr) {
                        break;
                    }
                }
            }

            tt = tt->last;
        } while(true);

        // If this is an enum id, just return the integer value
        if (tt_ent->entry_type == TT_ENUM_ID) {
            return ir_int_const(ts->tunit, trans_type(ts, tt_ent->type),
                                tt_ent->enum_val);
        }
        ir_symtab_entry_t *entry = tt_ent->var.ir_entry;

        // Lazily add used global variables to declaration list
        if (entry == NULL) {
            ht_ptr_elem_t *elem = ht_lookup(&ts->tunit->global_decls,
                                            &expr->var_id);
            assert(elem != NULL);
            trans_gdecl_node(ts, elem->val);
            entry = tt_ent->var.ir_entry;
        }

        // Must be valid if typechecked
        assert(entry != NULL && entry->type == IR_SYMTAB_ENTRY_VAR);

        ir_type_t *entry_type = ir_expr_type(entry->var.access);

        if (entry_type->type == IR_TYPE_PTR) {
            if (addrof || entry_type->ptr.base->type == IR_TYPE_FUNC) {
                // If we're taking address of variable, just return it
                // Also can't dereference a function
                return entry->var.access;
            }

            return trans_load_temp(ts, ir_stmts, entry->var.access);
        } else {
            if (addrof) { // Can't take address of register variable
                assert(false);
            }
            return entry->var.access;
        }
    }
    case EXPR_ASSIGN: {
        bool bitfield = false;
        expr_t *mem_acc;
        if (expr->assign.dest->type == EXPR_MEM_ACC) {
            mem_acc = expr->assign.dest;
            type_t *compound = ast_type_unmod(mem_acc->mem_acc.base->etype);
            if (compound->type == TYPE_PTR) {
                compound = ast_type_unmod(compound->ptr.base);
            }
            decl_node_t *node = ast_type_find_member(compound,
                                                     mem_acc->mem_acc.name,
                                                     NULL, NULL);
            assert(node != NULL);
            if (node->expr != NULL) {
                bitfield = true;
            }
        }
        ir_expr_t *dest_addr;
        if (bitfield) {
            bool addrof = mem_acc->mem_acc.op == OP_DOT;
            dest_addr = trans_expr(ts, addrof, mem_acc->mem_acc.base, ir_stmts);
        } else {
            dest_addr = trans_expr(ts, true, expr->assign.dest, ir_stmts);
        }

        type_t *src_type;
        ir_expr_t *val;
        if (expr->assign.op == OP_NOP) {
            val = trans_expr(ts, false, expr->assign.expr, ir_stmts);
            src_type = expr->assign.expr->etype;
        } else {
            bool result = typecheck_type_max(ts->ast_tunit, NULL,
                                             expr->assign.expr->etype,
                                             expr->etype, &src_type);
            assert(result && src_type != NULL);
            ir_expr_t *dest;
            ir_expr_t *op_expr = trans_binop(ts, expr->assign.dest, dest_addr,
                                             expr->assign.expr, expr->assign.op,
                                             src_type, ir_stmts, &dest);

            val  = trans_assign_temp(ts, ir_stmts, op_expr);
        }

        if (bitfield) {
            return trans_bitfield_helper(ts, ir_stmts,
                                         mem_acc->mem_acc.base->etype,
                                         mem_acc->mem_acc.name, dest_addr, val);
        } else {
            return trans_assign(ts, dest_addr, expr->assign.dest->etype, val,
                                src_type, ir_stmts);
        }
    }
    case EXPR_CONST_INT:
        return ir_int_const(ts->tunit, trans_type(ts, expr->const_val.type),
                            expr->const_val.int_val);

    case EXPR_CONST_FLOAT: {
        ir_expr_t *ir_expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_expr->const_params.ctype = IR_CONST_FLOAT;
        ir_expr->const_params.type = trans_type(ts, expr->const_val.type);
        ir_expr->const_params.float_val = expr->const_val.float_val;
        return ir_expr;
    }
    case EXPR_CONST_STR: {
        ir_expr_t *ir_expr = trans_string(ts, expr->const_val.str_val);
        return trans_assign_temp(ts, ir_stmts, ir_expr);
    }

    case EXPR_BIN: {
        ir_expr_t *op_expr = trans_binop(ts, expr->bin.expr1, NULL,
                                         expr->bin.expr2, expr->bin.op,
                                         expr->etype, ir_stmts, NULL);
        return trans_assign_temp(ts, ir_stmts, op_expr);
    }
    case EXPR_UNARY:
        return trans_unaryop(ts, addrof, expr, ir_stmts);

    case EXPR_COND: {
        ir_type_t *type = trans_type(ts, expr->etype);
        ir_expr_t *expr1 = trans_expr(ts, false, expr->cond.expr1, ir_stmts);
        ir_label_t *if_true = trans_numlabel_create(ts);
        ir_label_t *if_false = trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);

        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = trans_expr_bool(ts, expr1, ir_stmts);
        ir_stmt->br.if_true = if_true;
        ir_stmt->br.if_false = if_false;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // True branch
        // Label
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = if_true;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Expression
        ir_expr_t *expr2 = trans_expr(ts, false, expr->cond.expr2, ir_stmts);
        expr2 = trans_type_conversion(ts, expr->etype, expr->cond.expr2->etype,
                                      expr2, ir_stmts);

        // Set true source to last created label
        if_true = ts->func->func.last_label;

        // Jump to after
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);


        // False branch
        // Label
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = if_false;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Expression
        ir_expr_t *expr3 = trans_expr(ts, false, expr->cond.expr3, ir_stmts);
        expr3 = trans_type_conversion(ts, expr->etype, expr->cond.expr3->etype,
                                      expr3, ir_stmts);

        // Set false source to last created label
        if_false = ts->func->func.last_label;

        // Jump to after
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // End
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        ir_expr_t *phi = ir_expr_create(ts->tunit, IR_EXPR_PHI);
        phi->phi.type = type;

        ir_expr_label_pair_t *pred = emalloc(sizeof(*pred));
        pred->expr = expr2;
        pred->label = if_true;
        sl_append(&phi->phi.preds, &pred->link);

        pred = emalloc(sizeof(*pred));
        pred->expr = expr3;
        pred->label = if_false;
        sl_append(&phi->phi.preds, &pred->link);

        return trans_assign_temp(ts, ir_stmts, phi);
    }
    case EXPR_CAST: {
        if (expr->cast.base->type == EXPR_INIT_LIST) {
            return trans_compound_literal(ts, addrof, ir_stmts,
                                          expr->cast.base);
        } else {
            ir_expr_t *src_expr = trans_expr(ts, false, expr->cast.base,
                                             ir_stmts);
            return trans_type_conversion(ts, expr->etype,
                                         expr->cast.base->etype, src_expr,
                                         ir_stmts);
        }
    }
    case EXPR_CALL: {
        ir_expr_t *call = ir_expr_create(ts->tunit, IR_EXPR_CALL);
        type_t *func_sig = expr->call.func->etype;
        if (func_sig->type == TYPE_PTR) {
            func_sig = func_sig->ptr.base;
        }
        assert(func_sig->type == TYPE_FUNC);

        call->call.func_sig = trans_type(ts, func_sig);
        call->call.func_ptr = trans_expr(ts, false, expr->call.func, ir_stmts);

        bool oldstyle = false;
        if (sl_head(&func_sig->func.params) == NULL) {
            // Handle K & R style func sig
            oldstyle = true;

            // If this is a K & R style call for a function that was specified
            // later in this tunit, we need to set up the cast correctly
            if (expr->call.func->type == EXPR_VAR) {
                typetab_entry_t *entry = tt_lookup(ts->typetab,
                                                   expr->call.func->var_id);
                if (entry->type->type == TYPE_FUNC) {
                    call->call.func_sig = trans_type(ts, entry->type);
                }
            }

            ir_type_t *new_func_sig = ir_type_create(ts->tunit, IR_TYPE_FUNC);
            new_func_sig->func.type = call->call.func_sig->func.type;
            new_func_sig->func.varargs = true;

            ir_type_t *ptr_dest = ir_type_create(ts->tunit, IR_TYPE_PTR);
            ptr_dest->ptr.base = new_func_sig;

            ir_type_t *ptr_src = ir_type_create(ts->tunit, IR_TYPE_PTR);
            ptr_src->ptr.base = call->call.func_sig;

            ir_expr_t *convert = ir_expr_create(ts->tunit, IR_EXPR_CONVERT);
            convert->convert.type = IR_CONVERT_BITCAST;
            convert->convert.src_type = ptr_src;
            convert->convert.dest_type = ptr_dest;
            convert->convert.val = call->call.func_ptr;

            call->call.func_sig = new_func_sig;
            call->call.func_ptr = trans_assign_temp(ts, ir_stmts, convert);
        }

        sl_link_t *cur_sig = func_sig->func.params.head;
        sl_link_t *cur_expr = expr->call.params.head;
        while (cur_sig != NULL) {
            decl_t *decl = GET_ELEM(&func_sig->func.params, cur_sig);
            decl_node_t *node = sl_head(&decl->decls);
            type_t *sig_type = node == NULL ? decl->type : node->type;
            assert(cur_expr != NULL);

            expr_t *param = GET_ELEM(&expr->call.params, cur_expr);
            ir_expr_t *ir_expr = trans_expr(ts, false, param, ir_stmts);
            ir_expr = trans_type_conversion(ts, sig_type, param->etype, ir_expr,
                                            ir_stmts);

            // If the function parameter is aggregate, we must load it
            // we can't use trans_load_temp because it purposely doesn't load
            // aggregate types
            if (TYPE_IS_AGGREGATE(ast_type_unmod(sig_type))) {
                ir_type_t *type = ir_expr_type(ir_expr);
                assert(type->type == IR_TYPE_PTR);

                ir_expr_t *load = ir_expr_create(ts->tunit, IR_EXPR_LOAD);
                load->load.type = type->ptr.base;
                load->load.ptr = ir_expr;

                ir_expr = trans_assign_temp(ts, ir_stmts, load);
            }
            sl_append(&call->call.arglist, &ir_expr->link);

            cur_sig = cur_sig->next;
            cur_expr = cur_expr->next;
        }
        if (func_sig->func.varargs || oldstyle) {
            while (cur_expr != NULL) {
                expr_t *param = GET_ELEM(&expr->call.params, cur_expr);
                ir_expr_t *ir_expr = trans_expr(ts, false, param, ir_stmts);
                sl_append(&call->call.arglist, &ir_expr->link);

                if (oldstyle) {
                    vec_push_back(&call->call.func_sig->func.params,
                                  ir_expr_type(ir_expr));
                }

                cur_expr = cur_expr->next;
            }
        } else {
            assert(cur_expr == NULL);
        }

        ir_type_t *return_type = call->call.func_sig->func.type;
        ir_expr_t *result;
        // Void returning function, don't create a temp
        if (return_type->type == IR_TYPE_VOID) {
            ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_EXPR);
            ir_stmt->expr = call;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
            result = NULL;
        } else {
            result = trans_assign_temp(ts, ir_stmts, call);
        }

        return result;
    }
    case EXPR_CMPD: {
        ir_expr_t *ir_expr = NULL;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            expr_t *cur_expr = GET_ELEM(&expr->cmpd.exprs, cur);
            ir_expr = trans_expr(ts, false, cur_expr, ir_stmts);
        }
        return ir_expr;
    }
    case EXPR_SIZEOF: {
        long long val;
        if (expr->sizeof_params.type != NULL) {
            decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
            if (node != NULL) {
                val = ast_type_size(node->type);
            } else {
                assert(node == sl_tail(&expr->sizeof_params.type->decls));
                val = ast_type_size(expr->sizeof_params.type->type);
            }
        } else {
            expr_t *sz_expr = expr->sizeof_params.expr;
            assert(sz_expr != NULL);

            if (sz_expr->type == EXPR_VAR) {
                // Function's etypes are function pointers, but the size
                // is not a pointer size
                typetab_entry_t *entry = tt_lookup(ts->typetab,
                                                   sz_expr->var_id);
                assert(entry != NULL);
                val = ast_type_size(entry->type);
            } else {
                val = ast_type_size(sz_expr->etype);
            }
        }

        return ir_int_const(ts->tunit, trans_type(ts, expr->etype), val);
    }
    case EXPR_ALIGNOF: {
        long long val;
        if (expr->sizeof_params.type != NULL) {
            decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
            if (node != NULL) {
                val = ast_type_align(node->type);
            } else {
                assert(node == sl_tail(&expr->sizeof_params.type->decls));
                val = ast_type_align(expr->sizeof_params.type->type);
            }
        } else {
            assert(expr->sizeof_params.expr != NULL);
            val = ast_type_align(expr->sizeof_params.expr->etype);
        }

        return ir_int_const(ts->tunit, trans_type(ts, expr->etype), val);
    }
    case EXPR_OFFSETOF: {
        size_t offset = ast_type_offset(expr->offsetof_params.type->type,
                                        &expr->offsetof_params.path);
        return ir_int_const(ts->tunit, trans_type(ts, expr->etype), offset);
    }
    case EXPR_ARR_IDX:
    case EXPR_MEM_ACC: {
        ir_type_t *expr_type = trans_type(ts, expr->etype);
        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = expr_type;

        type_t *base_type;
        if (expr->type == EXPR_MEM_ACC) {
            base_type = ast_type_unmod(expr->mem_acc.base->etype);
        }

        ir_expr_t *pointer;
        // Handle bitfields
        if (ast_is_mem_acc_bitfield(expr)) {
            pointer = trans_expr(ts, false, expr->mem_acc.base, ir_stmts);
            return trans_bitfield_helper(ts, ir_stmts,
                                         expr->mem_acc.base->etype,
                                         expr->mem_acc.name, pointer, NULL);
        }

        // Handle unions
        if (expr->type == EXPR_MEM_ACC &&
            ((expr->mem_acc.op == OP_DOT && base_type->type == TYPE_UNION) ||
             (expr->mem_acc.op == OP_ARROW &&
              ast_type_unmod(base_type->ptr.base)->type == TYPE_UNION))) {

            pointer = trans_expr(ts, false, expr->mem_acc.base, ir_stmts);
            pointer = trans_ir_type_conversion(ts, ptr_type, false,
                                               ir_expr_type(pointer), false,
                                               pointer, ir_stmts);
            if (addrof) {
                return pointer;
            }
            return trans_load_temp(ts, ir_stmts, pointer);
        }

        ir_expr_t *elem_ptr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
        elem_ptr->getelemptr.type = ptr_type;

        bool is_union = false;
        bool last_array = false;
        while ((expr->type == EXPR_MEM_ACC && expr->mem_acc.op == OP_DOT)
            || expr->type == EXPR_ARR_IDX) {
            if (expr->type == EXPR_MEM_ACC) {
                if (expr->mem_acc.base->etype->type == TYPE_UNION) {
                    is_union = true;
                    break;
                }
                trans_struct_mem_offset(ts, expr->mem_acc.base->etype,
                                        expr->mem_acc.name,
                                        &elem_ptr->getelemptr.idxs);
                expr = expr->mem_acc.base;
            } else { // expr->type == EXPR_ARR_IDX
                type_t *arr_type = ast_type_unmod(expr->arr_idx.array->etype);

                ir_expr_t *index = trans_expr(ts, false, expr->arr_idx.index,
                                              ir_stmts);
                index = trans_type_conversion(ts, tt_size_t,
                                              expr->arr_idx.index->etype,
                                              index, ir_stmts);
                sl_prepend(&elem_ptr->getelemptr.idxs, &index->link);
                expr = expr->arr_idx.array;

                // If this is a pointer instead of an array, stop here because
                // we need to do a load for the next index
                if (arr_type->type == TYPE_PTR ||
                    (arr_type->type == TYPE_ARR && arr_type->arr.nelems == 0)) {
                    last_array = true;
                    break;
                }
            }
        }

        type_t *etype;
        if (expr->type == EXPR_MEM_ACC && expr->mem_acc.op == OP_ARROW) {
            etype = ast_type_unmod(expr->mem_acc.base->etype);
            assert(etype->type == TYPE_PTR);
            if (ast_type_unmod(etype->ptr.base)->type == TYPE_UNION) {
                is_union = true;
            }
        }

        bool prepend_zero = false;
        if (!last_array && !is_union && expr->type == EXPR_MEM_ACC) {
            assert(expr->mem_acc.op == OP_ARROW);
            trans_struct_mem_offset(ts, etype->ptr.base, expr->mem_acc.name,
                                    &elem_ptr->getelemptr.idxs);

            pointer = trans_expr(ts, false, expr->mem_acc.base, ir_stmts);
            prepend_zero = true;
        } else { // !is_arr_idx && expr->type != EXPR_MEM_ACC
            if (is_union) {
                pointer = trans_expr(ts, true, expr, ir_stmts);
            } else {
                pointer = trans_expr(ts, false, expr, ir_stmts);
            }
            ir_type_t *ptr_type = ir_expr_type(pointer);
            if (!last_array && ptr_type->type == IR_TYPE_PTR) {
                ir_type_t *base_type = ptr_type->ptr.base;

                switch (base_type->type) {
                case IR_TYPE_STRUCT:
                case IR_TYPE_ID_STRUCT:
                case IR_TYPE_ARR:
                    prepend_zero = true;
                default:
                    break;
                }
            }
        }
        if (prepend_zero) {
            ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i32);
            sl_prepend(&elem_ptr->getelemptr.idxs, &zero->link);
        }
        elem_ptr->getelemptr.ptr_type = ir_expr_type(pointer);
        elem_ptr->getelemptr.ptr_val = pointer;

        ir_expr_t *ptr = trans_assign_temp(ts, ir_stmts, elem_ptr);

        if (addrof) {
            return ptr;
        }

        return trans_load_temp(ts, ir_stmts, ptr);
    }
    case EXPR_INIT_LIST: {
        type_t *etype = ast_type_unmod(expr->etype);
        switch (etype->type) {
        case TYPE_UNION:
            return trans_union_init(ts, expr->etype, expr);
        case TYPE_STRUCT:
            return trans_struct_init(ts, expr);
        case TYPE_ARR:
            return trans_array_init(ts, expr);
        default: {
            // Just take the first element and tranlate it
            expr_t *head = sl_head(&expr->init_list.exprs);
            assert(head != NULL);
            return trans_expr(ts, false, head, ir_stmts);
        }
        }
        break;
    }
    case EXPR_VA_START: {
        ir_expr_t *ir_expr = trans_expr(ts, false, expr->vastart.ap, ir_stmts);
        trans_va_start(ts, ir_stmts, ir_expr);
        return NULL;
    }
    case EXPR_VA_END: {
        ir_expr_t *ir_expr = trans_expr(ts, false, expr->vaend.ap, ir_stmts);
        trans_va_end(ts, ir_stmts, ir_expr);
        return NULL;
    }
    case EXPR_VA_COPY: {
        ir_expr_t *dest = trans_expr(ts, false, expr->vacopy.dest, ir_stmts);
        ir_expr_t *src = trans_expr(ts, false, expr->vacopy.src, ir_stmts);
        trans_va_copy(ts, ir_stmts, dest, src);
        return NULL;
    }
    case EXPR_VA_ARG: {
        ir_expr_t *ap = trans_expr(ts, false, expr->vaarg.ap, ir_stmts);
        ap = trans_ir_type_conversion(ts, &ir_type_i8_ptr, false,
                                      ir_expr_type(ap), false,
                                      ap, ir_stmts);
        type_t *ast_type = DECL_TYPE(expr->vaarg.type);
        ir_type_t *type = trans_type(ts, ast_type);
        ir_expr_t *result = ir_expr_create(ts->tunit, IR_EXPR_VAARG);
        result->vaarg.va_list = ap;
        result->vaarg.arg_type = type;
        return trans_assign_temp(ts, ir_stmts, result);
    }
    case EXPR_DESIG_INIT:
    default:
        assert(false);
    }
    return NULL;
}

ir_expr_t *trans_assign(trans_state_t *ts, ir_expr_t *dest_ptr,
                        type_t *dest_type, ir_expr_t *src, type_t *src_type,
                        ir_inst_stream_t *ir_stmts) {
    src_type = ast_type_untypedef(src_type);
    if (src_type->type == TYPE_STRUCT || src_type->type == TYPE_UNION) {
        trans_memcpy(ts, ir_stmts, dest_ptr, src, ast_type_size(src_type),
                     ast_type_align(src_type), false);
        return src;
    }

    ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_STORE);
    ir_stmt->store.type = trans_type(ts, dest_type);
    ir_stmt->store.val = trans_type_conversion(ts, dest_type, src_type, src,
                                               ir_stmts);
    ir_stmt->store.ptr = dest_ptr;
    trans_add_stmt(ts, ir_stmts, ir_stmt);
    return src;
}

// I hate bitfields!
ir_expr_t *trans_bitfield_helper(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                                 type_t *type, char *field_name,
                                 ir_expr_t *addr, ir_expr_t *val) {
    ir_type_t *ir_type = trans_type(ts, type);
    if (ir_type->type == IR_TYPE_ID_STRUCT) {
        ir_type = ir_type->id_struct.type;
    }
    assert(ir_type->type == IR_TYPE_STRUCT);

    bool assign = val != NULL;

    int mem_num = 0;
    size_t bitfield_offset = 0;
    ssize_t bits_total = -1;
    decl_node_t *node = NULL;

    struct_iter_t iter;
    struct_iter_init(type, &iter);
    do {
        if (iter.node != NULL) {
            if (iter.node->expr != NULL) {
                assert(iter.node->expr->type == EXPR_CONST_INT);
                size_t bf_bits = iter.node->expr->const_val.int_val;

                if (iter.node->id != NULL && strcmp(iter.node->id, field_name)
                    == 0) {
                    node = iter.node;
                    bits_total = bf_bits;
                    break;
                }

                if (bf_bits == 0) {
                    int remain = bitfield_offset % CHAR_BIT;
                    if (remain != 0) {
                        bitfield_offset += CHAR_BIT - remain;
                    }
                } else {
                    bitfield_offset += bf_bits;
                }
            } else {
                // Skip members with no id or bitfield specified
                if (iter.node->id == NULL) {
                    continue;
                }
                bitfield_offset = 0;
                ++mem_num;
            }
        }

        if (iter.node == NULL && iter.decl != NULL &&
            (iter.decl->type->type == TYPE_STRUCT ||
             iter.decl->type->type == TYPE_UNION)) {
            ++mem_num;
            bitfield_offset = 0;
        }
    } while (struct_iter_advance(&iter));

    assert(node != NULL); // Must have found the member if typechecked
    ir_type_t *node_type = trans_type(ts, node->type);

    ir_type_t *ir_arr_type = vec_get(&ir_type->struct_params.types, mem_num);
    assert(ir_arr_type->type == IR_TYPE_ARR);

    ir_expr_t *bf_arr_addr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
    bf_arr_addr->getelemptr.type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    bf_arr_addr->getelemptr.type->ptr.base = ir_arr_type;
    bf_arr_addr->getelemptr.ptr_type = ir_expr_type(addr);
    bf_arr_addr->getelemptr.ptr_val = addr;

    ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i32);
    sl_append(&bf_arr_addr->getelemptr.idxs, &zero->link); // Get structure
    ir_expr_t *offset = ir_int_const(ts->tunit, &ir_type_i32, mem_num);
    sl_append(&bf_arr_addr->getelemptr.idxs, &offset->link); // Get bf array

    bf_arr_addr = trans_assign_temp(ts, ir_stmts, bf_arr_addr);

    size_t arr_idx = bitfield_offset / CHAR_BIT;
    bitfield_offset %= CHAR_BIT;
    ssize_t bit_offset = 0;

    while (bit_offset < bits_total) {
        int bits = CHAR_BIT;
        int mask = 0;
        int upto = bit_offset + CHAR_BIT;

        if (bitfield_offset != 0) { // Mask away lower bits
            mask |= (1 << bitfield_offset) - 1;
            bits -= bitfield_offset;
            upto = CHAR_BIT - bitfield_offset;
        }
        if (upto > bits_total) { // Mask away upper bits
            mask |= ((1 << (upto - bits_total)) - 1)
                << (bits_total - bit_offset + bitfield_offset);
            bits -= upto - bits_total - bitfield_offset;
        }

        ir_expr_t *cur_addr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
        cur_addr->getelemptr.type = &ir_type_i8_ptr;
        cur_addr->getelemptr.ptr_type = ir_expr_type(bf_arr_addr);
        cur_addr->getelemptr.ptr_val = bf_arr_addr;

        ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i32);
        sl_append(&cur_addr->getelemptr.idxs, &zero->link); // Get array

        ir_expr_t *idx = ir_int_const(ts->tunit, &ir_type_i32, arr_idx);
        sl_append(&cur_addr->getelemptr.idxs, &idx->link); // Get index

        cur_addr = trans_assign_temp(ts, ir_stmts, cur_addr);

        if (assign) {
            // Get the upto 8 bit value from between [bit_offset, until_bit)
            ir_expr_t *val_shifted;
            int shift = 0;
            if (bitfield_offset > 0) {
                shift = bitfield_offset;
                bitfield_offset = 0;
            } else {
                shift = -bit_offset;
            }
            if (shift == 0) {
                val_shifted = val;
            } else {
                val_shifted = ir_expr_create(ts->tunit, IR_EXPR_BINOP);
                val_shifted->binop.op = shift > 0 ? IR_OP_SHL : IR_OP_LSHR;
                val_shifted->binop.type = ir_expr_type(val);
                val_shifted->binop.expr1 = val;
                val_shifted->binop.expr2 = ir_int_const(ts->tunit,
                                                        ir_expr_type(val),
                                                        abs(shift));
                val_shifted = trans_assign_temp(ts, ir_stmts, val_shifted);
            }

            ir_expr_t *val_masked;
            if (mask == 0) {
                val_masked = val_shifted;
            } else {
                val_masked = ir_expr_create(ts->tunit, IR_EXPR_BINOP);
                val_masked->binop.op = IR_OP_AND;
                val_masked->binop.type = ir_expr_type(val);
                val_masked->binop.expr1 = val_shifted;
                val_masked->binop.expr2 = ir_int_const(ts->tunit,
                                                       ir_expr_type(val),
                                                       ~mask);
                val_masked = trans_assign_temp(ts, ir_stmts, val_masked);
            }

            // Make a copy if its a constant, because trans_ir_type_conversion
            // mutates constant val, and we need val to be the same for future
            // iterations
            if (val_masked->type == IR_EXPR_CONST) {
                assert(val_masked->const_params.ctype == IR_CONST_INT);
                val_masked = ir_int_const(ts->tunit, ir_expr_type(val_masked),
                                          val_masked->const_params.int_val);
            }
            val_masked = trans_ir_type_conversion(ts, &ir_type_i8, false,
                                                  ir_expr_type(val_masked),
                                                  false, val_masked, ir_stmts);

            ir_expr_t *store_val;
            if (mask == 0) { // Storing whole byte, just store masked value
                store_val = val_masked;
            } else {
                ir_expr_t *load_val = trans_load_temp(ts, ir_stmts, cur_addr);

                ir_expr_t *load_masked = ir_expr_create(ts->tunit,
                                                        IR_EXPR_BINOP);
                load_masked->binop.op = IR_OP_AND;
                load_masked->binop.type = ir_expr_type(load_val);
                load_masked->binop.expr1 = load_val;
                load_masked->binop.expr2 = ir_int_const(ts->tunit,
                                                        &ir_type_i8, mask);
                load_masked = trans_assign_temp(ts, ir_stmts, load_masked);

                store_val = ir_expr_create(ts->tunit, IR_EXPR_BINOP);
                store_val->binop.op = IR_OP_OR;
                store_val->binop.type = &ir_type_i8;
                store_val->binop.expr1 = load_masked;
                store_val->binop.expr2 = val_masked;
                store_val = trans_assign_temp(ts, ir_stmts, store_val);
            }

            ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_STORE);
            ir_stmt->store.type = &ir_type_i8;
            ir_stmt->store.val = store_val;
            ir_stmt->store.ptr = cur_addr;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        } else { // Load, not assign
            ir_expr_t *load_val = trans_load_temp(ts, ir_stmts, cur_addr);

            ir_expr_t *load_masked;
            if (mask == 0) {
                load_masked = load_val;
            } else {
                load_masked = ir_expr_create(ts->tunit, IR_EXPR_BINOP);
                load_masked->binop.op = IR_OP_AND;
                load_masked->binop.type = ir_expr_type(load_val);
                load_masked->binop.expr1 = load_val;
                load_masked->binop.expr2 = ir_int_const(ts->tunit,
                                                        &ir_type_i8, ~mask);
                load_masked = trans_assign_temp(ts, ir_stmts, load_masked);
            }

            load_masked = trans_ir_type_conversion(ts, node_type, false,
                                                   &ir_type_i8, false,
                                                   load_masked, ir_stmts);

            int shift = 0;
            if (bitfield_offset > 0) {
                shift = -bitfield_offset;
                bitfield_offset = 0;
            } else {
                shift = bit_offset;
            }

            ir_expr_t *load_shifted;
            if (shift == 0) {
                load_shifted = load_masked;
            } else {
                load_shifted = ir_expr_create(ts->tunit, IR_EXPR_BINOP);
                load_shifted->binop.op = shift > 0 ? IR_OP_SHL : IR_OP_LSHR;
                load_shifted->binop.type = node_type;
                load_shifted->binop.expr1 = load_masked;
                load_shifted->binop.expr2 = ir_int_const(ts->tunit,
                                                         node_type,
                                                         abs(shift));
                load_shifted = trans_assign_temp(ts, ir_stmts, load_shifted);
            }

            if (val == NULL) {
                val = load_shifted;
            } else {
                ir_expr_t *new_val = ir_expr_create(ts->tunit, IR_EXPR_BINOP);
                new_val->binop.op = IR_OP_OR;
                new_val->binop.type = ir_expr_type(val);
                new_val->binop.expr1 = val;
                new_val->binop.expr2 = load_shifted;

                val = trans_assign_temp(ts, ir_stmts, new_val);
            }
        }

        bit_offset += bits;
        ++arr_idx;
    }

    return val;
}

ir_expr_t *trans_expr_bool(trans_state_t *ts, ir_expr_t *expr,
                           ir_inst_stream_t *ir_stmts) {
    ir_type_t *type = ir_expr_type(expr);

    // Do nothing if expression is already the right type
    if (type->type == IR_TYPE_INT && type->int_params.width == 1) {
        return expr;
    }

    // If this is a constant, just change it into the boolean type
    if (expr->type == IR_EXPR_CONST) {
        bool is_true;
        switch (expr->const_params.ctype) {
        case IR_CONST_NULL:  is_true = false; break;
        case IR_CONST_STR:   is_true = true;  break;
        case IR_CONST_ZERO:  is_true = false;  break;
        case IR_CONST_INT:   is_true = expr->const_params.int_val != 0; break;
        case IR_CONST_FLOAT:
            is_true = expr->const_params.float_val != 0.0;
            break;

        case IR_CONST_STRUCT:
        case IR_CONST_ARR:
        case IR_CONST_UNDEF:
        default:
            assert(false);
            is_true = false;
        }

        expr->const_params.ctype = IR_CONST_INT;
        expr->const_params.type = &ir_type_i1;
        expr->const_params.int_val = is_true;

        return expr;
    }

    bool is_float = type->type == IR_TYPE_FLOAT;

    ir_expr_t *cmp;
    ir_expr_t *zero = ir_expr_zero(ts->tunit, type);
    if (is_float) {
        cmp = ir_expr_create(ts->tunit, IR_EXPR_FCMP);
        cmp->fcmp.cond = IR_FCMP_ONE;
        cmp->fcmp.type = type;
        cmp->fcmp.expr1 = expr;
        cmp->fcmp.expr2 = zero;
    } else {
        cmp = ir_expr_create(ts->tunit, IR_EXPR_ICMP);
        cmp->icmp.cond = IR_ICMP_NE;
        cmp->icmp.type = type;
        cmp->icmp.expr1 = expr;
        cmp->icmp.expr2 = zero;
    }

    return trans_assign_temp(ts, ir_stmts, cmp);
}

ir_expr_t *trans_binop(trans_state_t *ts, expr_t *left, ir_expr_t *left_addr,
                       expr_t *right, oper_t op, type_t *type,
                       ir_inst_stream_t *ir_stmts, ir_expr_t **left_loc) {
    type = ast_type_untypedef(type);
    bool is_float = false;
    bool is_signed = false;
    bool is_cmp = false;
    bool is_ptr = false;
    int cmp_type;
    switch (type->type) {

    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
        is_signed = true;
        break;

    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
        is_float = true;
        break;

    case TYPE_MOD:
        if (!TYPE_IS_UNSIGNED(type)) {
            is_signed = true;
        }
        break;

    case TYPE_PTR:
        is_ptr = true;
        break;

    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PAREN:
    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_ENUM:
    case TYPE_TYPEDEF:
    default:
        assert(false);
    }

    if (is_ptr) {
        // Can only get a result pointer as addition of pointer with int
        assert(op == OP_PLUS);
        expr_t *ptr_expr;
        expr_t *int_expr;
        if (TYPE_IS_PTR(ast_type_unmod(left->etype))) {
            assert(TYPE_IS_INTEGRAL(ast_type_unmod(right->etype)));
            ptr_expr = left;
            int_expr = right;
        } else {
            assert(TYPE_IS_PTR(ast_type_unmod(right->etype)));
            assert(TYPE_IS_INTEGRAL(ast_type_unmod(left->etype)));
            ptr_expr = right;
            int_expr = left;
        }

        // Just treat p + x as &p[x]
        expr_t *arr_idx = ast_expr_create(ts->ast_tunit, ptr_expr->mark,
                                          EXPR_ARR_IDX);
        arr_idx->arr_idx.array = ptr_expr;
        arr_idx->arr_idx.index = int_expr;

        // Set etype to ptr's base so that getelemptr's type is correctly set
        arr_idx->etype = type->ptr.base;

        return trans_expr(ts, true, arr_idx, ir_stmts);
    }

    ir_oper_t ir_op;
    switch (op) {
    case OP_TIMES:    ir_op = is_float ? IR_OP_FMUL : IR_OP_MUL; break;
    case OP_PLUS:     ir_op = is_float ? IR_OP_FADD : IR_OP_ADD; break;
    case OP_MINUS:    ir_op = is_float ? IR_OP_FSUB : IR_OP_SUB; break;
    case OP_DIV:
        ir_op = is_float ? IR_OP_FDIV : is_signed ? IR_OP_SDIV : IR_OP_UDIV;
        break;
    case OP_MOD:
        assert(!is_float);
        ir_op = is_signed ? IR_OP_SREM : IR_OP_UREM;
        break;
    case OP_LSHIFT:
        assert(!is_float);
        ir_op = IR_OP_SHL;
        break;
    case OP_RSHIFT:
        assert(!is_float);
        ir_op = is_signed ? IR_OP_ASHR : IR_OP_LSHR;
        break;

    case OP_BITAND:   ir_op = IR_OP_AND; break;
    case OP_BITXOR:   ir_op = IR_OP_XOR; break;
    case OP_BITOR:    ir_op = IR_OP_OR; break;

    case OP_LT:
    case OP_GT:
    case OP_LE:
    case OP_GE:
    case OP_EQ:
    case OP_NE:
        is_cmp = true;
        break;
    case OP_LOGICAND:
    case OP_LOGICOR: {
        bool is_and = op == OP_LOGICAND;

        ir_label_t *right_label = trans_numlabel_create(ts);
        ir_label_t *done = trans_numlabel_create(ts);

        // Create left expression
        ir_expr_t *left_expr = trans_expr(ts, false, left, ir_stmts);
        ir_expr_t *ir_expr = trans_expr_bool(ts, left_expr, ir_stmts);

        // Must be after translating first expression
        ir_label_t *cur_block = ts->func->func.last_label;

        // First branch
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = ir_expr;
        if (is_and) {
            ir_stmt->br.if_true = right_label;
            ir_stmt->br.if_false = done;
        } else {
            ir_stmt->br.if_true = done;
            ir_stmt->br.if_false = right_label;
        }
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Right side
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = right_label;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        ir_expr = trans_expr(ts, false, right, ir_stmts);
        ir_expr_t *right_val = trans_expr_bool(ts, ir_expr, ir_stmts);

        // Set right source to last created label
        right_label = ts->func->func.last_label;

        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = done;
        trans_add_stmt(ts, ir_stmts, ir_stmt);


        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = done;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        ir_expr = ir_expr_create(ts->tunit, IR_EXPR_PHI);
        ir_expr->phi.type = &ir_type_i1;

        ir_expr_label_pair_t *pred = emalloc(sizeof(*pred));
        if (is_and) {
            pred->expr = ir_int_const(ts->tunit, &ir_type_i1, 0);
        } else {
            pred->expr = ir_int_const(ts->tunit, &ir_type_i1, 1);
        }
        pred->label = cur_block;
        sl_append(&ir_expr->phi.preds, &pred->link);

        pred = emalloc(sizeof(*pred));
        pred->expr = right_val;
        pred->label = right_label;
        sl_append(&ir_expr->phi.preds, &pred->link);

        ir_expr = trans_assign_temp(ts, ir_stmts, ir_expr);

        // Result of comparison is an int (C11 S 6.8.5)
        ir_expr = trans_ir_type_conversion(ts, trans_type(ts, tt_int), false,
                                           &ir_type_i1, false, ir_expr,
                                           ir_stmts);

        if (left_loc != NULL) {
            *left_loc = ir_expr;
        }
        return ir_expr;
    }
    default:
        assert(false);
    }

    // Comparisons need to be handled separately
    if (is_cmp) {
        type_t *max_type;
        bool success = typecheck_type_max(ts->ast_tunit, NULL, left->etype,
                                          right->etype, &max_type);
        // Must be valid if typechecked
        assert(success && max_type != NULL);
        is_float = TYPE_IS_FLOAT(max_type);
        is_signed = !TYPE_IS_UNSIGNED(max_type);

        switch (op) {
        case OP_LT:
            cmp_type = is_float ? IR_FCMP_OLT :
                is_signed ? IR_ICMP_SLT : IR_ICMP_ULT;
            break;
        case OP_GT:
            cmp_type = is_float ? IR_FCMP_OGT :
                is_signed ? IR_ICMP_SGT : IR_ICMP_UGT;
            break;
        case OP_LE:
            cmp_type = is_float ? IR_FCMP_OLE :
                is_signed ? IR_ICMP_SLE : IR_ICMP_ULE;
            break;
        case OP_GE:
            cmp_type = is_float ? IR_FCMP_OGE :
                is_signed ? IR_ICMP_SGE : IR_ICMP_UGE;
            break;
        case OP_EQ:
            cmp_type = is_float ? IR_FCMP_OEQ : IR_ICMP_EQ;
            break;
        case OP_NE:
            cmp_type = is_float ? IR_FCMP_ONE : IR_ICMP_NE;
            break;
        default:
            assert(false);
        }
        ir_expr_t *cmp;

        ir_expr_t *left_expr = trans_expr(ts, false, left, ir_stmts);
        left_expr = trans_type_conversion(ts, max_type, left->etype, left_expr,
                                          ir_stmts);
        ir_expr_t *right_expr = trans_expr(ts, false, right, ir_stmts);
        right_expr = trans_type_conversion(ts, max_type, right->etype,
                                           right_expr, ir_stmts);
        if (is_float) {
            cmp = ir_expr_create(ts->tunit, IR_EXPR_FCMP);
            cmp->fcmp.cond = cmp_type;
            cmp->fcmp.expr1 = left_expr;
            cmp->fcmp.expr2 = right_expr;
            cmp->fcmp.type = trans_type(ts, max_type);
        } else {
            cmp = ir_expr_create(ts->tunit, IR_EXPR_ICMP);
            cmp->icmp.cond = cmp_type;
            cmp->icmp.expr1 = left_expr;
            cmp->icmp.expr2 = right_expr;
            cmp->icmp.type = trans_type(ts, max_type);
        }
        if (left_loc != NULL) {
            *left_loc = left_expr;
        }

        ir_expr_t *result = trans_assign_temp(ts, ir_stmts, cmp);

        // Result of comparison is an int (C11 S 6.8.5)
        result = trans_ir_type_conversion(ts, trans_type(ts, tt_int), false,
                                          &ir_type_i1, false, result, ir_stmts);
        return result;
    }

    // Basic bin op case
    ir_expr_t *op_expr = ir_expr_create(ts->tunit, IR_EXPR_BINOP);
    op_expr->binop.op = ir_op;
    op_expr->binop.type = trans_type(ts, type);

    // Evaluate the types and convert types if necessary
    ir_expr_t *left_expr;
    if (left_addr == NULL) {
        left_expr = trans_expr(ts, false, left, ir_stmts);
        ir_expr_t *right_expr = trans_expr(ts, false, right, ir_stmts);
        op_expr->binop.expr2 = trans_type_conversion(ts, type, right->etype,
                                                     right_expr, ir_stmts);
    } else {
        // Evaluate right first in clase left_addr's value is affected by right
        ir_expr_t *right_expr = trans_expr(ts, false, right, ir_stmts);
        op_expr->binop.expr2 = trans_type_conversion(ts, type, right->etype,
                                                     right_expr, ir_stmts);
        left_expr = trans_load_temp(ts, ir_stmts, left_addr);
    }
    op_expr->binop.expr1 = trans_type_conversion(ts, type, left->etype,
                                                 left_expr, ir_stmts);
    if (left_loc != NULL) {
        *left_loc = left_expr;
    }
    return op_expr;
}

ir_expr_t *trans_unaryop(trans_state_t *ts, bool addrof, expr_t *expr,
                         ir_inst_stream_t *ir_stmts) {
    assert(expr->type == EXPR_UNARY);
    oper_t op = expr->unary.op;
    switch (op) {
    case OP_ADDR:
        return trans_expr(ts, true, expr->unary.expr, ir_stmts);

    case OP_PREINC:
    case OP_PREDEC:
    case OP_POSTINC:
    case OP_POSTDEC: {
        ir_expr_t *expr_addr = trans_expr(ts, true, expr->unary.expr, ir_stmts);

        // TODO1: This doesn't work for pointers
        ir_expr_t *ir_expr = trans_load_temp(ts, ir_stmts, expr_addr);
        ir_type_t *type = ir_expr_type(ir_expr);
        ir_expr_t *op_expr = ir_expr_create(ts->tunit, IR_EXPR_BINOP);

        switch (op) {
        case OP_PREINC:
        case OP_POSTINC: op_expr->binop.op = IR_OP_ADD; break;
        case OP_PREDEC:
        case OP_POSTDEC: op_expr->binop.op = IR_OP_SUB; break;
        default: assert(false);
        }
        ir_expr_t *other = ir_int_const(ts->tunit, type, 1);
        op_expr->binop.expr1 = ir_expr;
        op_expr->binop.expr2 = other;
        op_expr->binop.type = type;

        ir_expr_t *temp = trans_assign_temp(ts, ir_stmts, op_expr);
        trans_assign(ts, expr_addr, expr->unary.expr->etype, temp, expr->etype,
                     ir_stmts);

        switch (op) {
        case OP_PREINC:
        case OP_PREDEC:  return temp;
        case OP_POSTINC:
        case OP_POSTDEC: return ir_expr;
        default: break;
        }
        assert(false);
        return NULL;
    }
    default:
        break;
    }

    ir_expr_t *ir_expr = trans_expr(ts, false, expr->unary.expr, ir_stmts);
    switch (op) {
    case OP_UMINUS:
    case OP_UPLUS:
    case OP_BITNOT:
        ir_expr = trans_type_conversion(ts, expr->etype,
                                        expr->unary.expr->etype, ir_expr,
                                        ir_stmts);
        break;
    default:
        break;
    }
    ir_type_t *type = ir_expr_type(ir_expr);
    switch (op) {
    case OP_UPLUS:
        return ir_expr;

    case OP_DEREF: {
        assert(type->type == IR_TYPE_PTR);

        // Derefernce of a function pointer is unchanged
        if (type->ptr.base->type == IR_TYPE_FUNC) {
            return ir_expr;
        }
        type_t *ptr_type = ast_type_unmod(expr->unary.expr->etype);

        ir_type_t *base = type->ptr.base;

        if (ptr_type->type == TYPE_ARR && base->type == IR_TYPE_ARR) {
            // Arrays are refered to by pointers to the array

            // Get the actual base and cast the array type to the appropriate
            // pointer type
            base = base->arr.elem_type;

            ir_type_t *base_ptr = ir_type_create(ts->tunit, IR_TYPE_PTR);
            base_ptr->ptr.base = base;

            ir_expr = trans_ir_type_conversion(ts, base_ptr, false,
                                               ir_expr_type(ir_expr), false,
                                               ir_expr, ir_stmts);
        }

        if (addrof) {
            return ir_expr;
        }
        return trans_load_temp(ts, ir_stmts, ir_expr);
    }

    case OP_LOGICNOT:
        // Convert expression to bool, then do a bitwise not
        ir_expr = trans_expr_bool(ts, ir_expr, ir_stmts);
        type = ir_expr_type(ir_expr);
        op = OP_BITNOT;
        // FALL THROUGH
    case OP_BITNOT:
    case OP_UMINUS: {
        bool is_bnot = op == OP_BITNOT;
        ir_expr_t *op_expr = ir_expr_create(ts->tunit, IR_EXPR_BINOP);
        if (is_bnot) {
            assert(type->type == IR_TYPE_INT);
            op_expr->binop.op = IR_OP_XOR;
        } else {
            switch (type->type) {
            case IR_TYPE_INT:
                op_expr->binop.op = IR_OP_SUB;
                break;
            case IR_TYPE_FLOAT:
                op_expr->binop.op = IR_OP_FSUB;
                break;
            default:
                assert(false); // Nothing else is allowed for unary minus
                return NULL;
            }
        }
        ir_expr_t *other;
        if (is_bnot) {
            other = ir_int_const(ts->tunit, type, -1);
        } else {
            other = ir_int_const(ts->tunit, type, 0);
        }
        op_expr->binop.expr1 = other;
        op_expr->binop.expr2 = ir_expr;
        op_expr->binop.type = type;

        return trans_assign_temp(ts, ir_stmts, op_expr);
    }
    default:
        break;
    }
    assert(false);
    return NULL;
}
