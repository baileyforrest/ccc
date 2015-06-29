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

#include "trans_init.h"

#include <limits.h>

#include "trans_expr.h"
#include "trans_intrinsic.h"
#include "trans_type.h"

void trans_initializer(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                       type_t *ast_type, ir_type_t *ir_type, ir_expr_t *addr,
                       expr_t *val) {

    // Handle compound literal
    if (val != NULL && val->type == EXPR_CAST &&
        val->cast.base->type == EXPR_INIT_LIST) {
        val = val->cast.base;
    }

    switch (ast_type->type) {
    case TYPE_STRUCT:
        trans_initializer_struct(ts, ir_stmts, ast_type, ir_type, addr, val);
        break;
    case TYPE_ARR: {
        if (val != NULL && val->type == EXPR_CONST_STR) {
            assert(val->etype->type == TYPE_ARR);
            size_t len = val->etype->arr.nelems;
            ir_expr_t *string_expr = trans_string(ts, val->const_val.str_val);
            string_expr = trans_assign_temp(ts, ir_stmts, string_expr);
            trans_memcpy(ts, ir_stmts, addr, string_expr, len, 1, false);
            return;
        }
        assert(val == NULL || val->type == EXPR_INIT_LIST);
        assert(ir_type->type == IR_TYPE_ARR);

        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = ir_type;

        ir_type_t *elem_type = trans_type(ts, ast_type->arr.base);

        size_t nelem = 0;
        if (val != NULL) {
            VEC_FOREACH(cur, &val->init_list.exprs) {
                ir_expr_t *cur_addr = ir_expr_create(ts->tunit,
                                                     IR_EXPR_GETELEMPTR);
                cur_addr->getelemptr.type = ir_type->arr.elem_type;
                cur_addr->getelemptr.ptr_type = ptr_type;
                cur_addr->getelemptr.ptr_val = addr;

                // We need to 0's on getelemptr, one to get the array, another
                // to get the array index
                ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i64);
                sl_append(&cur_addr->getelemptr.idxs, &zero->link);

                zero = ir_int_const(ts->tunit, &ir_type_i64, nelem);
                sl_append(&cur_addr->getelemptr.idxs, &zero->link);

                cur_addr = trans_assign_temp(ts, ir_stmts, cur_addr);
                expr_t *elem = vec_get(&val->init_list.exprs, cur);
                trans_initializer(ts, ir_stmts, ast_type->arr.base,
                                  elem_type, cur_addr, elem);
                ++nelem;
                if (nelem == ir_type->arr.nelems) {
                    break;
                }
            }
        }

        // TODO2: Optimization, for trailing zeros make a loop
        for(; nelem < ir_type->arr.nelems; ++nelem) {
            ir_expr_t *cur_addr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
            cur_addr->getelemptr.type = ir_type->arr.elem_type;
            cur_addr->getelemptr.ptr_type = ptr_type;
            cur_addr->getelemptr.ptr_val = addr;

            // We need to 0's on getelemptr, one to get the array, another to
            // get the array index
            ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i64);
            sl_append(&cur_addr->getelemptr.idxs, &zero->link);

            zero = ir_int_const(ts->tunit, &ir_type_i64, nelem);
            sl_append(&cur_addr->getelemptr.idxs, &zero->link);

            cur_addr = trans_assign_temp(ts, ir_stmts, cur_addr);

            trans_initializer(ts, ir_stmts, ast_type->arr.base,
                              elem_type, cur_addr, NULL);
        }
        break;
    }
    case TYPE_UNION: {
        assert(val == NULL || val->type == EXPR_INIT_LIST);


        if (val != NULL && vec_size(&val->init_list.exprs) > 0) {
            val = vec_front(&val->init_list.exprs);
            type_t *dest_type = val->etype;

            ir_type_t *ir_dest_type = trans_type(ts, dest_type);
            ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
            ptr_type->ptr.base = ir_dest_type;

            addr = trans_ir_type_conversion(ts, ptr_type, false,
                                            ir_expr_type(addr), false,
                                            addr, ir_stmts);
            ast_type = dest_type;
            ir_type = ir_dest_type;
        }
        // FALL THROUGH
    }
    default: {
        ir_expr_t *ir_val = val == NULL ? ir_expr_zero(ts->tunit, ir_type) :
            trans_expr(ts, false, val, ir_stmts);
        ir_stmt_t *store = ir_stmt_create(ts->tunit, IR_STMT_STORE);
        store->store.type = ir_type;
        if (val == NULL) {
            store->store.val = ir_val;
        } else {
            store->store.val = trans_type_conversion(ts, ast_type, val->etype,
                                                     ir_val, ir_stmts);
        }
        store->store.ptr = addr;
        trans_add_stmt(ts, ir_stmts, store);
    }
    }
}

