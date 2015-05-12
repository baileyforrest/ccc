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
 * IR tree implementation
 */

#include "ir.h"
#include "ir_priv.h"

#include <assert.h>

#include "util/string_store.h"

#define MAX_LABEL_LEN 512
#define ANON_LABEL_PREFIX "BB"

#define IR_INT_LIT(width)                                   \
    { SL_LINK_LIT, IR_TYPE_INT, { .int_params = { width } } }

#define IR_FLOAT_LIT(type)                                      \
    { SL_LINK_LIT, IR_TYPE_FLOAT, { .float_params = { type } } }

ir_type_t ir_type_void = { SL_LINK_LIT, IR_TYPE_VOID, { } };
ir_type_t ir_type_i1 = IR_INT_LIT(1);
ir_type_t ir_type_i8 = IR_INT_LIT(8);
ir_type_t ir_type_i16 = IR_INT_LIT(16);
ir_type_t ir_type_i32 = IR_INT_LIT(32);
ir_type_t ir_type_i64 = IR_INT_LIT(64);
ir_type_t ir_type_float = IR_FLOAT_LIT(IR_FLOAT_FLOAT);
ir_type_t ir_type_double = IR_FLOAT_LIT(IR_FLOAT_DOUBLE);
ir_type_t ir_type_x86_fp80 = IR_FLOAT_LIT(IR_FLOAT_X86_FP80);

ir_type_t ir_type_i8_ptr = { SL_LINK_LIT, IR_TYPE_PTR,
                             { .ptr = { &ir_type_i8 } } };

extern ir_stmt_t *ir_inst_stream_head(ir_inst_stream_t *stream);
extern ir_stmt_t *ir_inst_stream_tail(ir_inst_stream_t *stream);
extern void ir_inst_stream_append(ir_inst_stream_t *stream, ir_stmt_t *stmt);

ir_type_t *ir_expr_type(ir_expr_t *expr) {
    switch (expr->type) {
    case IR_EXPR_VAR:
        return expr->var.type;
    case IR_EXPR_CONST:
        return expr->const_params.type;
    case IR_EXPR_BINOP:
        return expr->binop.type;
    case IR_EXPR_ALLOCA:
        return expr->alloca.type;
    case IR_EXPR_LOAD:
        return expr->load.type;
    case IR_EXPR_GETELEMPTR:
        return expr->getelemptr.type;
    case IR_EXPR_CONVERT:
        return expr->convert.dest_type;
    case IR_EXPR_ICMP:
        return &ir_type_i1;
    case IR_EXPR_FCMP:
        return &ir_type_i1;
    case IR_EXPR_PHI:
        return expr->phi.type;
    case IR_EXPR_SELECT:
        return expr->select.type;
    case IR_EXPR_CALL:
        return expr->call.func_sig->func.type;
    case IR_EXPR_VAARG:
    default:
        assert(false);
    }
    return NULL;
}

bool ir_type_equal(ir_type_t *t1, ir_type_t *t2) {
    if (t1 == t2) {
        return true;
    }

    if (t1->type != t2->type) {
        return false;
    }

    switch (t1->type) {
    case IR_TYPE_INT: return t1->int_params.width == t2->int_params.width;
    case IR_TYPE_FLOAT: return t1->float_params.type == t2->float_params.type;
    case IR_TYPE_PTR: return ir_type_equal(t1->ptr.base, t2->ptr.base);

    case IR_TYPE_ARR:
        return t1->arr.nelems == t2->arr.nelems &&
            ir_type_equal(t1->arr.elem_type, t2->arr.elem_type);

    case IR_TYPE_STRUCT: {
        size_t size1 = vec_size(&t1->struct_params.types);
        if (size1 != vec_size(&t2->struct_params.types)) {
            return false;
        }
        for (size_t i = 0; i < size1; ++i) {
            if (!ir_type_equal(vec_get(&t1->struct_params.types, i),
                               vec_get(&t2->struct_params.types, i))) {
                return false;
            }
        }
        return true;
    }

    case IR_TYPE_FUNC:
        if (t1->func.varargs != t2->func.varargs) {
            return false;
        }
        if (!ir_type_equal(t1->func.type, t2->func.type)) {
            return false;
        }

        size_t size1 = vec_size(&t1->func.params);
        if (size1 != vec_size(&t2->func.params)) {
            return false;
        }
        for (size_t i = 0; i < size1; ++i) {
            if (!ir_type_equal(vec_get(&t1->func.params, i),
                               vec_get(&t2->func.params, i))) {
                return false;
            }
        }
        return true;

        // These types should only have one copy
    case IR_TYPE_ID_STRUCT:
    case IR_TYPE_OPAQUE:
    case IR_TYPE_VOID:
    default:
        assert(false);
    }
}

