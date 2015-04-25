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

// TODO1: Approximate this programatically using MAX_INT
#define MAX_NUM_LEN 20

void trans_add_stmt(trans_state_t *ts, ir_inst_stream_t *stream,
                    ir_stmt_t *stmt) {
    if (stmt->type == IR_STMT_LABEL) {
        ts->func->func.last_label = stmt->label;
    }
    dl_append(&stream->list, &stmt->link);
}

ir_label_t *trans_label_create(trans_state_t *ts, char *str) {
    return ir_label_create(ts->tunit, str);
}

ir_label_t *trans_numlabel_create(trans_state_t *ts) {
    return ir_numlabel_create(ts->tunit, ts->func->func.next_label++);
}

ir_expr_t *trans_temp_create(trans_state_t *ts, ir_type_t *type) {
    return ir_temp_create(ts->tunit, ts->func, type,
                          ts->func->func.next_temp++);
}
ir_trans_unit_t *trans_translate(trans_unit_t *ast) {
    assert(ast != NULL);

    trans_state_t ts = TRANS_STATE_LIT;
    return trans_trans_unit(&ts, ast);
}

// TODO0: Do decls first, then function definitions
ir_trans_unit_t *trans_trans_unit(trans_state_t *ts, trans_unit_t *ast) {
    ir_trans_unit_t *tunit = ir_trans_unit_create();
    ts->tunit = tunit;
    ts->typetab = &ast->typetab;
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
        trans_decl_node(ts, node, IR_DECL_NODE_FDEFN, NULL);

        assert(node != NULL);
        assert(node == sl_tail(&gdecl->decl->decls));

        ir_gdecl_t *ir_gdecl = ir_gdecl_create(IR_GDECL_FUNC);
        assert(ts->func == NULL); // Nested functions not allowed
        ts->func = ir_gdecl;

        ir_gdecl->func.type = trans_type(ts, node->type);
        ir_gdecl->func.name = node->id;

        ir_stmt_t *start_label = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        start_label->label = trans_numlabel_create(ts);
        trans_add_stmt(ts, &ir_gdecl->func.prefix, start_label);

        typetab_t *typetab_save = ts->typetab;
        assert(gdecl->fdefn.stmt->type == STMT_COMPOUND);
        ts->typetab = &gdecl->fdefn.stmt->compound.typetab;

        assert(node->type->type == TYPE_FUNC);
        SL_FOREACH(cur, &node->type->func.params) {
            decl_t *decl = GET_ELEM(&node->type->func.params, cur);
            decl_node_t *node = sl_head(&decl->decls);
            assert(node != NULL);

            trans_decl_node(ts, node, IR_DECL_NODE_FUNC_PARAM,
                            &ir_gdecl->func.body);
        }

        trans_stmt(ts, gdecl->fdefn.stmt, &ir_gdecl->func.body);
        sl_append(ir_gdecls, &ir_gdecl->link);

        // Restore state
        ts->func = NULL;
        ts->typetab = typetab_save;
        break;
    }
    case GDECL_DECL: {
        // Ignore typedefs
        type_t *type = gdecl->decl->type;
        if (type->type == TYPE_MOD && type->mod.type_mod & TMOD_TYPEDEF) {
            return;
        }
        SL_FOREACH(cur, &gdecl->decl->decls) {
            ir_gdecl_t *ir_gdecl;
            decl_node_t *node = GET_ELEM(&gdecl->decl->decls, cur);
            if (node->type->type == TYPE_FUNC) {
                ir_gdecl = ir_gdecl_create(IR_GDECL_FUNC_DECL);
                ir_gdecl->func_decl.name = node->id;
                ir_gdecl->func_decl.type = trans_type(ts, node->type);
            } else {
                ir_gdecl = ir_gdecl_create(IR_GDECL_GDATA);
                trans_decl_node(ts, node, IR_DECL_NODE_GLOBAL,
                                &ir_gdecl->gdata.stmts);
            }
            sl_append(ir_gdecls, &ir_gdecl->link);
        }
        break;
    }
    default:
        assert(false);
    }
}

