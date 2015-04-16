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

        assert(ts->func == NULL); // Nested functions not allowed
        ts->func = ir_gdecl;

        trans_stmt(ts, gdecl->fdefn.stmt, &ir_gdecl->func.body);
        sl_append(ir_gdecls, &ir_gdecl->link);
        ts->func = NULL;
        break;
    }
    case GDECL_DECL: {
        SL_FOREACH(cur, &gdecl->decl->decls) {
            decl_node_t *node = GET_ELEM(&gdecl->decl->decls, cur);
            ir_gdecl_t *ir_gdecl = ir_gdecl_create(IR_GDECL_GDATA);
            trans_decl_node(ts, node, &ir_gdecl->gdata.stmts);
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
        SL_FOREACH(cur, &stmt->decl->decls) {
            decl_node_t *node = GET_ELEM(&stmt->decl->decls, cur);
            trans_decl_node(ts, node, ir_stmts);
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

            ir_expr_label_pair_t *pair = emalloc(sizeof(ir_expr_label_pair_t));
            pair->expr = ir_expr_create(IR_EXPR_CONST);
            pair->expr->const_params.int_val = case_val;
            pair->expr->const_params.type = &SWITCH_VAL_TYPE;
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
            SL_FOREACH(cur, &stmt->for_params.decl1->decls) {
                trans_decl_node(ts,
                                GET_ELEM(&stmt->for_params.decl1->decls, cur),
                                ir_stmts);
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
        assert(ts->func->type == IR_GDECL_FUNC &&
               ts->func->func.type->type == IR_TYPE_FUNC);
        ir_stmt->ret.type = ir_type_ref(ts->func->func.type->func.type);
        ir_stmt->ret.val = trans_expr(ts, stmt->return_params.expr, ir_stmts);
        break;
    }

    case STMT_COMPOUND: {
        typetab_t *typetab_save = ts->typetab;
        ts->typetab = &stmt->compound.typetab;
        SL_FOREACH(cur, &stmt->compound.stmts) {
            stmt_t *cur_stmt = GET_ELEM(&stmt->compound.stmts, cur);
            trans_stmt(ts, cur_stmt, ir_stmts);
        }
        ts->typetab = typetab_save;
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
    switch (expr->type) {
    case EXPR_VOID:
        return NULL;
    case EXPR_PAREN:
        return trans_expr(ts, expr->paren_base, ir_stmts);
    case EXPR_VAR: {
        typetab_entry_t *entry = tt_lookup(ts->typetab, expr->var_id);

        // Must be valid if typechecked
        assert(entry != NULL && entry->entry_type == TT_VAR);

        ir_expr_t *ir_expr = ir_expr_create(IR_EXPR_VAR);
        ir_expr->var.type = trans_type(ts, entry->type);
        ir_expr->var.name.str = expr->var_id->str;
        ir_expr->var.name.len = expr->var_id->len;
        ir_expr->var.local = ts->func != NULL;
        return ir_expr;
    }
    case EXPR_ASSIGN: {
        if (expr->assign.op == OP_NOP) {
            ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_ASSIGN);
            ir_stmt->assign.dest = trans_expr(ts, expr->assign.expr, ir_stmts);
            ir_stmt->assign.src = trans_expr(ts, expr->assign.dest, ir_stmts);
            sl_append(ir_stmts, &ir_stmt->link);
            return ir_stmt->assign.dest;
        }
        ir_type_t *type = trans_type(ts, expr->etype);
        ir_expr_t *temp = ir_temp_create(type, ts->func->func.next_temp++);
        ir_expr_t *dest;
        ir_expr_t *op_expr = trans_binop(ts, expr->assign.dest,
                                         expr->assign.expr, expr->assign.op,
                                         expr->etype, ir_stmts, &dest);

        ir_stmt_t *binop = ir_stmt_create(IR_STMT_ASSIGN);
        binop->assign.dest = temp;
        binop->assign.src = op_expr;
        sl_append(ir_stmts, &binop->link);

        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_ASSIGN);
        ir_stmt->assign.dest = dest;
        ir_stmt->assign.src = temp;
        sl_append(ir_stmts, &ir_stmt->link);

        return dest;
    }
    case EXPR_CONST_INT: {
        ir_expr_t *ir_expr = ir_expr_create(IR_EXPR_CONST);
        ir_expr->const_params.type = trans_type(ts, expr->const_val.type);
        ir_expr->const_params.int_val = expr->const_val.int_val;
        return ir_expr;
    }
    case EXPR_CONST_FLOAT: {
        ir_expr_t *ir_expr = ir_expr_create(IR_EXPR_CONST);
        ir_expr->const_params.type = trans_type(ts, expr->const_val.type);
        ir_expr->const_params.float_val = expr->const_val.float_val;
        return ir_expr;
    }
    case EXPR_CONST_STR: {
        ir_expr_t *ir_expr = ir_expr_create(IR_EXPR_CONST);
        ir_expr->const_params.type = trans_type(ts, expr->const_val.type);
        // TODO: Create a global string, change str_val to point to that
        return ir_expr;
    }
    case EXPR_BIN: {
        ir_type_t *type = trans_type(ts, expr->etype);
        ir_expr_t *temp = ir_temp_create(type, ts->func->func.next_temp++);

        ir_expr_t *op_expr = trans_binop(ts, expr->bin.expr1, expr->bin.expr2,
                                         expr->bin.op, expr->etype, ir_stmts,
                                         NULL);
        ir_stmt_t *binop = ir_stmt_create(IR_STMT_ASSIGN);
        binop->assign.dest = temp;
        binop->assign.src = op_expr;

        sl_append(ir_stmts, &binop->link);
        return temp;
    }
    case EXPR_UNARY:
        // TODO : This
        return NULL;
    case EXPR_COND: {
        ir_type_t *type = trans_type(ts, expr->etype);
        ir_expr_t *temp = ir_temp_create(type, ts->func->func.next_temp++);
        ir_expr_t *expr1 = trans_expr(ts, expr->cond.expr1, ir_stmts);
        ir_label_t *if_true = ir_numlabel_create(ts->tunit,
                                                 ts->func->func.next_label++);
        ir_label_t *if_false = ir_numlabel_create(ts->tunit,
                                                  ts->func->func.next_label++);
        ir_label_t *after = ir_numlabel_create(ts->tunit,
                                               ts->func->func.next_label++);

        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = expr1;
        ir_stmt->br.if_true = if_true;
        ir_stmt->br.if_false = if_false;
        sl_append(ir_stmts, &ir_stmt->link);

        // True branch
        // Label
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = if_true;
        sl_append(ir_stmts, &ir_stmt->link);

        // Expression
        ir_expr_t *expr2 = trans_expr(ts, expr->cond.expr2, ir_stmts);

        // Assignment
        ir_stmt = ir_stmt_create(IR_STMT_ASSIGN);
        ir_stmt->assign.dest = temp;
        ir_stmt->assign.src = expr2;
        sl_append(ir_stmts, &ir_stmt->link);

        // Jump to after
        ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = after;
        sl_append(ir_stmts, &ir_stmt->link);


        // False branch
        // Label
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = if_false;
        sl_append(ir_stmts, &ir_stmt->link);

        // Expression
        ir_expr_t *expr3 = trans_expr(ts, expr->cond.expr3, ir_stmts);

        // Assignment
        ir_stmt = ir_stmt_create(IR_STMT_ASSIGN);
        ir_stmt->assign.dest = temp;
        ir_stmt->assign.src = expr3;
        sl_append(ir_stmts, &ir_stmt->link);

        // Jump to after
        ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = after;
        sl_append(ir_stmts, &ir_stmt->link);

        // End label
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = after;
        sl_append(ir_stmts, &ir_stmt->link);

        return temp;
    }
    case EXPR_CAST: {
        ir_type_t *dest_type = trans_type(ts, expr->etype);
        ir_type_t *src_type = trans_type(ts, expr->cast.base->etype);
        ir_expr_t *temp = ir_temp_create(dest_type, ts->func->func.next_temp++);

        ir_expr_t *src_expr = trans_expr(ts, expr->cast.base, ir_stmts);

        ir_expr_t *convert = ir_expr_create(IR_EXPR_CONVERT);
        // TODO: This. choose right type based on src/dest types
        //convert->convert.type = 
        convert->convert.src_type = src_type;
        convert->convert.val = src_expr;
        convert->convert.dest_type = dest_type;

        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_ASSIGN);
        ir_stmt->assign.dest = temp;
        ir_stmt->assign.src = src_expr;
        return temp;
    }
    case EXPR_CALL: {
        ir_expr_t *call = ir_expr_create(IR_EXPR_CALL);
        call->call.func_sig = trans_type(ts, expr->call.func->etype);
        call->call.func_ptr = trans_expr(ts, expr->call.func, ir_stmts);

        SL_FOREACH(cur, &expr->call.params) {
            expr_t *param = GET_ELEM(&expr->call.params, cur);
            ir_type_expr_pair_t *pair = emalloc(sizeof(*pair));
            pair->type = trans_type(ts, param->etype);
            pair->expr = trans_expr(ts, param, ir_stmts);
            sl_append(&call->call.arglist, &pair->link);
        }

        // Void returning function, don't create a temp
        if (expr->call.func->etype->func.type->type == TYPE_VOID) {
            return NULL;
        }

        ir_type_t *ret_type = ir_type_ref(call->call.func_sig->func.type);
        ir_expr_t *temp = ir_temp_create(ret_type, ts->func->func.next_temp++);
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_ASSIGN);
        ir_stmt->assign.dest = temp;
        ir_stmt->assign.src = call;
        sl_append(ir_stmts, &ir_stmt->link);

        return temp;
    }
    case EXPR_CMPD: {
        ir_expr_t *ir_expr = NULL;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            expr_t *expr = GET_ELEM(&expr->cmpd.exprs, cur);
            ir_expr = trans_expr(ts, expr, ir_stmts);
        }
        return ir_expr;
    }
    case EXPR_SIZEOF: {
        ir_expr_t *ir_expr = ir_expr_create(IR_EXPR_CONST);
        ir_expr->const_params.type = trans_type(ts, expr->etype);
        if (expr->sizeof_params.type != NULL) {
            decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
            if (node != NULL) {
                ir_expr->const_params.int_val = ast_type_size(node->type);
            } else {
                assert(node == sl_tail(&expr->sizeof_params.type->decls));
                ir_expr->const_params.int_val =
                    ast_type_size(expr->sizeof_params.type->type);
            }
        } else {
            assert(expr->sizeof_params.expr != NULL);
            ir_expr->const_params.int_val =
                ast_type_size(expr->sizeof_params.expr->etype);
        }
        return ir_expr;
    }
    case EXPR_ALIGNOF: {
        ir_expr_t *ir_expr = ir_expr_create(IR_EXPR_CONST);
        ir_expr->const_params.type = trans_type(ts, expr->etype);
        if (expr->sizeof_params.type != NULL) {
            decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
            if (node != NULL) {
                ir_expr->const_params.int_val = ast_type_align(node->type);
            } else {
                assert(node == sl_tail(&expr->sizeof_params.type->decls));
                ir_expr->const_params.int_val =
                    ast_type_align(expr->sizeof_params.type->type);
            }
        } else {
            assert(expr->sizeof_params.expr != NULL);
            ir_expr->const_params.int_val =
                ast_type_align(expr->sizeof_params.expr->etype);
        }
        return ir_expr;
    }
    case EXPR_OFFSETOF: {
        ir_expr_t *ir_expr = ir_expr_create(IR_EXPR_CONST);
        ir_expr->const_params.type = trans_type(ts, expr->etype);
        ir_expr->const_params.int_val =
            ast_type_offset(expr->offsetof_params.type->type,
                            expr->offsetof_params.path);
        return ir_expr;
    }
    case EXPR_MEM_ACC: {
        ir_expr_t *pointer = trans_expr(ts, expr->mem_acc.base, ir_stmts);
        if (expr->mem_acc.op == OP_ARROW) {
            // TODO: Handle this
        } else {
            assert(expr->mem_acc.op = OP_DOT);
        }
        ir_expr_t *elem_ptr = ir_expr_create(IR_EXPR_GETELEMPTR);
        elem_ptr->getelemptr.type = trans_type(ts, expr->etype);
        elem_ptr->getelemptr.ptr_val = pointer;

        // Get 0th index to point to structure
        ir_type_expr_pair_t *pair = emalloc(sizeof(*pair));
        pair->type = &ir_type_i32;
        pair->expr = ir_expr_create(IR_EXPR_CONST);
        pair->expr->const_params.type = &ir_type_i32;
        pair->expr->const_params.int_val = 0;
        sl_append(&elem_ptr->getelemptr.idxs, &pair->link);

        // Get index into the structure
        pair = emalloc(sizeof(*pair));
        pair->type = &ir_type_i32;
        pair->expr = ir_expr_create(IR_EXPR_CONST);
        pair->expr->const_params.type = &ir_type_i32;
        pair->expr->const_params.int_val =
            ast_get_member_num(expr->mem_acc.base->etype, expr->mem_acc.name);
        sl_append(&elem_ptr->getelemptr.idxs, &pair->link);

        // Load instruction
        ir_expr_t *load = ir_expr_create(IR_EXPR_LOAD);
        load->load.type = ir_type_ref(elem_ptr->getelemptr.type);
        load->load.ptr = elem_ptr;
        return load;
    }
    case EXPR_ARR_IDX: {
        ir_expr_t *elem_ptr = ir_expr_create(IR_EXPR_GETELEMPTR);
        elem_ptr->getelemptr.type = trans_type(ts, expr->etype);
        elem_ptr->getelemptr.ptr_val = trans_expr(ts, expr->arr_idx.array,
                                                  ir_stmts);

        // Get 0th index to point to array
        ir_type_expr_pair_t *pair = emalloc(sizeof(*pair));
        pair->type = &ir_type_i32;
        pair->expr = ir_expr_create(IR_EXPR_CONST);
        pair->expr->const_params.type = &ir_type_i32;
        pair->expr->const_params.int_val = 0;
        sl_append(&elem_ptr->getelemptr.idxs, &pair->link);

        // Get index into the array
        pair = emalloc(sizeof(*pair));
        pair->type = &ir_type_i64;
        pair->expr = trans_expr(ts, expr->arr_idx.index, ir_stmts);
        sl_append(&elem_ptr->getelemptr.idxs, &pair->link);

        ir_expr_t *load = ir_expr_create(IR_EXPR_LOAD);
        load->load.type = ir_type_ref(elem_ptr->getelemptr.type);
        load->load.ptr = elem_ptr;
        return load;
    }
    case EXPR_INIT_LIST:
    case EXPR_DESIG_INIT:
        // TODO: Create global data and copy it in
    default:
        assert(false);
    }
    return NULL;
}