ir_label_t *ir_label_create(ir_trans_unit_t *tunit, char *str) {
    ir_label_t *label = ht_lookup(&tunit->labels, &str);
    if (label != NULL) {
        return label;
    }

    label = emalloc(sizeof(ir_label_t));
    label->name = str;
    status_t status = ht_insert(&tunit->labels, &label->link);
    assert(status == CCC_OK);

    return label;
}

ir_label_t *ir_numlabel_create(ir_trans_unit_t *tunit, int num) {
    assert(num >= 0);
    char buf[MAX_LABEL_LEN];
    snprintf(buf, sizeof(buf), ANON_LABEL_PREFIX "%d", num);
    buf[sizeof(buf) - 1] = '\0';
    char *pbuf = buf;
    ir_label_t *label = ht_lookup(&tunit->labels, &pbuf);
    if (label != NULL) {
        return label;
    }

    // Allocate the label and its string in one chunk
    label = emalloc(sizeof(ir_label_t));
    label->name = sstore_lookup(buf);
    status_t status = ht_insert(&tunit->labels, &label->link);
    assert(status == CCC_OK);

    return label;
}

ir_expr_t *ir_temp_create(ir_trans_unit_t *tunit, ir_gdecl_t *func,
                          ir_type_t *type, int num) {
    assert(num >= 0);
    assert(func->type == IR_GDECL_FUNC);

    char buf[MAX_LABEL_LEN];
    snprintf(buf, sizeof(buf), "%d", num);
    buf[sizeof(buf) - 1] = '\0';

    ir_expr_t *temp = emalloc(sizeof(ir_expr_t));
    temp->type = IR_EXPR_VAR;
    temp->var.type = type;
    temp->var.name = sstore_lookup(buf);
    temp->var.local = true;
    sl_append(&tunit->exprs, &temp->heap_link);

    ir_symtab_entry_t *entry = ir_symtab_entry_create(IR_SYMTAB_ENTRY_VAR,
                                                      temp->var.name);
    entry->var.expr = temp;
    entry->var.access = temp;
    status_t status = ir_symtab_insert(&func->func.locals, entry);
    assert(status == CCC_OK);

    return temp;
}

ir_trans_unit_t *ir_trans_unit_create(void) {
    ir_trans_unit_t *tunit = emalloc(sizeof(ir_trans_unit_t));
    sl_init(&tunit->id_structs, offsetof(ir_gdecl_t, link));
    sl_init(&tunit->decls, offsetof(ir_gdecl_t, link));
    sl_init(&tunit->funcs, offsetof(ir_gdecl_t, link));
    sl_init(&tunit->stmts, offsetof(ir_stmt_t, heap_link));
    sl_init(&tunit->exprs, offsetof(ir_expr_t, heap_link));
    sl_init(&tunit->types, offsetof(ir_type_t, heap_link));
    ir_symtab_init(&tunit->globals);

    static const ht_params_t labels_params = {
        0,                          // Size estimate
        offsetof(ir_label_t, name), // Offset of key
        offsetof(ir_label_t, link), // Offset of ht link
        ind_str_hash,               // Hash function
        ind_str_eq,                 // void string compare
    };

    ht_init(&tunit->labels, &labels_params);

    static const ht_params_t fun_decls_params = {
        0,                             // Size estimate
        offsetof(ht_ptr_elem_t, key),  // Offset of key
        offsetof(ht_ptr_elem_t, link), // Offset of ht link
        ind_str_hash,                  // Hash function
        ind_str_eq,                    // void string compare
    };

    ht_init(&tunit->global_decls, &fun_decls_params);
    ht_init(&tunit->strings, &fun_decls_params);
    tunit->static_num = 0;
    return tunit;
}

