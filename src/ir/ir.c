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

#include <assert.h>

#define MAX_LABEL_LEN 512
#define ANON_LABEL_PREFIX "BB"

#define IR_INT_LIT(width)                                   \
    { SL_LINK_LIT, IR_TYPE_INT, 0, .int_params = { width } }

#define IR_FLOAT_LIT(type)                                      \
    { SL_LINK_LIT, IR_TYPE_FLOAT, 0, .float_params = { type } }

ir_type_t ir_type_void = { SL_LINK_LIT, IR_TYPE_VOID, 0, { } };
ir_type_t ir_type_i1 = IR_INT_LIT(1);
ir_type_t ir_type_i8 = IR_INT_LIT(8);
ir_type_t ir_type_i16 = IR_INT_LIT(16);
ir_type_t ir_type_i32 = IR_INT_LIT(32);
ir_type_t ir_type_i64 = IR_INT_LIT(64);
ir_type_t ir_type_float = IR_FLOAT_LIT(IR_FLOAT_FLOAT);
ir_type_t ir_type_double = IR_FLOAT_LIT(IR_FLOAT_DOUBLE);
ir_type_t ir_type_x86_fp80 = IR_FLOAT_LIT(IR_FLOAT_X86_FP80);

void ir_print(FILE *stream, ir_trans_unit_t *irtree) {
    assert(stream != NULL);
    assert(irtree != NULL);
    // TODO: This
}

void ir_symtab_init(ir_symtab_t *symtab) {
    assert(symtab != NULL);

    static const ht_params_t ht_params = {
        0,                                 // Size estimate
        offsetof(ir_symtab_entry_t, name), // Offset of key
        offsetof(ir_symtab_entry_t, link), // Offset of ht link
        strhash,                           // Hash function
        vstrcmp,                           // void string compare
    };

    ht_init(&symtab->table, &ht_params);
}

ir_label_t *ir_label_create(ir_trans_unit_t *tunit, len_str_t *str) {
    ir_label_t *label = ht_lookup(&tunit->labels, str);
    if (label != NULL) {
        return label;
    }

    label = emalloc(sizeof(ir_label_t));
    label->name.str = str->str;
    label->name.len = str->len;
    status_t status = ht_insert(&tunit->labels, &label->link);
    assert(status == CCC_OK);

    return label;
}

ir_label_t *ir_numlabel_create(ir_trans_unit_t *tunit, int num) {
    assert(num >= 0);
    char buf[MAX_LABEL_LEN];
    snprintf(buf, sizeof(buf), ANON_LABEL_PREFIX "%d", num);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);
    len_str_t lookup = { buf, len };
    ir_label_t *label = ht_lookup(&tunit->labels, &lookup);
    if (label != NULL) {
        return label;
    }

    // Allocate the label and its string in one chunk
    label = emalloc(sizeof(ir_label_t) + len + 1);
    label->name.str = (char *)label + sizeof(*label);
    label->name.len = len;
    strcpy(label->name.str, buf);
    status_t status = ht_insert(&tunit->labels, &label->link);
    assert(status == CCC_OK);

    return label;
}

ir_expr_t *ir_temp_create(ir_type_t *type, int num) {
    assert(num >= 0);
    char buf[MAX_LABEL_LEN];
    snprintf(buf, sizeof(buf), "%d", num);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);
    ir_expr_t *temp = emalloc(sizeof(ir_expr_t) + len + 1);
    temp->type = IR_EXPR_VAR;
    temp->refcnt = 1;
    temp->var.type = type;
    temp->var.name.str = (char *)temp + sizeof(*temp);
    temp->var.name.len = len;
    strcpy(temp->var.name.str, buf);
    temp->var.local = true;

    return temp;
}

ir_trans_unit_t *ir_trans_unit_create(void) {
    ir_trans_unit_t *tunit = emalloc(sizeof(ir_trans_unit_t));
    sl_init(&tunit->gdecls, offsetof(ir_gdecl_t, link));
    ir_symtab_init(&tunit->globals);

    static const ht_params_t ht_params = {
        0,                          // Size estimate
        offsetof(ir_label_t, name), // Offset of key
        offsetof(ir_label_t, link), // Offset of ht link
        strhash,                    // Hash function
        vstrcmp,                    // void string compare
    };

    ht_init(&tunit->labels, &ht_params);
    return tunit;
}

ir_gdecl_t *ir_gdecl_create(ir_gdecl_type_t type) {
    ir_gdecl_t *gdecl = emalloc(sizeof(ir_gdecl_t));
    gdecl->type = type;
    switch (type) {
    case IR_GDECL_GDATA:
        break;
    case IR_GDECL_FUNC:
        sl_init(&gdecl->func.body, offsetof(ir_stmt_t, link));
        ir_symtab_init(&gdecl->func.locals);
    default:
        assert(false);
    }

    return gdecl;
}

