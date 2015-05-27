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
#include "util/string_store.h"

#include "typecheck/typechecker.h"

#define MAX_NUM_LEN 21

#define MAX_GLOBAL_NAME 128
#define GLOBAL_PREFIX ".glo"
#define STRUCT_PREFIX "struct."
#define UNION_PREFIX "union."

#define LLVM_MEMCPY "llvm.memcpy.p0i8.p0i8.i64"

void trans_add_stmt(trans_state_t *ts, ir_inst_stream_t *stream,
                    ir_stmt_t *stmt) {
    if (stmt->type == IR_STMT_LABEL) {
        ts->func->func.last_label = stmt->label;

        // If we added a labeled statement, indicate that if the next statement
        // is labeled, we need to add a jump to that statement
        ts->branch_next_labeled = true;
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
    if (expr->type == IR_EXPR_VAR || stream == NULL) {
        return expr;
    }
    ir_expr_t *temp = trans_temp_create(ts, ir_expr_type(expr));

    ir_stmt_t *assign = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
    assign->assign.dest = temp;
    assign->assign.src = expr;
    trans_add_stmt(ts, stream, assign);

    return temp;
}

ir_expr_t *trans_load_temp(trans_state_t *ts, ir_inst_stream_t *stream,
                           ir_expr_t *expr) {
    ir_type_t *type = ir_expr_type(expr);
    assert(type->type == IR_TYPE_PTR);

    // Don't load from aggregate types
    switch (type->ptr.base->type) {
    case IR_TYPE_STRUCT:
    case IR_TYPE_ID_STRUCT:
    case IR_TYPE_ARR:
        return expr;
    default:
        break;
    }
    ir_expr_t *load = ir_expr_create(ts->tunit, IR_EXPR_LOAD);
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
    ts->ast_tunit = ast;
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
            decl_node_t *arg = sl_head(&decl->decls);
            assert(arg != NULL);

            trans_decl_node(ts, arg, IR_DECL_NODE_FUNC_PARAM, NULL);
        }

        ts->ignore_until_label = false;
        trans_stmt(ts, gdecl->fdefn.stmt, &ir_gdecl->func.body);

        ir_stmt_t *last = ir_inst_stream_tail(&ir_gdecl->func.body);

        // If the function didn't return, add a return
        if (last == NULL || last->type != IR_STMT_RET) {
            ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_RET);
            ir_stmt->ret.type = ir_gdecl->func.type->func.type;
            ir_stmt->ret.val = ir_expr_zero(ts->tunit, ir_stmt->ret.type);
            trans_add_stmt(ts, &ir_gdecl->func.body, ir_stmt);
        }

        sl_append(ir_gdecls, &ir_gdecl->link);

        // Restore state
        ts->func = NULL;
        ts->typetab = typetab_save;
        break;
    }
    case GDECL_DECL: {
        type_t *type = ast_type_untypedef(gdecl->decl->type);

        bool is_extern = false;
        if (type->type == TYPE_MOD) {
            // Ignore typedefs
            if (type->mod.type_mod & TMOD_TYPEDEF) {
                return;
            }
            if (type->mod.type_mod & TMOD_EXTERN) {
                is_extern = true;
            }
        }

        // Keep track of global decls, so they are only translated if used
        SL_FOREACH(cur, &gdecl->decl->decls) {
            decl_node_t *node = GET_ELEM(&gdecl->decl->decls, cur);
            type_t *node_type = ast_type_untypedef(node->type);

            // Translate nodes that are not function decls and are not external
            // Lazily translate the others if they are used
            if (node_type->type != TYPE_FUNC && !is_extern) {
                trans_gdecl_node(ts, node);
                continue;
            }

            ht_ptr_elem_t *elem = emalloc(sizeof(*elem));
            elem->key = node->id;
            elem->val = node;
            status_t status = ht_insert(&ts->tunit->global_decls, &elem->link);
            if (status != CCC_OK) {
                assert(status == CCC_DUPLICATE);
                free(elem);
            }
        }
        break;
    }
    default:
        assert(false);
    }
}