ir_gdecl_t *ir_gdecl_create(ir_gdecl_type_t type) {
    ir_gdecl_t *gdecl = emalloc(sizeof(ir_gdecl_t));
    gdecl->type = type;
    gdecl->linkage = IR_LINKAGE_DEFAULT;
    switch (type) {
    case IR_GDECL_FUNC_DECL:
    case IR_GDECL_ID_STRUCT:
        break;
    case IR_GDECL_GDATA:
        gdecl->gdata.flags = 0;
        break;
    case IR_GDECL_FUNC:
        sl_init(&gdecl->func.params, offsetof(ir_expr_t, link));
        dl_init(&gdecl->func.prefix.list, offsetof(ir_stmt_t, link));
        dl_init(&gdecl->func.body.list, offsetof(ir_stmt_t, link));
        ir_symtab_init(&gdecl->func.locals);
        gdecl->func.next_temp = 0;
        gdecl->func.next_label = 0;
        break;
    default:
        assert(false);
    }

    return gdecl;
}

ir_stmt_t *ir_stmt_create(ir_trans_unit_t *tunit, ir_stmt_type_t type) {
    ir_stmt_t *stmt = emalloc(sizeof(ir_stmt_t));
    stmt->type = type;
    sl_append(&tunit->stmts, &stmt->heap_link);

    switch (stmt->type) {
    case IR_STMT_LABEL:
    case IR_STMT_EXPR:
    case IR_STMT_RET:
    case IR_STMT_BR:
    case IR_STMT_ASSIGN:
    case IR_STMT_STORE:
        break;
    case IR_STMT_SWITCH:
        sl_init(&stmt->switch_params.cases,
                offsetof(ir_expr_label_pair_t, link));
        break;
    case IR_STMT_INDIR_BR:
        sl_init(&stmt->indirectbr.labels, offsetof(ir_label_t, link));
        break;
    default:
        assert(false);
    }
    return stmt;
}

ir_expr_t *ir_expr_create(ir_trans_unit_t *tunit, ir_expr_type_t type) {
    ir_expr_t *expr = emalloc(sizeof(ir_expr_t));
    expr->type = type;
    sl_append(&tunit->exprs, &expr->heap_link);

    switch (type) {
    case IR_EXPR_VAR:
    case IR_EXPR_CONST:
    case IR_EXPR_BINOP:
    case IR_EXPR_ALLOCA:
    case IR_EXPR_LOAD:
    case IR_EXPR_CONVERT:
    case IR_EXPR_ICMP:
    case IR_EXPR_FCMP:
    case IR_EXPR_SELECT:
        break;

    case IR_EXPR_GETELEMPTR:
        sl_init(&expr->getelemptr.idxs, offsetof(ir_expr_t, link));
        break;
    case IR_EXPR_PHI:
        sl_init(&expr->phi.preds, offsetof(ir_expr_label_pair_t, link));
        break;
    case IR_EXPR_CALL:
        sl_init(&expr->call.arglist, offsetof(ir_expr_t, link));
        break;
    case IR_EXPR_VAARG:
        sl_init(&expr->vaarg.arglist, offsetof(ir_expr_t, link));
        break;
    default:
        assert(false);
    }
    return expr;
}

