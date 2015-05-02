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

#define MAX_GLOBAL_NAME 128
#define GLOBAL_PREFIX ".glo"
#define STRUCT_PREFIX "struct."


void trans_add_stmt(trans_state_t *ts, ir_inst_stream_t *stream,
                    ir_stmt_t *stmt) {
    if (stmt->type == IR_STMT_LABEL) {
        ts->func->func.last_label = stmt->label;
    }
    ir_inst_stream_append(stream, stmt);
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

ir_expr_t *trans_assign_temp(trans_state_t *ts, ir_inst_stream_t *stream,
                             ir_expr_t *expr) {
    ir_expr_t *temp = trans_temp_create(ts, ir_expr_type(expr));

    ir_stmt_t *assign = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
    assign->assign.dest = temp;
    assign->assign.src = expr;
    trans_add_stmt(ts, stream, assign);

    return temp;
}

ir_expr_t *trans_load_temp(trans_state_t *ts, ir_inst_stream_t *stream,
                           ir_expr_t *expr) {
    ir_expr_t *load = ir_expr_create(ts->tunit, IR_EXPR_LOAD);
    ir_type_t *type = ir_expr_type(expr);
    assert(type->type == IR_TYPE_PTR);
    load->load.type = type->ptr.base;
    load->load.ptr = expr;

    return trans_assign_temp(ts, stream, load);
}

ir_trans_unit_t *trans_translate(trans_unit_t *ast) {
    assert(ast != NULL);

    trans_state_t ts = TRANS_STATE_LIT;
    return trans_trans_unit(&ts, ast);
}

ir_trans_unit_t *trans_trans_unit(trans_state_t *ts, trans_unit_t *ast) {
    ir_trans_unit_t *tunit = ir_trans_unit_create();
    ts->tunit = tunit;
    ts->typetab = &ast->typetab;

    // Add this translation unit's function declaration to symbol table
    SL_FOREACH(cur, &ast->gdecls) {
        gdecl_t *gdecl = GET_ELEM(&ast->gdecls, cur);
        if (gdecl->type != GDECL_FDEFN) {
            continue;
        }
        decl_node_t *node = sl_head(&gdecl->decl->decls);
        trans_decl_node(ts, node, IR_DECL_NODE_FDEFN, NULL);
    }

    SL_FOREACH(cur, &ast->gdecls) {
        gdecl_t *gdecl = GET_ELEM(&ast->gdecls, cur);
        trans_gdecl(ts, gdecl, &tunit->funcs);
    }

    return tunit;
}

void trans_gdecl_node(trans_state_t *ts, decl_node_t *node) {
    ir_gdecl_t *ir_gdecl;
    if (node->type->type == TYPE_FUNC) {
        ir_gdecl = ir_gdecl_create(IR_GDECL_FUNC_DECL);
        ir_gdecl->func_decl.type = trans_decl_node(ts, node, IR_DECL_NODE_FDEFN,
                                                   NULL);
        ir_gdecl->func_decl.name = node->id;
    } else {
        ir_gdecl = ir_gdecl_create(IR_GDECL_GDATA);
        trans_decl_node(ts, node, IR_DECL_NODE_GLOBAL, ir_gdecl);
    }
    sl_append(&ts->tunit->decls, &ir_gdecl->link);
}

void trans_gdecl(trans_state_t *ts, gdecl_t *gdecl, slist_t *ir_gdecls) {
    switch (gdecl->type) {
    case GDECL_FDEFN: {
        decl_node_t *node = sl_head(&gdecl->decl->decls);

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

            trans_decl_node(ts, node, IR_DECL_NODE_FUNC_PARAM, NULL);
        }

        bool returns = trans_stmt(ts, gdecl->fdefn.stmt, &ir_gdecl->func.body);

        // If the function didn't return, add a return
        if (!returns) {
            ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_RET);
            ir_stmt->ret.type = ir_gdecl->func.type->func.type;
            ir_stmt->ret.val = NULL;
            trans_add_stmt(ts, &ir_gdecl->func.body, ir_stmt);
        }

        // Remove trailing labels
        ir_stmt_t *last = ir_inst_stream_tail(&ir_gdecl->func.body);
        while (last->type == IR_STMT_LABEL) {
            dl_remove(&ir_gdecl->func.body.list, &last->link);
            last = ir_inst_stream_tail(&ir_gdecl->func.body);
        }

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

        // Keep track of global decls, so they are only translated if used
        SL_FOREACH(cur, &gdecl->decl->decls) {
            decl_node_t *node = GET_ELEM(&gdecl->decl->decls, cur);
            ht_ptr_elem_t *elem = emalloc(sizeof(*elem));
            elem->key = node->id;
            elem->val = node;
            ht_insert(&ts->tunit->global_decls, &elem->link);
        }
        break;
    }
    default:
        assert(false);
    }
}