void trans_stmt(trans_state_t *ts, stmt_t *stmt, ir_inst_stream_t *ir_stmts) {
    switch (stmt->type) {
    case STMT_NOP:
        break;

    case STMT_DECL: {
        SL_FOREACH(cur, &stmt->decl->decls) {
            decl_node_t *node = GET_ELEM(&stmt->decl->decls, cur);
            trans_decl_node(ts, node, IR_DECL_NODE_LOCAL, ir_stmts);
        }
        break;
    }

    case STMT_LABEL: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = trans_label_create(ts, stmt->label.label);
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        trans_stmt(ts, stmt->label.stmt, ir_stmts);
        break;
    }
    case STMT_CASE: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = stmt->case_params.label;
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        trans_stmt(ts, stmt->case_params.stmt, ir_stmts);
        break;
    }
    case STMT_DEFAULT: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = stmt->default_params.label;
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        trans_stmt(ts, stmt->default_params.stmt, ir_stmts);
        break;
    }

    case STMT_IF: {
        ir_label_t *if_true = trans_numlabel_create(ts);
        ir_label_t *if_false = stmt->if_params.false_stmt == NULL ?
            NULL : trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);

        ir_expr_t *cond = trans_expr(ts, false, stmt->if_params.expr, ir_stmts);
        cond = trans_expr_bool(ts, cond, ir_stmts);

        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = cond;
        ir_stmt->br.if_true = if_true;
        ir_stmt->br.if_false = if_false == NULL ? after : if_false;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // True branch
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = if_true;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        trans_stmt(ts, stmt->if_params.true_stmt, ir_stmts);
        // Unconditonal branch only if last instruction was not a return
        ir_stmt_t *last = ir_inst_stream_tail(ir_stmts);
        if (!(last != NULL && last->type == IR_STMT_RET)) {
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
            ir_stmt->br.cond = NULL;
            ir_stmt->br.uncond = after;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        }

        if (if_false != NULL) {
            // False branch
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
            ir_stmt->label = if_false;
            trans_add_stmt(ts, ir_stmts, ir_stmt);

            trans_stmt(ts, stmt->if_params.false_stmt, ir_stmts);
            // Unconditonal branch only if last instruction was not a return
            ir_stmt_t *last = ir_inst_stream_tail(ir_stmts);
            if (!(last != NULL && last->type == IR_STMT_RET)) {
                ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
                ir_stmt->br.cond = NULL;
                ir_stmt->br.uncond = after;
                trans_add_stmt(ts, ir_stmts, ir_stmt);
            }
        }

        // End label
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        break;
    }
    case STMT_SWITCH: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_SWITCH);
        ir_stmt->switch_params.expr =
            trans_expr(ts, false, stmt->switch_params.expr, ir_stmts);
        SL_FOREACH(cur, &stmt->switch_params.cases) {
            stmt_t *cur_case = GET_ELEM(&stmt->switch_params.cases, cur);
            ir_label_t *label = trans_numlabel_create(ts);

            assert(cur_case->type == STMT_CASE);
            cur_case->case_params.label = label;

            long long case_val;
            typecheck_const_expr_eval(ts->typetab,
                                      cur_case->case_params.val, &case_val);

            ir_expr_label_pair_t *pair = emalloc(sizeof(ir_expr_label_pair_t));
            pair->expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
            pair->expr->const_params.int_val = case_val;
            pair->expr->const_params.type = &SWITCH_VAL_TYPE;
            pair->expr->const_params.ctype = IR_CONST_INT;
            pair->label = label;

            sl_append(&ir_stmt->switch_params.cases, &pair->link);
        }

        // Generate default label
        ir_label_t *label = trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);

        ir_label_t *break_save = ts->break_target;
        ts->break_target = after;

        stmt->switch_params.default_stmt->default_params.label = label;
        ir_stmt->switch_params.default_case = label;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        trans_stmt(ts, stmt->switch_params.stmt, ir_stmts);

        // Preceeding label
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Restore break target
        ts->break_target = break_save;
        break;
    }

    case STMT_DO: {
        ir_label_t *body = trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);
        ir_label_t *break_save = ts->break_target;
        ir_label_t *continue_save = ts->continue_target;
        ts->break_target = after;
        ts->continue_target = body;

        // Unconditional branch to body
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = body;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Loop body
        trans_stmt(ts, stmt->while_params.stmt, ir_stmts);

        // Loop test
        ir_expr_t *test = trans_expr(ts, false, stmt->while_params.expr,
                                     ir_stmts);
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = test;
        ir_stmt->br.if_true = body;
        ir_stmt->br.if_false = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // End label
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Restore state
        ts->break_target = break_save;
        ts->continue_target = continue_save;
        break;
    }
    case STMT_WHILE: {
        ir_label_t *cond = trans_numlabel_create(ts);
        ir_label_t *body = trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);
        ir_label_t *break_save = ts->break_target;
        ir_label_t *continue_save = ts->continue_target;
        ts->break_target = after;
        ts->continue_target = cond;

        // Unconditional branch to test
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = cond;
        trans_add_stmt(ts, ir_stmts, ir_stmt);


        // Loop test
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = cond;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        ir_expr_t *test = trans_expr(ts, false, stmt->while_params.expr,
                                     ir_stmts);

        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = test;
        ir_stmt->br.if_true = body;
        ir_stmt->br.if_false = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Loop body
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = body;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        trans_stmt(ts, stmt->while_params.stmt, ir_stmts);
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = cond;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // End label
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Restore state
        ts->break_target = break_save;
        ts->continue_target = continue_save;
        break;
    }
    case STMT_FOR: {
        ir_label_t *cond = trans_numlabel_create(ts);
        ir_label_t *body = trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);
        ir_label_t *break_save = ts->break_target;
        ir_label_t *continue_save = ts->continue_target;
        ts->break_target = after;
        ts->continue_target = cond;

        typetab_t *typetab_save;
        // Loop header:
        if (stmt->for_params.decl1 != NULL) {
            typetab_save = ts->typetab;
            ts->typetab = stmt->for_params.typetab;
            SL_FOREACH(cur, &stmt->for_params.decl1->decls) {
                trans_decl_node(ts,
                                GET_ELEM(&stmt->for_params.decl1->decls, cur),
                                IR_DECL_NODE_LOCAL, ir_stmts);
            }
        } else if (stmt->for_params.expr1 != NULL) {
            trans_expr(ts, false, stmt->for_params.expr1, ir_stmts);
            typetab_save = NULL;
        }

        // Unconditional branch to test
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = cond;
        trans_add_stmt(ts, ir_stmts, ir_stmt);


        // Loop test
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = cond;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        if (stmt->for_params.expr2 == NULL) {
            // If there's not a second expression, just do an unconditional jump
            // to the body
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
            ir_stmt->br.cond = NULL;
            ir_stmt->br.uncond = body;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        } else {
            ir_expr_t *test = trans_expr(ts, false, stmt->for_params.expr2,
                                         ir_stmts);

            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
            ir_stmt->br.cond = test;
            ir_stmt->br.if_true = body;
            ir_stmt->br.if_false = after;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        }

        // Loop body
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = body;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        trans_stmt(ts, stmt->for_params.stmt, ir_stmts);
        if (stmt->for_params.expr3 != NULL) {
            trans_expr(ts, false, stmt->for_params.expr3, ir_stmts);
        }
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = cond;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // End label
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Restore state
        ts->break_target = break_save;
        ts->continue_target = continue_save;
        if (typetab_save != NULL) {
            ts->typetab = typetab_save;
        }
        break;
    }

    case STMT_GOTO: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = trans_label_create(ts, stmt->goto_params.label);
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        break;
    }
    case STMT_CONTINUE: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        assert(ts->continue_target != NULL);
        ir_stmt->br.uncond = ts->continue_target;
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        break;
    }
    case STMT_BREAK: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        assert(ts->break_target != NULL);
        ir_stmt->br.uncond = ts->break_target;
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        break;
    }
    case STMT_RETURN: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_RET);
        assert(ts->func->type == IR_GDECL_FUNC &&
               ts->func->func.type->type == IR_TYPE_FUNC);
        ir_stmt->ret.type = ts->func->func.type->func.type;
        ir_expr_t *ret_val = trans_expr(ts, false, stmt->return_params.expr,
                                        ir_stmts);
        ir_stmt->ret.val =
            trans_type_conversion(ts, stmt->return_params.type,
                                  stmt->return_params.expr->etype, ret_val,
                                  ir_stmts);
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        break;
    }

    case STMT_COMPOUND: {
        typetab_t *typetab_save = ts->typetab;
        ts->typetab = &stmt->compound.typetab;
        bool ignore_until_label = false;

        SL_FOREACH(cur, &stmt->compound.stmts) {
            stmt_t *cur_stmt = GET_ELEM(&stmt->compound.stmts, cur);
            if (ignore_until_label) {
                if (cur_stmt->type == STMT_LABEL) {
                    ignore_until_label = false;
                } else {
                    continue;
                }
            }
            // If we encounter a return statement, ignore all statements until
            // a labeled statement
            if (cur_stmt->type == STMT_RETURN) {
                ignore_until_label = true;
            }
            trans_stmt(ts, cur_stmt, ir_stmts);
        }
        ts->typetab = typetab_save;
        break;
    }

    case STMT_EXPR:
        trans_expr(ts, false, stmt->expr.expr, ir_stmts);
        break;
    default:
        assert(false);
    }
}

