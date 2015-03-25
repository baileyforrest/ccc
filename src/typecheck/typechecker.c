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
#include <stdio.h>

#include "util/logger.h"

bool typecheck_ast(trans_unit_t *ast) {
    tc_state_t tcs = TCS_LIT;
    return typecheck_trans_unit(&tcs, ast);
}

bool typecheck_const_expr(expr_t *expr, long long *result) {
    tc_state_t tcs = TCS_LIT;
    if (!typecheck_expr(&tcs, expr, TC_CONST)) {
        return false;
    }

    typecheck_const_expr_eval(expr, result);
    return true;
}

/**
 * TODO: Support more types of expressions
 */
void typecheck_const_expr_eval(expr_t *expr, long long *result) {
    switch (expr->type) {
    case EXPR_PAREN:
        typecheck_const_expr_eval(expr->paren_base, result);
        return;
    case EXPR_CONST_INT:
        *result = expr->const_val.int_val;
        return;
    case EXPR_BIN: {
        long long temp1;
        long long temp2;
        typecheck_const_expr_eval(expr->bin.expr1, &temp1);
        typecheck_const_expr_eval(expr->bin.expr2, &temp2);
        switch (expr->bin.op) {
        case OP_TIMES:    *result = temp1 *  temp2; break;
        case OP_DIV:      *result = temp1 /  temp2; break;
        case OP_MOD:      *result = temp1 %  temp2; break;
        case OP_PLUS:     *result = temp1 +  temp2; break;
        case OP_MINUS:    *result = temp1 -  temp2; break;
        case OP_LSHIFT:   *result = temp1 << temp2; break;
        case OP_RSHIFT:   *result = temp1 >> temp2; break;
        case OP_LT:       *result = temp1 <  temp2; break;
        case OP_GT:       *result = temp1 >  temp2; break;
        case OP_LE:       *result = temp1 <= temp2; break;
        case OP_GE:       *result = temp1 >= temp2; break;
        case OP_EQ:       *result = temp1 == temp2; break;
        case OP_NE:       *result = temp1 != temp2; break;
        case OP_BITAND:   *result = temp1 &  temp2; break;
        case OP_BITXOR:   *result = temp1 ^  temp2; break;
        case OP_BITOR:    *result = temp1 |  temp2; break;
        case OP_LOGICAND: *result = temp1 && temp2; break;
        case OP_LOGICOR:  *result = temp1 || temp2; break;
        default:
            assert(false);
        }
        return;
    }
    case EXPR_UNARY: {
        long long temp;
        typecheck_const_expr_eval(expr->unary.expr, &temp);
        switch (expr->unary.op) {
        case OP_UPLUS:    *result =  temp; break;
        case OP_UMINUS:   *result = -temp; break;
        case OP_BITNOT:   *result = ~temp; break;
        case OP_LOGICNOT: *result = !temp; break;
        default:
            assert(false);
        }
        return;
    }
    case EXPR_COND: {
        long long temp;
        typecheck_const_expr_eval(expr->cond.expr1, &temp);
        if (temp) {
            typecheck_const_expr_eval(expr->cond.expr2, result);
        } else {
            typecheck_const_expr_eval(expr->cond.expr3, result);
        }
        return;
    }
    case EXPR_CAST:
        typecheck_const_expr_eval(expr->cast.base, result);
        return;

    case EXPR_SIZEOF:
        if (expr->sizeof_params.type != NULL) {
            decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
            assert(node != NULL);
            *result = node->type->size;
        } else {
            assert(expr->sizeof_params.expr != NULL);
            *result = expr->sizeof_params.expr->etype->size;
        }
        return;
    case EXPR_VOID:
    case EXPR_VAR:
    case EXPR_ASSIGN:
    case EXPR_CONST_FLOAT:
    case EXPR_CONST_STR:
    case EXPR_CALL:
    case EXPR_CMPD:
    case EXPR_MEM_ACC:
    case EXPR_INIT_LIST:
    default:
        assert(false);
    }
}