bool trans_stmt(trans_state_t *ts, stmt_t *stmt, ir_inst_stream_t *ir_stmts) {
    bool returns = false;
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
        returns = trans_stmt(ts, stmt->label.stmt, ir_stmts);
        break;
    }
    case STMT_CASE: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = stmt->case_params.label;
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        returns = trans_stmt(ts, stmt->case_params.stmt, ir_stmts);
        break;
    }
    case STMT_DEFAULT: {
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = stmt->default_params.label;
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        returns = trans_stmt(ts, stmt->default_params.stmt, ir_stmts);
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

        bool true_ret = trans_stmt(ts, stmt->if_params.true_stmt, ir_stmts);

        if (!true_ret) {
            // Unconditonal branch only if last instruction was not a return
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
            ir_stmt->br.cond = NULL;
            ir_stmt->br.uncond = after;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        }

        bool false_ret = false;
        if (if_false != NULL) {
            // False branch
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
            ir_stmt->label = if_false;
            trans_add_stmt(ts, ir_stmts, ir_stmt);

            false_ret = trans_stmt(ts, stmt->if_params.false_stmt, ir_stmts);

            // Unconditonal branch only if last instruction was not a return
            if (!false_ret) {
                ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
                ir_stmt->br.cond = NULL;
                ir_stmt->br.uncond = after;
                trans_add_stmt(ts, ir_stmts, ir_stmt);
            }
        }

        if (true_ret && false_ret) {
            returns = true;
        } else {
            // Only add end label if both branches didn't return
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
            ir_stmt->label = after;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        }
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
            pair->expr = ir_int_const(ts->tunit, &SWITCH_VAL_TYPE, case_val);
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

        // TODO1: Set returns if all of the cases return
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
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = body;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        returns = trans_stmt(ts, stmt->do_params.stmt, ir_stmts);

        // Only translate test and end label if the body doesn't return
        if (!returns) {
            // Loop test
            ir_expr_t *test = trans_expr(ts, false, stmt->do_params.expr,
                                         ir_stmts);
            test = trans_expr_bool(ts, test, ir_stmts);
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
            ir_stmt->br.cond = test;
            ir_stmt->br.if_true = body;
            ir_stmt->br.if_false = after;
            trans_add_stmt(ts, ir_stmts, ir_stmt);

            // End label
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
            ir_stmt->label = after;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        }

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
        test = trans_expr_bool(ts, test, ir_stmts);

        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = test;
        ir_stmt->br.if_true = body;
        ir_stmt->br.if_false = after;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        // Loop body
        ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = body;
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        bool stmt_returns = trans_stmt(ts, stmt->while_params.stmt, ir_stmts);

        // Only add unconditional branch if loop doesn't return
        if (!stmt_returns) {
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
            ir_stmt->br.cond = NULL;
            ir_stmt->br.uncond = cond;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        }

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

        typetab_t *typetab_save = NULL;
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
            test = trans_expr_bool(ts, test, ir_stmts);

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

        bool stmt_returns = trans_stmt(ts, stmt->for_params.stmt, ir_stmts);

        // Only add 3rd expression and unconditional branch if last statement
        // wasn't a return
        if (!stmt_returns) {
            if (stmt->for_params.expr3 != NULL) {
                trans_expr(ts, false, stmt->for_params.expr3, ir_stmts);
            }
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
            ir_stmt->br.cond = NULL;
            ir_stmt->br.uncond = cond;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        }

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

        if (stmt->return_params.expr == NULL) {
            ir_stmt->ret.val = NULL;
        } else {
            ir_expr_t *ret_val = trans_expr(ts, false, stmt->return_params.expr,
                                            ir_stmts);
            ir_stmt->ret.val =
                trans_type_conversion(ts, stmt->return_params.type,
                                      stmt->return_params.expr->etype, ret_val,
                                      ir_stmts);
        }
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        returns = true; // Return statement always returns
        break;
    }

    case STMT_COMPOUND: {
        typetab_t *typetab_save = ts->typetab;
        ts->typetab = &stmt->compound.typetab;
        bool has_break = false;
        bool has_return = false;
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
            if (cur_stmt->type == STMT_BREAK) {
                has_break = true;
            }

            // If we encounter a statement which always returns, ignore all
            // statements until a labeled statement
            if (trans_stmt(ts, cur_stmt, ir_stmts)) {
                has_return = true;
                ignore_until_label = true;
            }
        }
        ts->typetab = typetab_save;

        // The compound statement returns if we found a returning statement,
        // and there is no break
        returns = has_return && !has_break;
        break;
    }

    case STMT_EXPR:
        trans_expr(ts, false, stmt->expr.expr, ir_stmts);
        break;
    default:
        assert(false);
    }

    return returns;
}