ir_expr_t *trans_expr_bool(trans_state_t *ts, ir_expr_t *expr, ir_type_t *type,
                           slist_t *ir_stmts) {
    bool is_float = type->type == IR_TYPE_FLOAT;

    ir_expr_t *temp =
        ir_temp_create(&ir_type_i1, ts->func->func.next_temp++);
    ir_expr_t *cmp;
    ir_expr_t *zero = ir_expr_create(IR_EXPR_CONST);
    zero->const_params.type = type;
    if (is_float) {
        zero->const_params.float_val = 0.0;
        cmp = ir_expr_create(IR_EXPR_FCMP);
        cmp->fcmp.cond = IR_FCMP_ONE;
        cmp->fcmp.type = ir_type_ref(type);
        cmp->fcmp.expr1 = expr;
        cmp->fcmp.expr2 = zero;
    } else {
        zero->const_params.int_val = 0;
        cmp = ir_expr_create(IR_EXPR_ICMP);
        cmp->icmp.cond = IR_ICMP_NE;
        cmp->icmp.type = ir_type_ref(type);
        cmp->icmp.expr1 = expr;
        cmp->icmp.expr2 = zero;
    }
    ir_stmt_t *stmt = ir_stmt_create(IR_STMT_ASSIGN);
    stmt->assign.dest = temp;
    stmt->assign.src = cmp;
    sl_append(ir_stmts, &stmt->link);
    return temp;
}