ir_expr_t *trans_expr(trans_state_t *ts, bool addrof, expr_t *expr,
                      ir_inst_stream_t *ir_stmts) {
    // TODO0: Handle other addrof cases
    switch (expr->type) {
    case EXPR_VOID:
        return NULL;
    case EXPR_PAREN:
        return trans_expr(ts, false, expr->paren_base, ir_stmts);
    case EXPR_VAR: {
        typetab_entry_t *tt_ent = tt_lookup(ts->typetab, expr->var_id);
        assert(tt_ent != NULL && tt_ent->entry_type == TT_VAR);
        ir_symtab_entry_t *entry = tt_ent->var.ir_entry;

        // Must be valid if typechecked
        assert(entry != NULL && entry->type == IR_SYMTAB_ENTRY_VAR);

        if (ir_expr_type(entry->var.access)->type == IR_TYPE_PTR) {
            if (addrof) {
                // If we're taking address of variable, just return it
                return entry->var.access;
            }
            ir_type_t *type = ir_expr_type(entry->var.access)->ptr.base;
            // Load var into a temp
            ir_expr_t *temp = trans_temp_create(ts, type);
            ir_expr_t *load = ir_expr_create(ts->tunit, IR_EXPR_LOAD);
            load->load.type = ir_expr_type(entry->var.access)->ptr.base;
            load->load.ptr = entry->var.access;

            ir_stmt_t *assign = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
            assign->assign.dest = temp;
            assign->assign.src = load;
            trans_add_stmt(ts, ir_stmts, assign);

            return temp;
        } else {
            if (addrof) { // Can't take address of register variable
                assert(false);
            }
            return entry->var.access;
        }
    }
    case EXPR_ASSIGN: {
        if (expr->assign.op == OP_NOP) {
            ir_expr_t *src = trans_expr(ts, false, expr->assign.expr, ir_stmts);
            return trans_assign(ts, expr->assign.dest, src, ir_stmts);
        }
        ir_type_t *type = trans_type(ts, expr->etype);
        ir_expr_t *dest;
        ir_expr_t *op_expr = trans_binop(ts, expr->assign.dest,
                                         expr->assign.expr, expr->assign.op,
                                         expr->etype, ir_stmts, &dest);

        ir_expr_t *temp = trans_temp_create(ts, type);

        ir_stmt_t *binop = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        binop->assign.dest = temp;
        binop->assign.src = op_expr;
        trans_add_stmt(ts, ir_stmts, binop);

        return trans_assign(ts, expr->assign.dest, temp, ir_stmts);
    }
    case EXPR_CONST_INT: {
        ir_expr_t *ir_expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_expr->const_params.ctype = IR_CONST_INT;
        ir_expr->const_params.type = trans_type(ts, expr->const_val.type);
        ir_expr->const_params.int_val = expr->const_val.int_val;
        return ir_expr;
    }
    case EXPR_CONST_FLOAT: {
        ir_expr_t *ir_expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_expr->const_params.ctype = IR_CONST_FLOAT;
        ir_expr->const_params.type = trans_type(ts, expr->const_val.type);
        ir_expr->const_params.float_val = expr->const_val.float_val;
        return ir_expr;
    }
    case EXPR_CONST_STR: {
        ir_expr_t *ir_expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_expr->const_params.ctype = IR_CONST_ARR;
        ir_expr->const_params.type = trans_type(ts, expr->const_val.type);
        // TODO0: Create a global string, change str_val to point to that
        assert(false);
        return ir_expr;
    }
    case EXPR_BIN: {
        ir_type_t *type = trans_type(ts, expr->etype);
        ir_expr_t *op_expr = trans_binop(ts, expr->bin.expr1, expr->bin.expr2,
                                         expr->bin.op, expr->etype, ir_stmts,
                                         NULL);
        ir_expr_t *temp = trans_temp_create(ts, type);

        ir_stmt_t *binop = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        binop->assign.dest = temp;
        binop->assign.src = op_expr;

        trans_add_stmt(ts, ir_stmts, binop);
        return temp;
    }
    case EXPR_UNARY:
        return trans_unaryop(ts, expr, ir_stmts);

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

        ir_expr_t *temp = trans_temp_create(ts, type);

        ir_stmt_t *assign = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        assign->assign.dest = temp;
        assign->assign.src = phi;
        trans_add_stmt(ts, ir_stmts, assign);

        return temp;
    }
    case EXPR_CAST: {
        ir_expr_t *src_expr = trans_expr(ts, false, expr->cast.base, ir_stmts);
        return trans_type_conversion(ts, expr->etype, expr->cast.base->etype,
                                     src_expr, ir_stmts);
    }
    case EXPR_CALL: {
        ir_expr_t *call = ir_expr_create(ts->tunit, IR_EXPR_CALL);
        call->call.func_sig = trans_type(ts, expr->call.func->etype);
        call->call.func_ptr = trans_expr(ts, false, expr->call.func, ir_stmts);

        SL_FOREACH(cur, &expr->call.params) {
            expr_t *param = GET_ELEM(&expr->call.params, cur);
            ir_type_expr_pair_t *pair = emalloc(sizeof(*pair));
            pair->type = trans_type(ts, param->etype);
            pair->expr = trans_expr(ts, false, param, ir_stmts);
            sl_append(&call->call.arglist, &pair->link);
        }

        // Void returning function, don't create a temp
        if (expr->call.func->etype->func.type->type == TYPE_VOID) {
            return NULL;
        }

        ir_type_t *ret_type = call->call.func_sig->func.type;
        ir_expr_t *temp = trans_temp_create(ts, ret_type);
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        ir_stmt->assign.dest = temp;
        ir_stmt->assign.src = call;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        return temp;
    }
    case EXPR_CMPD: {
        ir_expr_t *ir_expr = NULL;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            expr_t *expr = GET_ELEM(&expr->cmpd.exprs, cur);
            ir_expr = trans_expr(ts, false, expr, ir_stmts);
        }
        return ir_expr;
    }
    case EXPR_SIZEOF: {
        ir_expr_t *ir_expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_expr->const_params.type = trans_type(ts, expr->etype);
        ir_expr->const_params.ctype = IR_CONST_INT;
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
        ir_expr_t *ir_expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_expr->const_params.type = trans_type(ts, expr->etype);
        ir_expr->const_params.ctype = IR_CONST_INT;
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
        ir_expr_t *ir_expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_expr->const_params.type = trans_type(ts, expr->etype);
        ir_expr->const_params.ctype = IR_CONST_INT;
        ir_expr->const_params.int_val =
            ast_type_offset(expr->offsetof_params.type->type,
                            &expr->offsetof_params.path);
        return ir_expr;
    }
    case EXPR_MEM_ACC: {
        ir_expr_t *pointer = trans_expr(ts, false, expr->mem_acc.base,
                                        ir_stmts);
        if (expr->mem_acc.op == OP_ARROW) {
            // TODO0: Handle this
            assert(false);
        } else {
            assert(expr->mem_acc.op = OP_DOT);
        }
        ir_expr_t *elem_ptr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
        elem_ptr->getelemptr.type = trans_type(ts, expr->etype);
        elem_ptr->getelemptr.ptr_val = pointer;

        // Get 0th index to point to structure
        ir_type_expr_pair_t *pair = emalloc(sizeof(*pair));
        pair->type = &ir_type_i32;
        pair->expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        pair->expr->const_params.type = &ir_type_i32;
        pair->expr->const_params.ctype = IR_CONST_INT;
        pair->expr->const_params.int_val = 0;
        sl_append(&elem_ptr->getelemptr.idxs, &pair->link);

        // Get index into the structure
        pair = emalloc(sizeof(*pair));
        pair->type = &ir_type_i32;
        pair->expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        pair->expr->const_params.type = &ir_type_i32;
        pair->expr->const_params.ctype = IR_CONST_INT;
        pair->expr->const_params.int_val =
            ast_get_member_num(expr->mem_acc.base->etype, expr->mem_acc.name);
        sl_append(&elem_ptr->getelemptr.idxs, &pair->link);

        // Load instruction
        ir_expr_t *load = ir_expr_create(ts->tunit, IR_EXPR_LOAD);
        load->load.type = elem_ptr->getelemptr.type;
        load->load.ptr = elem_ptr;
        return load;
    }
    case EXPR_ARR_IDX: {
        ir_expr_t *elem_ptr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
        elem_ptr->getelemptr.type = trans_type(ts, expr->etype);
        elem_ptr->getelemptr.ptr_val = trans_expr(ts, false,
                                                  expr->arr_idx.array,
                                                  ir_stmts);

        // Get 0th index to point to array
        ir_type_expr_pair_t *pair = emalloc(sizeof(*pair));
        pair->type = &ir_type_i32;
        pair->expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        pair->expr->const_params.type = &ir_type_i32;
        pair->expr->const_params.ctype = IR_CONST_INT;
        pair->expr->const_params.int_val = 0;
        sl_append(&elem_ptr->getelemptr.idxs, &pair->link);

        // Get index into the array
        pair = emalloc(sizeof(*pair));
        pair->type = &ir_type_i64;
        pair->expr = trans_expr(ts, false, expr->arr_idx.index, ir_stmts);
        sl_append(&elem_ptr->getelemptr.idxs, &pair->link);

        ir_expr_t *load = ir_expr_create(ts->tunit, IR_EXPR_LOAD);
        load->load.type = elem_ptr->getelemptr.type;
        load->load.ptr = elem_ptr;
        return load;
    }
    case EXPR_INIT_LIST:
    case EXPR_DESIG_INIT:
        // TODO0: Create global data and copy it in
        assert(false);
    default:
        assert(false);
    }
    return NULL;
}