bool typecheck_type_integral(type_t *type) {
    switch (type->type) {
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_ENUM:
        return true;

    case TYPE_TYPEDEF:
        return typecheck_type_integral(type->typedef_params.base);
    case TYPE_MOD:
        return typecheck_type_integral(type->mod.base);
    case TYPE_PAREN:
        return typecheck_type_integral(type->paren_base);

    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:

    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:
        break;

    default:
        assert(false);
    }

    return false;
}

bool typecheck_type_conditional(type_t *type) {
    switch (type->type) {
    // Primitive types
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:

    case TYPE_ENUM:
    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:
        return true;

    case TYPE_TYPEDEF:
        return typecheck_type_conditional(type->typedef_params.base);
    case TYPE_MOD:
        return typecheck_type_conditional(type->mod.base);
    case TYPE_PAREN:
        return typecheck_type_conditional(type->paren_base);

    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
        break;

    default:
        assert(false);
    }

    return false;
}

bool typecheck_expr_integral(tc_state_t *tcs, expr_t *expr) {
    bool retval = typecheck_expr(tcs, expr, TC_NOCONST);
    if (!(retval &= typecheck_type_integral(expr->etype))) {
        logger_log(&expr->mark, "Integral value required", LOG_ERR);
    }

    return retval;
}

bool typecheck_expr_conditional(tc_state_t *tcs, expr_t *expr) {
    bool retval = typecheck_expr(tcs, expr, TC_NOCONST);
    if (!(retval &= typecheck_type_conditional(expr->etype))) {
        snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                 "used %s type value where scalar is required",
                 ast_basic_type_str(expr->etype->type));
        logger_log(&expr->mark, logger_fmt_buf, LOG_ERR);
    }

    return retval;
}

bool typecheck_trans_unit(tc_state_t *tcs, trans_unit_t *trans_unit) {
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

bool typecheck_gdecl(tc_state_t *tcs, gdecl_t *gdecl) {
    bool retval = true;

    retval &= typecheck_decl(tcs, gdecl->decl, TC_NOCONST);

    switch (gdecl->type) {
    case GDECL_NOP:
        // Valid AST shouldn't have this
        assert(false);
        break;
    case GDECL_FDEFN: {
        gdecl_t *func_save = tcs->func;
        tcs->func = gdecl;
        retval &= typecheck_stmt(tcs, gdecl->fdefn.stmt);
        sl_link_t *cur;
        SL_FOREACH(cur, &gdecl->fdefn.gotos) {
            stmt_t *goto_stmt = GET_ELEM(&gdecl->fdefn.gotos, cur);
            stmt_t *label = ht_lookup(&tcs->func->fdefn.labels,
                                      goto_stmt->goto_params.label);
            if (label == NULL) {
                snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                         "label %*.s used but not defined",
                         (int)goto_stmt->goto_params.label->len,
                         goto_stmt->goto_params.label->str);
                logger_log(&goto_stmt->mark, logger_fmt_buf, LOG_ERR);
                retval = false;
            }
        }
        tcs->func = func_save;
        break;
    }
    case GDECL_DECL:
        break;
    }

    return retval;
}