ir_expr_t *trans_binop(trans_state_t *ts, expr_t *left, expr_t *right,
                       oper_t op, type_t *type, slist_t *ir_stmts,
                       ir_expr_t **left_loc) {
    type = ast_type_untypedef(type);
    bool is_float = false;
    bool is_signed = false;
    bool is_cmp = false;
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
        if (!(type->mod.type_mod & TMOD_UNSIGNED) &&
            TYPE_IS_INTEGRAL(type->mod.base)) {
            is_signed = true;
        }
        break;

    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:
        break;

    case TYPE_PAREN:
    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_ENUM:
    case TYPE_TYPEDEF:
    default:
        assert(false);
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
        is_cmp = true;
        cmp_type = is_float ? IR_FCMP_OLT :
            is_signed ? IR_ICMP_SLT : IR_ICMP_ULT;
        break;
    case OP_GT:
        is_cmp = true;
        cmp_type = is_float ? IR_FCMP_OGT :
            is_signed ? IR_ICMP_SGT : IR_ICMP_UGT;
        break;
    case OP_LE:
        is_cmp = true;
        cmp_type = is_float ? IR_FCMP_OLE :
            is_signed ? IR_ICMP_SLE : IR_ICMP_ULE;
        break;
    case OP_GE:
        is_cmp = true;
        cmp_type = is_float ? IR_FCMP_OGE :
            is_signed ? IR_ICMP_SGE : IR_ICMP_UGE;
        break;
    case OP_EQ:
        is_cmp = true;
        cmp_type = is_float ? IR_FCMP_OEQ : IR_ICMP_EQ;
        break;
    case OP_NE:
        is_cmp = true;
        cmp_type = is_float ? IR_FCMP_ONE : IR_ICMP_NE;
        break;
    case OP_LOGICAND:
    case OP_LOGICOR: {
        bool is_and = op == OP_LOGICAND;

        ir_expr_t *temp =
            ir_temp_create(&ir_type_i1, ts->func->func.next_temp++);
        ir_label_t *right_label =
            ir_numlabel_create(ts->tunit, ts->func->func.next_label++);
        ir_label_t *done =
            ir_numlabel_create(ts->tunit, ts->func->func.next_label++);

        // Create left expression
        ir_expr_t *left_expr = trans_expr(ts, left, ir_stmts);
        ir_expr_t *ir_expr =
            trans_expr_bool(ts, left_expr, trans_type(ts, type), ir_stmts);

        // First branch
        ir_label_t *block = ts->func->func.last_label;
        ir_stmt_t *ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = ir_expr;
        if (is_and) {
            ir_stmt->br.if_true = right_label;
            ir_stmt->br.if_false = done;
        } else {
            ir_stmt->br.if_true = done;
            ir_stmt->br.if_false = right_label;
        }
        sl_append(ir_stmts, &ir_stmt->link);

        // Right side
        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = right_label;
        sl_append(ir_stmts, &ir_stmt->link);

        ir_expr = trans_expr(ts, right, ir_stmts);
        ir_expr_t *right_val =
            trans_expr_bool(ts, ir_expr, trans_type(ts, type), ir_stmts);

        ir_stmt = ir_stmt_create(IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = done;
        sl_append(ir_stmts, &ir_stmt->link);


        ir_stmt = ir_stmt_create(IR_STMT_LABEL);
        ir_stmt->label = done;
        sl_append(ir_stmts, &ir_stmt->link);

        ir_expr = ir_expr_create(IR_EXPR_PHI);
        ir_expr->phi.type = &ir_type_i1;

        ir_expr_label_pair_t *pred = emalloc(sizeof(*pred));
        pred->expr = ir_expr_create(IR_EXPR_CONST);
        pred->expr->const_params.type = &ir_type_i1;
        if (is_and) {
            pred->expr->const_params.int_val = 0;
        } else {
            pred->expr->const_params.int_val = 1;
        }
        pred->label = block;
        sl_append(&ir_expr->phi.preds, &pred->link);

        pred = emalloc(sizeof(*pred));
        pred->expr = right_val;
        pred->label = right_label;
        sl_append(&ir_expr->phi.preds, &pred->link);

        ir_stmt = ir_stmt_create(IR_STMT_ASSIGN);
        ir_stmt->assign.dest = temp;
        ir_stmt->assign.src = ir_expr; // The phi node
        sl_append(ir_stmts, &ir_stmt->link);

        if (left_loc != NULL) {
            *left_loc = left_expr;
        }
        return temp;
    }
    default:
        assert(false);
    }

    // Comparisons need to be handled separately
    if (is_cmp) {
        ir_expr_t *cmp;
        ir_expr_t *left_expr;
        if (is_float) {
            cmp = ir_expr_create(IR_EXPR_FCMP);
            cmp->fcmp.cond = cmp_type;
            cmp->fcmp.type = trans_type(ts, type);
            left_expr = trans_expr(ts, left, ir_stmts);
            cmp->fcmp.expr1 = left_expr;
            cmp->fcmp.expr2 = trans_expr(ts, right, ir_stmts);
        } else {
            cmp = ir_expr_create(IR_EXPR_ICMP);
            cmp->icmp.cond = cmp_type;
            cmp->icmp.type = trans_type(ts, type);
            left_expr = trans_expr(ts, left, ir_stmts);
            cmp->icmp.expr1 = left_expr;
            cmp->icmp.expr2 = trans_expr(ts, right, ir_stmts);;
        }
        if (left_loc != NULL) {
            *left_loc = left_expr;
        }
        return cmp;
    }

    // Basic bin op case
    ir_expr_t *op_expr = ir_expr_create(IR_EXPR_BINOP);
    op_expr->binop.op = ir_op;
    op_expr->binop.type = trans_type(ts, type);
    ir_expr_t *left_expr = trans_expr(ts, left, ir_stmts);
    op_expr->binop.expr1 = left_expr;
    op_expr->binop.expr2 = trans_expr(ts, right, ir_stmts);
    if (left_loc != NULL) {
        *left_loc = left_expr;
    }
    return op_expr;
}