void trans_initializer_struct(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                              type_t *ast_type, ir_type_t *ir_type,
                              ir_expr_t *addr, expr_t *val) {
    assert(val == NULL || val->type == EXPR_INIT_LIST);
    assert(ir_type->type == IR_TYPE_STRUCT ||
           ir_type->type == IR_TYPE_ID_STRUCT);

    // Type for pointer to the structure
    ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    ptr_type->ptr.base = ir_type;

    ir_type_t *struct_type = ir_type->type == IR_TYPE_ID_STRUCT ?
        ir_type->id_struct.type : ir_type;

    // Offset into ir struct type's members
    size_t offset = 0;

    // Current init list expr
    vec_iter_t vec_iter = { &val->init_list.exprs, 0 };

    struct_iter_t iter;
    struct_iter_init(ast_type, &iter);
    do {
        type_t *cur_ast_type = NULL;

        if (iter.node != NULL && iter.node->id != NULL) {
            if (iter.node->expr != NULL) {
                // Handle bitfields
                ir_type_t *cur_type = trans_type(ts, iter.node->type);
                ir_expr_t *cur_val;
                if (val == NULL || !vec_iter_has_next(&vec_iter)) {
                    cur_val = ir_expr_zero(ts->tunit, cur_type);
                } else {
                    expr_t *elem = vec_iter_get(&vec_iter);
                    vec_iter_advance(&vec_iter);
                    cur_val = trans_expr(ts, false, elem, ir_stmts);
                }
                trans_bitfield_helper(ts, ir_stmts, ast_type, iter.node->id,
                                      addr, cur_val);
            } else {
                cur_ast_type = iter.node->type;
            }
        }

        // Anonymous struct/union
        if (iter.node == NULL && iter.decl != NULL &&
            (iter.decl->type->type == TYPE_STRUCT ||
             iter.decl->type->type == TYPE_UNION)) {
            cur_ast_type = iter.decl->type;
        }

        if (cur_ast_type != NULL) {
            ir_type_t *cur_type = vec_get(&struct_type->struct_params.types,
                                          offset);
            ir_type_t *p_cur_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
            p_cur_type->ptr.base = cur_type;

            ir_expr_t *cur_addr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
            cur_addr->getelemptr.type = p_cur_type;
            cur_addr->getelemptr.ptr_type = ptr_type;
            cur_addr->getelemptr.ptr_val = addr;

            // Point to the structure
            ir_expr_t *ptr_offset = ir_expr_zero(ts->tunit, &ir_type_i32);
            sl_append(&cur_addr->getelemptr.idxs, &ptr_offset->link);

            // Get member offset
            ptr_offset = ir_int_const(ts->tunit, &ir_type_i32, offset);
            sl_append(&cur_addr->getelemptr.idxs, &ptr_offset->link);

            cur_addr = trans_assign_temp(ts, ir_stmts, cur_addr);

            expr_t *elem = NULL;
            if (val != NULL && vec_iter_has_next(&vec_iter)) {
                elem = vec_iter_get(&vec_iter);
                vec_iter_advance(&vec_iter);
            }
            trans_initializer(ts, ir_stmts, cur_ast_type, cur_type, cur_addr,
                              elem);
            ++offset;
        }
    } while (struct_iter_advance(&iter));
}

