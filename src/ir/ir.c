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

#define MAX_LABEL_LEN 512
#define ANON_LABEL_PREFIX "BB"

#define IR_INT_LIT(width)                                   \
    { SL_LINK_LIT, IR_TYPE_INT, .int_params = { width } }

#define IR_FLOAT_LIT(type)                                      \
    { SL_LINK_LIT, IR_TYPE_FLOAT, .float_params = { type } }

ir_type_t ir_type_void = { SL_LINK_LIT, IR_TYPE_VOID, { } };
ir_type_t ir_type_i1 = IR_INT_LIT(1);
ir_type_t ir_type_i8 = IR_INT_LIT(8);
ir_type_t ir_type_i16 = IR_INT_LIT(16);
ir_type_t ir_type_i32 = IR_INT_LIT(32);
ir_type_t ir_type_i64 = IR_INT_LIT(64);
ir_type_t ir_type_float = IR_FLOAT_LIT(IR_FLOAT_FLOAT);
ir_type_t ir_type_double = IR_FLOAT_LIT(IR_FLOAT_DOUBLE);
ir_type_t ir_type_x86_fp80 = IR_FLOAT_LIT(IR_FLOAT_X86_FP80);

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
    label = emalloc(sizeof(ir_label_t) + strlen(buf) + 1);
    label->name = (char *)label + sizeof(*label);
    strcpy(label->name, buf);
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
    size_t len = strlen(buf);
    ir_expr_t *temp = emalloc(sizeof(ir_expr_t) + len + 1);
    temp->type = IR_EXPR_VAR;
    temp->var.type = type;
    temp->var.name = (char *)temp + sizeof(*temp);
    strcpy(temp->var.name, buf);
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
    switch (type) {
    case IR_GDECL_FUNC_DECL:
    case IR_GDECL_ID_STRUCT:
        break;
    case IR_GDECL_GDATA:
        dl_init(&gdecl->gdata.stmts.list, offsetof(ir_stmt_t, link));
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
    case IR_STMT_INTRINSIC_FUNC:
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
        sl_init(&expr->getelemptr.idxs, offsetof(ir_type_expr_pair_t, link));
        break;
    case IR_EXPR_PHI:
        sl_init(&expr->phi.preds, offsetof(ir_expr_label_pair_t, link));
        break;
    case IR_EXPR_CALL:
        sl_init(&expr->call.arglist, offsetof(ir_type_expr_pair_t, link));
        break;
    case IR_EXPR_VAARG:
        sl_init(&expr->vaarg.arglist, offsetof(ir_type_expr_pair_t, link));
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
        case IR_CONST_BOOL:
        case IR_CONST_INT:
        case IR_CONST_FLOAT:
        case IR_CONST_NULL:
        case IR_CONST_ZERO:
        case IR_CONST_STR:
        case IR_CONST_ARR:
            break;
        case IR_CONST_STRUCT:
            SL_DESTROY_FUNC(&expr->const_params.struct_val, free);
            break;
        default:
            assert(false);
        }
        break;
    case IR_EXPR_GETELEMPTR:
        SL_DESTROY_FUNC(&expr->getelemptr.idxs, free);
        break;
    case IR_EXPR_PHI:
        SL_DESTROY_FUNC(&expr->phi.preds, free);
        break;
    case IR_EXPR_CALL:
        SL_DESTROY_FUNC(&expr->call.arglist, free);
        break;
    case IR_EXPR_VAARG:
        SL_DESTROY_FUNC(&expr->vaarg.arglist, free);
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
    case IR_STMT_INTRINSIC_FUNC:
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
        break;
    case IR_GDECL_ID_STRUCT:
        free(gdecl->id_struct.name);
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

ir_expr_t *ir_expr_zero(ir_type_t *type) {
    switch (type->type) {
    case IR_TYPE_INT:
        switch (type->int_params.width) {
        case 1: {
            static ir_expr_t expr =
                { SL_LINK_LIT, SL_LINK_LIT, IR_EXPR_CONST,
                  { .const_params = { IR_CONST_INT, &ir_type_i1,
                                      { .int_val = 0 } } } };
            return &expr;
        }
        case 8: {
            static ir_expr_t expr =
                { SL_LINK_LIT, SL_LINK_LIT, IR_EXPR_CONST,
                  { .const_params = { IR_CONST_INT, &ir_type_i8,
                                      { .int_val = 0 } } } };
            return &expr;
        }
        case 16: {
            static ir_expr_t expr =
                { SL_LINK_LIT, SL_LINK_LIT, IR_EXPR_CONST,
                  { .const_params = { IR_CONST_INT, &ir_type_i16,
                                      { .int_val = 0 } } } };
            return &expr;
        }
        case 32: {
            static ir_expr_t expr =
                { SL_LINK_LIT, SL_LINK_LIT, IR_EXPR_CONST,
                  { .const_params = { IR_CONST_INT, &ir_type_i32,
                                      { .int_val = 0 } } } };
            return &expr;
        }
        case 64: {
            static ir_expr_t expr =
                { SL_LINK_LIT, SL_LINK_LIT, IR_EXPR_CONST,
                  { .const_params = { IR_CONST_INT, &ir_type_i64,
                                      { .int_val = 0 } } } };
            return &expr;
        }
        default:
            assert(false);
        }
        return NULL;
    case IR_TYPE_FLOAT:
        switch (type->float_params.type) {
        case IR_FLOAT_FLOAT: {
            static ir_expr_t expr =
                { SL_LINK_LIT, SL_LINK_LIT, IR_EXPR_CONST,
                  { .const_params = { IR_CONST_FLOAT, &ir_type_float,
                                      { .float_val = 0.0 } } } };
            return &expr;
        }
        case IR_FLOAT_DOUBLE: {
            static ir_expr_t expr =
                { SL_LINK_LIT, SL_LINK_LIT, IR_EXPR_CONST,
                  { .const_params = { IR_CONST_FLOAT, &ir_type_double,
                                      { .float_val = 0.0 } } } };
            return &expr;
        }
        case IR_FLOAT_X86_FP80: {
            static ir_expr_t expr =
                { SL_LINK_LIT, SL_LINK_LIT, IR_EXPR_CONST,
                  { .const_params = { IR_CONST_FLOAT, &ir_type_x86_fp80,
                                      { .float_val = 0.0 } } } };
            return &expr;
        }
        default:
            assert(false);
        }
        return NULL;
    case IR_TYPE_PTR: {
        static ir_type_t ptr = { SL_LINK_LIT, IR_TYPE_PTR,
                                 .ptr = { &ir_type_void } };
        static ir_expr_t expr =
            { SL_LINK_LIT, SL_LINK_LIT, IR_EXPR_CONST,
              .const_params = { IR_CONST_NULL, &ptr, { } } };
        return &expr;
    }

    case IR_TYPE_VOID:
    case IR_TYPE_FUNC:
    case IR_TYPE_ARR:
    case IR_TYPE_STRUCT:
    case IR_TYPE_ID_STRUCT:
    case IR_TYPE_OPAQUE:
    default:
        assert(false);
    }
}
