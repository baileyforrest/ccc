/*
  Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>

  This file is part of CCC.

  CCC is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  CCC is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with CCC.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * Type checker implementation
 */

#include "typechecker.h"
#include "typechecker_priv.h"

#include <assert.h>


bool typecheck_ast(trans_unit_t *ast) {
    tc_state tcs = { NULL };
    return typecheck_trans_unit(&tcs, ast);
}

bool typecheck_trans_unit(tc_state *tcs, trans_unit_t *trans_unit) {
    typetab_t *save_tab = tcs->typetab;
    tcs->typetab = &trans_unit->typetab;
    bool retval = true;

    sl_link_t *cur;
    SL_FOREACH(cur, &trans_unit->gdecls) {
        retval &= typecheck_gdecl(tcs, GET_ELEM(&trans_unit->gdecls, cur));
    }

    tcs->typetab = save_tab;
    return retval;
}

bool typecheck_gdecl(tc_state *tcs, gdecl_t *gdecl) {
    bool retval = true;

    retval &= typecheck_decl(tcs, gdecl->decl, TC_NOCONST);

    switch (gdecl->type) {
    case GDECL_NOP:
        // Valid AST shouldn't have this
        assert(false);
        break;
    case GDECL_FDEFN:
        retval &= typecheck_stmt(tcs, gdecl->fdefn.stmt);
        break;
    case GDECL_DECL:
        break;
    }

    return retval;
}

bool typecheck_stmt(tc_state *tcs, stmt_t *stmt) {
    bool retval = true;
    switch (stmt->type) {
    case STMT_NOP:
        return true;

    case STMT_DECL:
        return typecheck_decl(tcs, stmt->decl, TC_NOCONST);

    case STMT_LABEL:
        return typecheck_stmt(tcs, stmt->label.stmt);
    case STMT_CASE:
        retval &= typecheck_expr(tcs, stmt->case_params.val, TC_CONST);
        // TODO: Make sure expression is an integral type
        retval &= typecheck_stmt(tcs, stmt->case_params->stmt);
        // TODO: Make sure inside a switch statement
        return retval;
    case STMT_DEFAULT:
        // TODO: Make sure inside a switch statement
        return typecheck_stmt(tcs, stmt->default_params);

    case STMT_IF:
        retval &= typecheck_expr(tcs, stmt->if_params.expr, TC_NOCONST);
        // TODO: Make sure expr some sort of true/false type
        retval &= typecheck_stmt(tcs, stmt->if_params.true_stmt);
        if (stmt->if_params.true_false != NULL) {
            retval &= typecheck_stmt(tcs, stmt->if_params.true_false);
        }
        return retval;
    case STMT_SWITCH:
        retval &= typecheck_expr(tcs, stmt->switch_params.expr,
                                 TC_NOCONST);
        // TODO: Make sure expr some sort of value type
        // TODO: Set current switch statement
        retval &= typecheck_stmt(tcs, stmt->switch_params.stmt);
        return retval;

    case STMT_DO:
        // TODO: Set current loop
        retval &= typecheck_stmt(tcs, stmt->do_params.stmt);
        retval &= typecheck_expr(tcs, stmt->do_params.expr, TC_NOCONST);
        // TODO: Make sure expr some sort of true/false type
        return retval;
    case STMT_WHILE:
        // TODO: Set current loop
        retval &= typecheck_expr(tcs, stmt->while_params.expr, TC_NOCONST);
        // TODO: Make sure expr some sort of true/false type
        retval &= typecheck_stmt(tcs, stmt->while_params.stmt);
        return retval;
    case STMT_FOR:
        // TODO: Set current loop
        retval &= typecheck_expr(tcs, stmt->for_params.expr1, TC_NOCONST);
        retval &= typecheck_expr(tcs, stmt->for_params.expr2, TC_NOCONST);
        // TODO: Make sure expr some sort of true/false type
        retval &= typecheck_expr(tcs, stmt->for_params.expr3, TC_NOCONST);
        retval &= typecheck_stmt(tcs, stmt->for_params.stmt);
        return retval;

    case STMT_GOTO:
        // TODO: Need to store a table of GOTOs and labels in a function
        return retval;
    case STMT_CONTINUE:
        // TODO: Need to make sure in a loop, set parent
        return retval;
    case STMT_BREAK:
        // TODO: Need to make sure in a loop or switch statement, set parent
        return retval;
    case STMT_RETURN:
        retval &= typecheck_expr(tcs, stmt->return_params.expr,
                                 TC_NOCONST);
        // TODO: Need to make sure compatible type with return value
        return retval;

    case STMT_COMPOUND: {
        // Enter new scope
        typetab_t *save_tab = tcs->typetab;
        tcs->typetab = &stmt->compound.typetab;

        sl_link_t *cur;
        SL_FOREACH(cur, &stmt->compound.stmts) {
            retval &= typecheck_stmt(tcs, GET_ELEM(&stmt->compound.stmts, cur));
        }

        // Restore scope
        tcs->typetab = save_tab;
        return retval;
    }

    case STMT_EXPR:
        return typecheck_expr(tcs, stmt->expr.expr, TC_NOCONST);

    default:
        assert(false);
    }

    return retval;
}

