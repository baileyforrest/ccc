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
#define INDENT "    "
#define DATALAYOUT "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
#define TRIPLE "x86_64-unknown-linux-gnu"

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

void ir_print(FILE *stream, ir_trans_unit_t *irtree, const char *module_name) {
    assert(stream != NULL);
    assert(irtree != NULL);
    assert(module_name != NULL);

    fprintf(stream, "; ModuleID = '%s'\n", module_name);

    ir_trans_unit_print(stream, irtree);
}

ir_type_t *ir_expr_type(ir_expr_t *expr) {
    switch (expr->type) {
    case IR_EXPR_VAR:
        return expr->var.type;
    case IR_EXPR_CONST:
        return expr->const_params.type;
    case IR_EXPR_BINOP:
        return expr->binop.type;
    case IR_EXPR_LOAD:
        return expr->load.type;
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
    case IR_EXPR_ALLOCA:
    case IR_EXPR_GETELEMPTR:
    default:
        assert(false);
    }
    return NULL;
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

ir_expr_t *ir_temp_create(ir_gdecl_t *func, ir_type_t *type, int num) {
    assert(num >= 0);
    assert(func->type == IR_GDECL_FUNC);

    char buf[MAX_LABEL_LEN];
    snprintf(buf, sizeof(buf), "%d", num);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);
    ir_expr_t *temp = emalloc(sizeof(ir_expr_t) + len + 1);
    temp->type = IR_EXPR_VAR;
    temp->var.type = type;
    temp->var.name.str = (char *)temp + sizeof(*temp);
    temp->var.name.len = len;
    strcpy(temp->var.name.str, buf);
    temp->var.local = true;

    ir_symtab_entry_t *entry = ir_symtab_entry_create(IR_SYMTAB_ENTRY_VAR,
                                                      &temp->var.name);
    entry->var = temp;
    status_t status = ir_symtab_insert(&func->func.locals, entry);
    assert(status == CCC_OK);

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
        sl_init(&gdecl->func.allocs, offsetof(ir_stmt_t, link));
        sl_init(&gdecl->func.body, offsetof(ir_stmt_t, link));
        ir_symtab_init(&gdecl->func.locals);
        // Must start with 1, because %0 is for entry block label
        gdecl->func.next_temp = 1;
        gdecl->func.next_label = 0;
        break;
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
    expr->type = type;

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
    ir_type->type = type;

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

void ir_expr_var_destroy(ir_expr_t *expr) {
    assert(expr->type == IR_EXPR_VAR);

    ir_type_destroy(expr->var.type);
    free(expr);
}

void ir_expr_destroy(ir_expr_t *expr) {
    switch (expr->type) {
    case IR_EXPR_VAR:
        // Variables are stored in symtab
        return;
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
        if (expr->alloca.nelem_type != NULL) {
            ir_type_destroy(expr->alloca.nelem_type);
        }
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
        ir_expr_destroy(expr->select.cond);
        ir_type_destroy(expr->select.type);
        ir_expr_destroy(expr->select.expr1);
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
        SL_DESTROY_FUNC(&gdecl->func.allocs, ir_stmt_destroy);
        SL_DESTROY_FUNC(&gdecl->func.body, ir_stmt_destroy);
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
    SL_DESTROY_FUNC(&trans_unit->gdecls, ir_gdecl_destroy);
    ir_symtab_destroy(&trans_unit->globals);
    HT_DESTROY_FUNC(&trans_unit->labels, free);
    free(trans_unit);
}

void ir_trans_unit_print(FILE *stream, ir_trans_unit_t *irtree) {
    fprintf(stream, "target datalayout = \"%s\"\n", DATALAYOUT);
    fprintf(stream, "target triple = \"%s\"\n", TRIPLE);
    fprintf(stream, "\n");
    SL_FOREACH(cur, &irtree->gdecls) {
        ir_gdecl_print(stream, GET_ELEM(&irtree->gdecls, cur));
    }
}

void ir_gdecl_print(FILE *stream, ir_gdecl_t *gdecl) {
    switch (gdecl->type) {
    case IR_GDECL_GDATA: {
        SL_FOREACH(cur, &gdecl->gdata.stmts) {
            ir_stmt_print(stream, GET_ELEM(&gdecl->gdata.stmts, cur), false);
        }
        break;
    }
    case IR_GDECL_FUNC:
        fprintf(stream, "define ");
        ir_type_print(stream, gdecl->func.type, &gdecl->func.name);
        fprintf(stream, " {\n");
        SL_FOREACH(cur, &gdecl->func.allocs) {
            ir_stmt_print(stream, GET_ELEM(&gdecl->func.allocs, cur), true);
        }
        SL_FOREACH(cur, &gdecl->func.body) {
            ir_stmt_print(stream, GET_ELEM(&gdecl->func.body, cur), true);
        }
        fprintf(stream, "}\n\n");
        break;
    default:
        assert(false);
    }
}

void ir_stmt_print(FILE *stream, ir_stmt_t *stmt, bool indent) {
    if (indent && stmt->type != IR_STMT_LABEL) {
        fprintf(stream, INDENT);
    }
    switch (stmt->type) {
    case IR_STMT_LABEL:
        fprintf(stream, "%.*s:", (int)stmt->label->name.len,
                stmt->label->name.str);
        break;
    case IR_STMT_RET:
        fprintf(stream, "ret ");
        ir_type_print(stream, stmt->ret.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, stmt->ret.val);
        break;
    case IR_STMT_BR:
        fprintf(stream, "br ");
        if (stmt->br.cond == NULL) {
            fprintf(stream, "label %%%.*s", (int)stmt->br.uncond->name.len,
                    stmt->br.uncond->name.str);
        } else {
            fprintf(stream, " i1 ");
            ir_expr_print(stream, stmt->br.cond);
            fprintf(stream, ", label %%%.*s, label %%%.*s",
                    (int)stmt->br.if_true->name.len, stmt->br.if_true->name.str,
                    (int)stmt->br.if_false->name.len,
                    stmt->br.if_false->name.str);
        }
        break;
    case IR_STMT_SWITCH:
        fprintf(stream, "switch ");
        ir_type_print(stream, &SWITCH_VAL_TYPE, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, stmt->switch_params.expr);
        fprintf(stream, ", label %%%.*s [ ",
                (int)stmt->switch_params.default_case->name.len,
                stmt->switch_params.default_case->name.str);
        SL_FOREACH(cur, &stmt->switch_params.cases) {
            ir_expr_label_pair_t *pair =
                GET_ELEM(&stmt->switch_params.cases, cur);
            ir_type_print(stream, &SWITCH_VAL_TYPE, NULL);
            ir_expr_print(stream, pair->expr);
            fprintf(stream, " ");
            fprintf(stream, ", label %%%.*s ",
                    (int)pair->label->name.len, pair->label->name.str);
        }
        fprintf(stream, "]");
        break;
    case IR_STMT_INDIR_BR:
        // TODO0: Remove if unused
        break;
    case IR_STMT_ASSIGN:
        ir_expr_print(stream, stmt->assign.dest);
        fprintf(stream, " = ");
        ir_expr_print(stream, stmt->assign.src);
        break;
    case IR_STMT_STORE:
        fprintf(stream, "store ");
        ir_type_print(stream, stmt->store.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, stmt->store.val);
        fprintf(stream, ", ");
        ir_type_print(stream, stmt->store.type, NULL);
        fprintf(stream, "* ");
        ir_expr_print(stream, stmt->store.ptr);
        break;
    case IR_STMT_INTRINSIC_FUNC:
        // TODO0: This
        break;
    default:
        assert(false);
    }
    fprintf(stream, "\n");
}

void ir_expr_print(FILE *stream, ir_expr_t *expr) {
    switch (expr->type) {
    case IR_EXPR_VAR:
        expr->var.local ? fprintf(stream, "%%") : fprintf(stream, "@");
        fprintf(stream, "%.*s", (int)expr->var.name.len, expr->var.name.str);
        break;
    case IR_EXPR_CONST:
        switch (expr->const_params.ctype) {
        case IR_CONST_BOOL:
            if (expr->const_params.bool_val) {
                fprintf(stream, "true");
            } else {
                fprintf(stream, "false");
            }
            break;
        case IR_CONST_INT:
            fprintf(stream, "%lld", expr->const_params.int_val);
            break;
        case IR_CONST_FLOAT:
            fprintf(stream, "%Lf", expr->const_params.float_val);
            break;
        case IR_CONST_NULL:
            fprintf(stream, "null");

            break;
        case IR_CONST_STRUCT:
            fprintf(stream, "{ ");
            SL_FOREACH(cur, &expr->const_params.struct_val) {
                ir_type_expr_pair_t *pair =
                    GET_ELEM(&expr->const_params.struct_val, cur);
                ir_type_print(stream, pair->type, NULL);
                fprintf(stream, " ");
                ir_expr_print(stream, pair->expr);
                if (pair != sl_tail(&expr->const_params.struct_val)) {
                    fprintf(stream, " ,");
                }
            }
            fprintf(stream, " }");
            break;
        case IR_CONST_ARR: {
            fprintf(stream, "[ ");
            assert(expr->const_params.type->type == IR_TYPE_ARR);
            ir_type_t *elem_type = expr->const_params.type->arr.elem_type;
            SL_FOREACH(cur, &expr->const_params.struct_val) {
                ir_expr_t *elem = GET_ELEM(&expr->const_params.arr_val, cur);
                ir_type_print(stream, elem_type, NULL);
                fprintf(stream, " ");
                ir_expr_print(stream, elem);
                if (elem != sl_tail(&expr->const_params.arr_val)) {
                    fprintf(stream, " ,");
                }
            }
            fprintf(stream, " ]");
            break;
        }
        case IR_CONST_ZERO:
            fprintf(stream, "zeroinitializer");
            break;
        default:
            assert(false);
        }
        break;
    case IR_EXPR_BINOP: {
        fprintf(stream, "%s ", ir_oper_str(expr->binop.op));
        ir_type_print(stream, expr->binop.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->binop.expr1);
        fprintf(stream, ", ");
        ir_expr_print(stream, expr->binop.expr2);
        break;
    }
    case IR_EXPR_ALLOCA:
        fprintf(stream, "alloca ");
        ir_type_print(stream, expr->alloca.type, NULL);
        if (expr->alloca.nelem_type != NULL) {
            fprintf(stream, ", ");
            ir_type_print(stream, expr->alloca.nelem_type, NULL);
            fprintf(stream, " %d", expr->alloca.nelems);
        }
        if (expr->alloca.align != 0) {
            fprintf(stream, ", align %d", expr->alloca.align);
        }
        break;
    case IR_EXPR_LOAD:
        fprintf(stream, "load ");
        ir_type_print(stream, expr->load.type, NULL);
        fprintf(stream, "* ");
        ir_expr_print(stream, expr->load.ptr);
        break;
    case IR_EXPR_GETELEMPTR:
        fprintf(stream, "getelementptr ");
        ir_type_print(stream, expr->getelemptr.type, NULL);
        fprintf(stream, ", ");
        ir_type_print(stream, expr->getelemptr.type, NULL);
        fprintf(stream, "* ");
        ir_expr_print(stream, expr->getelemptr.ptr_val);
        fprintf(stream, ", ");
        SL_FOREACH(cur, &expr->getelemptr.idxs) {
            ir_type_expr_pair_t *pair = GET_ELEM(&expr->getelemptr.idxs, cur);
            ir_type_print(stream, pair->type, NULL);
            fprintf(stream, " ");
            ir_expr_print(stream, pair->expr);
            if (pair != sl_tail(&expr->getelemptr.idxs)) {
                fprintf(stream, ", ");
            }
        }
        break;
    case IR_EXPR_CONVERT:
        fprintf(stream, "%s ", ir_convert_str(expr->convert.type));
        ir_type_print(stream, expr->convert.src_type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->convert.val);
        fprintf(stream, " to ");
        ir_type_print(stream, expr->convert.dest_type, NULL);
        break;
    case IR_EXPR_ICMP:
        fprintf(stream, "icmp %s ", ir_icmp_str(expr->icmp.cond));
        ir_type_print(stream, expr->icmp.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->icmp.expr1);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->icmp.expr2);
        break;
    case IR_EXPR_FCMP:
        fprintf(stream, "fcmp %s ", ir_icmp_str(expr->fcmp.cond));
        ir_type_print(stream, expr->fcmp.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->fcmp.expr1);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->fcmp.expr2);
        break;
    case IR_EXPR_PHI: {
        fprintf(stream, "phi ");
        ir_type_print(stream, expr->phi.type, NULL);
        SL_FOREACH(cur, &expr->phi.preds) {
            ir_expr_label_pair_t *pair = GET_ELEM(&expr->phi.preds, cur);
            fprintf(stream, "[ ");
            ir_expr_print(stream, pair->expr);
            fprintf(stream, ", %%%.*s",
                    (int)pair->label->name.len, pair->label->name.str);
            fprintf(stream, " ]");
            if (pair != sl_tail(&expr->phi.preds)) {
                fprintf(stream, ", ");
            }
        }
        break;
    }
    case IR_EXPR_SELECT:
        fprintf(stream, "select i1 ");
        ir_expr_print(stream, expr->select.cond);
        fprintf(stream, ", ");
        ir_type_print(stream, expr->select.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->select.expr1);
        fprintf(stream, ", ");
        ir_type_print(stream, expr->select.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->select.expr1);
        break;
    case IR_EXPR_CALL: {
        assert(expr->call.func_sig->type == IR_TYPE_FUNC);
        ir_type_t *func_sig = expr->call.func_sig;
        fprintf(stream, "call ");
        ir_type_print(stream, func_sig->func.type, NULL);
        ir_expr_print(stream, expr->call.func_ptr);
        fprintf(stream, " (");

        SL_FOREACH(cur, &expr->call.arglist) {
            ir_type_expr_pair_t *pair = GET_ELEM(&expr->call.arglist, cur);
            ir_type_print(stream, pair->type, NULL);
            fprintf(stream, " ");
            ir_expr_print(stream, pair->expr);
            if (pair != sl_tail(&expr->call.arglist)) {
                fprintf(stream, ", ");
            }
        }
        fprintf(stream, ")");
        break;
    }
    case IR_EXPR_VAARG:
        // TODO0: This
        break;
    default:
        assert(false);
    }
}