ir_stmt_t *ir_stmt_create(ir_stmt_type_t type) {
    ir_stmt_t *stmt = emalloc(sizeof(ir_stmt_t));
    stmt->type = type;
    switch (stmt->type) {
    case IR_STMT_LABEL:
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

ir_expr_t *ir_expr_create(ir_expr_type_t type) {
    ir_expr_t *expr = emalloc(sizeof(ir_expr_t));
    expr->refcnt = 1;
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

ir_type_t *ir_type_create(ir_type_type_t type) {
    ir_type_t *ir_type = emalloc(sizeof(ir_type_t));
    ir_type->refcnt = 1;

    switch (type) {
    case IR_TYPE_VOID:
    case IR_TYPE_INT:
    case IR_TYPE_FLOAT:
        assert(false && "Use the static types");
        break;

    case IR_TYPE_FUNC:
        sl_init(&ir_type->func.params, offsetof(ir_type_t, link));
        break;
    case IR_TYPE_STRUCT:
        sl_init(&ir_type->struct_params.types, offsetof(ir_type_t, link));
        break;

    case IR_TYPE_PTR:
    case IR_TYPE_ARR:
    case IR_TYPE_OPAQUE:
        break;
    default:
        assert(false);
    }

    return ir_type;
}

ir_expr_t *ir_expr_ref(ir_expr_t *expr) {
    assert(expr->refcnt >= 1);
    ++expr->refcnt;
    return expr;
}

ir_type_t *ir_type_ref(ir_type_t *type) {
    switch (type->type) {
    case IR_TYPE_VOID:
    case IR_TYPE_INT:
    case IR_TYPE_FLOAT:
        return type;
    default:
        break;
    }
    assert(type->refcnt >= 1);
    ++type->refcnt;
    return type;
}

void ir_type_destroy(ir_type_t *type) {
    switch (type->type) {
    case IR_TYPE_VOID:
    case IR_TYPE_INT:
    case IR_TYPE_FLOAT:
        // These types are allocated statically
        return;
    default:
        break;
    }
    if (--type->refcnt > 0) {
        return;
    }

    switch (type->type) {
    case IR_TYPE_FUNC:
        ir_type_destroy(type->func.type);
        SL_DESTROY_FUNC(&type->func.params, ir_type_destroy);
        break;
    case IR_TYPE_PTR:
        ir_type_destroy(type->ptr.base);
        break;
    case IR_TYPE_ARR:
        ir_type_destroy(type->arr.elem_type);
        break;
    case IR_TYPE_STRUCT:
        SL_DESTROY_FUNC(&type->struct_params.types, ir_type_destroy);
        break;
    case IR_TYPE_OPAQUE:
        break;
    default:
        assert(false);
    }
    free(type);
}

void ir_type_expr_pair_destroy(ir_type_expr_pair_t *pair) {
    ir_type_destroy(pair->type);
    ir_expr_destroy(pair->expr);
    free(pair);
}

void ir_expr_label_pair_destroy(ir_expr_label_pair_t *pair) {
    ir_expr_destroy(pair->expr);
    free(pair);
}

void ir_expr_destroy(ir_expr_t *expr) {
    if (--expr->refcnt > 0) {
        return;
    }
    switch (expr->type) {
    case IR_EXPR_VAR:
        ir_type_destroy(expr->var.type);
        break;
    case IR_EXPR_CONST:
        switch (expr->const_params.ctype) {
        case IR_CONST_BOOL:
        case IR_CONST_INT:
        case IR_CONST_FLOAT:
        case IR_CONST_NULL:
        case IR_CONST_ZERO:
            break;
        case IR_CONST_STRUCT:
            SL_DESTROY_FUNC(&expr->const_params.struct_val,
                            ir_type_expr_pair_destroy);
            break;
        case IR_CONST_ARR:
            SL_DESTROY_FUNC(&expr->const_params.struct_val,
                            ir_expr_destroy);
            break;
        default:
            assert(false);
        }
        break;
    case IR_EXPR_BINOP:
        ir_type_destroy(expr->binop.type);
        ir_expr_destroy(expr->binop.expr1);
        ir_expr_destroy(expr->binop.expr2);
        break;
    case IR_EXPR_ALLOCA:
        ir_type_destroy(expr->alloca.type);
        ir_type_destroy(expr->alloca.nelem_type);
        break;
    case IR_EXPR_LOAD:
        ir_type_destroy(expr->load.type);
        ir_expr_destroy(expr->load.ptr);
        break;
    case IR_EXPR_GETELEMPTR:
        ir_type_destroy(expr->getelemptr.type);
        ir_expr_destroy(expr->getelemptr.ptr_val);
        SL_DESTROY_FUNC(&expr->getelemptr.idxs, ir_type_expr_pair_destroy);
        break;
    case IR_EXPR_CONVERT:
        ir_type_destroy(expr->convert.src_type);
        ir_expr_destroy(expr->convert.val);
        ir_type_destroy(expr->convert.dest_type);
        break;
    case IR_EXPR_ICMP:
        ir_type_destroy(expr->icmp.type);
        ir_expr_destroy(expr->icmp.expr1);
        ir_expr_destroy(expr->icmp.expr2);
        break;
    case IR_EXPR_FCMP:
        ir_type_destroy(expr->fcmp.type);
        ir_expr_destroy(expr->fcmp.expr1);
        ir_expr_destroy(expr->fcmp.expr2);
        break;
    case IR_EXPR_PHI:
        ir_type_destroy(expr->phi.type);
        SL_DESTROY_FUNC(&expr->phi.preds, ir_expr_label_pair_destroy);
        break;
    case IR_EXPR_SELECT:
        ir_type_destroy(expr->select.selty);
        ir_expr_destroy(expr->select.cond);
        ir_type_destroy(expr->select.type1);
        ir_expr_destroy(expr->select.expr1);
        ir_type_destroy(expr->select.type2);
        ir_expr_destroy(expr->select.expr2);
        break;
    case IR_EXPR_CALL:
        ir_type_destroy(expr->call.func_sig);
        ir_expr_destroy(expr->call.func_ptr);
        SL_DESTROY_FUNC(&expr->call.arglist, ir_type_expr_pair_destroy);
        break;
    case IR_EXPR_VAARG:
        ir_type_destroy(expr->vaarg.va_list_type);
        SL_DESTROY_FUNC(&expr->vaarg.arglist, ir_type_expr_pair_destroy);
        ir_type_destroy(expr->vaarg.arg_type);
        break;
    default:
        assert(false);
    }
    free(expr);
}

void ir_stmt_destroy(ir_stmt_t *stmt) {
    switch (stmt->type) {
    case IR_STMT_LABEL:
        break;
    case IR_STMT_RET:
        ir_type_destroy(stmt->ret.type);
        ir_expr_destroy(stmt->ret.val);
        break;
    case IR_STMT_BR:
        if (stmt->br.cond != NULL) {
            ir_expr_destroy(stmt->br.cond);
        }
        break;
    case IR_STMT_SWITCH:
        ir_expr_destroy(stmt->switch_params.expr);
        SL_DESTROY_FUNC(&stmt->switch_params.cases, ir_expr_label_pair_destroy);
        break;
    case IR_STMT_INDIR_BR:
        ir_type_destroy(stmt->indirectbr.type);
        ir_expr_destroy(stmt->indirectbr.addr);
        SL_DESTROY_FUNC(&stmt->indirectbr.labels, free);
        break;
    case IR_STMT_ASSIGN:
        ir_expr_destroy(stmt->assign.dest);
        ir_expr_destroy(stmt->assign.src);
        break;
    case IR_STMT_STORE:
        ir_type_destroy(stmt->store.type);
        ir_expr_destroy(stmt->store.val);
        ir_expr_destroy(stmt->store.ptr);
        break;
    case IR_STMT_INTRINSIC_FUNC:
        ir_type_destroy(stmt->intrinsic_func.func_sig);
        break;
    default:
        assert(false);
    }
    free(stmt);
}

void ir_gdecl_destroy(ir_gdecl_t *gdecl) {
    switch (gdecl->type) {
    case IR_GDECL_GDATA:
        SL_DESTROY_FUNC(&gdecl->gdata.stmts, ir_stmt_destroy);
        break;
    case IR_GDECL_FUNC:
        ir_type_destroy(gdecl->func.type);
        SL_DESTROY_FUNC(&gdecl->func.body, ir_stmt_destroy);
        break;
    default:
        assert(false);
    }
    free(gdecl);
}

void ir_trans_unit_destroy(ir_trans_unit_t *trans_unit) {
    SL_DESTROY_FUNC(&trans_unit->gdecls, ir_gdecl_destroy);
    ir_symtab_destroy(&trans_unit->globals);
    HT_DESTROY_FUNC(&trans_unit->labels, free);
    free(trans_unit);
}

void ir_symtab_entry_destroy(ir_symtab_entry_t *entry) {
    ir_type_destroy(entry->entry_type);
    switch (entry->type) {
    case IR_SYMTAB_ENTRY_GDECL:
        ir_gdecl_destroy(entry->gdecl);
        break;
    case IR_SYMTAB_ENTRY_LOCAL:
        ir_expr_destroy(entry->local);
        break;
    default:
        assert(false);
    }
    free(entry);
}

void ir_symtab_destroy(ir_symtab_t *symtab) {
    HT_DESTROY_FUNC(&symtab->table, ir_symtab_entry_destroy);
}