bool trans_stmt(trans_state_t *ts, stmt_t *stmt, ir_inst_stream_t *ir_stmts) {
    ir_stmt_t *branch = NULL;
    if (ts->branch_next_labeled) {
        ts->branch_next_labeled = false;
        if (STMT_LABELED(stmt) != NULL) {
            branch = ir_stmt_create(ts->tunit, IR_STMT_BR);
            branch->br.cond = NULL;
            branch->br.uncond = NULL;
            trans_add_stmt(ts, ir_stmts, branch);
        }
    }

    bool jumps = false;
    switch (stmt->type) {
    case STMT_NOP:
        break;

    case STMT_DECL: {
        // Ignore typedefs
        if (TYPE_HAS_MOD(stmt->decl->type, TMOD_TYPEDEF)) {
            break;
        }
        SL_FOREACH(cur, &stmt->decl->decls) {
            decl_node_t *node = GET_ELEM(&stmt->decl->decls, cur);
            trans_decl_node(ts, node, IR_DECL_NODE_LOCAL, ir_stmts);
        }
        break;
    }

    case STMT_LABEL: {
        ts->ignore_until_label = false;
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = trans_label_create(ts, stmt->label.label);
        if (branch != NULL) {
            branch->br.uncond = ir_stmt->label;
        }

        trans_add_stmt(ts, ir_stmts, ir_stmt);
        jumps = trans_stmt(ts, stmt->label.stmt, ir_stmts);

        break;
    }
    case STMT_CASE: {
        ts->ignore_until_label = false;
        ts->cur_case_jumps = false;
        ts->break_count = 0;

        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = stmt->case_params.label;
        if (branch != NULL) {
            branch->br.uncond = ir_stmt->label;
        }

        trans_add_stmt(ts, ir_stmts, ir_stmt);
        jumps = trans_stmt(ts, stmt->case_params.stmt, ir_stmts);
        break;
    }
    case STMT_DEFAULT: {
        ts->ignore_until_label = false;
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
        ir_stmt->label = stmt->default_params.label;
        if (branch != NULL) {
            branch->br.uncond = ir_stmt->label;
        }

        trans_add_stmt(ts, ir_stmts, ir_stmt);
        jumps = trans_stmt(ts, stmt->default_params.stmt, ir_stmts);
        break;
    }

    case STMT_IF: {
        if (ts->ignore_until_label) {
            if (trans_stmt(ts, stmt->if_params.true_stmt, ir_stmts)) {
                return true;
            } else if (stmt->if_params.false_stmt != NULL) {
                return trans_stmt(ts, stmt->if_params.false_stmt, ir_stmts);
            }
            return false;
        }

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
            // Unconditonal branch only if last instruction was not a jump
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

            // Unconditonal branch only if last instruction was not a jump
            if (!false_ret) {
                ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
                ir_stmt->br.cond = NULL;
                ir_stmt->br.uncond = after;
                trans_add_stmt(ts, ir_stmts, ir_stmt);
            }
        }

        if (true_ret && false_ret) {
            jumps = true;
        } else {
            // Only add end label if both branches didn't jump
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
            ir_stmt->label = after;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        }
        break;
    }
    case STMT_SWITCH: {
        if (ts->ignore_until_label) {
            return trans_stmt(ts, stmt->switch_params.stmt, ir_stmts);
        }
        // Just translate the default case if there are no labels
        if (sl_head(&stmt->switch_params.cases) == NULL) {
            if (stmt->switch_params.default_stmt != NULL) {
                return trans_stmt(ts, stmt->switch_params.default_stmt,
                                  ir_stmts);
            }
            return false;
        }
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_SWITCH);
        ir_expr_t *switch_expr =
            trans_expr(ts, false, stmt->switch_params.expr, ir_stmts);
        ir_stmt->switch_params.expr = switch_expr;

        ir_type_t *switch_type = ir_expr_type(switch_expr);
        SL_FOREACH(cur, &stmt->switch_params.cases) {
            stmt_t *cur_case = GET_ELEM(&stmt->switch_params.cases, cur);
            ir_label_t *label = trans_numlabel_create(ts);

            assert(cur_case->type == STMT_CASE);
            cur_case->case_params.label = label;

            long long case_val;
            typecheck_const_expr_eval(ts->typetab,
                                      cur_case->case_params.val, &case_val);

            ir_expr_label_pair_t *pair = emalloc(sizeof(ir_expr_label_pair_t));
            pair->expr = ir_int_const(ts->tunit, switch_type, case_val);
            pair->label = label;

            sl_append(&ir_stmt->switch_params.cases, &pair->link);
        }

        // Generate default label
        ir_label_t *label = trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);

        ir_label_t *break_save = ts->break_target;
        int break_count_save = ts->break_count;
        ts->break_target = after;
        ts->break_count = 0;

        bool has_default;
        if (stmt->switch_params.default_stmt != NULL) {
            has_default = true;
            stmt->switch_params.default_stmt->default_params.label = label;
            ir_stmt->switch_params.default_case = label;
        } else {
            has_default = false;
            ir_stmt->switch_params.default_case = after;
        }
        trans_add_stmt(ts, ir_stmts, ir_stmt);

        ts->in_switch = true;
        jumps = trans_stmt(ts, stmt->switch_params.stmt, ir_stmts);
        ts->in_switch = false;

        if (!jumps || !has_default) {
            // Preceeding label
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
            ir_stmt->label = after;
            trans_add_stmt(ts, ir_stmts, ir_stmt);
        }

        // Restore break target
        ts->break_target = break_save;
        ts->break_count = break_count_save;
        break;
    }

    case STMT_DO: {
        if (ts->ignore_until_label) {
            return trans_stmt(ts, stmt->do_params.stmt, ir_stmts);
        }
        ir_label_t *body = trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);

        ir_label_t *break_save = ts->break_target;
        int break_count_save = ts->break_count;
        ir_label_t *continue_save = ts->continue_target;
        ts->break_target = after;
        ts->break_count = 0;
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

        jumps = trans_stmt(ts, stmt->do_params.stmt, ir_stmts);

        // Only translate test and end label if the body doesn't jump
        if (!jumps) {
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
        ts->break_count = break_count_save;
        ts->continue_target = continue_save;
        break;
    }
    case STMT_WHILE: {
        if (ts->ignore_until_label) {
            return trans_stmt(ts, stmt->while_params.stmt, ir_stmts);
        }
        ir_label_t *cond = trans_numlabel_create(ts);
        ir_label_t *body = trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);
        ir_label_t *break_save = ts->break_target;
        int break_count_save = ts->break_count;
        ir_label_t *continue_save = ts->continue_target;
        ts->break_target = after;
        ts->break_count = 0;
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

        bool stmt_jumps = trans_stmt(ts, stmt->while_params.stmt, ir_stmts);

        // Only add unconditional branch if loop doesn't jump
        if (!stmt_jumps) {
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
        ts->break_count = break_count_save;
        ts->continue_target = continue_save;
        break;
    }
    case STMT_FOR: {
        if (ts->ignore_until_label) {
            return trans_stmt(ts, stmt->for_params.stmt, ir_stmts);
        }
        ir_label_t *cond = trans_numlabel_create(ts);
        ir_label_t *body = trans_numlabel_create(ts);
        ir_label_t *update = trans_numlabel_create(ts);
        ir_label_t *after = trans_numlabel_create(ts);
        ir_label_t *break_save = ts->break_target;
        int break_count_save = ts->break_count;
        ir_label_t *continue_save = ts->continue_target;
        ts->break_target = after;
        ts->break_count = 0;
        ts->continue_target = update;

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

        bool stmt_jumps = trans_stmt(ts, stmt->for_params.stmt, ir_stmts);

        // Only add 3rd expression and unconditional branch if last statement
        // wasn't a jump
        if (!stmt_jumps) {
            // Unconditional branch to update
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
            ir_stmt->br.cond = NULL;
            ir_stmt->br.uncond = update;
            trans_add_stmt(ts, ir_stmts, ir_stmt);

            // Update target
            ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_LABEL);
            ir_stmt->label = update;
            trans_add_stmt(ts, ir_stmts, ir_stmt);

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
        ts->break_count = break_count_save;
        ts->continue_target = continue_save;
        if (typetab_save != NULL) {
            ts->typetab = typetab_save;
        }
        break;
    }

    case STMT_GOTO: {
        if (ts->ignore_until_label) {
            return false;
        }
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        ir_stmt->br.uncond = trans_label_create(ts, stmt->goto_params.label);
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        jumps = true;
        break;
    }
    case STMT_CONTINUE: {
        if (ts->ignore_until_label) {
            return false;
        }
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        assert(ts->continue_target != NULL);
        ir_stmt->br.uncond = ts->continue_target;
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        jumps = true;
        break;
    }
    case STMT_BREAK: {
        if (ts->ignore_until_label) {
            return false;
        }
        ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
        ir_stmt->br.cond = NULL;
        assert(ts->break_target != NULL);
        ir_stmt->br.uncond = ts->break_target;
        trans_add_stmt(ts, ir_stmts, ir_stmt);
        ++ts->break_count;
        jumps = true;
        break;
    }
    case STMT_RETURN: {
        if (ts->ignore_until_label) {
            return false;
        }
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

        jumps = true;
        break;
    }

    case STMT_COMPOUND: {
        typetab_t *typetab_save = ts->typetab;
        ts->typetab = &stmt->compound.typetab;

        bool has_jump = false;

        bool set_ignore = false;
        bool is_switch = false;
        if (ts->in_switch) {
            is_switch = true;
            ts->in_switch = false;
            ts->ignore_until_label = true;
            ts->cur_case_jumps = false;
            set_ignore = true;
        }
        bool switch_has_jump = true;

        SL_FOREACH(cur, &stmt->compound.stmts) {
            stmt_t *cur_stmt = GET_ELEM(&stmt->compound.stmts, cur);

            if (STMT_LABELED(cur_stmt) != NULL && !ts->ignore_until_label) {
                ts->branch_next_labeled = true;
            }

            // If we encounter a statement which always jumps, ignore all
            // statements until a labeled statement
            if (trans_stmt(ts, cur_stmt, ir_stmts)) {
                ts->ignore_until_label = true;
                set_ignore = true;
                has_jump = true;

                // If we encounter a statement which always jumps, and there
                // were no breaks, then the current case jumps
                if (ts->break_count == 0) {
                    ts->cur_case_jumps = true;
                }
            }

            // If we encounter a break anywhere, switch does not have an
            // unconditional jump
            if (ts->break_count != 0) {
                switch_has_jump = false;
            }
        }

        ts->typetab = typetab_save;
        if (is_switch) {
            // Switch always jumps if all previous cases jumped, and the current
            // case jumps
            jumps = switch_has_jump && ts->cur_case_jumps;

            // If last case didn't jump, add unconditional jump to break target
            if (!ts->cur_case_jumps) {
                ir_stmt_t *ir_stmt = ir_stmt_create(ts->tunit, IR_STMT_BR);
                ir_stmt->br.cond = NULL;
                ir_stmt->br.uncond = ts->break_target;
                trans_add_stmt(ts, ir_stmts, ir_stmt);
            }
        } else {
            jumps = has_jump;
        }

        if (set_ignore) {
            ts->ignore_until_label = false;
        }
        break;
    }

    case STMT_EXPR:
        if (ts->ignore_until_label) {
            return false;
        }
        trans_expr(ts, false, stmt->expr.expr, ir_stmts);
        break;
    default:
        assert(false);
    }

    return jumps;
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
            if ((tt_ent->entry_type == TT_VAR ||
                 tt_ent->entry_type == TT_ENUM_ID)) {
                if (tt_ent->type == expr->etype) {
                    break;
                }

                // Function expressions evaluate to function pointers
                if (expr->type == EXPR_VAR && tt_ent->type->type == TYPE_FUNC) {
                    assert(expr->etype->type == TYPE_PTR);
                    if (expr->etype->ptr.base == tt_ent->type) {
                        break;
                    }
                }
            }

            tt = tt->last;
        } while(true);

        // If this is an enum id, just return the integer value
        if (tt_ent->entry_type == TT_ENUM_ID) {
            return ir_int_const(ts->tunit, trans_type(ts, tt_ent->type),
                                tt_ent->enum_val);
        }
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

        ir_type_t *entry_type = ir_expr_type(entry->var.access);

        if (entry_type->type == IR_TYPE_PTR) {
            if (addrof || entry_type->ptr.base->type == IR_TYPE_FUNC) {
                // If we're taking address of variable, just return it
                // Also can't dereference a function
                return entry->var.access;
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
        type_t *max_type;
        bool result = typecheck_type_max(ts->ast_tunit, NULL,
                                         expr->assign.expr->etype,
                                         expr->etype, &max_type);
        assert(result && max_type != NULL);
        ir_expr_t *dest;
        ir_expr_t *op_expr = trans_binop(ts, expr->assign.dest, dest_addr,
                                         expr->assign.expr, expr->assign.op,
                                         max_type, ir_stmts, &dest);

        ir_expr_t *temp = trans_assign_temp(ts, ir_stmts, op_expr);

        return trans_assign(ts, dest_addr, expr->assign.dest->etype, temp,
                            max_type, ir_stmts);
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
        if (expr->cast.base->type == EXPR_INIT_LIST) {
            return trans_compound_literal(ts, addrof, ir_stmts,
                                          expr->cast.base);
        } else {
            ir_expr_t *src_expr = trans_expr(ts, false, expr->cast.base,
                                             ir_stmts);
            return trans_type_conversion(ts, expr->etype,
                                         expr->cast.base->etype, src_expr,
                                         ir_stmts);
        }
    }
    case EXPR_CALL: {
        ir_expr_t *call = ir_expr_create(ts->tunit, IR_EXPR_CALL);
        type_t *func_sig = expr->call.func->etype;
        if (func_sig->type == TYPE_PTR) {
            func_sig = func_sig->ptr.base;
        }
        call->call.func_sig = trans_type(ts, func_sig);
        call->call.func_ptr = trans_expr(ts, false, expr->call.func, ir_stmts);
        assert(func_sig->type == TYPE_FUNC);

        sl_link_t *cur_sig = func_sig->func.params.head;
        sl_link_t *cur_expr = expr->call.params.head;
        while (cur_sig != NULL) {
            decl_t *decl = GET_ELEM(&func_sig->func.params, cur_sig);
            decl_node_t *node = sl_head(&decl->decls);
            type_t *sig_type = node == NULL ? decl->type : node->type;
            assert(cur_expr != NULL);

            expr_t *param = GET_ELEM(&expr->call.params, cur_expr);
            ir_expr_t *ir_expr = trans_expr(ts, false, param, ir_stmts);
            ir_expr = trans_type_conversion(ts, sig_type, param->etype, ir_expr,
                                            ir_stmts);

            // If the function parameter is aggregate, we must load it
            // we can't use trans_load_temp because it purposely doesn't load
            // aggregate types
            if (TYPE_IS_AGGREGATE(ast_type_unmod(sig_type))) {
                ir_type_t *type = ir_expr_type(ir_expr);
                assert(type->type == IR_TYPE_PTR);

                ir_expr_t *load = ir_expr_create(ts->tunit, IR_EXPR_LOAD);
                load->load.type = type->ptr.base;
                load->load.ptr = ir_expr;

                ir_expr = trans_assign_temp(ts, ir_stmts, load);
            }
            sl_append(&call->call.arglist, &ir_expr->link);

            cur_sig = cur_sig->next;
            cur_expr = cur_expr->next;
        }
        if (func_sig->func.varargs) {
            while (cur_expr != NULL) {
                expr_t *param = GET_ELEM(&expr->call.params, cur_expr);
                ir_expr_t *ir_expr = trans_expr(ts, false, param, ir_stmts);
                sl_append(&call->call.arglist, &ir_expr->link);

                cur_expr = cur_expr->next;
            }
        } else {
            assert(cur_expr == NULL);
        }

        type_t *return_type = ast_type_unmod(func_sig->func.type);
        ir_expr_t *result;
        // Void returning function, don't create a temp
        if (return_type->type == TYPE_VOID) {
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
            expr_t *cur_expr = GET_ELEM(&expr->cmpd.exprs, cur);
            ir_expr = trans_expr(ts, false, cur_expr, ir_stmts);
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
            expr_t *sz_expr = expr->sizeof_params.expr;
            assert(sz_expr != NULL);

            if (sz_expr->type == EXPR_VAR) {
                // Function's etypes are function pointers, but the size
                // is not a pointer size
                typetab_entry_t *entry = tt_lookup(ts->typetab,
                                                   sz_expr->var_id);
                assert(entry != NULL);
                val = ast_type_size(entry->type);
            } else {
                val = ast_type_size(sz_expr->etype);
            }
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
        ir_type_t *expr_type = trans_type(ts, expr->etype);
        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = expr_type;

        type_t *base_type;
        if (expr->type == EXPR_MEM_ACC) {
            base_type = ast_type_unmod(expr->mem_acc.base->etype);
        }

        ir_expr_t *pointer;
        // Unions are handled differently
        if (expr->type == EXPR_MEM_ACC &&
            ((expr->mem_acc.op == OP_DOT && base_type->type == TYPE_UNION) ||
             (expr->mem_acc.op == OP_ARROW &&
              ast_type_unmod(base_type->ptr.base)->type == TYPE_UNION))) {

            pointer = trans_expr(ts, false, expr->mem_acc.base, ir_stmts);
            pointer = trans_ir_type_conversion(ts, ptr_type, false,
                                               ir_expr_type(pointer), false,
                                               pointer, ir_stmts);
            if (addrof) {
                return pointer;
            }
            return trans_load_temp(ts, ir_stmts, pointer);
        }

        ir_expr_t *elem_ptr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
        elem_ptr->getelemptr.type = ptr_type;

        bool is_union = false;
        bool last_array = false;
        while ((expr->type == EXPR_MEM_ACC && expr->mem_acc.op == OP_DOT)
            || expr->type == EXPR_ARR_IDX) {
            if (expr->type == EXPR_MEM_ACC) {
                if (expr->mem_acc.base->etype->type == TYPE_UNION) {
                    is_union = true;
                    break;
                }
                trans_struct_mem_offset(ts, expr->mem_acc.base->etype,
                                        expr->mem_acc.name,
                                        &elem_ptr->getelemptr.idxs);
                expr = expr->mem_acc.base;
            } else { // expr->type == EXPR_ARR_IDX
                type_t *arr_type = ast_type_unmod(expr->arr_idx.array->etype);

                ir_expr_t *index = trans_expr(ts, false, expr->arr_idx.index,
                                              ir_stmts);
                index = trans_type_conversion(ts, tt_size_t,
                                              expr->arr_idx.index->etype,
                                              index, ir_stmts);
                sl_prepend(&elem_ptr->getelemptr.idxs, &index->link);
                expr = expr->arr_idx.array;

                // If this is a pointer instead of an array, stop here because
                // we need to do a load for the next index
                if (arr_type->type == TYPE_PTR ||
                    (arr_type->type == TYPE_ARR && arr_type->arr.nelems == 0)) {
                    last_array = true;
                    break;
                }
            }
        }

        type_t *etype;
        if (expr->type == EXPR_MEM_ACC && expr->mem_acc.op == OP_ARROW) {
            etype = ast_type_unmod(expr->mem_acc.base->etype);
            assert(etype->type == TYPE_PTR);
            if (ast_type_unmod(etype->ptr.base)->type == TYPE_UNION) {
                is_union = true;
            }
        }

        bool prepend_zero = false;
        if (!last_array && !is_union && expr->type == EXPR_MEM_ACC) {
            assert(expr->mem_acc.op == OP_ARROW);
            trans_struct_mem_offset(ts, etype->ptr.base, expr->mem_acc.name,
                                    &elem_ptr->getelemptr.idxs);

            pointer = trans_expr(ts, false, expr->mem_acc.base, ir_stmts);
            prepend_zero = true;
        } else { // !is_arr_idx && expr->type != EXPR_MEM_ACC
            if (is_union) {
                pointer = trans_expr(ts, true, expr, ir_stmts);
            } else {
                pointer = trans_expr(ts, false, expr, ir_stmts);
            }
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
            ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i32);
            sl_prepend(&elem_ptr->getelemptr.idxs, &zero->link);
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
        case TYPE_UNION:
            return trans_union_init(ts, expr->etype, expr);
        case TYPE_STRUCT:
            return trans_struct_init(ts, expr);
        case TYPE_ARR:
            return trans_array_init(ts, expr);
        default: {
            // Just take the first element and tranlate it
            expr_t *head = sl_head(&expr->init_list.exprs);
            assert(head != NULL);
            return trans_expr(ts, false, head, ir_stmts);
        }
        }
        break;
    }
    case EXPR_DESIG_INIT:
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

    // Do nothing if expression is already the right type
    if (type->type == IR_TYPE_INT && type->int_params.width == 1) {
        return expr;
    }

    // If this is a constant, just change it into the boolean type
    if (expr->type == IR_EXPR_CONST) {
        bool is_true;
        switch (expr->const_params.ctype) {
        case IR_CONST_NULL:  is_true = false; break;
        case IR_CONST_STR:   is_true = true;  break;
        case IR_CONST_ZERO:  is_true = false;  break;
        case IR_CONST_INT:   is_true = expr->const_params.int_val != 0; break;
        case IR_CONST_FLOAT:
            is_true = expr->const_params.float_val != 0.0;
            break;

        case IR_CONST_STRUCT:
        case IR_CONST_ARR:
        case IR_CONST_UNDEF:
        default:
            assert(false);
            is_true = false;
        }

        expr->const_params.ctype = IR_CONST_INT;
        expr->const_params.type = &ir_type_i1;
        expr->const_params.int_val = is_true;

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
    bool is_ptr = false;
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
        if (!TYPE_IS_UNSIGNED(type)) {
            is_signed = true;
        }
        break;

    case TYPE_PTR:
        is_ptr = true;
        break;

    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PAREN:
    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_ENUM:
    case TYPE_TYPEDEF:
    default:
        assert(false);
    }

    if (is_ptr) {
        // Can only get a result pointer as addition of pointer with int
        assert(op == OP_PLUS);
        expr_t *ptr_expr;
        expr_t *int_expr;
        if (TYPE_IS_PTR(ast_type_unmod(left->etype))) {
            assert(TYPE_IS_INTEGRAL(ast_type_unmod(right->etype)));
            ptr_expr = left;
            int_expr = right;
        } else {
            assert(TYPE_IS_PTR(ast_type_unmod(right->etype)));
            assert(TYPE_IS_INTEGRAL(ast_type_unmod(left->etype)));
            ptr_expr = right;
            int_expr = left;
        }

        // Just treat p + x as &p[x]
        expr_t *arr_idx = ast_expr_create(ts->ast_tunit, ptr_expr->mark,
                                          EXPR_ARR_IDX);
        arr_idx->arr_idx.array = ptr_expr;
        arr_idx->arr_idx.index = int_expr;

        // Set etype to ptr's base so that getelemptr's type is correctly set
        arr_idx->etype = type->ptr.base;

        return trans_expr(ts, true, arr_idx, ir_stmts);
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

        ir_expr = trans_assign_temp(ts, ir_stmts, ir_expr);

        // Result of comparison is an int (C11 S 6.8.5)
        ir_expr = trans_ir_type_conversion(ts, trans_type(ts, tt_int), false,
                                           &ir_type_i1, false, ir_expr,
                                           ir_stmts);

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
        type_t *max_type;
        bool success = typecheck_type_max(ts->ast_tunit, NULL, left->etype,
                                          right->etype, &max_type);
        // Must be valid if typechecked
        assert(success && max_type != NULL);
        is_float = TYPE_IS_FLOAT(max_type);
        is_signed = !TYPE_IS_UNSIGNED(max_type);

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

        ir_expr_t *result = trans_assign_temp(ts, ir_stmts, cmp);

        // Result of comparison is an int (C11 S 6.8.5)
        result = trans_ir_type_conversion(ts, trans_type(ts, tt_int), false,
                                          &ir_type_i1, false, result, ir_stmts);
        return result;
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

        // TODO1: This doesn't work for pointers
        ir_expr_t *ir_expr = trans_load_temp(ts, ir_stmts, expr_addr);
        ir_type_t *type = ir_expr_type(ir_expr);
        ir_expr_t *op_expr = ir_expr_create(ts->tunit, IR_EXPR_BINOP);

        switch (op) {
        case OP_PREINC:
        case OP_POSTINC: op_expr->binop.op = IR_OP_ADD; break;
        case OP_PREDEC:
        case OP_POSTDEC: op_expr->binop.op = IR_OP_SUB; break;
        default: assert(false);
        }
        ir_expr_t *other = ir_int_const(ts->tunit, type, 1);
        op_expr->binop.expr1 = ir_expr;
        op_expr->binop.expr2 = other;
        op_expr->binop.type = type;

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
    switch (op) {
    case OP_UMINUS:
    case OP_UPLUS:
    case OP_BITNOT:
        ir_expr = trans_type_conversion(ts, expr->etype,
                                        expr->unary.expr->etype, ir_expr,
                                        ir_stmts);
        break;
    default:
        break;
    }
    ir_type_t *type = ir_expr_type(ir_expr);
    switch (op) {
    case OP_UPLUS:
        return ir_expr;

    case OP_DEREF: {
        assert(type->type == IR_TYPE_PTR);

        // Derefernce of a function pointer is unchanged
        if (type->ptr.base->type == IR_TYPE_FUNC) {
            return ir_expr;
        }
        type_t *ptr_type = ast_type_unmod(expr->unary.expr->etype);

        ir_type_t *base = type->ptr.base;

        if (ptr_type->type == TYPE_ARR && base->type == IR_TYPE_ARR) {
            // Arrays are refered to by pointers to the array

            // Get the actual base and cast the array type to the appropriate
            // pointer type
            base = base->arr.elem_type;

            ir_type_t *base_ptr = ir_type_create(ts->tunit, IR_TYPE_PTR);
            base_ptr->ptr.base = base;

            ir_expr = trans_ir_type_conversion(ts, base_ptr, false,
                                               ir_expr_type(ir_expr), false,
                                               ir_expr, ir_stmts);
        }

        if (addrof) {
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

char *trans_decl_node_name(ir_symtab_t *symtab, char *name) {
    ir_symtab_entry_t *entry = ir_symtab_lookup(symtab, name);
    if (entry == NULL) {
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

    return sstore_insert(patch_name);
}

ir_type_t *trans_decl_node(trans_state_t *ts, decl_node_t *node,
                           ir_decl_node_type_t type,
                           void *context) {
    type_t *node_type = ast_type_untypedef(node->type);
    ir_expr_t *var_expr = ir_expr_create(ts->tunit, IR_EXPR_VAR);
    ir_type_t *expr_type = trans_type(ts, node_type);
    ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    ptr_type->ptr.base = expr_type;


    ir_symtab_t *symtab;
    ir_expr_t *access;

    switch (type) {
    case IR_DECL_NODE_FDEFN: {
        var_expr->var.type = ptr_type;
        var_expr->var.name = node->id;
        var_expr->var.local = false;

        symtab = &ts->tunit->globals;
        access = var_expr;
        break;
    }
    case IR_DECL_NODE_GLOBAL: {
        ir_gdecl_t *gdecl = context;
        assert(gdecl->type == IR_GDECL_GDATA);

        // Set up correct linkage and modifiers
        if ((node_type->type == TYPE_MOD &&
             node_type->mod.type_mod & TMOD_CONST) ||
            (node_type->type == TYPE_PTR &&
             node_type->ptr.type_mod & TMOD_CONST)) {
            gdecl->gdata.flags |= IR_GDATA_CONSTANT;
        }

        // storage class specifers (auto/register/static/extern) are attached
        // to the base type, need to remove pointers
        type_t *mod_check = node_type;
        while (mod_check->type == TYPE_PTR) {
            mod_check = ast_type_untypedef(mod_check->ptr.base);
        }
        bool external = false;
        if (mod_check->type == TYPE_MOD) {
            if (mod_check->mod.type_mod & TMOD_STATIC) {
                gdecl->linkage = IR_LINKAGE_INTERNAL;
            } else if (mod_check->mod.type_mod & TMOD_EXTERN) {
                gdecl->linkage = IR_LINKAGE_EXTERNAL;
                external = true;
            }

        }

        if (external) {
            gdecl->gdata.init = NULL;
        } else {
            ir_expr_t *init;
            if (node->expr != NULL) {
                if (node->expr->type == EXPR_CONST_STR &&
                    node_type->type == TYPE_ARR) {
                    init = ir_expr_create(ts->tunit, IR_EXPR_CONST);
                    init->const_params.ctype = IR_CONST_STR;
                    init->const_params.type = trans_type(ts, node->expr->etype);
                    init->const_params.str_val =
                        unescape_str(node->expr->const_val.str_val);

                } else {
                    init = trans_expr(ts, false, node->expr, NULL);
                    init = trans_type_conversion(ts, node_type,
                                                 node->expr->etype, init, NULL);
                }
            } else {
                init = ir_expr_zero(ts->tunit, expr_type);
            }
            gdecl->gdata.init = init;
            expr_type = ir_expr_type(init);
            ptr_type->ptr.base = expr_type;
        }

        var_expr->var.type = ptr_type;
        var_expr->var.name = node->id;
        var_expr->var.local = false;

        gdecl->gdata.type = expr_type;
        gdecl->gdata.var = var_expr;

        gdecl->gdata.align = ast_type_align(node_type);

        symtab = &ts->tunit->globals;
        access = var_expr;
        break;
    }
    case IR_DECL_NODE_LOCAL: {
        // storage class specifers (auto/register/static/extern) are attached
        // to the base type, need to remove pointers
        type_t *mod_check = node_type;
        while (mod_check->type == TYPE_PTR) {
            mod_check = ast_type_untypedef(mod_check->ptr.base);
        }
        ir_linkage_t linkage = IR_LINKAGE_DEFAULT;
        if (mod_check->type == TYPE_MOD) {
            if (mod_check->mod.type_mod & TMOD_STATIC) {
                linkage = IR_LINKAGE_INTERNAL;
            } else if (mod_check->mod.type_mod & TMOD_EXTERN) {
                linkage = IR_LINKAGE_EXTERNAL;
            }
        }
        // TODO1: Handle extern, need to not translate this, add it to the
        // gdecls hashtable

        symtab = &ts->func->func.locals;
        access = var_expr;

        if (linkage == IR_LINKAGE_INTERNAL) {
            char namebuf[MAX_GLOBAL_NAME];
            snprintf(namebuf, sizeof(namebuf), "%s.%s",
                     ts->func->func.name, node->id);
            var_expr->var.type = ptr_type;
            var_expr->var.name = sstore_lookup(namebuf);
            var_expr->var.local = false;

            ir_expr_t *init;
            // TODO1: This is copied from GDATA, move to function
            if (node->expr != NULL) {
                if (node->expr->type == EXPR_CONST_STR &&
                    node_type->type == TYPE_ARR) {
                    init = ir_expr_create(ts->tunit, IR_EXPR_CONST);
                    init->const_params.ctype = IR_CONST_STR;
                    init->const_params.type = trans_type(ts, node->expr->etype);
                    init->const_params.str_val =
                        unescape_str(node->expr->const_val.str_val);

                } else {
                    init = trans_expr(ts, false, node->expr, NULL);
                    init = trans_type_conversion(ts, node_type,
                                                 node->expr->etype, init, NULL);
                }
            } else {
                init = ir_expr_zero(ts->tunit, expr_type);
            }

            ir_gdecl_t *global = ir_gdecl_create(IR_GDECL_GDATA);
            global->linkage = linkage;
            global->gdata.flags = IR_GDATA_NOFLAG;
            global->gdata.type = expr_type;
            global->gdata.var = var_expr;
            global->gdata.init = init;
            global->gdata.align = ast_type_align(node->type);
            sl_append(&ts->tunit->decls, &global->link);

            break;
        }

        ir_inst_stream_t *ir_stmts = context;

        var_expr->var.type = ptr_type;
        var_expr->var.name = trans_decl_node_name(symtab, node->id);
        var_expr->var.local = true;

        // Have to allocate variable on the stack
        ir_expr_t *src = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
        src->alloca.type = ptr_type;
        src->alloca.elem_type = var_expr->var.type->ptr.base;
        src->alloca.nelem_type = NULL;
        src->alloca.align = ast_type_align(node_type);

        // Assign the named variable to the allocation
        ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        stmt->assign.dest = var_expr;
        stmt->assign.src = src;
        trans_add_stmt(ts, &ts->func->func.prefix, stmt);

        if (node->expr != NULL && !ts->ignore_until_label) {
            trans_initializer(ts, ir_stmts, node_type, expr_type, var_expr,
                              node->expr);
        }

        break;
    }
    case IR_DECL_NODE_FUNC_PARAM: {
        symtab = &ts->func->func.locals;

        var_expr->var.type = expr_type;
        var_expr->var.name = trans_decl_node_name(symtab, node->id);
        var_expr->var.local = true;

        ir_expr_t *alloca = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
        alloca->alloca.type = ptr_type;
        alloca->alloca.elem_type = var_expr->var.type;
        alloca->alloca.nelem_type = NULL;
        alloca->alloca.align = ast_type_align(node_type);

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

void trans_struct_init_helper(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                              type_t *ast_type, ir_type_t *ir_type,
                              ir_expr_t *addr, expr_t *val, ir_type_t *ptr_type,
                              sl_link_t **cur_expr, size_t offset) {
    if (ir_type->type == IR_TYPE_ID_STRUCT) {
        ir_type = ir_type->id_struct.type;
    }

    ir_type_t *cur_type = vec_get(&ir_type->struct_params.types, offset);
    ir_expr_t *cur_addr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
    cur_addr->getelemptr.type = cur_type;
    cur_addr->getelemptr.ptr_type = ptr_type;
    cur_addr->getelemptr.ptr_val = addr;

    // We need to 0's on getelemptr, one to get the struct, another
    // to get the struct index
    ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i32);
    sl_append(&cur_addr->getelemptr.idxs, &zero->link);

    zero = ir_int_const(ts->tunit, &ir_type_i32, offset);
    sl_append(&cur_addr->getelemptr.idxs, &zero->link);

    cur_addr = trans_assign_temp(ts, ir_stmts, cur_addr);

    if (*cur_expr == NULL) {
        trans_initializer(ts, ir_stmts, ast_type, cur_type, cur_addr, NULL);
    } else {
        expr_t *elem = GET_ELEM(&val->init_list.exprs, *cur_expr);
        trans_initializer(ts, ir_stmts, ast_type, cur_type, cur_addr, elem);

        *cur_expr = (*cur_expr)->next;
    }
}

void trans_initializer(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                       type_t *ast_type, ir_type_t *ir_type, ir_expr_t *addr,
                       expr_t *val) {
    switch (ast_type->type) {
    case TYPE_STRUCT: {
        assert(val == NULL || val->type == EXPR_INIT_LIST);
        assert(ir_type->type == IR_TYPE_STRUCT ||
               ir_type->type == IR_TYPE_ID_STRUCT);

        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = ir_type;

        size_t offset = 0;
        sl_link_t *cur_expr = val->init_list.exprs.head;
        SL_FOREACH(cur_decl, &ast_type->struct_params.decls) {
            decl_t *decl = GET_ELEM(&ast_type->struct_params.decls, cur_decl);
            SL_FOREACH(cur_node, &decl->decls) {
                decl_node_t *node = GET_ELEM(&decl->decls, cur_node);
                trans_struct_init_helper(ts, ir_stmts, node->type,
                                         ir_type, addr, val, ptr_type,
                                         &cur_expr, offset);
                ++offset;
            }

            if (sl_head(&decl->decls) == NULL &&
                (decl->type->type == TYPE_STRUCT ||
                 decl->type->type == TYPE_UNION)) {
                trans_struct_init_helper(ts, ir_stmts, decl->type,
                                         ir_type, addr, val, ptr_type,
                                         &cur_expr, offset);
                ++offset;
            }
        }
        break;
    }
    case TYPE_ARR: {
        if (val != NULL && val->type == EXPR_CONST_STR) {
            assert(val->etype->type == TYPE_ARR);
            size_t len = val->etype->arr.nelems;
            ir_expr_t *string_expr = trans_string(ts, val->const_val.str_val);
            string_expr = trans_assign_temp(ts, ir_stmts, string_expr);
            trans_memcpy(ts, ir_stmts, addr, string_expr, len, 1, false);
            return;
        }
        assert(val == NULL || val->type == EXPR_INIT_LIST);
        assert(ir_type->type == IR_TYPE_ARR);

        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = ir_type;

        ir_type_t *elem_type = trans_type(ts, ast_type->arr.base);

        size_t nelem = 0;
        if (val != NULL) {
            SL_FOREACH(cur, &val->init_list.exprs) {
                ir_expr_t *cur_addr = ir_expr_create(ts->tunit,
                                                     IR_EXPR_GETELEMPTR);
                cur_addr->getelemptr.type = ir_type->arr.elem_type;
                cur_addr->getelemptr.ptr_type = ptr_type;
                cur_addr->getelemptr.ptr_val = addr;

                // We need to 0's on getelemptr, one to get the array, another
                // to get the array index
                ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i64);
                sl_append(&cur_addr->getelemptr.idxs, &zero->link);

                zero = ir_int_const(ts->tunit, &ir_type_i64, nelem);
                sl_append(&cur_addr->getelemptr.idxs, &zero->link);

                cur_addr = trans_assign_temp(ts, ir_stmts, cur_addr);
                expr_t *elem = GET_ELEM(&val->init_list.exprs, cur);
                trans_initializer(ts, ir_stmts, ast_type->arr.base,
                                  elem_type, cur_addr, elem);
                ++nelem;
                if (nelem == ir_type->arr.nelems) {
                    break;
                }
            }
        }

        // TODO2: Optimization, for trailing zeros make a loop
        for(; nelem < ir_type->arr.nelems; ++nelem) {
            ir_expr_t *cur_addr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
            cur_addr->getelemptr.type = ir_type->arr.elem_type;
            cur_addr->getelemptr.ptr_type = ptr_type;
            cur_addr->getelemptr.ptr_val = addr;

            // We need to 0's on getelemptr, one to get the array, another to
            // get the array index
            ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i64);
            sl_append(&cur_addr->getelemptr.idxs, &zero->link);

            zero = ir_int_const(ts->tunit, &ir_type_i64, nelem);
            sl_append(&cur_addr->getelemptr.idxs, &zero->link);

            cur_addr = trans_assign_temp(ts, ir_stmts, cur_addr);

            trans_initializer(ts, ir_stmts, ast_type->arr.base,
                              elem_type, cur_addr, NULL);
        }
        break;
    }
    case TYPE_UNION: {
        assert(val == NULL || val->type == EXPR_INIT_LIST);

        type_t *dest_type = ast_get_union_type(ast_type, val, &val);

        ir_type_t *ir_dest_type = trans_type(ts, dest_type);
        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = ir_dest_type;

        addr = trans_ir_type_conversion(ts, ptr_type, false,
                                        ir_expr_type(addr), false,
                                        addr, ir_stmts);
        ast_type = dest_type;
        ir_type = ir_dest_type;
        // FALL THROUGH
    }
    default: {
        ir_expr_t *ir_val = val == NULL ? ir_expr_zero(ts->tunit, ir_type) :
            trans_expr(ts, false, val, ir_stmts);
        ir_stmt_t *store = ir_stmt_create(ts->tunit, IR_STMT_STORE);
        store->store.type = ir_type;
        if (val == NULL) {
            store->store.val = ir_val;
        } else {
            store->store.val = trans_type_conversion(ts, ast_type, val->etype,
                                                     ir_val, ir_stmts);
        }
        store->store.ptr = addr;
        trans_add_stmt(ts, ir_stmts, store);
    }
    }
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
    case TYPE_LONG_DOUBLE: return &ir_type_double;
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
                    ir_type_t *node_type = trans_type(ts, node->type);
                    vec_push_back(&ir_type->struct_params.types, node_type);
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
            decl_node_t *node = sl_head(&decl->decls);
            type_t *ptype = node == NULL ? decl->type : node->type;
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
    default:
        assert(false);
    }
    return NULL;
}

ir_expr_t *trans_create_anon_global(trans_state_t *ts, ir_type_t *type,
                                    ir_expr_t *init, size_t align,
                                    ir_linkage_t linkage,
                                    ir_gdata_flags_t flags) {
    char namebuf[MAX_GLOBAL_NAME];

    snprintf(namebuf, MAX_GLOBAL_NAME, "%s%d", GLOBAL_PREFIX,
             ts->tunit->static_num++);

    ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    ptr_type->ptr.base = type;

    ir_expr_t *var = ir_expr_create(ts->tunit, IR_EXPR_VAR);
    var->var.name = sstore_lookup(namebuf);
    var->var.type = ptr_type;
    var->var.local = false;

    ir_gdecl_t *global = ir_gdecl_create(IR_GDECL_GDATA);
    global->linkage = linkage;
    global->gdata.flags = flags;
    global->gdata.type = type;
    global->gdata.var = var;
    global->gdata.init = init;
    global->gdata.align = align;
    sl_append(&ts->tunit->decls, &global->link);

    return var;
}

ir_expr_t *trans_string(trans_state_t *ts, char *str) {
    ht_ptr_elem_t *elem = ht_lookup(&ts->tunit->strings, &str);
    if (elem != NULL) {
        return elem->val;
    }

    char *unescaped = unescape_str(str);

    ir_type_t *type = ir_type_create(ts->tunit, IR_TYPE_ARR);
    type->arr.nelems = strlen(unescaped) + 1;
    type->arr.elem_type = &ir_type_i8;
    ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    ptr_type->ptr.base = type;

    ir_expr_t *arr_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    arr_lit->const_params.ctype = IR_CONST_STR;
    arr_lit->const_params.type = type;
    arr_lit->const_params.str_val = unescaped;

    ir_expr_t *var =
        trans_create_anon_global(ts, type, arr_lit, 1, IR_LINKAGE_PRIVATE,
                                 IR_GDATA_CONSTANT | IR_GDATA_UNNAMED_ADDR);

    ir_expr_t *elem_ptr = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
    ir_type_t *elem_ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    elem_ptr_type->ptr.base = type->arr.elem_type;
    elem_ptr->getelemptr.type = elem_ptr_type;
    elem_ptr->getelemptr.ptr_type = ptr_type;
    elem_ptr->getelemptr.ptr_val = var;

    // We need to 0's on getelemptr, one to get the array, another to get
    // the array's address
    ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i32);
    sl_append(&elem_ptr->getelemptr.idxs, &zero->link);

    zero = ir_expr_zero(ts->tunit, &ir_type_i32);
    sl_append(&elem_ptr->getelemptr.idxs, &zero->link);

    elem = emalloc(sizeof(*elem));
    elem->key = str;
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
    type_t *ast_elem_type = expr->etype->arr.base;

    ir_expr_t *arr_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    sl_init(&arr_lit->const_params.arr_val, offsetof(ir_expr_t, link));
    arr_lit->const_params.ctype = IR_CONST_ARR;
    arr_lit->const_params.type = type;

    size_t nelems = 0;
    SL_FOREACH(cur, &expr->init_list.exprs) {
        expr_t *elem = GET_ELEM(&expr->init_list.exprs, cur);
        ir_expr_t *ir_elem = trans_expr(ts, false, elem, NULL);
        ir_elem = trans_type_conversion(ts, ast_elem_type, elem->etype,
                                        ir_elem, NULL);
        sl_append(&arr_lit->const_params.arr_val, &ir_elem->link);
        ++nelems;
    }

    while (nelems++ < type->arr.nelems) {
        ir_expr_t *zero = ir_expr_zero(ts->tunit, elem_type);
        sl_append(&arr_lit->const_params.arr_val, &zero->link);
    }

    return arr_lit;
}

ir_expr_t *trans_struct_init(trans_state_t *ts, expr_t *expr) {
    assert(expr->type == EXPR_INIT_LIST);
    assert(expr->etype->type == TYPE_STRUCT);

    ir_type_t *type = trans_type(ts, expr->etype);
    assert(type->type == IR_TYPE_STRUCT || type->type == IR_TYPE_ID_STRUCT);
    if (type->type == IR_TYPE_ID_STRUCT) {
        type = type->id_struct.type;
        assert(type->type == IR_TYPE_STRUCT);
    }

    ir_expr_t *struct_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    sl_init(&struct_lit->const_params.struct_val, offsetof(ir_expr_t, link));
    struct_lit->const_params.ctype = IR_CONST_STRUCT;
    struct_lit->const_params.type = type;

    sl_link_t *cur_elem = expr->init_list.exprs.head;
    VEC_FOREACH(cur, &type->struct_params.types) {
        ir_type_t *cur_type = vec_get(&type->struct_params.types, cur);

        ir_expr_t *ir_elem;
        if (cur_elem == NULL) {
            ir_elem = ir_expr_zero(ts->tunit, cur_type);
        } else {
            expr_t *elem = GET_ELEM(&expr->init_list.exprs, cur_elem);
            ir_elem = trans_expr(ts, false, elem, NULL);
            ir_elem = trans_ir_type_conversion(ts, cur_type, false,
                                               ir_expr_type(ir_elem), false,
                                               ir_elem, NULL);
            cur_elem = cur_elem->next;
        }
        sl_append(&struct_lit->const_params.struct_val, &ir_elem->link);
    }

    return struct_lit;
}

ir_expr_t *trans_union_init(trans_state_t *ts, type_t *type, expr_t *expr) {
    assert(expr->type == EXPR_INIT_LIST);
    assert(expr->etype->type == TYPE_UNION);

    expr_t *head;
    type_t *elem_type = ast_get_union_type(type, expr, &head);
    size_t total_size = ast_type_size(type);
    size_t elem_size = ast_type_size(elem_type);

    ir_type_t *ir_elem_type = trans_type(ts, elem_type);

    ir_type_t *expr_type = ir_type_create(ts->tunit, IR_TYPE_STRUCT);
    vec_push_back(&expr_type->struct_params.types, ir_elem_type);

    ir_type_t *pad_type = NULL;
    if (elem_size != total_size) {
        assert(elem_size < total_size);
        pad_type = ir_type_create(ts->tunit, IR_TYPE_ARR);
        pad_type->arr.nelems = total_size - elem_size;
        pad_type->arr.elem_type = &ir_type_i8;
        vec_push_back(&expr_type->struct_params.types, pad_type);
    }

    ir_expr_t *struct_lit = ir_expr_create(ts->tunit, IR_EXPR_CONST);
    sl_init(&struct_lit->const_params.struct_val, offsetof(ir_expr_t, link));
    struct_lit->const_params.ctype = IR_CONST_STRUCT;
    struct_lit->const_params.type = expr_type;

    ir_expr_t *ir_elem = trans_expr(ts, false, head, NULL);
    sl_append(&struct_lit->const_params.struct_val, &ir_elem->link);

    if (elem_size != total_size) {
        assert(pad_type != NULL);
        ir_elem = ir_expr_create(ts->tunit, IR_EXPR_CONST);
        ir_elem->const_params.ctype = IR_CONST_UNDEF;
        ir_elem->const_params.type = pad_type;
        sl_append(&struct_lit->const_params.struct_val, &ir_elem->link);
    }

    return struct_lit;
}


void trans_memcpy(trans_state_t *ts, ir_inst_stream_t *ir_stmts,
                  ir_expr_t *dest, ir_expr_t *src, size_t len,
                  size_t align, bool isvolatile) {
    ir_expr_t *dest_ptr = trans_ir_type_conversion(ts, &ir_type_i8_ptr, false,
                                                   ir_expr_type(dest), false,
                                                   dest, ir_stmts);
    ir_expr_t *src_ptr = trans_ir_type_conversion(ts, &ir_type_i8_ptr, false,
                                                  ir_expr_type(src), false,
                                                  src, ir_stmts);

    ir_expr_t *len_expr = ir_int_const(ts->tunit, &ir_type_i64, len);
    ir_expr_t *align_expr = ir_int_const(ts->tunit, &ir_type_i32, align);

    ir_expr_t *volatile_expr = ir_int_const(ts->tunit, &ir_type_i1, isvolatile);

    char *func_name = LLVM_MEMCPY;
    ir_symtab_entry_t *func = ir_symtab_lookup(&ts->tunit->globals, func_name);

    // Lazily create the function declaration and object
    if (func == NULL) {
        ir_type_t *func_type = ir_type_create(ts->tunit, IR_TYPE_FUNC);
        func_type->func.type = &ir_type_void;
        func_type->func.varargs = false;
        vec_push_back(&func_type->func.params, &ir_type_i8_ptr);
        vec_push_back(&func_type->func.params, &ir_type_i8_ptr);
        vec_push_back(&func_type->func.params, &ir_type_i64);
        vec_push_back(&func_type->func.params, &ir_type_i32);
        vec_push_back(&func_type->func.params, &ir_type_i1);

        ir_expr_t *var_expr = ir_expr_create(ts->tunit, IR_EXPR_VAR);
        var_expr->var.type = func_type;
        var_expr->var.name = func_name;
        var_expr->var.local = false;

        func = ir_symtab_entry_create(IR_SYMTAB_ENTRY_VAR, var_expr->var.name);
        func->var.expr = var_expr;
        func->var.access = var_expr;

        status_t status = ir_symtab_insert(&ts->tunit->globals, func);
        assert(status == CCC_OK);

        // Add the declaration
        ir_gdecl_t *ir_gdecl = ir_gdecl_create(IR_GDECL_FUNC_DECL);
        ir_gdecl->func_decl.type = func_type;
        ir_gdecl->func_decl.name = func_name;
        sl_append(&ts->tunit->decls, &ir_gdecl->link);
    }
    assert(func->type == IR_SYMTAB_ENTRY_VAR);

    ir_expr_t *func_expr = func->var.access;

    ir_expr_t *call = ir_expr_create(ts->tunit, IR_EXPR_CALL);
    call->call.func_sig = ir_expr_type(func_expr);
    call->call.func_ptr = func_expr;

    sl_append(&call->call.arglist, &dest_ptr->link);
    sl_append(&call->call.arglist, &src_ptr->link);
    sl_append(&call->call.arglist, &len_expr->link);
    sl_append(&call->call.arglist, &align_expr->link);
    sl_append(&call->call.arglist, &volatile_expr->link);

    ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_EXPR);
    stmt->expr = call;
    trans_add_stmt(ts, ir_stmts, stmt);
}

bool trans_struct_mem_offset(trans_state_t *ts, type_t *type, char *mem_name,
                             slist_t *indexs) {
    type = ast_type_unmod(type);
    assert(type->type == TYPE_STRUCT);

    size_t offset = 0;
    SL_FOREACH(cur_decl, &type->struct_params.decls) {
        decl_t *decl = GET_ELEM(&type->struct_params.decls, cur_decl);
        SL_FOREACH(cur_node, &decl->decls) {
            decl_node_t *node = GET_ELEM(&decl->decls, cur_node);
            if (strcmp(node->id, mem_name) == 0) {
                ir_expr_t *index =
                    ir_int_const(ts->tunit, &ir_type_i32, offset);
                sl_prepend(indexs, &index->link);
                return true;
            }
            ++offset;
        }

        if (sl_head(&decl->decls) == NULL &&
            (decl->type->type == TYPE_STRUCT ||
             decl->type->type == TYPE_UNION)) {
            if (trans_struct_mem_offset(ts, decl->type, mem_name, indexs)) {
                ir_expr_t *index =
                    ir_int_const(ts->tunit, &ir_type_i32, offset);
                sl_prepend(indexs, &index->link);
                return true;
            }
            ++offset;
        }
    }

    return false;
}

ir_expr_t *trans_compound_literal(trans_state_t *ts, bool addrof,
                                  ir_inst_stream_t *ir_stmts, expr_t *expr) {
    assert(expr->type == EXPR_INIT_LIST);

    ir_type_t *type = trans_type(ts, expr->etype);

    ir_expr_t *addr;
    if (ts->func == NULL) { // Global
        ir_expr_t *init = trans_expr(ts, false, expr, NULL);
        addr = trans_create_anon_global(ts, type, init,
                                        ast_type_align(expr->etype),
                                        IR_LINKAGE_INTERNAL,
                                        IR_GDATA_NOFLAG);
    } else { // Local
        ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
        ptr_type->ptr.base = type;
        ir_expr_t *alloc = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
        alloc->alloca.type = ptr_type;
        alloc->alloca.elem_type = type;
        alloc->alloca.nelem_type = NULL;
        alloc->alloca.align = ast_type_align(expr->etype);

        addr = trans_temp_create(ts, ptr_type);

        // Assign to temp
        // Note we can't use trans_assign_temp because its an alloca
        ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        stmt->assign.dest = addr;
        stmt->assign.src = alloc;
        trans_add_stmt(ts, ir_stmts, stmt);

        // Store the initalizer
        trans_initializer(ts, ir_stmts, expr->etype, type, addr, expr);

    }

    if (addrof) {
        return addr;
    } else {
        return trans_load_temp(ts, ir_stmts, addr);
    }
}