void ir_type_print(FILE *stream, ir_type_t *type, len_str_t *func_name) {
    switch (type->type) {
    case IR_TYPE_VOID:
        fprintf(stream, "void");
        break;
    case IR_TYPE_FUNC:
        ir_type_print(stream, type->func.type, NULL);
        if (func_name != NULL) {
            fprintf(stream, " @%.*s", (int)func_name->len, func_name->str);
        }
        fprintf(stream, " (");
        SL_FOREACH(cur, &type->func.params) {
            ir_type_t *arg = GET_ELEM(&type->func.params, cur);
            ir_type_print(stream, arg, NULL);
            if (arg != sl_tail(&type->func.params)) {
                fprintf(stream, ", ");
            }
        }
        fprintf(stream, ")");
        break;
    case IR_TYPE_INT:
        fprintf(stream, "i%d", type->int_params.width);
        break;
    case IR_TYPE_FLOAT:
        fprintf(stream, "%s", ir_float_type_str(type->float_params.type));
        break;
    case IR_TYPE_PTR:
        ir_type_print(stream, type->ptr.base, NULL);
        fprintf(stream, "*");
        break;
    case IR_TYPE_ARR:
        fprintf(stream, "[ %zu x ", type->arr.nelems);
        ir_type_print(stream, type->arr.elem_type, NULL);
        fprintf(stream, " ]");
        break;
    case IR_TYPE_STRUCT: {
        fprintf(stream, "type { ");
        SL_FOREACH(cur, &type->struct_params.types) {
            ir_type_t *elem = GET_ELEM(&type->struct_params.types, cur);
            ir_type_print(stream, elem, NULL);
            if (elem != sl_head(&type->struct_params.types)) {
                fprintf(stream, ", ");
            }
        }
        fprintf(stream, " }");
        break;
    }
    case IR_TYPE_OPAQUE:
        fprintf(stream, "type opaque");
        break;
    default:
        assert(false);
    }
}

