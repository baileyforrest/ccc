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
            *result = 0; // TODO: Get type size
        } else {
            assert(expr->sizeof_params.expr != NULL);
            *result = 0; // TODO: Get type size
        }
        return;
    case EXPR_ALIGNOF: {
        decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
        assert(node != NULL);
        *result = 0; // TODO: Get type align
        return;
    }
    case EXPR_VOID:
    case EXPR_VAR:
    case EXPR_ASSIGN:
    case EXPR_CONST_FLOAT:
    case EXPR_CONST_STR:
    case EXPR_CALL:
    case EXPR_CMPD:
    case EXPR_MEM_ACC:
    case EXPR_ARR_IDX:
    case EXPR_INIT_LIST:
    case EXPR_DESIG_INIT:
    default:
        assert(false);
    }
}

bool typecheck_type_equal(type_t *t1, type_t *t2) {
    t1 = typecheck_untypedef(t1);
    t2 = typecheck_untypedef(t2);

    if (t1 == t2) { // Pointers equal
        return true;
    }

    if (t1->type != t2->type) {
        return false;
    }

    switch (t1->type) {
    case TYPE_VOID:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
        assert(false && "Primitive types should have same adderss");
        return false;

    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_ENUM:
        // compound types which aren't the same address cannot be the same type
        return false;

    case TYPE_TYPEDEF:
        assert(false && "Should be untypedefed");
        return false;

    case TYPE_MOD:
        return (t1->mod.type_mod == t2->mod.type_mod) &&
            typecheck_type_equal(t1->mod.base, t2->mod.base);

    case TYPE_PAREN:
        assert(false && "Parens should be removed");
        return false;

    case TYPE_FUNC: {
        if (!typecheck_type_equal(t1->func.type, t2->func.type)) {
            return false;
        }
        sl_link_t *ptype1, *ptype2;
        while (ptype1 != NULL && ptype2 != NULL) {
            decl_t *decl1 = GET_ELEM(&t1->func.params, ptype1);
            decl_t *decl2 = GET_ELEM(&t2->func.params, ptype2);
            if (!typecheck_type_equal(decl1->type, decl2->type)) {
                return false;
            }
            ptype1 = ptype1->next;
            ptype2 = ptype2->next;
        }
        if (ptype1 != NULL || ptype2 != NULL) {
            return false;
        }

        return true;
    }

    case TYPE_ARR: {
        long long len1, len2;
        return typecheck_const_expr(t1->arr.len, &len1) &&
            typecheck_const_expr(t2->arr.len, &len2) &&
            (len1 == len2) && typecheck_type_equal(t1->arr.base, t2->arr.base);
    }
    case TYPE_PTR:
        return (t1->ptr.type_mod == t2->ptr.type_mod) &&
            typecheck_type_equal(t1->ptr.base, t2->ptr.base);
    }

    return true;
}

type_t *typecheck_untypedef(type_t *type) {
    bool done = false;
    while (!done) {
        switch (type->type) {
        case TYPE_TYPEDEF:
            type = type->typedef_params.base;
            break;
        case TYPE_PAREN:
            type = type->paren_base;
            break;
        default:
            done = true;
        }
    }

    return type;
}

type_t *typecheck_unmod(type_t *type) {
    if (type->type == TYPE_MOD) {
        type = type->mod.base;
    }

    return type;
}

bool typecheck_type_assignable(type_t *to, type_t *from) {
    // TODO: This
    (void)to;
    (void)from;
    return true;
}