ir_expr_t *trans_assign(trans_state_t *ts, expr_t *dest, ir_expr_t *src,
                        ir_inst_stream_t *ir_stmts) {
    ir_expr_t *ptr = NULL;
    while (dest->type == EXPR_PAREN) {
        dest = dest->paren_base;
    }
    switch (dest->type) {
    case EXPR_VAR: {
        ir_symtab_entry_t *entry = ir_symtab_lookup(&ts->func->func.locals,
                                                    dest->var_id);
        // Must be valid if typechecked
        assert(entry != NULL && entry->type == IR_SYMTAB_ENTRY_VAR);

        ptr = entry->var.access;
        break;
    }

    case EXPR_MEM_ACC:
        // TODO0: This
        assert(false);
        break;
    case EXPR_ARR_IDX:
        // TODO0: This
        assert(false);
        break;
    case EXPR_UNARY: {
        ptr = trans_unaryop(ts, dest, ir_stmts);
        break;
    }
    case EXPR_CMPD: {
        // TODO0: This
        assert(false);
        break;
    }
    default:
        // Would not have typechecked if its not an lvalue
        assert(false);
    }
    ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_STORE);
    ir_stmt->store.type = ir_expr_type(src);
    ir_stmt->store.val = src;
    ir_stmt->store.ptr = ptr;
    trans_add_stmt(ts, ir_stmts, ir_stmt);
    return src;
}