ir_type_t *ir_type_create(ir_trans_unit_t *tunit, ir_type_type_t type) {
    ir_type_t *ir_type = emalloc(sizeof(ir_type_t));
    ir_type->type = type;
    sl_append(&tunit->types, &ir_type->heap_link);

    switch (type) {
    case IR_TYPE_VOID:
    case IR_TYPE_INT:
    case IR_TYPE_FLOAT:
        assert(false && "Use the static types");
        break;

    case IR_TYPE_FUNC:
        vec_init(&ir_type->func.params, 0);
        break;
    case IR_TYPE_STRUCT:
        vec_init(&ir_type->struct_params.types, 0);
        break;

    case IR_TYPE_PTR:
    case IR_TYPE_ARR:
    case IR_TYPE_OPAQUE:
    case IR_TYPE_ID_STRUCT:
        break;
    default:
        assert(false);
    }

    return ir_type;
}

void ir_type_destroy(ir_type_t *type) {
    switch (type->type) {
    case IR_TYPE_FUNC:
        vec_destroy(&type->func.params);
        break;
    case IR_TYPE_STRUCT:
        vec_destroy(&type->struct_params.types);
        break;
    case IR_TYPE_VOID:
    case IR_TYPE_INT:
    case IR_TYPE_FLOAT:
    case IR_TYPE_PTR:
    case IR_TYPE_ARR:
    case IR_TYPE_OPAQUE:
    case IR_TYPE_ID_STRUCT:
        break;
    default:
        assert(false);
    }
    free(type);
}

void ir_expr_destroy(ir_expr_t *expr) {
    switch (expr->type) {
    case IR_EXPR_VAR:
    case IR_EXPR_BINOP:
    case IR_EXPR_ALLOCA:
    case IR_EXPR_LOAD:
    case IR_EXPR_CONVERT:
    case IR_EXPR_ICMP:
    case IR_EXPR_FCMP:
    case IR_EXPR_SELECT:
        break;
    case IR_EXPR_CONST:
        switch (expr->const_params.ctype) {
        case IR_CONST_INT:
        case IR_CONST_FLOAT:
        case IR_CONST_NULL:
        case IR_CONST_ZERO:
        case IR_CONST_STR:
        case IR_CONST_UNDEF:
            break;
        case IR_CONST_ARR:
            sl_destroy(&expr->const_params.arr_val);
            break;
        case IR_CONST_STRUCT:
            sl_destroy(&expr->const_params.struct_val);
            break;
        default:
            assert(false);
        }
        break;
    case IR_EXPR_GETELEMPTR:
        sl_destroy(&expr->getelemptr.idxs);
        break;
    case IR_EXPR_PHI:
        SL_DESTROY_FUNC(&expr->phi.preds, free);
        break;
    case IR_EXPR_CALL:
        sl_destroy(&expr->call.arglist);
        break;
    case IR_EXPR_VAARG:
        sl_destroy(&expr->vaarg.arglist);
        break;
    default:
        assert(false);
    }
    free(expr);
}

void ir_stmt_destroy(ir_stmt_t *stmt) {
    switch (stmt->type) {
    case IR_STMT_LABEL:
    case IR_STMT_EXPR:
    case IR_STMT_ASSIGN:
    case IR_STMT_STORE:
    case IR_STMT_RET:
    case IR_STMT_BR:
        break;
    case IR_STMT_SWITCH:
        SL_DESTROY_FUNC(&stmt->switch_params.cases, free);
        break;
    case IR_STMT_INDIR_BR:
        SL_DESTROY_FUNC(&stmt->indirectbr.labels, free);
        break;
    default:
        assert(false);
    }
    free(stmt);
}

void ir_gdecl_destroy(ir_gdecl_t *gdecl) {
    switch (gdecl->type) {
    case IR_GDECL_GDATA:
    case IR_GDECL_FUNC_DECL:
    case IR_GDECL_ID_STRUCT:
        break;
    case IR_GDECL_FUNC:
        ir_symtab_destroy(&gdecl->func.locals);
        break;
    default:
        assert(false);
    }
    free(gdecl);
}