bool typecheck_types_binop(oper_t op, type_t *t1, type_t *t2) {
    t1 = typecheck_untypedef(t1);
    t2 = typecheck_untypedef(t2);
    type_t *umod1 = typecheck_unmod(t1);
    type_t *umod2 = typecheck_unmod(t2);

    bool is_numeric1 = TYPE_IS_NUMERIC(umod1);
    bool is_numeric2 = TYPE_IS_NUMERIC(umod2);
    bool is_int1 = TYPE_IS_INTEGRAL(umod1);
    bool is_int2 = TYPE_IS_INTEGRAL(umod2);
    bool is_ptr1 = TYPE_IS_PTR(umod1);
    bool is_ptr2 = TYPE_IS_PTR(umod2);

    // If both are integer types, they can use any binaary operator
    if (is_int1 && is_int2) {
        return true;
    }

    switch (op) {
    case OP_TIMES:
    case OP_DIV:
        // times, div, only allow numeric operands
        return is_numeric1 && is_numeric2;

        // Require both operands to be integers, which was already checked
    case OP_BITAND:
    case OP_BITXOR:
    case OP_BITOR:
    case OP_MOD:
    case OP_LSHIFT:
    case OP_RSHIFT:
        return false;

    case OP_PLUS:
    case OP_MINUS:
        // Allow pointers to be added with integers
        return (is_ptr1 && is_int2) || (is_int1 && is_ptr2);

    case OP_LT:
    case OP_GT:
    case OP_LE:
    case OP_GE:
    case OP_EQ:
    case OP_NE:
    case OP_LOGICAND:
    case OP_LOGICOR:
        return (is_ptr1 && is_ptr2) || (is_ptr1 && is_int2) ||
            (is_int1 && is_ptr2);

    default:
        assert(false);
    }

    return true;
}

bool typecheck_type_unaryop(oper_t op, type_t *type) {
    bool is_numeric = TYPE_IS_NUMERIC(type);
    bool is_int = TYPE_IS_INTEGRAL(type);
    bool is_ptr = TYPE_IS_PTR(type);

    switch (op) {
    case OP_PREINC:
    case OP_POSTINC:
    case OP_PREDEC:
    case OP_POSTDEC:
        return is_numeric || is_int || is_ptr;

    case OP_ADDR:
        return true;

    case OP_DEREF:
        return is_ptr;

    case OP_UPLUS:
    case OP_UMINUS:
        return is_numeric;

    case OP_BITNOT:
        return is_int;

    case OP_LOGICNOT:
        return true;
    default:
        assert(false);
    }
}

bool typecheck_type_max(type_t *t1, type_t *t2, type_t **result) {
    t1 = typecheck_untypedef(t1);
    t2 = typecheck_untypedef(t2);

    if (typecheck_type_equal(t1, t2)) {
        *result = t1;
        return true;
    }

    type_t *umod1 = typecheck_unmod(t1);
    type_t *umod2 = typecheck_unmod(t2);

    bool is_numeric1 = TYPE_IS_NUMERIC(umod1);
    bool is_numeric2 = TYPE_IS_NUMERIC(umod2);
    bool is_int2 = TYPE_IS_INTEGRAL(umod2);
    bool is_ptr2 = TYPE_IS_PTR(umod2);

    if (is_numeric1 && is_numeric2) {
        if (umod1->type >= umod2->type) {
            *result = t1;
        } else {
            *result = t2;
        }
        return true;
    }

    switch (umod1->type) {
    case TYPE_VOID: // Void cannot be converted to anything
        return false;

    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
        if (is_ptr2) {
            *result = t2;
            return true;
        }
        return false;

    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_ENUM:
        // Compound types cannot be converted
        return false;

        // Only allowed to combine with integral types
    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:
        if (is_int2) {
            *result = t1;
            return true;
        }
        return false;
    default:
        assert(false);
    }
}

bool typecheck_type_integral(type_t *type) {
    switch (type->type) {
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
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
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:

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
        logger_log(&expr->mark, LOG_ERR, "Integral value required");
    }

    return retval;
}

