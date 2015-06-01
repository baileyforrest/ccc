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

#include "trans_type.h"

#include <limits.h>

#include "trans_expr.h"

#include "typecheck/typecheck.h"
#include "util/string_store.h"

#define STRUCT_PREFIX "struct."
#define UNION_PREFIX "union."

#define VA_LIST_NAME "struct.__va_list_tag"

ir_expr_t *trans_type_conversion(trans_state_t *ts, type_t *dest, type_t *src,
                                 ir_expr_t *src_expr,
                                 ir_inst_stream_t *ir_stmts) {
    type_t *orig_dest = ast_type_untypedef(dest);
    type_t *orig_src = ast_type_untypedef(src);
    dest = ast_type_unmod(orig_dest);
    src = ast_type_unmod(orig_src);

    if (dest->type == TYPE_BOOL) {
        ir_expr_t *i1_expr = trans_expr_bool(ts, src_expr, ir_stmts);
        return trans_ir_type_conversion(ts, &BOOL_TYPE, false,
                                        ir_expr_type(i1_expr), false,
                                        i1_expr, ir_stmts);
    }

    // Don't do anything if types are equal
    if (typecheck_type_equal(dest, src)) {
        return src_expr;
    }

    ir_type_t *dest_type = trans_type(ts, dest);
    ir_type_t *src_type = ir_expr_type(src_expr);

    bool dest_signed = !TYPE_IS_UNSIGNED(orig_dest);

    bool src_signed = !TYPE_IS_UNSIGNED(orig_src);

    return trans_ir_type_conversion(ts, dest_type, dest_signed,
                                    src_type, src_signed, src_expr,
                                    ir_stmts);
}

ir_expr_t *trans_ir_type_conversion(trans_state_t *ts, ir_type_t *dest_type,
                                    bool dest_signed, ir_type_t *src_type,
                                    bool src_signed, ir_expr_t *src_expr,
                                    ir_inst_stream_t *ir_stmts) {
    if (ir_type_equal(dest_type, src_type)) {
        return src_expr;
    }

    // Special case, changing type of constant integer/float, just change its
    // type to the dest type
    if (src_expr->type == IR_EXPR_CONST) {
        if ((src_expr->const_params.ctype == IR_CONST_INT &&
             dest_type->type == IR_TYPE_INT) ||
            (src_expr->const_params.ctype == IR_CONST_FLOAT &&
             dest_type->type == IR_TYPE_FLOAT)) {
            src_expr->const_params.type = dest_type;
            return src_expr;
        }
    }

    ir_convert_t convert_op;
    switch (dest_type->type) {
    case IR_TYPE_INT: {
        switch (src_type->type) {
        case IR_TYPE_INT:
            if (dest_type->int_params.width < src_type->int_params.width) {
                convert_op = IR_CONVERT_TRUNC;
            } else {
                // Bools are treated as unsigned
                if (src_signed && src_type->int_params.width != 1) {
                    convert_op = IR_CONVERT_SEXT;
                } else {
                    convert_op = IR_CONVERT_ZEXT;
                }
            }
            break;
        case IR_TYPE_FLOAT:
            if (dest_signed) {
                convert_op = IR_CONVERT_FPTOSI;
            } else {
                convert_op = IR_CONVERT_FPTOUI;
            }
            break;
        case IR_TYPE_FUNC:
        case IR_TYPE_PTR:
        case IR_TYPE_ARR:
            convert_op = IR_CONVERT_PTRTOINT;
            break;
        default:
            assert(false);
        }
        break;
    }
    case IR_TYPE_FLOAT:
        switch (src_type->type) {
        case IR_TYPE_INT:
            if (src_signed) {
                convert_op = IR_CONVERT_SITOFP;
            } else {
                convert_op = IR_CONVERT_UITOFP;
            }
            break;
        case IR_TYPE_FLOAT:
            if (src_type->float_params.type < dest_type->float_params.type) {
                convert_op = IR_CONVERT_FPEXT;
            } else {
                // We would have returned if they were equal
                assert(src_type->float_params.type >
                       dest_type->float_params.type);
                convert_op = IR_CONVERT_FPTRUNC;
            }
            break;
        default:
            assert(false);
        }
        break;
    case IR_TYPE_FUNC:
    case IR_TYPE_PTR:
    case IR_TYPE_ARR:
        switch (src_type->type) {
        case IR_TYPE_INT:
            convert_op = IR_CONVERT_INTTOPTR;
            break;
        case IR_TYPE_FUNC:
        case IR_TYPE_PTR:
        case IR_TYPE_ARR:
            convert_op = IR_CONVERT_BITCAST;
            break;
        default:
            assert(false);
        }
        break;
    case IR_TYPE_VOID:
        // Casted to void expression cannot be used, typechecker should ensure
        // this
        return NULL;

    case IR_TYPE_OPAQUE:
    case IR_TYPE_STRUCT:
    default:
        assert(false);
    }

    ir_expr_t *convert = ir_expr_create(ts->tunit, IR_EXPR_CONVERT);
    convert->convert.type = convert_op;
    convert->convert.src_type = src_type;
    convert->convert.val = src_expr;
    convert->convert.dest_type = dest_type;

    return trans_assign_temp(ts, ir_stmts, convert);
}