const char *ir_oper_str(ir_oper_t op) {
    switch (op) {
    case IR_OP_ADD:  return "add";
    case IR_OP_FADD: return "fadd";
    case IR_OP_SUB:  return "sub";
    case IR_OP_FSUB: return "fsub";
    case IR_OP_MUL:  return "mul";
    case IR_OP_FMUL: return "fmul";
    case IR_OP_UDIV: return "udiv";
    case IR_OP_SDIV: return "sdiv";
    case IR_OP_FDIV: return "fdiv";
    case IR_OP_UREM: return "urem";
    case IR_OP_SREM: return "srem";
    case IR_OP_FREM: return "frem";
    case IR_OP_SHL:  return "shl";
    case IR_OP_LSHR: return "lshr";
    case IR_OP_ASHR: return "ashr";
    case IR_OP_AND:  return "and";
    case IR_OP_OR:   return "or";
    case IR_OP_XOR:  return "xor";
    default:
        assert(false);
    }
    return NULL;
}

const char *ir_convert_str(ir_convert_t conv) {
    switch (conv) {
    case IR_CONVERT_TRUNC:    return "trunc";
    case IR_CONVERT_ZEXT:     return "zext";
    case IR_CONVERT_SEXT:     return "sext";
    case IR_CONVERT_FPTRUNC:  return "fptruc";
    case IR_CONVERT_FPEXT:    return "fpext";
    case IR_CONVERT_FPTOUI:   return "fptoui";
    case IR_CONVERT_FPTOSI:   return "fptosi";
    case IR_CONVERT_UITOFP:   return "uitopf";
    case IR_CONVERT_SITOFP:   return "sitofp";
    case IR_CONVERT_PTRTOINT: return "ptrtoint";
    case IR_CONVERT_INTTOPTR: return "inttoptr";
    default:
        assert(false);
    }

    return NULL;
}