bool typecheck_decl(tc_state *tcs, decl_t *decl, bool constant) {
    bool retval = true;

    retval &= typecheck_type(decl->type);
    sl_link_t *cur;
    SL_FOREACH(cur, &decl->decls) {
        retval &= typecheck_decr_node(tcs, GET_ELEM(&decl->decls, cur),
                                      constant);
    }

    return retval;
}

bool typecheck_decl_node(tc_state *tcs, decl_node_t *decl_node, bool constant) {
    bool retval = true;
    retval &= typecheck_type(decl_node->type);
    retval &= typecheck_expr(decl_node->expr, constant);
    // TODO: Make sure expression is compatible with type
    // if constant == TC_CONST, make sure they are compatible with int

    return retval;
}

bool typecheck_expr(tc_state *tcs, expr_t *expr, bool constant) {
    bool retval = true;
    switch(expr->type) {
    case EXPR_VOID:
        expr->etype = tt_void;
        return retval;
    case EXPR_PAREN:
        retval &= typecheck_expr(tcs, expr->paren_base, constant);
        expr->etype = expr->paren_base.etype;
        return retval;
    case EXPR_VAR: {
        tt_key_t lookup = { expr->var_id, TT_VAR };
        typetab_entry_t *entry = tt_lookup(tcs->typetab, lookup);
        if (entry == NULL) {
            snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE, "'%.s' undeclared.",
                     (int)expr->var_id->len, expr->var_id->str);
            logger_log(&expr->mark, logger_fmt_buf, LOG_ERR);
            retval = false;
        }
        expr->etype = type;
        return retval;
    }
    case EXPR_ASSIGN:
        retval &= typecheck_expr(tcs, expr->assign.dest, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->assign.expr, TC_NOCONST);
        // TODO: Make sure dest can be assigned to expr
        expr->etype = expr->assign.dest.etype;
        return retval;
    case EXPR_CONST_INT:
        expr->etype = expr->const_val.type;
        // TODO: Check for type bounds.
        // Problem: What if negative? Probably have lexer handle it
        return retval;
    case EXPR_CONST_FLOAT:
        expr->etype = expr->const_val.type;
        return retval;
    case EXPR_CONST_STR:
        expr->etype = expr->const_val.type;
        return retval;
    case EXPR_BIN:
        retval &= typecheck_expr(tcs, expr->bin.expr1, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->bin.expr2, TC_NOCONST);
        // TODO: Make sure expr1 & expr2 are compatible with op
        // TODO: Perform type promotion
        expr->etype = expr->bin.expr1.etype; // TODO: Temporary
        return retval;
    case EXPR_UNARY:
        retval &= typecheck_expr(tcs, unary->bin.expr, TC_NOCONST);
        // TODO: Make sure retval compatible with op
        expr->etype = expr->unary.expr.etype;
        return retval;
    case EXPR_COND:
        retval &= typecheck_expr(tcs, expr->cond.expr1, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->cond.expr2, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->cond.expr3, TC_NOCONST);
        // TODO: Make sure expr1 can be true/false
        // TODO: Make sure expr2 and expr3 are same type, perform promotion
        expr->etype = expr->cond.expr1.etype; // TODO: Temporary
        return retval;
    case EXPR_CAST:
        retval &= typecheck_expr(tcs, expr->cast.base, TC_NOCONST);
        // TODO: make sure can be casted to type
        expr->etype = expr->cast.cast;
        return retval;
    case EXPR_CALL: {
        retval &= typecheck_expr(tcs, expr->call.func, TC_NOCONST);
        type_t *func_sig = expr->call.func->etype;
        if (func_sig->type != TYPE_FUNC) {
            logger_log(&expr->mark,
                       "Called object is not a function or function pointer",
                       LOG_ERR);
            return false;
        }
        int arg_num = 1;
        sl_link_t *cur_sig, *cur_expr;
        cur_sig = func_sig->func.params.head;
        cur_expr = expr->call.params.head;
        while (cur_sig != NULL && cur_exp != NULL) {
            decl_t *decl = GET_ELEM(&func_sig->func.params, cur_sig);
            decl_node_t *param = sl_head(&decl->decls);
            expr_t *expr = GET_ELEM(&expr->call.params, cur_expr);
            retval &= typecheck_expr(expr, TC_NOCONST);

            // TODO: Make sure expr->etype and param->type are compatible
            if (false) {
                snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                         "incompatible type for argument %d of function",
                         arg_num);
                logger_log(&expr->mark, logger_fmt_buf, LOG_ERR);
                // TODO: Print note with expected types and function sig
            }

            ++arg_num;
            cur_sig = cur_sig->next;
            cur_expr = cur_expr->next;
        }
        if (cur_sig != NULL) {
            logger_log(&expr->mark, "too few arguments to function", LOG_ERR);
            // TODO: print type
        }
        if (cur_exp != NULL) {
            logger_log(&expr->mark, "too many arguments to function", LOG_ERR);
            // TODO: print type
        }
        expr->etype = func_sig.type;
        return retval;
    }
    case EXPR_CMPD: {
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            retval &= typecheck_expr(GET_ELEM(&expr->cmpd.exprs, cur),
                                     TC_NOCONST);
        }
        expr_t *tail = sl_tail(&expr->cmpd.exprs);
        expr->etype = tail->etype;
        return retval;
    }
    case EXPR_SIZEOF:
        if (expr->sizeof_params.type != NULL) {
            retval &= typecheck_type(expr->sizeof_params.type);
        }
        if (expr->sizeof_params.expr != NULL) {
            retval &= typecheck_expr(expr->sizeof_params.expr, NC_NOCONST);
        }
        expr->etype = tt_long; // TODO: Fix this
        return retval;
    case EXPR_MEM_ACC:
        retval &= typecheck_expr(expr->mem_acc.base, NC_NOCONST);
        // TODO: Make sure compatible type with op, make sure member exists
        expr->etype = expr->mem_acc.base; // TODO: Return member's type
        return retval;
    case EXPR_INIT_LIST:
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->init_list.exprs) {
            retval &= typecheck_expr(GET_ELEM(&expr->init_list.exprs, cur),
                                     TC_NOCONST);
        }
        // TODO: This needs to be set by user of init list, and verified with
        // the exprs
        expr->etype = NULL;
        return retval;
    default:
        assert(false);
    }

    return retval;
}