ir_expr_t *trans_expr_bool(trans_state_t *ts, ir_expr_t *expr,
                           ir_inst_stream_t *ir_stmts) {
    ir_type_t *type = ir_expr_type(expr);
    if (type->type == IR_TYPE_INT && type->int_params.width == 1) {
        return expr;
    }
    bool is_float = type->type == IR_TYPE_FLOAT;

    ir_expr_t *temp = trans_temp_create(ts, &ir_type_i1);
    ir_expr_t *cmp;
    ir_expr_t *zero = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    zero->const_params.type = type;
    zero->const_params.ctype = IR_CONST_INT;
    if (is_float) {
        zero->const_params.float_val = 0.0;
        cmp = ir_expr_create(ts->tunit, IR_EXPR_FCMP);
        cmp->fcmp.cond = IR_FCMP_ONE;
        cmp->fcmp.type = type;
        cmp->fcmp.expr1 = expr;
        cmp->fcmp.expr2 = zero;
    } else {
        zero->const_params.int_val = 0;
        cmp = ir_expr_create(ts->tunit, IR_EXPR_ICMP);
        cmp->icmp.cond = IR_ICMP_NE;
        cmp->icmp.type = type;
        cmp->icmp.expr1 = expr;
        cmp->icmp.expr2 = zero;
    }
    ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
    stmt->assign.dest = temp;
    stmt->assign.src = cmp;
    trans_add_stmt(ts, ir_stmts, stmt);
    return temp;
}