bool typecheck_expr_conditional(tc_state_t *tcs, expr_t *expr) {
    bool retval = typecheck_expr(tcs, expr, TC_NOCONST);
    if (!(retval &= typecheck_type_conditional(expr->etype))) {
        logger_log(&expr->mark, LOG_ERR,
                   "used %s type value where scalar is required",
                   ast_basic_type_str(expr->etype->type));
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
                logger_log(&goto_stmt->mark, LOG_ERR,
                           "label %.*s used but not defined",
                           goto_stmt->goto_params.label->len,
                           goto_stmt->goto_params.label->str);
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
            logger_log(&stmt->mark, LOG_ERR,
                       "'case' label not within a switch statement");
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
            logger_log(&stmt->mark, LOG_ERR,
                       "'default' label not within a switch statement");
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
            logger_log(&stmt->mark, LOG_ERR,
                       "continue statement not within a loop");
            retval = false;
        } else {
            stmt->continue_params.parent = tcs->last_loop;
        }
        return retval;
    case STMT_BREAK:
        if (tcs->last_break == NULL) {
            logger_log(&stmt->mark, LOG_ERR,
                       "break statement not within loop or switch");
            retval = false;
        } else {
            stmt->break_params.parent = tcs->last_loop;
        }
        return retval;
    case STMT_RETURN:
        retval &= typecheck_expr(tcs, stmt->return_params.expr, TC_NOCONST);
        retval &= typecheck_type_assignable(tcs->func->decl->type,
                                            stmt->return_params.expr->etype);
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
    retval &= typecheck_type_assignable(decl_node->type,
                                        decl_node->expr->etype);
    return retval;
}