ir_expr_t *trans_expr(trans_state_t *ts, bool addrof, expr_t *expr,
                      ir_inst_stream_t *ir_stmts) {
    switch (expr->type) {
    case EXPR_VOID:
        return NULL;
    case EXPR_PAREN:
        return trans_expr(ts, addrof, expr->paren_base, ir_stmts);
    case EXPR_VAR: {
        typetab_t *tt = ts->typetab;
        typetab_entry_t *tt_ent;
        do {
            assert(tt != NULL);
            tt_ent = tt_lookup(tt, expr->var_id);
            assert(tt_ent != NULL);

            // Search through scopes until we find this expression's entry
            if (tt_ent->entry_type == TT_VAR &&
                tt_ent->type == expr->etype) {
                break;
            }

            tt = tt->last;
        } while(true);
        ir_symtab_entry_t *entry = tt_ent->var.ir_entry;

        // Lazily add used global variables to declaration list
        if (entry == NULL) {
            ht_ptr_elem_t *elem = ht_lookup(&ts->tunit->global_decls,
                                            &expr->var_id);
            assert(elem != NULL);
            trans_gdecl_node(ts, elem->val);
            entry = tt_ent->var.ir_entry;
        }

        // Must be valid if typechecked
        assert(entry != NULL && entry->type == IR_SYMTAB_ENTRY_VAR);

        if (ir_expr_type(entry->var.access)->type == IR_TYPE_PTR) {
            if (addrof) {
                // If we're taking address of variable, just return it
                return entry->var.access;
            }
            ir_type_t *type = ir_expr_type(entry->var.access)->ptr.base;

            // Structs and arrays are always refered to by addresses
            switch (type->type) {
            case IR_TYPE_STRUCT:
            case IR_TYPE_ID_STRUCT:
            case IR_TYPE_ARR:
                return entry->var.access;
            default:
                break;
            }

            return trans_load_temp(ts, ir_stmts, entry->var.access);
        } else {
            if (addrof) { // Can't take address of register variable
                assert(false);
            }
            return entry->var.access;
        }
    }
    case EXPR_ASSIGN: {
        ir_expr_t *dest_addr = trans_expr(ts, true, expr->assign.dest,
                                          ir_stmts);
        if (expr->assign.op == OP_NOP) {
            ir_expr_t *src = trans_expr(ts, false, expr->assign.expr, ir_stmts);
            return trans_assign(ts, dest_addr, expr->assign.dest->etype, src,
                                expr->assign.expr->etype, ir_stmts);
        }
        ir_expr_t *dest;
        ir_expr_t *op_expr = trans_binop(ts, expr->assign.dest, dest_addr,
                                         expr->assign.expr, expr->assign.op,
                                         expr->etype, ir_stmts, &dest);

        ir_expr_t *temp = trans_assign_temp(ts, ir_stmts, op_expr);

        return trans_assign(ts, dest_addr, expr->assign.dest->etype, temp,
                            expr->assign.expr->etype, ir_stmts);
    }
    case EXPR_CONST_INT:
        return ir_int_const(ts->tunit, trans_type(ts, expr->const_val.type),
                            expr->const_val.int_val);

    case EXPR_CONST_FLOAT: {
        ir_expr_t *ir_expr = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_expr->const_params.ctype = IR_CONST_FLOAT;
        ir_expr->const_params.type = trans_type(ts, expr->const_val.type);
        ir_expr->const_params.float_val = expr->const_val.float_val;
        return ir_expr;
    }
    case EXPR_CONST_STR: {
        ir_expr_t *ir_expr = trans_string(ts, expr->const_val.str_val);
        return trans_assign_temp(ts, ir_stmts, ir_expr);
    }

    case EXPR_BIN: {
        ir_expr_t *op_expr = trans_binop(ts, expr->bin.expr1, NULL,
                                         expr->bin.expr2, expr->bin.op,
                                         expr->etype, ir_stmts, NULL);
        return trans_assign_temp(ts, ir_stmts, op_expr);
    }
    case EXPR_UNARY:
        return trans_unaryop(ts, addrof, expr, ir_stmts);

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
        expr2 = trans_type_conversion(ts, expr->etype, expr->cond.expr2->etype,
                                      expr2, ir_stmts);

        // Set true source to last created label
        if_true = ts->func->func.last_label;

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
        expr3 = trans_type_conversion(ts, expr->etype, expr->cond.expr3->etype,
                                      expr3, ir_stmts);

        // Set false source to last created label
        if_false = ts->func->func.last_label;

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

        return trans_assign_temp(ts, ir_stmts, phi);
    }
    case EXPR_CAST: {
        ir_expr_t *src_expr = trans_expr(ts, false, expr->cast.base, ir_stmts);
        return trans_type_conversion(ts, expr->etype, expr->cast.base->etype,
                                     src_expr, ir_stmts);
    }
    case EXPR_CALL: {
        ir_expr_t *call = ir_expr_create(ts->tunit, IR_EXPR_CALL);
        type_t *func_sig = expr->call.func->etype;
        call->call.func_sig = trans_type(ts, func_sig);
        call->call.func_ptr = trans_expr(ts, false, expr->call.func, ir_stmts);
        assert(func_sig->type == TYPE_FUNC);

        sl_link_t *cur_sig = func_sig->func.params.head;
        sl_link_t *cur_expr = expr->call.params.head;
        while (cur_sig != NULL) {
            assert(cur_expr != NULL);
            decl_t *decl = GET_ELEM(&func_sig->func.params, cur_sig);
            decl_node_t *node = sl_head(&decl->decls);
            type_t *sig_type = node == NULL ? decl->type : node->type;

            expr_t *param = GET_ELEM(&expr->call.params, cur_expr);
            ir_expr_t *ir_expr = trans_expr(ts, false, param, ir_stmts);
            ir_expr = trans_type_conversion(ts, sig_type, param->etype, ir_expr,
                                            ir_stmts);
            ir_type_expr_pair_t *pair = emalloc(sizeof(*pair));
            pair->type = trans_type(ts, sig_type);
            pair->expr = ir_expr;
            sl_append(&call->call.arglist, &pair->link);

            cur_sig = cur_sig->next;
            cur_expr = cur_expr->next;
        }
        assert(cur_expr == NULL);

        ir_expr_t *result;
        // Void returning function, don't create a temp
        if (expr->call.func->etype->func.type->type == TYPE_VOID) {
            ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_EXPR);
            ir_stmt->expr = call;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
            result = NULL;
        } else {
            result = trans_assign_temp(ts, ir_stmts, call);
        }


        return result;
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
        long long val;
        if (expr->sizeof_params.type != NULL) {
            decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
            if (node != NULL) {
                val = ast_type_size(node->type);
            } else {
                assert(node == sl_tail(&expr->sizeof_params.type->decls));
                val = ast_type_size(expr->sizeof_params.type->type);
            }
        } else {
            assert(expr->sizeof_params.expr != NULL);
            val = ast_type_size(expr->sizeof_params.expr->etype);
        }

        return ir_int_const(ts->tunit, trans_type(ts, expr->etype), val);
    }
    case EXPR_ALIGNOF: {
        long long val;
        if (expr->sizeof_params.type != NULL) {
            decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
            if (node != NULL) {
                val = ast_type_align(node->type);
            } else {
                assert(node == sl_tail(&expr->sizeof_params.type->decls));
                val = ast_type_align(expr->sizeof_params.type->type);
            }
        } else {
            assert(expr->sizeof_params.expr != NULL);
            val = ast_type_align(expr->sizeof_params.expr->etype);
        }

        return ir_int_const(ts->tunit, trans_type(ts, expr->etype), val);
    }
    case EXPR_OFFSETOF: {
        size_t offset = ast_type_offset(expr->offsetof_params.type->type,
                                        &expr->offsetof_params.path);
        return ir_int_const(ts->tunit, trans_type(ts, expr->etype), offset);
    }
    case EXPR_ARR_IDX:
    case EXPR_MEM_ACC: {
        ir_expr_t *elem_ptr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
        ir_type_t *expr_type = trans_type(ts, expr->etype);
        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = expr_type;
        elem_ptr->getelemptr.type = ptr_type;

        bool last_array = false;
        ir_expr_t *pointer;
        ir_type_expr_pair_t *pair;
        while ((expr->type == EXPR_MEM_ACC && expr->mem_acc.op == OP_DOT)
            || expr->type == EXPR_ARR_IDX) {
            if (expr->type == EXPR_MEM_ACC) {
                // Get index into the structure
                pair = emalloc(sizeof(*pair));
                pair->type = &ir_type_i32;
                int mem_num = ast_get_member_num(expr->mem_acc.base->etype,
                                                 expr->mem_acc.name);
                pair->expr = ir_int_const(ts->tunit, &ir_type_i32, mem_num);
                expr = expr->mem_acc.base;
                sl_prepend(&elem_ptr->getelemptr.idxs, &pair->link);
            } else { // expr->type == EXPR_ARR_IDX
                type_t *arr_type = ast_type_unmod(expr->arr_idx.array->etype);

                pair = emalloc(sizeof(*pair));
                ir_expr_t *index = trans_expr(ts, false, expr->arr_idx.index,
                                              ir_stmts);
                index = trans_type_conversion(ts, tt_size_t,
                                              expr->arr_idx.index->etype,
                                              index, ir_stmts);
                pair->type = trans_type(ts, tt_size_t);
                pair->expr = index;
                expr = expr->arr_idx.array;
                sl_prepend(&elem_ptr->getelemptr.idxs, &pair->link);

                // If this is a pointer instead of an array, stop here because
                // we need to do a load for the next index
                if (arr_type->type == TYPE_PTR) {
                    last_array = true;
                    break;
                }
            }
        }

        bool prepend_zero = false;
        if (!last_array && expr->type == EXPR_MEM_ACC) {
            assert(expr->mem_acc.op == OP_ARROW);
            type_t *etype = ast_type_unmod(expr->mem_acc.base->etype);
            assert(etype->type == TYPE_PTR);

            // Get index into the structure
            pair = emalloc(sizeof(*pair));
            pair->type = &ir_type_i32;
            int mem_num =
                ast_get_member_num(etype->ptr.base, expr->mem_acc.name);
            pair->expr = ir_int_const(ts->tunit, &ir_type_i32, mem_num);
            sl_prepend(&elem_ptr->getelemptr.idxs, &pair->link);

            pointer = trans_expr(ts, false, expr->mem_acc.base, ir_stmts);
            prepend_zero = true;
        } else { // !is_arr_idx && expr->type != EXPR_MEM_ACC
            pointer = trans_expr(ts, false, expr, ir_stmts);
            ir_type_t *ptr_type = ir_expr_type(pointer);
            if (!last_array && ptr_type->type == IR_TYPE_PTR) {
                ir_type_t *base_type = ptr_type->ptr.base;

                switch (base_type->type) {
                case IR_TYPE_STRUCT:
                case IR_TYPE_ID_STRUCT:
                case IR_TYPE_ARR:
                    prepend_zero = true;
                default:
                    break;
                }
            }
        }
        if (prepend_zero) {
            pair = emalloc(sizeof(*pair));
            pair->type = &ir_type_i32;
            pair->expr = ir_expr_zero(ts->tunit, &ir_type_i32);
            sl_prepend(&elem_ptr->getelemptr.idxs, &pair->link);
        }
        elem_ptr->getelemptr.ptr_type = ir_expr_type(pointer);
        elem_ptr->getelemptr.ptr_val = pointer;

        ir_expr_t *ptr = trans_assign_temp(ts, ir_stmts, elem_ptr);

        if (addrof) {
            return ptr;
        }

        return trans_load_temp(ts, ir_stmts, ptr);
    }
    case EXPR_INIT_LIST: {
        type_t *etype = ast_type_unmod(expr->etype);
        switch (etype->type) {
        case TYPE_STRUCT:
        case TYPE_UNION:
            // TODO0: This
            break;
        case TYPE_ARR:
            return trans_array_init(ts, expr);
        default: {
            expr_t *head = sl_head(&expr->init_list.exprs);
            assert(head != NULL);
            return trans_expr(ts, false, head, ir_stmts);
        }
        }
        break;
    }
    case EXPR_DESIG_INIT:
        // TODO0: Create global data and copy it in
        assert(false);
    default:
        assert(false);
    }
    return NULL;
}