const char *ir_icmp_str(ir_icmp_type_t conv) {
    switch (conv) {
    case IR_ICMP_EQ:  return "eq";
    case IR_ICMP_NE:  return "ne";
    case IR_ICMP_UGT: return "ugt";
    case IR_ICMP_UGE: return "uge";
    case IR_ICMP_ULT: return "ult";
    case IR_ICMP_ULE: return "ule";
    case IR_ICMP_SGT: return "sgt";
    case IR_ICMP_SGE: return "sge";
    case IR_ICMP_SLT: return "slt";
    case IR_ICMP_SLE: return "sle";
    default:
        assert(false);
    }
    return NULL;
}

const char *ir_fcmp_str(ir_fcmp_type_t conv) {
    switch (conv) {
    case IR_FCMP_FALSE: return "false";
    case IR_FCMP_OEQ:   return "oeq";
    case IR_FCMP_OGT:   return "ogt";
    case IR_FCMP_OGE:   return "oge";
    case IR_FCMP_OLT:   return "olt";
    case IR_FCMP_OLE:   return "ole";
    case IR_FCMP_ONE:   return "one";
    case IR_FCMP_ORD:   return "ord";
    case IR_FCMP_UEQ:   return "ueq";
    case IR_FCMP_UGT:   return "ugt";
    case IR_FCMP_UGE:   return "uge";
    case IR_FCMP_ULT:   return "ule";
    case IR_FCMP_ULE:   return "ule";
    case IR_FCMP_UNE:   return "une";
    case IR_FCMP_UNO:   return "uno";
    case IR_FCMP_TRUE:  return "true";
    default:
        assert(false);
    }

    return NULL;
}

const char *ir_float_type_str(ir_float_type_t ftype) {
    switch (ftype) {
    case IR_FLOAT_FLOAT:    return "float";
    case IR_FLOAT_DOUBLE:   return "double";
    case IR_FLOAT_X86_FP80: return "x86_fp80";
    default:
        assert(false);
    }
    return NULL;
}