ir_expr_t *trans_string(trans_state_t *ts, char *str) {
    ht_ptr_elem_t *elem = ht_lookup(&ts->tunit->strings, &str);
    if (elem != NULL) {
        return elem->val;
    }

    char *unescaped = unescape_str(str);

    ir_type_t *type = ir_type_create(ts->tunit, IR_TYPE_ARR);
    type->arr.nelems = strlen(unescaped) + 1;
    type->arr.elem_type = &ir_type_i8;
    ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    ptr_type->ptr.base = type;

    ir_expr_t *arr_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    arr_lit->const_params.ctype = IR_CONST_STR;
    arr_lit->const_params.type = type;
    arr_lit->const_params.str_val = unescaped;

    ir_expr_t *var =
        trans_create_anon_global(ts, type, arr_lit, 1, IR_LINKAGE_PRIVATE,
                                 IR_GDATA_CONSTANT | IR_GDATA_UNNAMED_ADDR);

    ir_expr_t *elem_ptr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
    ir_type_t *elem_ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    elem_ptr_type->ptr.base = type->arr.elem_type;
    elem_ptr->getelemptr.type = elem_ptr_type;
    elem_ptr->getelemptr.ptr_type = ptr_type;
    elem_ptr->getelemptr.ptr_val = var;

    // We need to 0's on getelemptr, one to get the array, another to get
    // the array's address
    ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i32);
    sl_append(&elem_ptr->getelemptr.idxs, &zero->link);

    zero = ir_expr_zero(ts->tunit, &ir_type_i32);
    sl_append(&elem_ptr->getelemptr.idxs, &zero->link);

    elem = emalloc(sizeof(*elem));
    elem->key = str;
    elem->val = elem_ptr;
    ht_insert(&ts->tunit->strings, &elem->link);

    return elem_ptr;
}

ir_expr_t *trans_array_init(trans_state_t *ts, expr_t *expr) {
    assert(expr->type == EXPR_INIT_LIST);
    assert(expr->etype->type == TYPE_ARR);

    ir_type_t *type = trans_type(ts, expr->etype);
    assert(type->type == IR_TYPE_ARR);
    ir_type_t *elem_type = type->arr.elem_type;
    type_t *ast_elem_type = expr->etype->arr.base;

    ir_expr_t *arr_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    sl_init(&arr_lit->const_params.arr_val, offsetof(ir_expr_t, link));
    arr_lit->const_params.ctype = IR_CONST_ARR;
    arr_lit->const_params.type = type;

    size_t nelems = 0;
    VEC_FOREACH(cur, &expr->init_list.exprs) {
        expr_t *elem = vec_get(&expr->init_list.exprs, cur);
        ir_expr_t *ir_elem = trans_expr(ts, false, elem, NULL);
        ir_elem = trans_type_conversion(ts, ast_elem_type, elem->etype,
                                        ir_elem, NULL);
        sl_append(&arr_lit->const_params.arr_val, &ir_elem->link);
        ++nelems;
    }

    while (nelems++ < type->arr.nelems) {
        ir_expr_t *zero = ir_expr_zero(ts->tunit, elem_type);
        sl_append(&arr_lit->const_params.arr_val, &zero->link);
    }

    return arr_lit;
}

void trans_struct_init_finalize_bf_array(trans_state_t *ts,
                                         ir_expr_t **parr_lit,
                                         size_t *pbitfield_offset,
                                         uint8_t *pcur_byte,
                                         size_t *ir_type_off,
                                         ir_expr_t *struct_lit) {
    ir_expr_t *arr_lit = *parr_lit;
    if (arr_lit == NULL) {
        return;
    }
    size_t bitfield_offset = *pbitfield_offset;
    uint8_t cur_byte = *pcur_byte;

    // Add any remaining bits
    if (bitfield_offset % CHAR_BIT != 0) {
        ir_expr_t *ir_elem = ir_int_const(ts->tunit, &ir_type_i8, cur_byte);
        sl_append(&arr_lit->const_params.arr_val, &ir_elem->link);
    }

    ++(*ir_type_off);
    sl_append(&struct_lit->const_params.struct_val, &arr_lit->link);

    // Reset state
    *parr_lit = NULL;
    *pbitfield_offset = 0;
    *pcur_byte = 0;
}

void trans_struct_init_append_val(trans_state_t *ts, ir_type_t *cur_type,
                                  vec_iter_t *iter, size_t *ir_type_off,
                                  ir_expr_t *struct_lit) {
    ir_expr_t *ir_elem;
    if (!vec_iter_has_next(iter)) {
        ir_elem = ir_expr_zero(ts->tunit, cur_type);
    } else {
        expr_t *elem = vec_iter_get(iter);
        ir_elem = trans_expr(ts, false, elem, NULL);
        ir_elem = trans_ir_type_conversion(ts, cur_type, false,
                                           ir_expr_type(ir_elem), false,
                                           ir_elem, NULL);
        vec_iter_advance(iter);
    }
    ++(*ir_type_off);
    sl_append(&struct_lit->const_params.struct_val, &ir_elem->link);
}

