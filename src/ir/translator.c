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
 * AST to IR translator implementation
 */

#include "translator.h"
#include "translator_priv.h"

#include <assert.h>

#include "util/util.h"
#include "typecheck/typechecker.h"

ir_trans_unit_t *trans_translate(trans_unit_t *ast) {
    assert(ast != NULL);

    trans_state_t ts = TRANS_STATE_LIT;
    return trans_trans_unit(&ts, ast);
}

ir_trans_unit_t *trans_trans_unit(trans_state_t *ts, trans_unit_t *ast) {
    ir_trans_unit_t *tunit = ir_trans_unit_create();
    ts->tunit = tunit;
    sl_link_t *cur;
    SL_FOREACH(cur, &ast->gdecls) {
        gdecl_t *gdecl = GET_ELEM(&ast->gdecls, cur);
        trans_gdecl(ts, gdecl, &tunit->gdecls);
    }

    return tunit;
}

void trans_gdecl(trans_state_t *ts, gdecl_t *gdecl, slist_t *ir_gdecls) {
    switch (gdecl->type) {
    case GDECL_FDEFN: {
        decl_node_t *node = sl_head(&gdecl->decl->decls);
        assert(node != NULL);
        assert(node == sl_tail(&gdecl->decl->decls));
        ir_gdecl_t *ir_gdecl = ir_gdecl_create(IR_GDECL_FUNC);
        ir_gdecl->func.type = trans_type(ts, node->type);
        ir_gdecl->func.name.str = node->id->str;
        ir_gdecl->func.name.len = node->id->len;

        sl_link_t *cur;
        assert(gdecl->fdefn.stmt->type == STMT_COMPOUND);
        SL_FOREACH(cur, &gdecl->fdefn.stmt->compound.stmts) {
            stmt_t *stmt = GET_ELEM(&gdecl->fdefn.stmt->compound.stmts, cur);
            trans_stmt(ts, stmt, &ir_gdecl->func.body);
        }
        sl_append(ir_gdecls, &ir_gdecl->link);
        break;
    }
    case GDECL_DECL: {
        sl_link_t *cur;
        SL_FOREACH(cur, &gdecl->decl->decls) {
            decl_node_t *node = GET_ELEM(&gdecl->decl->decls, cur);
            ir_gdecl_t *ir_gdecl = ir_gdecl_create(IR_GDECL_GDATA);
            ir_gdecl->gdata.stmt = trans_decl_node(ts, node);
            sl_append(ir_gdecls, &ir_gdecl->link);
        }
        break;
    }
    default:
        assert(false);
    }
}