ir_expr_t *trans_assign(trans_state_t *ts, ir_expr_t *dest_ptr,
                        type_t *dest_type, ir_expr_t *src, type_t *src_type,
                        ir_inst_stream_t *ir_stmts) {
    ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_STORE);
    ir_stmt->store.type = trans_type(ts, dest_type);
    ir_stmt->store.val = trans_type_conversion(ts, dest_type, src_type, src,
                                               ir_stmts);
    ir_stmt->store.ptr = dest_ptr;
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

    ir_expr_t *cmp;
    ir_expr_t *zero = ir_expr_zero(ts->tunit, type);
    if (is_float) {
        cmp = ir_expr_create(ts->tunit, IR_EXPR_FCMP);
        cmp->fcmp.cond = IR_FCMP_ONE;
        cmp->fcmp.type = type;
        cmp->fcmp.expr1 = expr;
        cmp->fcmp.expr2 = zero;
    } else {
        cmp = ir_expr_create(ts->tunit, IR_EXPR_ICMP);
        cmp->icmp.cond = IR_ICMP_NE;
        cmp->icmp.type = type;
        cmp->icmp.expr1 = expr;
        cmp->icmp.expr2 = zero;
    }

    return trans_assign_temp(ts, ir_stmts, cmp);
}

ir_expr_t *trans_binop(trans_state_t *ts, expr_t *left, ir_expr_t *left_addr,
                       expr_t *right, oper_t op, type_t *type,
                       ir_inst_stream_t *ir_stmts, ir_expr_t **left_loc) {
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
            TYPE_IS_INTEGRAL(ast_type_untypedef(type->mod.base))) {
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
    case OP_GT:
    case OP_LE:
    case OP_GE:
    case OP_EQ:
    case OP_NE:
        is_cmp = true;
        break;
    case OP_LOGICAND:
    case OP_LOGICOR: {
        bool is_and = op == OP_LOGICAND;

        ir_label_t *right_label = trans_numlabel_create(ts);
        ir_label_t *done = trans_numlabel_create(ts);

        // Create left expression
        ir_expr_t *left_expr = trans_expr(ts, false, left, ir_stmts);
        ir_expr_t *ir_expr = trans_expr_bool(ts, left_expr, ir_stmts);

        // Must be after translating first expression
        ir_label_t *cur_block = ts->func->func.last_label;

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

        // Set right source to last created label
        right_label = ts->func->func.last_label;

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
        if (is_and) {
            pred->expr = ir_int_const(ts->tunit, &ir_type_i1, 0);
        } else {
            pred->expr = ir_int_const(ts->tunit, &ir_type_i1, 1);
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
        type_t *left_type = ast_type_untypedef(left->etype);
        type_t *right_type = ast_type_untypedef(right->etype);
        type_t *max_type;
        bool success = typecheck_type_max(NULL, left->etype, right->etype,
                                          &max_type);
        // Must be valid if typechecked
        assert(success && max_type != NULL);
        is_float = TYPE_IS_FLOAT(max_type);
        is_signed = !TYPE_IS_UNSIGNED(left_type) &&
            !TYPE_IS_UNSIGNED(right_type);

        switch (op) {
        case OP_LT:
            cmp_type = is_float ? IR_FCMP_OLT :
                is_signed ? IR_ICMP_SLT : IR_ICMP_ULT;
            break;
        case OP_GT:
            cmp_type = is_float ? IR_FCMP_OGT :
                is_signed ? IR_ICMP_SGT : IR_ICMP_UGT;
            break;
        case OP_LE:
            cmp_type = is_float ? IR_FCMP_OLE :
                is_signed ? IR_ICMP_SLE : IR_ICMP_ULE;
            break;
        case OP_GE:
            cmp_type = is_float ? IR_FCMP_OGE :
                is_signed ? IR_ICMP_SGE : IR_ICMP_UGE;
            break;
        case OP_EQ:
            cmp_type = is_float ? IR_FCMP_OEQ : IR_ICMP_EQ;
            break;
        case OP_NE:
            cmp_type = is_float ? IR_FCMP_ONE : IR_ICMP_NE;
            break;
        default:
            assert(false);
        }
        ir_expr_t *cmp;

        ir_expr_t *left_expr = trans_expr(ts, false, left, ir_stmts);
        left_expr = trans_type_conversion(ts, max_type, left->etype, left_expr,
                                          ir_stmts);
        ir_expr_t *right_expr = trans_expr(ts, false, right, ir_stmts);
        right_expr = trans_type_conversion(ts, max_type, right->etype,
                                           right_expr, ir_stmts);
        if (is_float) {
            cmp = ir_expr_create(ts->tunit, IR_EXPR_FCMP);
            cmp->fcmp.cond = cmp_type;
            cmp->fcmp.expr1 = left_expr;
            cmp->fcmp.expr2 = right_expr;
            cmp->fcmp.type = trans_type(ts, max_type);
        } else {
            cmp = ir_expr_create(ts->tunit, IR_EXPR_ICMP);
            cmp->icmp.cond = cmp_type;
            cmp->icmp.expr1 = left_expr;
            cmp->icmp.expr2 = right_expr;
            cmp->icmp.type = trans_type(ts, max_type);
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
    ir_expr_t *left_expr;
    if (left_addr == NULL) {
        left_expr = trans_expr(ts, false, left, ir_stmts);
        ir_expr_t *right_expr = trans_expr(ts, false, right, ir_stmts);
        op_expr->binop.expr2 = trans_type_conversion(ts, type, right->etype,
                                                     right_expr, ir_stmts);
    } else {
        // Evaluate right first in clase left_addr's value is affected by right
        ir_expr_t *right_expr = trans_expr(ts, false, right, ir_stmts);
        op_expr->binop.expr2 = trans_type_conversion(ts, type, right->etype,
                                                     right_expr, ir_stmts);
        left_expr = trans_load_temp(ts, ir_stmts, left_addr);
    }
    op_expr->binop.expr1 = trans_type_conversion(ts, type, left->etype,
                                                 left_expr, ir_stmts);
    if (left_loc != NULL) {
        *left_loc = left_expr;
    }
    return op_expr;
}

ir_expr_t *trans_unaryop(trans_state_t *ts, bool addrof, expr_t *expr,
                         ir_inst_stream_t *ir_stmts) {
    assert(expr->type == EXPR_UNARY);
    oper_t op = expr->unary.op;
    switch (op) {
    case OP_ADDR:
        return trans_expr(ts, true, expr->unary.expr, ir_stmts);

    case OP_PREINC:
    case OP_PREDEC:
    case OP_POSTINC:
    case OP_POSTDEC: {
        ir_expr_t *expr_addr = trans_expr(ts, true, expr->unary.expr, ir_stmts);
        ir_type_t *type = ir_expr_type(expr_addr);
        assert(type->type == IR_TYPE_PTR);

        ir_expr_t *ir_expr = trans_load_temp(ts, ir_stmts, expr_addr);
        ir_expr_t *op_expr = ir_expr_create(ts->tunit, IR_EXPR_BINOP);

        switch (op) {
        case OP_PREINC:
        case OP_POSTINC: op_expr->binop.op = IR_OP_ADD; break;
        case OP_PREDEC:
        case OP_POSTDEC: op_expr->binop.op = IR_OP_SUB; break;
        default: assert(false);
        }
        ir_expr_t *other = ir_int_const(ts->tunit, type->ptr.base, 1);
        op_expr->binop.expr1 = ir_expr;
        op_expr->binop.expr2 = other;
        op_expr->binop.type = type->ptr.base;

        ir_expr_t *temp = trans_assign_temp(ts, ir_stmts, op_expr);
        trans_assign(ts, expr_addr, expr->unary.expr->etype, temp, expr->etype,
                     ir_stmts);

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
    default:
        break;
    }

    ir_expr_t *ir_expr = trans_expr(ts, false, expr->unary.expr, ir_stmts);
    ir_type_t *type = ir_expr_type(ir_expr);
    switch (op) {
    case OP_UPLUS:
        return ir_expr;

    case OP_DEREF: {
        assert(type->type == IR_TYPE_PTR);
        if (addrof) {
            return ir_expr;
        }
        // Don't load from structs
        if (type->ptr.base->type == IR_TYPE_STRUCT ||
            type->ptr.base->type == IR_TYPE_ID_STRUCT) {
            return ir_expr;
        }
        return trans_load_temp(ts, ir_stmts, ir_expr);
    }

    case OP_LOGICNOT:
        // Convert expression to bool, then do a bitwise not
        ir_expr = trans_expr_bool(ts, ir_expr, ir_stmts);
        type = ir_expr_type(ir_expr);
        op = OP_BITNOT;
        // FALL THROUGH
    case OP_BITNOT:
    case OP_UMINUS: {
        bool is_bnot = op == OP_BITNOT;
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
        ir_expr_t *other;
        if (is_bnot) {
            other = ir_int_const(ts->tunit, type, -1);
        } else {
            other = ir_int_const(ts->tunit, type, 0);
        }
        op_expr->binop.expr1 = other;
        op_expr->binop.expr2 = ir_expr;
        op_expr->binop.type = type;

        return trans_assign_temp(ts, ir_stmts, op_expr);
    }
    default:
        break;
    }
    assert(false);
    return NULL;
}

ir_expr_t *trans_type_conversion(trans_state_t *ts, type_t *dest, type_t *src,
                                 ir_expr_t *src_expr,
                                 ir_inst_stream_t *ir_stmts) {
    type_t *orig_dest = ast_type_untypedef(dest);
    type_t *orig_src = ast_type_untypedef(src);
    dest = ast_type_unmod(orig_dest);
    src = ast_type_unmod(orig_src);
    // Don't do anything if types are equal
    if (typecheck_type_equal(dest, src)) {
        return src_expr;
    }
    if (dest->type == TYPE_BOOL) {
        return trans_expr_bool(ts, src_expr, ir_stmts);
    }

    // Special case: If we're assigning array to pointer, and src_expr
    // is already a pointer, then do no conversion if pointed types are equal
    if (dest->type == TYPE_PTR && src->type == TYPE_ARR &&
        ir_expr_type(src_expr)->type == IR_TYPE_PTR) {
        type_t *pointed_dest = ast_type_unmod(dest->ptr.base);
        type_t *pointed_src = ast_type_unmod(src->arr.base);
        if (typecheck_type_equal(pointed_dest, pointed_src)) {
            return src_expr;
        }
    }

    ir_type_t *dest_type = trans_type(ts, dest);
    ir_type_t *src_type = trans_type(ts, src);


    ir_expr_t *convert = ir_expr_create(ts->tunit, IR_EXPR_CONVERT);
    ir_convert_t convert_op;
    switch (dest_type->type) {
    case IR_TYPE_INT: {
        bool dest_signed = !(orig_dest->type == TYPE_MOD &&
                             orig_dest->mod.type_mod & TMOD_UNSIGNED);
        switch (src_type->type) {
        case IR_TYPE_INT:
            if (dest_type->int_params.width < src_type->int_params.width) {
                convert_op = IR_CONVERT_TRUNC;
            } else {
                bool src_signed = !(orig_src->type == TYPE_MOD &&
                                    orig_src->mod.type_mod & TMOD_UNSIGNED);
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
    case IR_TYPE_VOID:
        // Casted to void expression cannot be used, typechecker should ensure
        // this
        return NULL;

    case IR_TYPE_OPAQUE:
    case IR_TYPE_STRUCT:
    default:
        assert(false);
    }
    convert->convert.type = convert_op;
    convert->convert.src_type = src_type;
    convert->convert.val = src_expr;
    convert->convert.dest_type = dest_type;

    return trans_assign_temp(ts, ir_stmts, convert);
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

ir_type_t *trans_decl_node(trans_state_t *ts, decl_node_t *node,
                           ir_decl_node_type_t type,
                           void *context) {
    type_t *node_type = ast_type_untypedef(node->type);
    ir_expr_t *var_expr = ir_expr_create(ts->tunit, IR_EXPR_VAR);
    ir_type_t *expr_type = trans_type(ts, node->type);

    ir_symtab_t *symtab;
    ir_expr_t *access;
    bool name_owned = false;

    switch (type) {
    case IR_DECL_NODE_FDEFN:
        var_expr->var.type = expr_type;
        var_expr->var.name = node->id;
        var_expr->var.local = false;

        symtab = &ts->tunit->globals;
        access = var_expr;
        break;
    case IR_DECL_NODE_GLOBAL: {
        ir_gdecl_t *gdecl = context;
        assert(gdecl->type == IR_GDECL_GDATA);

        // Set up correct linkage and modifiers
        if (node_type->type == TYPE_MOD) {
            if (node_type->mod.type_mod & TMOD_STATIC) {
                gdecl->linkage = IR_LINKAGE_INTERNAL;
            } else if (node_type->mod.type_mod & TMOD_EXTERN) {
                gdecl->linkage = IR_LINKAGE_INTERNAL;
            }

            if (node_type->mod.type_mod & TMOD_CONST) {
                gdecl->gdata.flags |= IR_GDATA_CONSTANT;
            }
        }

        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = expr_type;

        var_expr->var.type = ptr_type;
        var_expr->var.name = node->id;
        var_expr->var.local = false;

        gdecl->gdata.type = expr_type;
        gdecl->gdata.var = var_expr;
        gdecl->gdata.init = node->expr == NULL ?
            NULL : trans_expr(ts, false, node->expr, &gdecl->gdata.setup);
        gdecl->gdata.align = ast_type_align(node->type);

        symtab = &ts->tunit->globals;
        access = var_expr;
        break;
    }
    case IR_DECL_NODE_LOCAL: {
        ir_inst_stream_t *ir_stmts = context;
        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = expr_type;

        symtab = &ts->func->func.locals;

        var_expr->var.type = ptr_type;
        var_expr->var.name = trans_decl_node_name(symtab, node->id,
                                                  &name_owned);
        var_expr->var.local = true;

        // Have to allocate variable on the stack
        ir_expr_t *src = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
        src->alloca.type = ptr_type;
        src->alloca.elem_type = var_expr->var.type->ptr.base;
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
            store->store.type = expr_type;
            ir_expr_t *val = trans_expr(ts, false, node->expr, ir_stmts);

            store->store.val = trans_type_conversion(ts, node->type,
                                                     node->expr->etype, val,
                                                     ir_stmts);
            store->store.ptr = var_expr;
            trans_add_stmt(ts, ir_stmts, store);
        }

        access = var_expr;
        break;
    }
    case IR_DECL_NODE_FUNC_PARAM: {
        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = expr_type;

        symtab = &ts->func->func.locals;

        var_expr->var.type = expr_type;
        var_expr->var.name = trans_decl_node_name(symtab, node->id,
                                                  &name_owned);
        var_expr->var.local = true;

        ir_expr_t *alloca = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
        alloca->alloca.type = ptr_type;
        alloca->alloca.elem_type = var_expr->var.type;
        alloca->alloca.nelem_type = NULL;
        alloca->alloca.align = ast_type_align(node->type);

        // Stack variable to refer to paramater by
        ir_expr_t *temp = trans_assign_temp(ts, &ts->func->func.prefix, alloca);

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
    }
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

    return expr_type;
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

    case TYPE_STRUCT: {
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
            char *name = emalloc(strlen(type->struct_params.name) +
                                 sizeof(STRUCT_PREFIX));
            sprintf(name, STRUCT_PREFIX"%s", type->struct_params.name);
            ir_type_t *id_type = ir_type_create(ts->tunit, IR_TYPE_ID_STRUCT);
            id_type->id_struct.name = name;
            id_type->id_struct.type = ir_type;

            id_gdecl = ir_gdecl_create(IR_GDECL_ID_STRUCT);
            id_gdecl->id_struct.name = name;
            id_gdecl->id_struct.id_type = id_type;
            sl_append(&ts->tunit->id_structs, &id_gdecl->link);
            type->struct_params.trans_state = id_gdecl;
        }

        // Create a new structure object
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
        if (id_gdecl != NULL) {
            id_gdecl->id_struct.type = ir_type;
            return id_gdecl->id_struct.id_type;
        }

        return ir_type;
    }

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
        ir_type->arr.nelems = type->arr.nelems;
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

ir_expr_t *trans_string(trans_state_t *ts, char *str) {
    ht_ptr_elem_t *elem = ht_lookup(&ts->tunit->strings, &str);
    if (elem != NULL) {
        return elem->val;
    }

    char namebuf[MAX_GLOBAL_NAME];

    size_t len = snprintf(namebuf, MAX_GLOBAL_NAME, "%s%d", GLOBAL_PREFIX,
                          ts->tunit->static_num++);

    elem = emalloc(sizeof(*elem) + len + 1);
    elem->key = str;
    char *name = (char *)elem + sizeof(*elem);
    strcpy(name, namebuf);

    ir_type_t *type = ir_type_create(ts->tunit, IR_TYPE_ARR);
    type->arr.nelems = strlen(str) + 1;
    type->arr.elem_type = &ir_type_i8;
    ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    ptr_type->ptr.base = type;

    ir_expr_t *var = ir_expr_create(ts->tunit, IR_EXPR_VAR);
    var->var.name = name;
    var->var.local = false;
    var->var.type = ptr_type;

    ir_expr_t *arr_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    arr_lit->const_params.ctype = IR_CONST_STR;
    arr_lit->const_params.type = type;
    arr_lit->const_params.str_val = str;

    ir_gdecl_t *global = ir_gdecl_create(IR_GDECL_GDATA);
    global->linkage = IR_LINKAGE_PRIVATE;
    global->gdata.flags = IR_GDATA_CONSTANT | IR_GDATA_UNNAMED_ADDR;
    global->gdata.type = type;
    global->gdata.var = var;
    global->gdata.init = arr_lit;
    global->gdata.align = 1;
    sl_append(&ts->tunit->decls, &global->link);

    ir_expr_t *elem_ptr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
    ir_type_t *elem_ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    elem_ptr_type->ptr.base = type->arr.elem_type;
    elem_ptr->getelemptr.type = elem_ptr_type;
    elem_ptr->getelemptr.ptr_type = ptr_type;
    elem_ptr->getelemptr.ptr_val = var;

    ir_type_expr_pair_t *pair = emalloc(sizeof(*pair));
    pair->type = &ir_type_i32;
    pair->expr = ir_expr_zero(ts->tunit, &ir_type_i32);
    sl_append(&elem_ptr->getelemptr.idxs, &pair->link);

    pair = emalloc(sizeof(*pair));
    pair->type = &ir_type_i32;
    pair->expr = ir_expr_zero(ts->tunit, &ir_type_i32);
    sl_append(&elem_ptr->getelemptr.idxs, &pair->link);
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

    ir_expr_t *arr_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    sl_init(&arr_lit->const_params.arr_val, offsetof(ir_expr_t, link));
    arr_lit->const_params.ctype = IR_CONST_ARR;
    arr_lit->const_params.type = type;

    size_t nelems = 0;
    SL_FOREACH(cur, &expr->init_list.exprs) {
        expr_t *elem = GET_ELEM(&expr->init_list.exprs, cur);
        ir_expr_t *ir_elem = trans_expr(ts, false, elem, NULL);
        sl_append(&arr_lit->const_params.arr_val, &ir_elem->link);
        ++nelems;
    }

    while (nelems++ < type->arr.nelems) {
        ir_expr_t *zero = ir_expr_zero(ts->tunit, elem_type);
        sl_append(&arr_lit->const_params.arr_val, &zero->link);
    }

    return arr_lit;
}