// TODO: Types may get typechecked multiple times, may need to add a mechanism
// to prevent this. For example: hashtable of type pointers inside tcs.
// We're only really afraid of compound types, so maybe only store those/or
// implement a policy decision
bool typecheck_type(tc_state *tcs, type_t *type) {
    bool retval = true;

    switch(type->type) {
    case TYPE_VOID:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
        return retval;

    case TYPE_STRUCT:
    case TYPE_UNION: {
        sl_link_t *cur;
        SL_FOREACH(cur, &type->struct_params.decls) {
            retval &= typecheck_decl(GET_ELEM(&type->struct_params.decls, cur),
                                     TC_CONST);
        }
        return retval;
    }
    case TYPE_ENUM: {
        retval &= typecheck_type(type->enum_params.type);
        sl_link_t *cur;
        SL_FOREACH(cur, &type->enum_params.ids) {
            retval &= typecheck_decl_node(GET_ELEM(&type->enum_params.ids, cur),
                                     TC_CONST);
        }
        return retval;
    }

    case TYPE_TYPEDEF:
        return retval;

    case TYPE_MOD:
        retval &= typecheck_type(type->mod.base);
        // TODO: Check for contradictory modifiers
        return retval;

    case TYPE_PAREN:
        return typecheck_type(type->paren_base);
    case TYPE_FUNC:
        retval &= typecheck_type(type->func.type);
        sl_link_t *cur;
        SL_FOREACH(cur, &type->func.params) {
            retval &= typecheck_decl(GET_ELEM(&type->func.params, cur),
                                     TC_CONST);
        }
        return retval;
    case TYPE_ARR:
        retval &= typecheck_type(type->arr.base);
        retval &= typecheck_expr(type->arr.len, TC_CONST);
        return retval;
    case TYPE_PTR:
        return typecheck_type(base);

    default:
        assert(false);
    }

    return retval;
}