ir_expr_t *trans_binop(trans_state_t *ts, expr_t *left, expr_t *right,
                       oper_t op, type_t *type, ir_inst_stream_t *ir_stmts,
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
        ir_label_t *cur_block = ts->func->func.last_label;

        ir_label_t *right_label = trans_numlabel_create(ts);
        ir_label_t *done = trans_numlabel_create(ts);

        // Create left expression
        ir_expr_t *left_expr = trans_expr(ts, false, left, ir_stmts);
        ir_expr_t *ir_expr = trans_expr_bool(ts, left_expr, ir_stmts);

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
        pred->expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        pred->expr->const_params.type = &ir_type_i1;
        pred->expr->const_params.ctype = IR_CONST_INT;
        if (is_and) {
            pred->expr->const_params.int_val = 0;
        } else {
            pred->expr->const_params.int_val = 1;
        }
        pred->label = cur_block;
        sl_append(&ir_expr->phi.preds, &pred->link);

        pred = emalloc(sizeof(*pred));
        pred->expr = right_val;
        pred->label = right_label;
        sl_append(&ir_expr->phi.preds, &pred->link);

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
        ir_expr_t *cmp;
        ir_expr_t *left_expr;
        if (is_float) {
            cmp = ir_expr_create(ts->tunit, IR_EXPR_FCMP);
            cmp->fcmp.cond = cmp_type;
            left_expr = trans_expr(ts, false, left, ir_stmts);
            cmp->fcmp.expr1 = left_expr;
            cmp->fcmp.expr2 = trans_expr(ts, false, right, ir_stmts);
            cmp->fcmp.type = ir_expr_type(left_expr);
        } else {
            cmp = ir_expr_create(ts->tunit, IR_EXPR_ICMP);
            cmp->icmp.cond = cmp_type;
            left_expr = trans_expr(ts, false, left, ir_stmts);
            cmp->icmp.expr1 = left_expr;
            cmp->icmp.expr2 = trans_expr(ts, false, right, ir_stmts);;
            cmp->icmp.type = ir_expr_type(left_expr);
        }
        if (left_loc != NULL) {
            *left_loc = left_expr;
        }
        return cmp;
    }

    // Basic bin op case
    ir_expr_t *op_expr = ir_expr_create(ts->tunit, IR_EXPR_BINOP);
    op_expr->binop.op = ir_op;
    op_expr->binop.type = trans_type(ts, type);

    // Evaluate the types and convert types if necessary
    ir_expr_t *left_expr = trans_expr(ts, false, left, ir_stmts);
    op_expr->binop.expr1 = trans_type_conversion(ts, type, left->etype,
                                                 left_expr, ir_stmts);
    ir_expr_t *right_expr = trans_expr(ts, false, right, ir_stmts);
    op_expr->binop.expr2 = trans_type_conversion(ts, type, right->etype,
                                                 right_expr, ir_stmts);
    if (left_loc != NULL) {
        *left_loc = left_expr;
    }
    return op_expr;
}