ir_expr_t *trans_struct_init(trans_state_t *ts, expr_t *expr) {
    assert(expr->type == EXPR_INIT_LIST);
    assert(expr->etype->type == TYPE_STRUCT);

    ir_type_t *type = trans_type(ts, expr->etype);
    if (type->type == IR_TYPE_ID_STRUCT) {
        type = type->id_struct.type;
    }
    assert(type->type == IR_TYPE_STRUCT);

    ir_expr_t *struct_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    sl_init(&struct_lit->const_params.struct_val, offsetof(ir_expr_t, link));
    struct_lit->const_params.ctype = IR_CONST_STRUCT;
    struct_lit->const_params.type = type;

    vec_iter_t vec_iter = { &expr->init_list.exprs, 0 };
    size_t ir_type_off = 0;
    ir_expr_t *arr_lit = NULL;
    size_t bitfield_offset = 0;
    uint8_t cur_byte;

    struct_iter_t iter;
    struct_iter_init(expr->etype, &iter);
    do {
        // If we were filling in bitfield elems, and this element is not a
        // bitfield, finalize the bitfield array
        if (arr_lit != NULL &&
            !(iter.node != NULL && iter.node->expr != NULL)) {
            trans_struct_init_finalize_bf_array(ts, &arr_lit, &bitfield_offset,
                                                &cur_byte, &ir_type_off,
                                                struct_lit);
        }

        ir_type_t *cur_type = vec_get(&type->struct_params.types, ir_type_off);

        if (iter.node != NULL) {
            if (iter.node->expr != NULL) { // Bitfield
                assert(iter.node->expr->type == EXPR_CONST_INT);
                int bf_bits = iter.node->expr->const_val.int_val;
                if (cur_type->type != IR_TYPE_ARR) {
                    continue;
                }
                if (arr_lit == NULL) {
                    arr_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
                    sl_init(&arr_lit->const_params.struct_val,
                            offsetof(ir_expr_t, link));
                    arr_lit->const_params.ctype = IR_CONST_ARR;
                    arr_lit->const_params.type = cur_type;
                    bitfield_offset = 0;
                    cur_byte = 0;
                }
                if (bf_bits == 0) { // Align to next bit
                    if (bitfield_offset != 0) {
                        ir_expr_t *ir_elem = ir_int_const(ts->tunit,
                                                          &ir_type_i8,
                                                          cur_byte);
                        sl_append(&arr_lit->const_params.arr_val,
                                  &ir_elem->link);
                        cur_byte = 0;
                    }
                    continue;
                }

                long long val = 0;
                if (vec_iter_has_next(&vec_iter)) {
                    expr_t *elem = vec_iter_get(&vec_iter);
                    // TODO0: This assertion isn't correct
                    assert(elem->type == EXPR_CONST_INT);
                    val = elem->const_val.int_val;
                    vec_iter_advance(&vec_iter);
                }
                int offset = 0;
                while (offset < bf_bits) {
                    int bits = CHAR_BIT;
                    int mask = 0;
                    int upto = offset + CHAR_BIT;

                    if (bitfield_offset != 0) { // Mask away lower bits
                        mask |= (1 << bitfield_offset) - 1;
                        bits -= bitfield_offset;
                        upto = CHAR_BIT - bitfield_offset;
                    }
                    if (upto > bf_bits) { // Mask away upper bits
                        mask |= ((1 << (upto - bf_bits)) - 1)
                            << (bf_bits - offset + bitfield_offset);
                        bits -= upto - bf_bits;
                    }
                    long long temp = val;
                    if (bitfield_offset > 0) {
                        temp <<= bitfield_offset;
                    } else {
                        temp >>= offset;
                    }
                    temp &= ~mask;

                    cur_byte |= temp;

                    bitfield_offset += bits;

                    if (bitfield_offset >= CHAR_BIT) {
                        ir_expr_t *ir_elem = ir_int_const(ts->tunit,
                                                          &ir_type_i8,
                                                          cur_byte);
                        sl_append(&arr_lit->const_params.arr_val,
                                  &ir_elem->link);
                        bitfield_offset %= CHAR_BIT;
                        cur_byte = 0;
                    }

                    offset += bits;
                }

            } else if (iter.node->id != NULL) {
                trans_struct_init_append_val(ts, cur_type, &vec_iter,
                                             &ir_type_off, struct_lit);
            }
        }
        // Anonymous struct/union
        if (iter.node == NULL && iter.decl != NULL &&
            (iter.decl->type->type == TYPE_STRUCT ||
             iter.decl->type->type == TYPE_UNION)) {
            trans_struct_init_append_val(ts, cur_type, &vec_iter,
                                         &ir_type_off, struct_lit);
        }
    } while (struct_iter_advance(&iter));

    trans_struct_init_finalize_bf_array(ts, &arr_lit, &bitfield_offset,
                                        &cur_byte, &ir_type_off, struct_lit);
    return struct_lit;
}