void ir_trans_unit_destroy(ir_trans_unit_t *trans_unit) {
    if (trans_unit == NULL) {
        return;
    }
    SL_DESTROY_FUNC(&trans_unit->id_structs, ir_gdecl_destroy);
    SL_DESTROY_FUNC(&trans_unit->decls, ir_gdecl_destroy);
    SL_DESTROY_FUNC(&trans_unit->funcs, ir_gdecl_destroy);
    SL_DESTROY_FUNC(&trans_unit->stmts, ir_stmt_destroy);
    SL_DESTROY_FUNC(&trans_unit->exprs, ir_expr_destroy);
    SL_DESTROY_FUNC(&trans_unit->types, ir_type_destroy);
    ir_symtab_destroy(&trans_unit->globals);
    HT_DESTROY_FUNC(&trans_unit->labels, free);
    HT_DESTROY_FUNC(&trans_unit->global_decls, free);
    HT_DESTROY_FUNC(&trans_unit->strings, free);
    free(trans_unit);
}

ir_expr_t *ir_int_const(ir_trans_unit_t *tunit, ir_type_t *type,
                        long long value) {
    assert(type->type == IR_TYPE_INT);
    ir_expr_t *expr = ir_expr_create(tunit, IR_EXPR_CONST);
    expr->const_params.ctype = IR_CONST_INT;
    expr->const_params.type = type;
    expr->const_params.int_val = value;
    return expr;
}


ir_expr_t *ir_expr_zero(ir_trans_unit_t *tunit, ir_type_t *type) {
    switch (type->type) {
    case IR_TYPE_INT:
        return ir_int_const(tunit, type, 0);

    case IR_TYPE_FLOAT: {
        ir_expr_t *expr = ir_expr_create(tunit, IR_EXPR_CONST);
        expr->const_params.ctype = IR_CONST_FLOAT;
        expr->const_params.type = type;
        expr->const_params.float_val = 0.0;
        return expr;
    }
    case IR_TYPE_PTR: {
        ir_expr_t *expr = ir_expr_create(tunit, IR_EXPR_CONST);
        expr->const_params.ctype = IR_CONST_NULL;
        expr->const_params.type = type;
        return expr;
    }

    case IR_TYPE_ID_STRUCT:
        return ir_expr_zero(tunit, type->id_struct.type);

    case IR_TYPE_ARR: {
        ir_expr_t *expr = ir_expr_create(tunit, IR_EXPR_CONST);
        expr->const_params.ctype = IR_CONST_ARR;
        expr->const_params.type = type;
        sl_init(&expr->const_params.arr_val, offsetof(ir_expr_t, link));

        for (size_t i = 0; i < type->arr.nelems; ++i) {
            ir_expr_t *zero = ir_expr_zero(tunit, type->arr.elem_type);
            sl_append(&expr->const_params.arr_val, &zero->link);
        }

        return expr;
    }
    case IR_TYPE_STRUCT: {
        ir_expr_t *expr = ir_expr_create(tunit, IR_EXPR_CONST);
        expr->const_params.ctype = IR_CONST_STRUCT;
        expr->const_params.type = type;
        sl_init(&expr->const_params.struct_val, offsetof(ir_expr_t, link));

        VEC_FOREACH(cur, &type->struct_params.types) {
            ir_type_t *cur_type = vec_get(&type->struct_params.types, cur);
            ir_expr_t *zero = ir_expr_zero(tunit, cur_type);
            sl_append(&expr->const_params.struct_val, &zero->link);
        }

        return expr;
    }

    case IR_TYPE_VOID:
    case IR_TYPE_FUNC:
    case IR_TYPE_OPAQUE:
    default:
        assert(false);
    }
}
