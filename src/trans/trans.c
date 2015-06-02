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

#include "trans.h"
#include "trans_priv.h"

#include "trans_decl.h"
#include "trans_expr.h"
#include "trans_type.h"

#include <assert.h>

#include "util/util.h"
#include "util/string_store.h"
#include "typecheck/typecheck.h"

#define GLOBAL_PREFIX ".glo"

ir_trans_unit_t *trans_translate(trans_unit_t *ast) {
    assert(ast != NULL);

    trans_state_t ts = TRANS_STATE_LIT;
    return trans_trans_unit(&ts, ast);
}

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

bool trans_struct_mem_offset(trans_state_t *ts, type_t *type, char *mem_name,
                             slist_t *indexs) {
    type = ast_type_unmod(type);
    if (type->type == TYPE_UNION) {
        return true;
    }

    assert(type->type == TYPE_STRUCT);

    bool bitfield_last = false;
    size_t offset = 0;
    SL_FOREACH(cur_decl, &type->struct_params.decls) {
        decl_t *decl = GET_ELEM(&type->struct_params.decls, cur_decl);
        SL_FOREACH(cur_node, &decl->decls) {
            decl_node_t *node = GET_ELEM(&decl->decls, cur_node);
            if (node->id == NULL) {
                continue;
            }
            if (node->expr == NULL) {
                bitfield_last = false;
            } else {
                if (bitfield_last) {
                    continue;
                }
                bitfield_last = true;
            }
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
            if (ast_type_find_member(decl->type, mem_name, NULL, NULL)
                != NULL) {
                ir_expr_t *index =
                    ir_int_const(ts->tunit, &ir_type_i32, offset);
                sl_prepend(indexs, &index->link);
                return true;
            }
            ++offset;
            bitfield_last = false;
        }
    }

    return false;
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