void trans_stmt(trans_state_t *ts, stmt_t *stmt, slist_t *ir_stmts) {
    switch (stmt->type) {
    case STMT_NOP:
        break;

    case STMT_DECL: {
        sl_link_t *cur;
        SL_FOREACH(cur, &stmt->decl->decls) {
            decl_node_t *node = GET_ELEM(&stmt->decl->decls, cur);
            ir_stmt_t *ir_stmt = trans_decl_node(ts, node);
            sl_append(ir_stmts, &ir_stmt->link);
        }
        break;
    }

    case STMT_LABEL: {
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = ir_label_create(ts->tunit, stmt->label.label);
        sl_append(ir_stmts, &ir_stmt->link);
        trans_stmt(ts, stmt->label.stmt, ir_stmts);
        break;
    }
    case STMT_CASE: {
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = stmt->case_params.label;
        sl_append(ir_stmts, &ir_stmt->link);
        trans_stmt(ts, stmt->case_params.stmt, ir_stmts);
        break;
    }
    case STMT_DEFAULT: {
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = stmt->default_params.label;
        sl_append(ir_stmts, &ir_stmt->link);
        trans_stmt(ts, stmt->default_params.stmt, ir_stmts);
        break;
    }

    case STMT_IF: {
        ir_label_t *if_true = ir_numlabel_create(ts->tunit,
                                                 ts->func->func.next_label++);
        ir_label_t *if_false = stmt->if_params.false_stmt == NULL ?
            NULL : ir_numlabel_create(ts->tunit, ts->func->func.next_label++);
        ir_label_t *after = ir_numlabel_create(ts->tunit,
                                               ts->func->func.next_label++);

        ir_expr_t *cond = trans_expr(ts, stmt->if_params.expr, ir_stmts);

        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = cond;
        ir_stmt->br.if_true = if_true;
        ir_stmt->br.if_false = if_false == NULL ? after : if_false;
        sl_append(ir_stmts, &ir_stmt->link);

        // True branch
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = if_true;
        sl_append(ir_stmts, &ir_stmt->link);

        trans_stmt(ts, stmt->if_params.true_stmt, ir_stmts);
        ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = after;
        sl_append(ir_stmts, &ir_stmt->link);

        if (if_false != NULL) {
            // False branch
            ir_stmt = ir_stmt_create(IR_STMT_LABEL);
            ir_stmt->label = if_false;
            sl_append(ir_stmts, &ir_stmt->link);

            trans_stmt(ts, stmt->if_params.false_stmt, ir_stmts);
            ir_stmt = ir_stmt_create(IR_STMT_BR);
            ir_stmt->br.cond = NULL;
            ir_stmt->br.uncond = after;
            sl_append(ir_stmts, &ir_stmt->link);
        }

        // End label
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = after;
        sl_append(ir_stmts, &ir_stmt->link);
        break;
    }
    case STMT_SWITCH: {
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_SWITCH);
        ir_stmt->switch_params.expr =
            trans_expr(ts, stmt->switch_params.expr, ir_stmts);
        sl_link_t *cur;
        SL_FOREACH(cur, &stmt->switch_params.cases) {
            stmt_t *cur_case = GET_ELEM(&stmt->switch_params.cases, cur);
            ir_label_t *label = ir_numlabel_create(ts->tunit,
                                                   ts->func->func.next_label++);
            assert(cur_case->type == STMT_CASE);
            cur_case->case_params.label = label;

            long long case_val;
            bool retval =
                typecheck_const_expr(cur_case->case_params.val, &case_val);
            assert(retval == true);

            ir_val_label_pair_t *pair = emalloc(sizeof(ir_val_label_pair_t));
            pair->val = ir_expr_create(IR_EXPR_CONST);
            pair->val->const_params.val = case_val;
            pair->val->const_params.type = &ir_type_i64;
            pair->label = label;

            sl_append(&ir_stmt->switch_params.cases, &pair->link);
        }

        // Generate default label
        ir_label_t *label = ir_numlabel_create(ts->tunit,
                                               ts->func->func.next_label++);
        ir_label_t *after = ir_numlabel_create(ts->tunit,
                                               ts->func->func.next_label++);

        ir_label_t *break_save = ts->break_target;
        ts->break_target = after;

        stmt->switch_params.default_stmt->default_params.label = label;
        ir_stmt->switch_params.default_case = label;
        sl_append(ir_stmts, &ir_stmt->link);

        trans_stmt(ts, stmt->switch_params.stmt, ir_stmts);

        // Preceeding label
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = after;
        sl_append(ir_stmts, &ir_stmt->link);

        // Restore break target
        ts->break_target = break_save;
        break;
    }

    case STMT_DO: {
        ir_label_t *body = ir_numlabel_create(ts->tunit,
                                              ts->func->func.next_label++);
        ir_label_t *after = ir_numlabel_create(ts->tunit,
                                               ts->func->func.next_label++);
        ir_label_t *break_save = ts->break_target;
        ir_label_t *continue_save = ts->continue_target;
        ts->break_target = after;
        ts->continue_target = body;

        // Unconditional branch to body
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = body;
        sl_append(ir_stmts, &ir_stmt->link);

        // Loop body
        trans_stmt(ts, stmt->while_params.stmt, ir_stmts);

        // Loop test
        ir_expr_t *test = trans_expr(ts, stmt->while_params.expr, ir_stmts);
        ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = test;
        ir_stmt->br.if_true = body;
        ir_stmt->br.if_false = after;
        sl_append(ir_stmts, &ir_stmt->link);

        // End label
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = after;
        sl_append(ir_stmts, &ir_stmt->link);

        // Restore state
        ts->break_target = break_save;
        ts->continue_target = continue_save;
        break;
    }
    case STMT_WHILE: {
        ir_label_t *cond = ir_numlabel_create(ts->tunit,
                                              ts->func->func.next_label++);
        ir_label_t *body = ir_numlabel_create(ts->tunit,
                                              ts->func->func.next_label++);
        ir_label_t *after = ir_numlabel_create(ts->tunit,
                                               ts->func->func.next_label++);
        ir_label_t *break_save = ts->break_target;
        ir_label_t *continue_save = ts->continue_target;
        ts->break_target = after;
        ts->continue_target = cond;

        // Unconditional branch to test
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = cond;
        sl_append(ir_stmts, &ir_stmt->link);


        // Loop test
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = cond;
        sl_append(ir_stmts, &ir_stmt->link);

        ir_expr_t *test = trans_expr(ts, stmt->while_params.expr, ir_stmts);

        ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = test;
        ir_stmt->br.if_true = body;
        ir_stmt->br.if_false = after;
        sl_append(ir_stmts, &ir_stmt->link);

        // Loop body
        trans_stmt(ts, stmt->while_params.stmt, ir_stmts);
        ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = cond;
        sl_append(ir_stmts, &ir_stmt->link);

        // End label
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = after;
        sl_append(ir_stmts, &ir_stmt->link);

        // Restore state
        ts->break_target = break_save;
        ts->continue_target = continue_save;
        break;
    }
    case STMT_FOR: {
        ir_label_t *cond = ir_numlabel_create(ts->tunit,
                                              ts->func->func.next_label++);
        ir_label_t *body = ir_numlabel_create(ts->tunit,
                                              ts->func->func.next_label++);
        ir_label_t *after = ir_numlabel_create(ts->tunit,
                                               ts->func->func.next_label++);
        ir_label_t *break_save = ts->break_target;
        ir_label_t *continue_save = ts->continue_target;
        ts->break_target = after;
        ts->continue_target = cond;

        // Loop header:
        if (stmt->for_params.decl1 != NULL) {
            sl_link_t *cur;
            SL_FOREACH(cur, &stmt->for_params.decl1->decls) {
                ir_stmt_t *ir_stmt = trans_decl_node(
                    ts, GET_ELEM(&stmt->for_params.decl1->decls, cur));
                sl_append(ir_stmts, &ir_stmt->link);
            }
        } else {
            trans_expr(ts, stmt->for_params.expr1, ir_stmts);
        }

        // Unconditional branch to test
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = cond;
        sl_append(ir_stmts, &ir_stmt->link);


        // Loop test
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = cond;
        sl_append(ir_stmts, &ir_stmt->link);

        ir_expr_t *test = trans_expr(ts, stmt->for_params.expr2, ir_stmts);

        ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = test;
        ir_stmt->br.if_true = body;
        ir_stmt->br.if_false = after;
        sl_append(ir_stmts, &ir_stmt->link);

        // Loop body
        trans_stmt(ts, stmt->for_params.stmt, ir_stmts);
        trans_expr(ts, stmt->for_params.expr3, ir_stmts);
        ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = cond;
        sl_append(ir_stmts, &ir_stmt->link);

        // End label
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = after;
        sl_append(ir_stmts, &ir_stmt->link);

        // Restore state
        ts->break_target = break_save;
        ts->continue_target = continue_save;
        break;
    }

    case STMT_GOTO: {
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = ir_label_create(ts->tunit,
                                             stmt->goto_params.label);
        break;
    }
    case STMT_CONTINUE: {
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        assert(ts->continue_target != NULL);
        ir_stmt->br.uncond = ts->continue_target;
        break;
    }
    case STMT_BREAK: {
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        assert(ts->break_target != NULL);
        ir_stmt->br.uncond = ts->break_target;
        break;
    }
    case STMT_RETURN: {
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_RET);
        assert(ts->func->func.type->type == IR_TYPE_FUNC);
        ir_stmt->ret.type = ts->func->func.type->func.type;
        ir_stmt->ret.val = trans_expr(ts, stmt->return_params.expr, ir_stmts);
        break;
    }

    case STMT_COMPOUND: {
        sl_link_t *cur;
        SL_FOREACH(cur, &stmt->compound.stmts) {
            stmt_t *cur_stmt = GET_ELEM(&stmt->compound.stmts, cur);
            trans_stmt(ts, cur_stmt, ir_stmts);
        }
        break;
    }

    case STMT_EXPR:
        trans_expr(ts, stmt->expr.expr, ir_stmts);
        break;
    default:
        assert(false);
    }
}

ir_expr_t *trans_expr(trans_state_t *ts, expr_t *expr, slist_t *ir_stmts) {
    // TODO: This
    (void)ts;
    (void)ir_stmts;
    switch (expr->type) {
    case EXPR_VOID:
    case EXPR_PAREN:
    case EXPR_VAR:
    case EXPR_ASSIGN:
    case EXPR_CONST_INT:
    case EXPR_CONST_FLOAT:
    case EXPR_CONST_STR:
    case EXPR_BIN:
    case EXPR_UNARY:
    case EXPR_COND:
    case EXPR_CAST:
    case EXPR_CALL:
    case EXPR_CMPD:
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
    case EXPR_OFFSETOF:
    case EXPR_MEM_ACC:
    case EXPR_ARR_IDX:
    case EXPR_INIT_LIST:
    case EXPR_DESIG_INIT:
    default:
        assert(false);
    }
    return NULL;
}

ir_stmt_t *trans_decl_node(trans_state_t *ts, decl_node_t *node) {
    // TODO: This
    (void)ts;
    (void)node;
    return NULL;
}

ir_type_t *trans_type(trans_state_t *ts, type_t *type) {
    // TODO: This
    (void)ts;
    (void)type;
    return NULL;
}
