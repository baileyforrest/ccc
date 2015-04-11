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

#define IR_INT_LIT(width) { SL_LINK_LIT, IR_TYPE_INT, .int_params = { width } }

ir_type_t ir_type_void = { SL_LINK_LIT, IR_TYPE_VOID, { } };
ir_type_t ir_type_i8 = IR_INT_LIT(8);
ir_type_t ir_type_i16 = IR_INT_LIT(16);
ir_type_t ir_type_i32 = IR_INT_LIT(32);
ir_type_t ir_type_i64 = IR_INT_LIT(64);

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
                offsetof(ir_val_label_pair_t, link));
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
    switch (type) {
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
        sl_init(&expr->phi.preds, offsetof(ir_val_label_pair_t, link));
        break;
    case IR_EXPR_CALL:
        sl_init(&expr->call.arglist, offsetof(ir_type_expr_pair_t, link));
        break;
    case IR_EXPR_VA_ARG:
        sl_init(&expr->va_arg.arglist, offsetof(ir_type_expr_pair_t, link));
        break;
    default:
        assert(false);
    }
    return expr;
}