ir_type_t *trans_type(trans_state_t *ts, type_t *type) {
    ir_type_t *ir_type = NULL;
    switch (type->type) {
    case TYPE_VOID:        return &ir_type_void;
    case TYPE_BOOL:        return &ir_type_i8;
    case TYPE_CHAR:        return &ir_type_i8;
    case TYPE_SHORT:       return &ir_type_i16;
    case TYPE_INT:         return &ir_type_i32;
    case TYPE_LONG:        return &ir_type_i64;
    case TYPE_LONG_LONG:   return &ir_type_i64;
    case TYPE_FLOAT:       return &ir_type_float;
    case TYPE_DOUBLE:      return &ir_type_double;
    case TYPE_LONG_DOUBLE: return &ir_type_x86_fp80;
    case TYPE_ENUM:        return &ir_type_i32;

    case TYPE_TYPEDEF:     return trans_type(ts, type->typedef_params.base);
    case TYPE_MOD:         return trans_type(ts, type->mod.base);
    case TYPE_PAREN:       return trans_type(ts, type->paren_base);

    case TYPE_UNION:
    case TYPE_STRUCT: {
        bool is_union = type->type == TYPE_UNION;

        // If there is a named definition of this structure, return that
        if (type->struct_params.trans_state != NULL) {
            ir_gdecl_t *gdecl = type->struct_params.trans_state;
            assert(gdecl->type == IR_GDECL_ID_STRUCT);
            return gdecl->id_struct.id_type;
        }
        ir_gdecl_t *id_gdecl = NULL;

        // Must create named entry before creating the actual struct to
        // prevent infinite recursion
        // If this is a named structure, create a struct id type
        if (type->struct_params.name != NULL) {
            char *name;
            if (is_union) {
                name = emalloc(strlen(type->struct_params.name) +
                               sizeof(UNION_PREFIX));
                sprintf(name, UNION_PREFIX"%s", type->struct_params.name);
            } else {
                name = emalloc(strlen(type->struct_params.name) +
                               sizeof(STRUCT_PREFIX));
                sprintf(name, STRUCT_PREFIX"%s", type->struct_params.name);
            }
            name = sstore_insert(name);
            ir_type_t *id_type = ir_type_create(ts->tunit, IR_TYPE_ID_STRUCT);
            id_type->id_struct.name = name;
            id_type->id_struct.type = ir_type;

            id_gdecl = ir_gdecl_create(IR_GDECL_ID_STRUCT);
            id_gdecl->id_struct.name = name;
            id_gdecl->id_struct.id_type = id_type;
            sl_append(&ts->tunit->id_structs, &id_gdecl->link);
            type->struct_params.trans_state = id_gdecl;
        }

        type_t *max_type = NULL;
        size_t max_size = 0;
        int bitfield_bits = 0;

        // Create a new structure object
        ir_type = ir_type_create(ts->tunit, IR_TYPE_STRUCT);
        SL_FOREACH(cur_decl, &type->struct_params.decls) {
            decl_t *decl = GET_ELEM(&type->struct_params.decls, cur_decl);
            SL_FOREACH(cur_node, &decl->decls) {
                decl_node_t *node = GET_ELEM(&decl->decls, cur_node);
                if (is_union) {
                    size_t size = ast_type_size(node->type);
                    if (size > max_size) {
                        max_size = size;
                        max_type = node->type;
                    }
                } else {
                    if (node->expr != NULL) {
                        // Handle bitfields
                        assert(node->expr->type == EXPR_CONST_INT);
                        int decl_bf_bits = node->expr->const_val.int_val;
                        if (decl_bf_bits == 0) {
                            int remain = bitfield_bits % CHAR_BIT;
                            if (remain != 0) {
                                bitfield_bits += CHAR_BIT - remain;
                            }
                            continue;
                        }
                        bitfield_bits += decl_bf_bits;
                    } else {
                        if (bitfield_bits != 0) {
                            ir_type_t *bf_type = ir_type_create(ts->tunit,
                                                                IR_TYPE_ARR);
                            bf_type->arr.nelems =
                                (bitfield_bits + (CHAR_BIT - 1)) / CHAR_BIT;
                            bf_type->arr.elem_type = &ir_type_i8;
                            vec_push_back(&ir_type->struct_params.types,
                                          bf_type);
                            bitfield_bits = 0;
                        }
                        ir_type_t *node_type = trans_type(ts, node->type);
                        vec_push_back(&ir_type->struct_params.types, node_type);
                    }
                }
            }

            // Add anonymous struct and union members to the struct
            if (sl_head(&decl->decls) == NULL &&
                (decl->type->type == TYPE_STRUCT ||
                 decl->type->type == TYPE_UNION)) {
                if (is_union) {
                    size_t size = ast_type_size(decl->type);
                    if (size > max_size) {
                        max_size = size;
                        max_type = decl->type;
                    }
                } else {
                    ir_type_t *decl_type = trans_type(ts, decl->type);
                    vec_push_back(&ir_type->struct_params.types, decl_type);
                }
            }
        }

        // Handle trailing bitfield bits
        if (bitfield_bits != 0) {
            ir_type_t *bf_type = ir_type_create(ts->tunit,
                                                IR_TYPE_ARR);
            bf_type->arr.nelems =
                bitfield_bits + (CHAR_BIT - 1) / CHAR_BIT;
            bf_type->arr.elem_type = &ir_type_i8;
            vec_push_back(&ir_type->struct_params.types,
                          bf_type);
        }

        if (is_union && max_type != NULL) {
            ir_type_t *ir_max_type = trans_type(ts, max_type);
            vec_push_back(&ir_type->struct_params.types, ir_max_type);
        }

        if (id_gdecl != NULL) {
            id_gdecl->id_struct.type = ir_type;
            id_gdecl->id_struct.id_type->id_struct.type = ir_type;
            return id_gdecl->id_struct.id_type;
        }

        return ir_type;
    }
    case TYPE_FUNC:
        ir_type = ir_type_create(ts->tunit, IR_TYPE_FUNC);
        ir_type->func.type = trans_type(ts, type->func.type);
        ir_type->func.varargs = type->func.varargs;

        SL_FOREACH(cur, &type->func.params) {
            decl_t *decl = GET_ELEM(&type->func.params, cur);
            type_t *ptype = DECL_TYPE(decl);
            ir_type_t *param_type = trans_type(ts, ptype);

            vec_push_back(&ir_type->func.params, param_type);
        }

        return ir_type;
    case TYPE_ARR:
        // Convert [] to *
        if (type->arr.nelems == 0) {
            ir_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
            ir_type->ptr.base = trans_type(ts, type->arr.base);
        } else {
            ir_type = ir_type_create(ts->tunit, IR_TYPE_ARR);
            ir_type->arr.nelems = type->arr.nelems;
            ir_type->arr.elem_type = trans_type(ts, type->arr.base);
        }
        return ir_type;
    case TYPE_PTR:
        ir_type = ir_type_create(ts->tunit, IR_TYPE_PTR);

        // LLVM IR doesn't allowe void*, so convert void* to i8*
        if (ast_type_unmod(type->ptr.base)->type == TYPE_VOID) {
            ir_type->ptr.base = &ir_type_i8;
        } else {
            ir_type->ptr.base = trans_type(ts, type->ptr.base);
        }
        return ir_type;
    case TYPE_VA_LIST: {
        if (ts->va_type != NULL) {
            return ts->va_type;
        }
#ifdef __x86_64__
        ir_type = ir_type_create(ts->tunit, IR_TYPE_STRUCT);
        vec_push_back(&ir_type->struct_params.types, &ir_type_i32);
        vec_push_back(&ir_type->struct_params.types, &ir_type_i32);
        vec_push_back(&ir_type->struct_params.types, &ir_type_i8_ptr);
        vec_push_back(&ir_type->struct_params.types, &ir_type_i8_ptr);
#else
#error "Unsupported platform"
#endif

        ir_type_t *id_type = ir_type_create(ts->tunit, IR_TYPE_ID_STRUCT);
        id_type->id_struct.name = VA_LIST_NAME;
        id_type->id_struct.type = ir_type;

        ir_gdecl_t *id_gdecl = ir_gdecl_create(IR_GDECL_ID_STRUCT);
        id_gdecl->id_struct.name = VA_LIST_NAME;
        id_gdecl->id_struct.id_type = id_type;
        id_gdecl->id_struct.type = ir_type;
        sl_append(&ts->tunit->id_structs, &id_gdecl->link);

        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = id_type;

        return ts->va_type = ptr_type;
    }
    default:
        assert(false);
    }
    return NULL;
}