void trans_decl_node(trans_state_t *ts, decl_node_t *node, slist_t *ir_stmts) {
    bool global = ts->func == NULL;
    ir_expr_t *name = ir_expr_create(IR_EXPR_VAR);
    name->var.type = trans_type(ts, node->type);
    name->var.name.str = node->id->str;
    name->var.name.len = node->id->len;

    ir_stmt_t *stmt = ir_stmt_create(IR_STMT_ASSIGN);
    stmt->assign.dest = name;

    if (global) {
        ir_expr_t *src = node->expr == NULL ?
            NULL : trans_expr(ts, node->expr, ir_stmts);
        stmt->assign.src = src;
        sl_append(ir_stmts, &stmt->link);
    } else {
        ir_expr_t *src = ir_expr_create(IR_EXPR_ALLOCA);
        src->alloca.type = ir_type_ref(name->var.type);
        src->alloca.nelem_type = &NELEM_TYPE;
        src->alloca.nelems = 0;
        src->alloca.align = ast_type_align(node->type);
        stmt->assign.src = src;
        sl_append(ir_stmts, &stmt->link);
        if (node->expr != NULL) {
            ir_stmt_t *store = ir_stmt_create(IR_STMT_STORE);
            store->store.type = trans_type(ts, node->type);
            store->store.val = trans_expr(ts, node->expr, ir_stmts);
            store->store.ptr = name;
            sl_append(ir_stmts, &store->link);
        }
    }
}