ir_expr_t *trans_unaryop(trans_state_t *ts, expr_t *expr,
                         ir_inst_stream_t *ir_stmts) {
    assert(expr->type == EXPR_UNARY);
    oper_t op = expr->unary.op;
    if (op == OP_ADDR) {
        return trans_expr(ts, true, expr->unary.expr, ir_stmts);
    }
    ir_expr_t *ir_expr = trans_expr(ts, false, expr->unary.expr, ir_stmts);
    ir_type_t *type = ir_expr_type(ir_expr);
    switch (op) {
    case OP_UPLUS:
        // Do nothing
        return ir_expr;

    case OP_ADDR:
        // Handled above
        assert(false);
        return NULL;

    case OP_PREINC:
    case OP_PREDEC:
    case OP_POSTINC:
    case OP_POSTDEC: {
        ir_expr_t *temp = trans_temp_create(ts, type);
        ir_expr_t *op_expr = ir_expr_create(ts->tunit, IR_EXPR_BINOP);

        switch (op) {
        case OP_PREINC:
        case OP_POSTINC: op_expr->binop.op = IR_OP_ADD; break;
        case OP_PREDEC:
        case OP_POSTDEC: op_expr->binop.op = IR_OP_ADD; break;
        default: assert(false);
        }
        ir_expr_t *other = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        other->const_params.ctype = IR_CONST_INT;
        other->const_params.type = type;
        other->const_params.int_val = 1;
        op_expr->binop.expr1 = ir_expr;
        op_expr->binop.expr2 = other;
        op_expr->binop.type = type;

        ir_stmt_t *assign = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        assign->assign.dest = temp;
        assign->assign.src = op_expr;
        trans_add_stmt(ts, ir_stmts, assign);

        trans_assign(ts, expr->unary.expr, temp, ir_stmts);

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
    case OP_DEREF: {
        ir_expr_t *temp = trans_temp_create(ts, type);
        ir_expr_t *load = ir_expr_create(ts->tunit, IR_EXPR_LOAD);
        assert(type->type == IR_TYPE_PTR);
        load->load.type = type->ptr.base;
        load->load.ptr = ir_expr;

        ir_stmt_t *assign = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        assign->assign.dest = temp;
        assign->assign.src = load;
        trans_add_stmt(ts, ir_stmts, assign);
        return temp;
    }

    case OP_LOGICNOT:
        // Convert expression to bool, then do a bitwise not
        ir_expr = trans_expr_bool(ts, ir_expr, ir_stmts);
        op = OP_BITNOT;
        // FALL THROUGH
    case OP_BITNOT:
    case OP_UMINUS: {
        bool is_bnot = op == OP_BITNOT;
        ir_expr_t *temp = trans_temp_create(ts, type);
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
        ir_expr_t *other = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        other->const_params.ctype = IR_CONST_INT;
        other->const_params.type = type;
        if (is_bnot) {
            other->const_params.int_val = -1;
        } else {
            other->const_params.int_val = 0;
        }
        op_expr->binop.expr1 = other;
        op_expr->binop.expr2 = ir_expr;
        op_expr->binop.type = type;

        ir_stmt_t *assign = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        assign->assign.dest = temp;
        assign->assign.src = op_expr;
        trans_add_stmt(ts, ir_stmts, assign);
        return temp;
    }
    default: break;
    }
    assert(false);
    return NULL;
}

ir_expr_t *trans_type_conversion(trans_state_t *ts, type_t *dest, type_t *src,
                                 ir_expr_t *src_expr,
                                 ir_inst_stream_t *ir_stmts) {
    // Don't do anything if types are equal
    if (typecheck_type_equal(ast_type_unmod(dest), ast_type_unmod(src))) {
        return src_expr;
    }

    ir_type_t *dest_type = trans_type(ts, dest);
    ir_type_t *src_type = trans_type(ts, src);
    ir_expr_t *temp = trans_temp_create(ts, dest_type);

    ir_expr_t *convert = ir_expr_create(ts->tunit, IR_EXPR_CONVERT);
    ir_convert_t convert_op;
    switch (dest_type->type) {
    case IR_TYPE_INT: {
        bool dest_signed =
            dest->type == TYPE_MOD && dest->mod.type_mod & TMOD_UNSIGNED;
        switch (src_type->type) {
        case IR_TYPE_INT:
            if (dest_type->int_params.width < src_type->int_params.width) {
                convert_op = IR_CONVERT_TRUNC;
            } else {
                bool src_signed =
                    src->type == TYPE_MOD && src->mod.type_mod & TMOD_UNSIGNED;
                if (src_signed) {
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
        case IR_TYPE_INT: {
            bool src_signed =
                src->type == TYPE_MOD && src->mod.type_mod & TMOD_UNSIGNED;
            if (src_signed) {
                convert_op = IR_CONVERT_FPTOSI;
            } else {
                convert_op = IR_CONVERT_FPTOUI;
            }
            break;
        }
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

    case IR_TYPE_OPAQUE:
    case IR_TYPE_VOID:
    case IR_TYPE_STRUCT:
    default:
        assert(false);
    }
    convert->convert.type = convert_op;
    convert->convert.src_type = src_type;
    convert->convert.val = src_expr;
    convert->convert.dest_type = dest_type;

    ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
    ir_stmt->assign.dest = temp;
    ir_stmt->assign.src = convert;
    trans_add_stmt(ts, ir_stmts, ir_stmt);
    return temp;
}

char *trans_decl_node_name(ir_symtab_t *symtab, char *name, bool *name_owned) {
    ir_symtab_entry_t *entry = ir_symtab_lookup(symtab, name);
    if (entry == NULL) {
        *name_owned = false;
        return name;
    }

    size_t name_len = strlen(name);
    size_t patch_len = name_len + MAX_NUM_LEN;
    char *patch_name = emalloc(patch_len);
    int number = ++entry->number;
    sprintf(patch_name, "%s%d", name, number);

    do { // Keep trying to increment number until we find unused name
        ir_symtab_entry_t *test = ir_symtab_lookup(symtab, patch_name);
        if (test == NULL) {
            break;
        }
        ++number;
        sprintf(patch_name + name_len, "%d", number);
    } while(true);

    // Record next number to try from
    entry->number = number;

    *name_owned = true;
    return patch_name;
}

void trans_decl_node(trans_state_t *ts, decl_node_t *node,
                     ir_decl_node_type_t type, ir_inst_stream_t *ir_stmts) {
    ir_expr_t *var_expr = ir_expr_create(ts->tunit, IR_EXPR_VAR);
    ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    ptr_type->ptr.base = trans_type(ts, node->type);

    ir_symtab_t *symtab;
    ir_expr_t *access;
    bool name_owned = false;

    switch (type) {
    case IR_DECL_NODE_FDEFN:
        var_expr->var.type = ptr_type->ptr.base;
        var_expr->var.name = node->id;
        var_expr->var.local = false;

        symtab = &ts->tunit->globals;
        access = var_expr;
        break;
    case IR_DECL_NODE_GLOBAL: {
        var_expr->var.type = ptr_type;
        var_expr->var.name = node->id;
        var_expr->var.local = false;

        ir_expr_t *src = node->expr == NULL ?
            NULL : trans_expr(ts, false, node->expr, ir_stmts);

        ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        stmt->assign.dest = var_expr;
        stmt->assign.src = src;
        trans_add_stmt(ts, ir_stmts, stmt);

        symtab = &ts->tunit->globals;
        access = var_expr;
        break;
    }
    case IR_DECL_NODE_LOCAL: {
        symtab = &ts->func->func.locals;

        var_expr->var.type = ptr_type;
        var_expr->var.name = trans_decl_node_name(symtab, node->id,
                                                  &name_owned);
        var_expr->var.local = true;

        // Have to allocate variable on the stack
        ir_expr_t *src = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
        src->alloca.type = var_expr->var.type->ptr.base;
        src->alloca.nelem_type = NULL;
        src->alloca.align = ast_type_align(node->type);

        // Assign the named variable to the allocation
        ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        stmt->assign.dest = var_expr;
        stmt->assign.src = src;
        trans_add_stmt(ts, &ts->func->func.prefix, stmt);

        // If there's an initialization, evaluate it and store it
        if (node->expr != NULL) {
            ir_stmt_t *store = ir_stmt_create(ts->tunit, IR_STMT_STORE);
            store->store.type = trans_type(ts, node->type);
            store->store.val = trans_expr(ts, false, node->expr, ir_stmts);
            store->store.ptr = var_expr;
            trans_add_stmt(ts, ir_stmts, store);
        }

        access = var_expr;
        break;
    }
    case IR_DECL_NODE_FUNC_PARAM:
        symtab = &ts->func->func.locals;

        var_expr->var.type = trans_type(ts, node->type);
        var_expr->var.name = trans_decl_node_name(symtab, node->id,
                                                  &name_owned);
        var_expr->var.local = true;

        ir_expr_t *alloca = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
        alloca->alloca.type = var_expr->var.type;
        alloca->alloca.nelem_type = NULL;
        alloca->alloca.align = ast_type_align(node->type);

        // Stack variable to refer to paramater by
        ir_expr_t *temp = trans_temp_create(ts, ptr_type);
        ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        stmt->assign.dest = temp;
        stmt->assign.src = alloca;
        trans_add_stmt(ts, &ts->func->func.prefix, stmt);

        // Record the function parameter
        sl_append(&ts->func->func.params, &var_expr->link);

        // Store the paramater's value into the stack allocated space
        ir_stmt_t *store = ir_stmt_create(ts->tunit, IR_STMT_STORE);
        store->store.type = var_expr->var.type;
        store->store.val = var_expr;
        store->store.ptr = temp;
        trans_add_stmt(ts, &ts->func->func.body, store);

        access = temp;
        break;
    default:
        assert(false);
    }

    // Create the symbol table entry
    ir_symtab_entry_t *entry =
        ir_symtab_entry_create(IR_SYMTAB_ENTRY_VAR, var_expr->var.name);
    if (name_owned) {
        entry->number = -1;
    }
    entry->var.expr = var_expr;
    entry->var.access = access;
    status_t status = ir_symtab_insert(symtab, entry);
    assert(status == CCC_OK);

    // Associate the given variable with the created entry
    typetab_entry_t *tt_ent = tt_lookup(ts->typetab, node->id);
    assert(tt_ent != NULL && tt_ent->entry_type == TT_VAR);
    tt_ent->var.ir_entry = entry;
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
        ir_type = ir_type_create(ts->tunit, IR_TYPE_STRUCT);
        SL_FOREACH(cur_decl, &type->struct_params.decls) {
            decl_t *decl = GET_ELEM(&type->struct_params.decls, cur_decl);
            SL_FOREACH(cur_node, &decl->decls) {
                decl_node_t *node = GET_ELEM(&decl->decls, cur_node);
                ir_type_t *node_type = trans_type(ts, node->type);
                vec_push_back(&ir_type->struct_params.types, node_type);
            }

            // Add anonymous struct and union members to the struct
            if (sl_head(&decl->decls) == NULL &&
                (decl->type->type == TYPE_STRUCT ||
                 decl->type->type == TYPE_UNION)) {
                ir_type_t *decl_type = trans_type(ts, decl->type);
                vec_push_back(&ir_type->struct_params.types, decl_type);
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
        ir_type = ir_type_create(ts->tunit, IR_TYPE_FUNC);
        ir_type->func.type = trans_type(ts, type->func.type);
        ir_type->func.varargs = type->func.varargs;

        SL_FOREACH(cur, &type->func.params) {
            decl_t *decl = GET_ELEM(&type->func.params, cur);
            decl_node_t *node = sl_head(&decl->decls);
            type_t *ptype = node == NULL ? decl->type : node->type;
            ir_type_t *param_type = trans_type(ts, ptype);
            vec_push_back(&ir_type->func.params, param_type);
        }

        return ir_type;
    case TYPE_ARR:
        ir_type = ir_type_create(ts->tunit, IR_TYPE_ARR);
        if (type->arr.len != NULL) {
            long long size;
            typecheck_const_expr_eval(ts->typetab, type->arr.len, &size);
            ir_type->arr.nelems = size;
        } else {
            ir_type->arr.nelems = 0;
        }
        ir_type->arr.elem_type = trans_type(ts, type->arr.base);
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
    default:
        assert(false);
    }
    return NULL;
}