bool typecheck_stmt(tc_state_t *tcs, stmt_t *stmt) {
    status_t status;
    bool retval = true;
    switch (stmt->type) {
    case STMT_NOP:
        return true;

    case STMT_DECL:
        return typecheck_decl(tcs, stmt->decl, TC_NOCONST);

    case STMT_LABEL:
        retval &= typecheck_stmt(tcs, stmt->label.stmt);

        assert(tcs->func != NULL);
        if (CCC_OK != (status = ht_insert(&tcs->func->fdefn.labels,
                                          &stmt->label.link))) {
            goto fail;
        }
        return retval;
    case STMT_CASE:
        if (tcs->last_switch == NULL) {
            logger_log(&stmt->mark,
                       "'case' label not within a switch statement",
                       LOG_ERR);
            retval = false;
        } else {
            sl_append(&tcs->last_switch->switch_params.cases,
                      &stmt->case_params.link);
        }
        retval &= typecheck_expr_integral(tcs, stmt->case_params.val);
        retval &= typecheck_stmt(tcs, stmt->case_params.stmt);
        return retval;
    case STMT_DEFAULT:
        if (tcs->last_switch == NULL) {
            logger_log(&stmt->mark,
                       "'default' label not within a switch statement",
                       LOG_ERR);
            retval = false;
        } else {
            tcs->last_switch->switch_params.default_stmt = stmt;
        }
        retval &= typecheck_stmt(tcs, stmt->default_params.stmt);
        return retval;

    case STMT_IF:
        retval &= typecheck_expr(tcs, stmt->if_params.expr, TC_NOCONST);
        retval &= typecheck_expr_conditional(tcs, stmt->if_params.expr);
        retval &= typecheck_stmt(tcs, stmt->if_params.true_stmt);
        if (stmt->if_params.false_stmt != NULL) {
            retval &= typecheck_stmt(tcs, stmt->if_params.false_stmt);
        }
        return retval;
    case STMT_SWITCH: {
        retval &= typecheck_expr_integral(tcs, stmt->switch_params.expr);

        stmt_t *switch_save = tcs->last_switch;
        stmt_t *break_save = tcs->last_break;
        tcs->last_switch = stmt;
        tcs->last_break = stmt;

        retval &= typecheck_stmt(tcs, stmt->switch_params.stmt);

        tcs->last_switch = switch_save;
        tcs->last_break = break_save;
        return retval;
    }

    case STMT_DO: {
        stmt_t *loop_save = tcs->last_loop;
        stmt_t *break_save = tcs->last_break;
        tcs->last_loop = stmt;
        tcs->last_break = stmt;

        retval &= typecheck_stmt(tcs, stmt->do_params.stmt);
        retval &= typecheck_expr_conditional(tcs, stmt->do_params.expr);

        tcs->last_loop = loop_save;
        tcs->last_break = break_save;
        return retval;
    }
    case STMT_WHILE: {
        retval &= typecheck_expr_conditional(tcs, stmt->while_params.expr);

        stmt_t *loop_save = tcs->last_loop;
        stmt_t *break_save = tcs->last_break;
        tcs->last_loop = stmt;
        tcs->last_break = stmt;

        retval &= typecheck_stmt(tcs, stmt->while_params.stmt);

        tcs->last_loop = loop_save;
        tcs->last_break = break_save;
        return retval;
    }
    case STMT_FOR:
        retval &= typecheck_expr(tcs, stmt->for_params.expr1, TC_NOCONST);
        retval &= typecheck_expr_conditional(tcs, stmt->for_params.expr2);
        retval &= typecheck_expr(tcs, stmt->for_params.expr3, TC_NOCONST);

        stmt_t *loop_save = tcs->last_loop;
        stmt_t *break_save = tcs->last_break;
        tcs->last_loop = stmt;
        tcs->last_break = stmt;

        retval &= typecheck_stmt(tcs, stmt->for_params.stmt);

        tcs->last_loop = loop_save;
        tcs->last_break = break_save;
        return retval;

    case STMT_GOTO:
        assert(tcs->func != NULL);
        sl_append(&tcs->func->fdefn.gotos, &stmt->goto_params.link);
        return retval;
    case STMT_CONTINUE:
        if (tcs->last_loop == NULL) {
            logger_log(&stmt->mark, "continue statement not within a loop",
                       LOG_ERR);
            retval = false;
        } else {
            stmt->continue_params.parent = tcs->last_loop;
        }
        return retval;
    case STMT_BREAK:
        if (tcs->last_break == NULL) {
            logger_log(&stmt->mark, "break statement not within loop or switch",
                       LOG_ERR);
            retval = false;
        } else {
            stmt->break_params.parent = tcs->last_loop;
        }
        return retval;
    case STMT_RETURN:
        retval &= typecheck_expr(tcs, stmt->return_params.expr, TC_NOCONST);
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

fail:
    return retval;
}

bool typecheck_decl(tc_state_t *tcs, decl_t *decl, bool constant) {
    bool retval = true;

    retval &= typecheck_type(tcs, decl->type);
    sl_link_t *cur;
    SL_FOREACH(cur, &decl->decls) {
        retval &= typecheck_decl_node(tcs, GET_ELEM(&decl->decls, cur),
                                      constant);
    }

    return retval;
}

bool typecheck_decl_node(tc_state_t *tcs, decl_node_t *decl_node,
                         bool constant) {
    bool retval = true;
    retval &= typecheck_type(tcs, decl_node->type);
    retval &= typecheck_expr(tcs, decl_node->expr, constant);
    // TODO: Make sure expression is compatible with type
    // if constant == TC_CONST, make sure they are compatible with int

    return retval;
}

bool typecheck_expr(tc_state_t *tcs, expr_t *expr, bool constant) {
    bool retval = true;
    switch(expr->type) {
    case EXPR_VOID:
        expr->etype = tt_void;
        return retval;
    case EXPR_PAREN:
        retval &= typecheck_expr(tcs, expr->paren_base, constant);
        expr->etype = expr->paren_base->etype;
        return retval;
    case EXPR_VAR: {
        if (constant == TC_CONST) {
            logger_log(&expr->mark, "Expected constant value", LOG_ERR);
            return false;
        }
        tt_key_t lookup = { expr->var_id, TT_VAR };
        typetab_entry_t *entry = tt_lookup(tcs->typetab, &lookup);
        if (entry == NULL) {
            snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE, "'%.*s' undeclared.",
                     (int)expr->var_id->len, expr->var_id->str);
            logger_log(&expr->mark, logger_fmt_buf, LOG_ERR);
            retval = false;
        }
        expr->etype = entry->type;
        return retval;
    }
    case EXPR_ASSIGN:
        retval &= typecheck_expr(tcs, expr->assign.dest, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->assign.expr, TC_NOCONST);
        // TODO: Make sure dest can be assigned to expr
        expr->etype = expr->assign.dest->etype;
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
        expr->etype = expr->bin.expr1->etype; // TODO: Temporary
        return retval;
    case EXPR_UNARY:
        retval &= typecheck_expr(tcs, expr->unary.expr, TC_NOCONST);
        // TODO: Make sure expr compatible with op
        expr->etype = expr->unary.expr->etype;
        return retval;
    case EXPR_COND:
        retval &= typecheck_expr_conditional(tcs, expr->cond.expr1);
        retval &= typecheck_expr(tcs, expr->cond.expr2, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->cond.expr3, TC_NOCONST);
        // TODO: Make sure expr1 can be true/false
        // TODO: Make sure expr2 and expr3 are same type, perform promotion
        expr->etype = expr->cond.expr1->etype; // TODO: Temporary
        return retval;
    case EXPR_CAST: {
        retval &= typecheck_expr(tcs, expr->cast.base, TC_NOCONST);
        decl_node_t *node = sl_head(&expr->cast.cast->decls);
        // TODO: make sure can be casted to type
        expr->etype = node->type;
        return retval;
    }
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
        while (cur_sig != NULL && cur_expr != NULL) {
            decl_t *decl = GET_ELEM(&func_sig->func.params, cur_sig);
            decl_node_t *param = sl_head(&decl->decls);
            expr_t *expr = GET_ELEM(&expr->call.params, cur_expr);
            retval &= typecheck_expr(tcs, expr, TC_NOCONST);

            // TODO: Make sure expr->etype and param->type are compatible
            (void)param;
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
        if (cur_expr != NULL) {
            logger_log(&expr->mark, "too many arguments to function", LOG_ERR);
            // TODO: print type
        }
        expr->etype = func_sig->func.type;
        return retval;
    }
    case EXPR_CMPD: {
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            retval &= typecheck_expr(tcs, GET_ELEM(&expr->cmpd.exprs, cur),
                                     TC_NOCONST);
        }
        expr_t *tail = sl_tail(&expr->cmpd.exprs);
        expr->etype = tail->etype;
        return retval;
    }
    case EXPR_SIZEOF:
        if (expr->sizeof_params.type != NULL) {
            retval &= typecheck_decl(tcs, expr->sizeof_params.type, TC_NOCONST);
        }
        if (expr->sizeof_params.expr != NULL) {
            retval &= typecheck_expr(tcs, expr->sizeof_params.expr, TC_NOCONST);
        }
        expr->etype = tt_long; // TODO: Fix this
        return retval;
    case EXPR_MEM_ACC:
        retval &= typecheck_expr(tcs, expr->mem_acc.base, TC_NOCONST);
        // TODO: Make sure compatible type with op, make sure member exists
        expr->etype = expr->mem_acc.base->etype; // TODO: Return member's type
        return retval;
    case EXPR_INIT_LIST: {
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->init_list.exprs) {
            retval &= typecheck_expr(tcs, GET_ELEM(&expr->init_list.exprs, cur),
                                     TC_NOCONST);
        }
        // TODO: This needs to be set by user of init list, and verified with
        // the exprs
        expr->etype = NULL;
        return retval;
    }
    default:
        assert(false);
    }

    return retval;
}