ir_type_t *trans_type(trans_state_t *ts, type_t *type) {
    ir_type_t *ir_type = NULL;
    switch (type->type) {
    case TYPE_VOID:        return &ir_type_void;
    case TYPE_BOOL:        return &ir_type_i1;
    case TYPE_CHAR:        return &ir_type_i8;
    case TYPE_SHORT:       return &ir_type_i16;
    case TYPE_INT:         return &ir_type_i32;
    case TYPE_LONG:        return &ir_type_i64;
    case TYPE_LONG_LONG:   return &ir_type_i64;
    case TYPE_FLOAT:       return &ir_type_float;
    case TYPE_DOUBLE:      return &ir_type_double;
    case TYPE_LONG_DOUBLE: return &ir_type_double;
    case TYPE_ENUM:        return &ir_type_i32;

    case TYPE_TYPEDEF:     return trans_type(ts, type->typedef_params.base);
    case TYPE_MOD:         return trans_type(ts, type->mod.base);
    case TYPE_PAREN:       return trans_type(ts, type->paren_base);

    case TYPE_STRUCT:
        ir_type = ir_type_create(IR_TYPE_STRUCT);
        SL_FOREACH(cur_decl, &type->struct_params.decls) {
            decl_t *decl = GET_ELEM(&type->struct_params.decls, cur_decl);
            SL_FOREACH(cur_node, &decl->decls) {
                decl_node_t *node = GET_ELEM(&decl->decls, cur_node);
                ir_type_t *node_type = trans_type(ts, node->type);
                sl_append(&ir_type->struct_params.types, &node_type->link);
            }

            // Add anonymous struct and union members to the struct
            if (sl_head(&decl->decls) == NULL &&
                (decl->type->type == TYPE_STRUCT ||
                 decl->type->type == TYPE_UNION)) {
                ir_type_t *decl_type = trans_type(ts, decl->type);
                sl_append(&ir_type->struct_params.types, &decl_type->link);
            }
        }
        return ir_type;

    case TYPE_UNION: {
        type_t *max_type = NULL;
        size_t max_size = 0;
        SL_FOREACH(cur_decl, &type->struct_params.decls) {
            decl_t *decl = GET_ELEM(&type->struct_params.decls, cur_decl);
            SL_FOREACH(cur_node, &decl->decls) {
                decl_node_t *node = GET_ELEM(&decl->decls, cur_node);
                size_t size = ast_type_size(node->type);
                if (size > max_size) {
                    max_size = size;
                    max_type = node->type;
                }
            }

            // Add anonymous struct and union members to the struct
            if (sl_head(&decl->decls) == NULL &&
                (decl->type->type == TYPE_STRUCT ||
                 decl->type->type == TYPE_UNION)) {
                size_t size = ast_type_size(decl->type);
                if (size > max_size) {
                    max_size = size;
                    max_type = decl->type;
                }
            }
        }
        return trans_type(ts, max_type);
    }

    case TYPE_FUNC:
        ir_type = ir_type_create(IR_TYPE_FUNC);
        ir_type->func.type = trans_type(ts, type->func.type);
        ir_type->func.varargs = type->func.varargs;
        SL_FOREACH(cur, &type->func.params) {
            ir_type_t *param_type =
                trans_type(ts, GET_ELEM(&type->func.params, cur));
            sl_append(&ir_type->func.params, &param_type->link);
        }

        return ir_type;
    case TYPE_ARR:
        ir_type = ir_type_create(IR_TYPE_ARR);
        long long size;
        bool retval =
            typecheck_const_expr(type->arr.len, &size);
        ir_type->arr.nelems = size;
        assert(retval == true);
        ir_type->arr.elem_type = trans_type(ts, type->arr.base);
        return ir_type;
    case TYPE_PTR:
        ir_type = ir_type_create(IR_TYPE_PTR);
        ir_type->ptr.base = trans_type(ts, type->ptr.base);
        return ir_type;
    default:
        assert(false);
    }
    return NULL;
}