bool typecheck_expr(tc_state_t *tcs, expr_t *expr, bool constant) {
    bool retval = true;
    expr->etype = NULL;

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
            logger_log(&expr->mark, LOG_ERR, "Expected constant value");
            return false;
        }
        typetab_entry_t *entry = tt_lookup(tcs->typetab, expr->var_id);
        if (entry == NULL || entry->entry_type != TT_VAR) {
            logger_log(&expr->mark, LOG_ERR, "'%.*s' undeclared.",
                       expr->var_id->len, expr->var_id->str);
            return false;
        }
        expr->etype = entry->type;
        return retval;
    }

    case EXPR_ASSIGN:
        retval &= typecheck_expr(tcs, expr->assign.dest, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->assign.expr, TC_NOCONST);
        retval &= typecheck_type_assignable(expr->assign.dest->etype,
                                            expr->assign.expr->etype);
        expr->etype = expr->assign.dest->etype;
        return retval;

    case EXPR_CONST_INT:
        expr->etype = expr->const_val.type;
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
        retval &= typecheck_types_binop(expr->bin.op, expr->bin.expr1->etype,
                                        expr->bin.expr2->etype);
        retval &= typecheck_type_max(expr->bin.expr1->etype,
                                     expr->bin.expr2->etype,
                                     &expr->etype);
        return retval;

    case EXPR_UNARY:
        retval &= typecheck_expr(tcs, expr->unary.expr, TC_NOCONST);
        retval &= typecheck_type_unaryop(expr->unary.op,
                                         expr->unary.expr->etype);
        expr->etype = expr->unary.expr->etype;
        return retval;

    case EXPR_COND:
        retval &= typecheck_expr_conditional(tcs, expr->cond.expr1);
        retval &= typecheck_expr(tcs, expr->cond.expr2, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->cond.expr3, TC_NOCONST);
        retval &= typecheck_type_max(expr->cond.expr2->etype,
                                     expr->cond.expr3->etype,
                                     &expr->etype);
        return retval;

    case EXPR_CAST: {
        retval &= typecheck_expr(tcs, expr->cast.base, TC_NOCONST);
        decl_node_t *node = sl_head(&expr->cast.cast->decls);
        // TODO: make sure can be casted to type
        expr->etype = node->type;
        return retval;
    }

    case EXPR_CALL: {
        if (!(retval &= typecheck_expr(tcs, expr->call.func, TC_NOCONST))) {
            return false;
        }
        type_t *func_sig = expr->call.func->etype;
        if (func_sig->type != TYPE_FUNC) {
            logger_log(&expr->mark, LOG_ERR,
                       "Called object is not a function or function pointer");
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
            retval &= typecheck_type_assignable(param->type, expr->etype);
            if (false) {
                logger_log(&expr->mark, LOG_ERR,
                           "incompatible type for argument %d of function",
                           arg_num);
                ast_print_type(func_sig);
            }

            ++arg_num;
            cur_sig = cur_sig->next;
            cur_expr = cur_expr->next;
        }
        if (cur_sig != NULL) {
            logger_log(&expr->mark, LOG_ERR, "too few arguments to function");
            ast_print_type(func_sig);
        }
        if (cur_expr != NULL) {
            logger_log(&expr->mark, LOG_ERR, "too many arguments to function");
            ast_print_type(func_sig);
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
        expr->etype = tt_size_t;
        return retval;

    case EXPR_ALIGNOF:
        retval &= typecheck_decl(tcs, expr->alignof_params.type, TC_NOCONST);
        expr->etype = tt_size_t;
        break;

    case EXPR_MEM_ACC: {
        retval &= typecheck_expr(tcs, expr->mem_acc.base, TC_NOCONST);
        type_t *compound;
        switch (expr->mem_acc.base->etype->type) {
        case TYPE_STRUCT:
        case TYPE_UNION:
            if (expr->mem_acc.op != OP_DOT) {
                return false;
            }
            compound = expr->mem_acc.base->etype;
            break;
        case TYPE_PTR:
            if (expr->mem_acc.op != OP_ARROW) {
                return false;
            }
            compound = expr->mem_acc.base->etype->ptr.base;
            if (compound->type != TYPE_STRUCT && compound->type != TYPE_UNION) {
                return false;
            }
            break;
        default:
            return false;
        }
        sl_link_t *cur;
        SL_FOREACH(cur, &compound->struct_params.decls) {
            decl_node_t *decl = GET_ELEM(&compound->struct_params.decls, cur);
            if (vstrcmp(decl->id, expr->mem_acc.name)) {
                expr->etype = decl->type;
                return true;
            }
        }
        return false;
    }

    case EXPR_ARR_IDX: {
        retval &= typecheck_expr(tcs, expr->arr_idx.array, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->arr_idx.index, TC_NOCONST);
        type_t *umod_arr =
            typecheck_unmod(typecheck_untypedef(expr->arr_idx.array->etype));
        type_t *umod_index =
            typecheck_unmod(typecheck_untypedef(expr->arr_idx.index->etype));

        if (umod_arr->type != TYPE_PTR && umod_arr->type != TYPE_ARR) {
            logger_log(&expr->arr_idx.array->mark, LOG_ERR,
                       "subscripted value is neither array nor pointer nor"
                       " vector");
            retval = false;
        }
        if (!TYPE_IS_INTEGRAL(umod_index)) {
            logger_log(&expr->arr_idx.index->mark, LOG_ERR,
                       "array subscript is not an integer");
            retval = false;
        }

        return retval;
    }

    case EXPR_INIT_LIST: {
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->init_list.exprs) {
            retval &= typecheck_expr(tcs, GET_ELEM(&expr->init_list.exprs, cur),
                                     TC_NOCONST);
        }
        // TODO: This needs to be set by user of init list, and verified with
        // the exprs. Need to set etype
        return retval;
    }

    case EXPR_DESIG_INIT:
        // TODO: This
        break;
    default:
        assert(false);
    }

    return retval;
}

bool typecheck_type(tc_state_t *tcs, type_t *type) {
    bool retval = true;

    switch(type->type) {
    case TYPE_VOID:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
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
            logger_log(&type->mark, LOG_ERR,
                       "both ‘signed’ and ‘unsigned’ in declaration specifiers");
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
            logger_log(&type->mark, LOG_ERR,
                       "multiple storage classes in declaration specifiers");
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