// TODO: Types may get typechecked multiple times, may need to add a mechanism
// to prevent this. For example: hashtable of type pointers inside tcs.
// We're only really afraid of compound types, so maybe only store those/or
// implement a policy decision
bool typecheck_type(tc_state_t *tcs, type_t *type) {
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
            retval &= typecheck_decl(tcs,
                                     GET_ELEM(&type->struct_params.decls, cur),
                                     TC_CONST);
        }
        return retval;
    }
    case TYPE_ENUM: {
        retval &= typecheck_type(tcs, type->enum_params.type);
        sl_link_t *cur;
        SL_FOREACH(cur, &type->enum_params.ids) {
            retval &= typecheck_decl_node(tcs,
                                          GET_ELEM(&type->enum_params.ids, cur),
                                          TC_CONST);
        }
        return retval;
    }

    case TYPE_TYPEDEF:
        return retval;

    case TYPE_MOD:
        retval &= typecheck_type(tcs, type->mod.base);
        if (type->mod.type_mod & TMOD_SIGNED &&
            type->mod.type_mod & TMOD_UNSIGNED) {
            logger_log(&type->mark,
                       "both ‘signed’ and ‘unsigned’ in declaration specifiers",
                       LOG_ERR);
            retval = false;
        }
        switch (type->mod.type_mod &
                (TMOD_AUTO | TMOD_REGISTER | TMOD_STATIC | TMOD_EXTERN)) {
        case TMOD_NONE:
        case TMOD_AUTO:
        case TMOD_REGISTER:
        case TMOD_STATIC:
        case TMOD_EXTERN:
            break;
        default:
            logger_log(&type->mark,
                       "multiple storage classes in declaration specifiers",
                       LOG_ERR);
            retval = false;
        }
        return retval;

    case TYPE_PAREN:
        return typecheck_type(tcs, type->paren_base);
    case TYPE_FUNC:
        retval &= typecheck_type(tcs, type->func.type);
        sl_link_t *cur;
        SL_FOREACH(cur, &type->func.params) {
            retval &= typecheck_decl(tcs, GET_ELEM(&type->func.params, cur),
                                     TC_CONST);
        }
        return retval;
    case TYPE_ARR:
        retval &= typecheck_type(tcs, type->arr.base);
        retval &= typecheck_expr(tcs, type->arr.len, TC_CONST);
        return retval;
    case TYPE_PTR:
        return typecheck_type(tcs, type->ptr.base);

    default:
        assert(false);
    }

    return retval;
}