ir_expr_t *trans_union_init(trans_state_t *ts, type_t *type, expr_t *expr) {
    ir_type_t *union_type = trans_type(ts, type);
    if (expr == NULL) {
        return ir_expr_zero(ts->tunit, union_type);
    }
    assert(expr->type == EXPR_INIT_LIST);
    assert(expr->etype->type == TYPE_UNION);

    if (vec_size(&expr->init_list.exprs) == 0) {
        return ir_expr_zero(ts->tunit, union_type);
    }

    expr_t *head = vec_front(&expr->init_list.exprs);
    type_t *elem_type = head->etype;
    size_t total_size = ast_type_size(type);
    size_t elem_size = ast_type_size(elem_type);

    ir_type_t *ir_elem_type = trans_type(ts, elem_type);

    ir_type_t *expr_type = ir_type_create(ts->tunit, IR_TYPE_STRUCT);
    vec_push_back(&expr_type->struct_params.types, ir_elem_type);

    ir_type_t *pad_type = NULL;
    if (elem_size != total_size) {
        assert(elem_size < total_size);
        pad_type = ir_type_create(ts->tunit, IR_TYPE_ARR);
        pad_type->arr.nelems = total_size - elem_size;
        pad_type->arr.elem_type = &ir_type_i8;
        vec_push_back(&expr_type->struct_params.types, pad_type);
    }

    ir_expr_t *struct_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    sl_init(&struct_lit->const_params.struct_val, offsetof(ir_expr_t, link));
    struct_lit->const_params.ctype = IR_CONST_STRUCT;
    struct_lit->const_params.type = expr_type;

    ir_expr_t *ir_elem = trans_expr(ts, false, head, NULL);
    sl_append(&struct_lit->const_params.struct_val, &ir_elem->link);

    if (elem_size != total_size) {
        assert(pad_type != NULL);
        ir_elem = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_elem->const_params.ctype = IR_CONST_UNDEF;
        ir_elem->const_params.type = pad_type;
        sl_append(&struct_lit->const_params.struct_val, &ir_elem->link);
    }

    return struct_lit;
}

ir_expr_t *trans_compound_literal(trans_state_t *ts, bool addrof,
                                  ir_inst_stream_t *ir_stmts, expr_t *expr) {
    assert(expr->type == EXPR_INIT_LIST);

    ir_type_t *type = trans_type(ts, expr->etype);

    ir_expr_t *addr;
    if (ts->func == NULL) { // Global
        ir_expr_t *init = trans_expr(ts, false, expr, NULL);
        addr = trans_create_anon_global(ts, type, init,
                                        ast_type_align(expr->etype),
                                        IR_LINKAGE_INTERNAL,
                                        IR_GDATA_NOFLAG);
    } else { // Local
        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = type;
        ir_expr_t *alloc = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
        alloc->alloca.type = ptr_type;
        alloc->alloca.elem_type = type;
        alloc->alloca.nelem_type = NULL;
        alloc->alloca.align = ast_type_align(expr->etype);

        addr = trans_temp_create(ts, ptr_type);

        // Assign to temp
        // Note we can't use trans_assign_temp because its an alloca
        ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        stmt->assign.dest = addr;
        stmt->assign.src = alloc;
        trans_add_stmt(ts, ir_stmts, stmt);

        // Store the initalizer
        trans_initializer(ts, ir_stmts, expr->etype, type, addr, expr);

    }

    if (addrof) {
        return addr;
    } else {
        return trans_load_temp(ts, ir_stmts, addr);
    }
}
