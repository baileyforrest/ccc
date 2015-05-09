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
 * Type checker implementation
 */

#include "typechecker.h"
#include "typechecker_priv.h"

#include <assert.h>
#include <stdio.h>

#include "util/logger.h"

void tc_state_init(tc_state_t *tcs) {
    sl_init(&tcs->etypes, offsetof(type_t, heap_link));
    tcs->tunit = NULL;
    tcs->typetab = NULL;
    tcs->func = NULL;
    tcs->last_switch = NULL;
    tcs->last_loop = NULL;
    tcs->last_break = NULL;
}

void tc_state_destroy(tc_state_t *tcs) {
    // Use free rather than ast_type_destroy because we don't want to
    // recursively free other nodes
    SL_DESTROY_FUNC(&tcs->etypes, free);
}

bool typecheck_ast(trans_unit_t *ast) {
    tc_state_t tcs;
    tc_state_init(&tcs);
    tcs.tunit = ast;
    bool retval = typecheck_trans_unit(&tcs, ast);
    tc_state_destroy(&tcs);
    return retval;
}

bool typecheck_const_expr(expr_t *expr, long long *result) {
    bool retval;
    tc_state_t tcs;
    tc_state_init(&tcs);
    if (typecheck_expr(&tcs, expr, TC_CONST)) {
        typecheck_const_expr_eval(tcs.typetab, expr, result);
        retval = true;
    } else {
        retval = false;
    }
    tc_state_destroy(&tcs);

    return retval;
}

void typecheck_const_expr_eval(typetab_t *typetab, expr_t *expr,
                               long long *result) {
    switch (expr->type) {
    case EXPR_PAREN:
        typecheck_const_expr_eval(typetab, expr->paren_base, result);
        return;
    case EXPR_CONST_INT:
        *result = expr->const_val.int_val;
        return;
    case EXPR_BIN: {
        long long temp1;
        long long temp2;
        typecheck_const_expr_eval(typetab, expr->bin.expr1, &temp1);
        typecheck_const_expr_eval(typetab, expr->bin.expr2, &temp2);
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
        typecheck_const_expr_eval(typetab, expr->unary.expr, &temp);
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
        typecheck_const_expr_eval(typetab, expr->cond.expr1, &temp);
        if (temp) {
            typecheck_const_expr_eval(typetab, expr->cond.expr2, result);
        } else {
            typecheck_const_expr_eval(typetab, expr->cond.expr3, result);
        }
        return;
    }
    case EXPR_CAST:
        typecheck_const_expr_eval(typetab, expr->cast.base, result);
        return;

    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        if (expr->sizeof_params.type != NULL) {
            type_t *type;
            decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
            if (node == NULL) {
                type = expr->sizeof_params.type->type;
            } else {
                type = node->type;
            }
            if (expr->type == EXPR_SIZEOF) {
                *result = ast_type_size(type);
            } else { // expr->type == EXPR_ALIGNOF
                *result = ast_type_align(type);
            }
        } else {
            assert(expr->sizeof_params.expr != NULL);
            if (expr->type == EXPR_SIZEOF) {
                *result = ast_type_size(expr->sizeof_params.expr->etype);
            } else { // expr->type == EXPR_ALIGNOF
                *result = ast_type_align(expr->sizeof_params.expr->etype);
            }
        }
        return;
    case EXPR_VAR: {
        typetab_entry_t *entry = tt_lookup(typetab, expr->var_id);
        if (entry->entry_type == TT_ENUM_ID) {
            *result = entry->enum_val;
            return;
        }
    }
    case EXPR_VOID:
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
    // Types which differ in these modifiers are still equal
#define IGNORE_MASK (~(TMOD_EXTERN | TMOD_TYPEDEF | TMOD_INLINE))

    t1 = ast_type_untypedef(t1);
    t2 = ast_type_untypedef(t2);

    // Remove any modifiers which do not effect equality
    while (t1->type == TYPE_MOD && (t1->mod.type_mod & IGNORE_MASK) == 0) {
        t1 = t1->mod.base;
        t1 = ast_type_untypedef(t1);
    }
    while (t2->type == TYPE_MOD && (t2->mod.type_mod & IGNORE_MASK) == 0) {
        t2 = t2->mod.base;
        t2 = ast_type_untypedef(t2);
    }

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
    case TYPE_VA_LIST:
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

    case TYPE_MOD: {
        return ((t1->mod.type_mod & IGNORE_MASK) ==
                (t2->mod.type_mod & IGNORE_MASK)) &&
            typecheck_type_equal(t1->mod.base, t2->mod.base);
    }

    case TYPE_PAREN:
        assert(false && "Parens should be removed");
        return false;

    case TYPE_FUNC: {
        if (!typecheck_type_equal(t1->func.type, t2->func.type)) {
            return false;
        }
        sl_link_t *ptype1 = t1->func.params.head;
        sl_link_t *ptype2 = t2->func.params.head;
        while (ptype1 != NULL && ptype2 != NULL) {
            decl_t *decl1 = GET_ELEM(&t1->func.params, ptype1);
            decl_t *decl2 = GET_ELEM(&t2->func.params, ptype2);
            decl_node_t *node1 = sl_head(&decl1->decls);
            decl_node_t *node2 = sl_head(&decl2->decls);
            assert(node1 == sl_tail(&decl1->decls));
            assert(node2 == sl_tail(&decl2->decls));
            if ((node1 == NULL && node2 != NULL) ||
                (node1 != NULL && node2 == NULL)) {
                return false;
            }
            if (node1 == NULL) {
                if (!typecheck_type_equal(decl1->type, decl1->type)) {
                    return false;
                }
            } else {
                if (!typecheck_type_equal(node1->type, node2->type)) {
                    return false;
                }
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
    default:
        assert(false);
    }

    return true;
}

bool typecheck_expr_lvalue(tc_state_t *tcs, expr_t *expr) {
    switch (expr->type) {
    case EXPR_PAREN:
        return typecheck_expr_lvalue(tcs, expr->paren_base);

    case EXPR_MEM_ACC:
    case EXPR_ARR_IDX:
    case EXPR_VAR:
        return true;

    case EXPR_UNARY:
        switch (expr->unary.op) {
        case OP_PREINC:
        case OP_POSTINC:
        case OP_PREDEC:
        case OP_POSTDEC:
            return typecheck_expr_lvalue(tcs, expr->unary.expr);
        case OP_DEREF:
            return true;
        default:
            break;
        }
        break;
    case EXPR_CMPD: {
        expr_t *last = sl_tail(&expr->cmpd.exprs);
        return typecheck_expr_lvalue(tcs, last);
    }
    default:
        break;
    }
    logger_log(&expr->mark, LOG_ERR,
               "lvalue required as left operand of assignment");
    return false;
}

bool typecheck_type_assignable(fmark_t *mark, type_t *to, type_t *from) {
    to = ast_type_untypedef(to);
    from = ast_type_untypedef(from);

    type_t *umod_to = ast_type_unmod(to);
    type_t *umod_from = ast_type_unmod(from);

    if (umod_to->type == TYPE_VOID) {
        if (mark != NULL) {
            logger_log(mark, LOG_ERR, "invalid use of void expression");
        }
        return false;
    }

    if (umod_from->type == TYPE_VOID) {
        if (mark != NULL) {
            logger_log(mark, LOG_ERR,
                       "void value not ignored as it ought to be");
        }
        return false;
    }

    // TODO1: This ignores const
    if (typecheck_type_equal(umod_to, umod_from)) {
        return true;
    }


    if (umod_from->type == TYPE_STRUCT || umod_from->type == TYPE_UNION) {
        goto fail;
    }

    bool is_num_from = TYPE_IS_NUMERIC(umod_from);
    bool is_int_from = TYPE_IS_INTEGRAL(umod_from);
    bool is_ptr_from = TYPE_IS_PTR(umod_from);

    switch (umod_to->type) {
    case TYPE_VOID:
        if (mark != NULL) {
            logger_log(mark, LOG_ERR, "can't assign to void");
        }
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
        if (is_num_from || umod_from->type == TYPE_ENUM) {
            return true;
        }

        if (is_ptr_from) {
            if (mark != NULL) {
                logger_log(mark, LOG_WARN, "initialization makes integer from"
                           " pointer without a cast");
            }
            return true;
        }

        goto fail;

    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_VA_LIST:
        goto fail;

    case TYPE_ENUM:
        if (is_num_from) {
            return true;
        }
        goto fail;

    case TYPE_ARR:
        if (umod_from->type == TYPE_PTR &&
            typecheck_type_assignable(mark, umod_to->arr.base,
                                      umod_from->ptr.base)) {
            return true;
        }
        if (mark != NULL) {
            logger_log(mark, LOG_ERR,
                       "assignment to expression with array type");
        }
        return false;
    case TYPE_PTR:
        if (is_int_from) {
            return true;
        }

        // Can assign any pointer type to void *
        if (umod_to->ptr.base->type == TYPE_VOID && is_ptr_from) {
            return true;
        }

        switch (umod_from->type) {
        case TYPE_FUNC:
            if (typecheck_type_equal(umod_to->ptr.base, umod_from)) {
                return true;
            }
            break;
        case TYPE_ARR:
            if (typecheck_type_assignable(mark, umod_to->ptr.base,
                                          umod_from->arr.base)) {
                return true;
            }
            break;
        case TYPE_PTR:
            // Can freely assign to/from void *
            if (ast_type_unmod(umod_to->ptr.base)->type == TYPE_VOID ||
                ast_type_unmod(umod_from->ptr.base)->type == TYPE_VOID) {
                return true;
            }
            if (typecheck_type_assignable(mark, umod_to->ptr.base,
                                          umod_from->ptr.base)) {
                return true;
            }
            break;
        default:
            break;
        }

        goto fail;
    default:
        assert(false);
    }

fail:
    if (mark != NULL) {
        logger_log(mark, LOG_ERR,
                   "incompatible types when assigning");
    }
    return false;
}

bool typecheck_types_binop(fmark_t *mark, oper_t op, type_t *t1, type_t *t2) {
    t1 = ast_type_untypedef(t1);
    t2 = ast_type_untypedef(t2);
    type_t *umod1 = ast_type_unmod(t1);
    type_t *umod2 = ast_type_unmod(t2);

    bool is_numeric1 = TYPE_IS_NUMERIC(umod1) || umod1->type == TYPE_ENUM;
    bool is_numeric2 = TYPE_IS_NUMERIC(umod2) || umod2->type == TYPE_ENUM;
    bool is_int1 = TYPE_IS_INTEGRAL(umod1) || umod1->type == TYPE_ENUM;
    bool is_int2 = TYPE_IS_INTEGRAL(umod2) || umod2->type == TYPE_ENUM;
    bool is_ptr1 = TYPE_IS_PTR(umod1);
    bool is_ptr2 = TYPE_IS_PTR(umod2);

    // If both are integer types, they can use any binary operator
    if (is_int1 && is_int2) {
        return true;
    }

    switch (op) {
    case OP_TIMES:
    case OP_DIV:
        // times, div, only allow numeric operands
        if (is_numeric1 && is_numeric2) {
            return true;
        }
        break;

    case OP_BITAND:
    case OP_BITXOR:
    case OP_BITOR:
    case OP_MOD:
    case OP_LSHIFT:
    case OP_RSHIFT:
        // Require both operands to be integers, which was already checked
        break;

    case OP_MINUS:
        if (is_ptr1 && is_ptr2) {
            type_t *umod_base1 = ast_type_unmod(typecheck_get_ptr_base(umod1));
            type_t *umod_base2 = ast_type_unmod(typecheck_get_ptr_base(umod2));
            if (typecheck_type_equal(umod_base1, umod_base2)) {
                return true;
            }
        }
        // FALL THROUGH

    case OP_PLUS:
        if (is_numeric1 && is_numeric2) {
            return true;
        }
        // Allow pointers to be added with integers
        if ((is_ptr1 && is_int2) || (is_int1 && is_ptr2)) {
            return true;
        }

        break;

    case OP_LT:
    case OP_GT:
    case OP_LE:
    case OP_GE:
    case OP_EQ:
    case OP_NE:
    case OP_LOGICAND:
    case OP_LOGICOR:
        // Two integral types allowed
        if (is_numeric1 && is_numeric2) {
            return true;
        }
        // Allow combinations of pointers and ints
        if((is_ptr1 && is_ptr2) || (is_ptr1 && is_int2) ||
           (is_int1 && is_ptr2)) {
            return true;
        }
        break;

    default:
        assert(false);
    }

    logger_log(mark, LOG_ERR, "invalid operands to binary %s",
               ast_oper_str(op));
    return false;
}

bool typecheck_type_unaryop(fmark_t *mark, oper_t op, type_t *type) {
    type = ast_type_unmod(type);
    bool is_numeric = TYPE_IS_NUMERIC(type);
    bool is_int = TYPE_IS_INTEGRAL(type);
    bool is_ptr = TYPE_IS_PTR(type);

    switch (op) {
    case OP_PREINC:
    case OP_POSTINC:
    case OP_PREDEC:
    case OP_POSTDEC:
        if (is_numeric || is_int || is_ptr) {
            return true;
        }
        break;

    case OP_ADDR:
        // Can take the address of anything
        return true;

    case OP_DEREF:
        if (is_ptr) {
            return true;
        }
        break;

    case OP_UPLUS:
    case OP_UMINUS:
        if (is_numeric) {
            return true;
        }
        break;

    case OP_BITNOT:
        if (is_int) {
            return true;
        }
        break;

    case OP_LOGICNOT:
        if (is_numeric || is_int || is_ptr || type->type == TYPE_ENUM) {
            return true;
        }
        break;
    default:
        assert(false);
    }

    logger_log(mark, LOG_ERR, "invalid operand to operator %s",
               ast_oper_str(op));
    return false;
}

type_t *typecheck_get_ptr_base(type_t *t1) {
    switch (t1->type) {
    case TYPE_FUNC: return t1;
    case TYPE_PTR:  return t1->ptr.base;
    case TYPE_ARR:  return t1->arr.base;
    default:
        assert(false);
    }
    return NULL;
}

bool typecheck_type_max(fmark_t *mark, type_t *t1, type_t *t2,
                        type_t **result) {
    t1 = ast_type_untypedef(t1);
    t2 = ast_type_untypedef(t2);

    if (typecheck_type_equal(t1, t2)) {
        *result = t1;
        return true;
    }

    type_t *umod1 = ast_type_unmod(t1);
    type_t *umod2 = ast_type_unmod(t2);

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
        break;

    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
        if (umod2->type == TYPE_ENUM) {
            *result = t1;
            return true;
        }

        if (is_ptr2) {
            *result = t2;
            return true;
        }
        break;

    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_VA_LIST:
        // Compound types cannot be converted
        break;

    case TYPE_ENUM:
        if (umod2->type == TYPE_ENUM) {
            *result = t1;
            return true;
        }
        if (is_int2) {
            *result = t2;
            return true;
        }
        break;

        // Only allowed to combine with integral types
    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:
        if (is_int2) {
            *result = t1;
            return true;
        }

        // If both are pointers, and one is a void *, return the other
        if (umod2->type == TYPE_PTR &&
            ast_type_unmod(umod2->ptr.base)->type == TYPE_VOID) {
            *result = t1;
            return true;
        }

        if (is_ptr2 && umod1->type == TYPE_PTR &&
            ast_type_unmod(umod1->ptr.base)->type == TYPE_VOID) {
            *result = t2;
            return true;
        }

        if (is_ptr2) {
            type_t *umod_base1 = ast_type_unmod(typecheck_get_ptr_base(umod1));
            type_t *umod_base2 = ast_type_unmod(typecheck_get_ptr_base(umod2));
            if (typecheck_type_equal(umod_base1, umod_base2)) {
                *result = t1;
                return true;
            }
        }
        break;
    default:
        assert(false);
    }

    logger_log(mark, LOG_ERR, "Incompatable types");
    return false;
}

bool typecheck_type_cast(fmark_t *mark, type_t *to, type_t *from) {
    to = ast_type_untypedef(to);
    from = ast_type_untypedef(from);

    if (typecheck_type_equal(to, from)) {
        return true;
    }

    // Anything can be casted to void
    if (to->type == TYPE_VOID) {
        return true;
    }

    type_t *umod_to = ast_type_unmod(to);
    type_t *umod_from = ast_type_unmod(from);

    // Can't cast to struct/union types
    if (umod_to->type == TYPE_STRUCT || umod_to->type == TYPE_UNION) {
        logger_log(mark, LOG_ERR, "conversion to non-scalar type requested");
        return false;
    }
    // Can't cast from struct/union types
    if (umod_from->type == TYPE_STRUCT || umod_from->type == TYPE_UNION) {
        logger_log(mark, LOG_ERR, "conversion from non-scalar type requested");
        return false;
    }

    return true;
}

bool typecheck_type_integral(fmark_t *mark, type_t *type) {
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
        return typecheck_type_integral(mark, type->typedef_params.base);
    case TYPE_MOD:
        return typecheck_type_integral(mark, type->mod.base);
    case TYPE_PAREN:
        return typecheck_type_integral(mark, type->paren_base);

    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:

    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:

    case TYPE_VA_LIST:
        break;

    default:
        assert(false);
    }

    logger_log(mark, LOG_ERR, "integral type required");
    return false;
}

bool typecheck_type_conditional(fmark_t *mark, type_t *type) {
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
    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:
        return true;

    case TYPE_TYPEDEF:
        return typecheck_type_conditional(mark, type->typedef_params.base);
    case TYPE_MOD:
        return typecheck_type_conditional(mark, type->mod.base);
    case TYPE_PAREN:
        return typecheck_type_conditional(mark, type->paren_base);

    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_VA_LIST:
        break;

    default:
        assert(false);
    }

    logger_log(mark, LOG_ERR, "conditional type required");
    return false;
}

bool typecheck_expr_integral(tc_state_t *tcs, expr_t *expr) {
    if (!typecheck_expr(tcs, expr, TC_NOCONST)) {
        return false;
    }
    return typecheck_type_integral(&expr->mark, expr->etype);
}

bool typecheck_expr_conditional(tc_state_t *tcs, expr_t *expr) {
    if (!typecheck_expr(tcs, expr, TC_NOCONST)) {
        return false;
    }
    return typecheck_type_conditional(&expr->mark, expr->etype);
}

bool typecheck_expr_va_list(tc_state_t *tcs, expr_t *expr) {
    if (!typecheck_expr(tcs, expr, TC_NOCONST)) {
        return false;
    }
    type_t *type = ast_type_unmod(expr->etype);
    if (type->type != TYPE_VA_LIST) {
        logger_log(&expr->mark, LOG_ERR, "Expected __builtin_va_list");
        return false;
    }
    return true;
}

bool typecheck_trans_unit(tc_state_t *tcs, trans_unit_t *trans_unit) {
    typetab_t *save_tab = tcs->typetab;
    tcs->typetab = &trans_unit->typetab;
    bool retval = true;

    SL_FOREACH(cur, &trans_unit->gdecls) {
        retval &= typecheck_gdecl(tcs, GET_ELEM(&trans_unit->gdecls, cur));
    }

    tcs->typetab = save_tab;
    return retval;
}

bool typecheck_gdecl(tc_state_t *tcs, gdecl_t *gdecl) {
    bool retval = true;

    switch (gdecl->type) {
    case GDECL_FDEFN: {
        gdecl_t *func_save = tcs->func;
        assert(func_save == NULL); // Can't have nested functions in C
        tcs->func = gdecl;

        retval &= typecheck_decl(tcs, gdecl->decl, TYPE_VOID);
        retval &= typecheck_stmt(tcs, gdecl->fdefn.stmt);
        SL_FOREACH(cur, &gdecl->fdefn.gotos) {
            stmt_t *goto_stmt = GET_ELEM(&gdecl->fdefn.gotos, cur);
            stmt_t *label = ht_lookup(&tcs->func->fdefn.labels,
                                      &goto_stmt->goto_params.label);
            if (label == NULL) {
                logger_log(&goto_stmt->mark, LOG_ERR,
                           "label %s used but not defined",
                           goto_stmt->goto_params.label);
                retval = false;
            }
        }

        // Restore old state
        tcs->func = func_save;
        break;
    }
    case GDECL_DECL:
        retval &= typecheck_decl(tcs, gdecl->decl, TYPE_VOID);
        break;
    default:
        assert(false);
        retval = false;
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
        return typecheck_decl(tcs, stmt->decl, TYPE_VOID);

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
    case STMT_FOR: {

        stmt_t *loop_save = tcs->last_loop;
        stmt_t *break_save = tcs->last_break;
        typetab_t *last_tab = tcs->typetab;
        tcs->last_loop = stmt;
        tcs->last_break = stmt;
        if (stmt->for_params.typetab != NULL) {
            tcs->typetab = stmt->for_params.typetab;
        }

        if (stmt->for_params.expr1 != NULL) {
            retval &= typecheck_expr(tcs, stmt->for_params.expr1, TC_NOCONST);
        }
        if (stmt->for_params.decl1 != NULL) {
            retval &= typecheck_decl(tcs, stmt->for_params.decl1, TYPE_VOID);
        }
        if (stmt->for_params.expr2 != NULL) {
            retval &= typecheck_expr_conditional(tcs, stmt->for_params.expr2);
        }
        if (stmt->for_params.expr3 != NULL) {
            retval &= typecheck_expr(tcs, stmt->for_params.expr3, TC_NOCONST);
        }

        retval &= typecheck_stmt(tcs, stmt->for_params.stmt);

        // Restore state
        tcs->typetab = last_tab;
        tcs->last_loop = loop_save;
        tcs->last_break = break_save;
        return retval;
    }

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
    case STMT_RETURN: {
        decl_node_t *func_sig = sl_head(&tcs->func->decl->decls);
        assert(func_sig->type->type == TYPE_FUNC);

        if (stmt->return_params.expr == NULL) {
            if (ast_type_unmod(func_sig->type->func.type)->type != TYPE_VOID) {
                logger_log(&stmt->mark, LOG_WARN,
                           "'return' with no value, in function returning"
                           " non-void");
            }
        } else {
            if (!typecheck_expr(tcs, stmt->return_params.expr, TC_NOCONST)) {
                return false;
            }
            retval &=
                typecheck_type_assignable(&stmt->mark,
                                          func_sig->type->func.type,
                                          stmt->return_params.expr->etype);
            stmt->return_params.type = func_sig->type->func.type;
        }
        return retval;
    }

    case STMT_COMPOUND: {
        // Enter new scope
        typetab_t *save_tab = tcs->typetab;
        tcs->typetab = &stmt->compound.typetab;

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

bool typecheck_decl(tc_state_t *tcs, decl_t *decl, type_type_t type) {
    bool retval = true;

    retval &= typecheck_type(tcs, decl->type);

    if (decl->type->type == TYPE_MOD &&
        decl->type->mod.type_mod & TMOD_TYPEDEF) {
        // Don't need to typecheck decl nodes if its a typedef
        return retval;
    }
    SL_FOREACH(cur, &decl->decls) {
        retval &= typecheck_decl_node(tcs, GET_ELEM(&decl->decls, cur), type);
    }

    return retval;
}


bool typecheck_init_list(tc_state_t *tcs, type_t *type, expr_t *expr) {
    assert(expr->type == EXPR_INIT_LIST);
    bool retval = true;
    type = ast_type_unmod(type);
    expr->etype = type;

    switch (type->type) {
    case TYPE_UNION: {
        ast_canonicalize_init_list(tcs->tunit, type, expr);

        expr_t *head;
        type_t *dest_type = ast_get_union_type(type, expr, &head);
        if (head == NULL) {
            return true;
        }

        if (head->type == EXPR_INIT_LIST) {
            retval &= typecheck_init_list(tcs, dest_type, head);
        } else {
            retval &= typecheck_expr(tcs, head, TC_NOCONST);
            if (!retval) {
                return false;
            }

            retval &= typecheck_type_assignable(&head->mark, dest_type,
                                                head->etype);
        }
        return retval;
    }
    case TYPE_STRUCT: {
        ast_canonicalize_init_list(tcs->tunit, type, expr);
        struct_iter_t iter;
        struct_iter_init(type, &iter);

        SL_FOREACH(cur, &expr->init_list.exprs) {
            expr_t *cur_expr = GET_ELEM(&expr->init_list.exprs, cur);

            // Skip anonymous members that can't be struct/union
            while (iter.node != NULL && iter.node->id == NULL) {
                struct_iter_advance(&iter);
            }

            if (cur_expr->type == EXPR_VOID) {
                struct_iter_advance(&iter);
                continue;
            }
            if (iter.node != NULL && iter.node->id == NULL) {
                if (cur_expr->type == EXPR_INIT_LIST) {
                    retval &= typecheck_init_list(tcs, iter.node->type,
                                                  cur_expr);
                } else {
                    retval &= typecheck_expr(tcs, cur_expr, TC_NOCONST);
                    if (!retval) {
                        return false;
                    }
                    retval &= typecheck_type_assignable(&iter.node->mark,
                                                        iter.node->type,
                                                        cur_expr->etype);
                }
            }

            if (iter.node == NULL && iter.decl != NULL &&
                (iter.decl->type->type == TYPE_STRUCT ||
                 iter.decl->type->type == TYPE_UNION)) {
                assert(cur_expr->type == EXPR_INIT_LIST);
                retval &= typecheck_init_list(tcs, iter.decl->type, cur_expr);
            }

            struct_iter_advance(&iter);
        }

        return retval;
    }
    case TYPE_ARR: {
        long long decl_len = -1;
        if (type->arr.len != NULL) {
            if (!typecheck_expr(tcs, type->arr.len, TC_CONST)) {
                return false;
            }
            typecheck_const_expr_eval(tcs->typetab, type->arr.len, &decl_len);
        }

        long len = 0;
        SL_FOREACH(cur, &expr->init_list.exprs) {
            ++len;
            expr_t *cur_expr = GET_ELEM(&expr->init_list.exprs, cur);
            retval &= typecheck_expr(tcs, cur_expr, TC_NOCONST);
            if (cur_expr->type == EXPR_INIT_LIST) {
                retval &= typecheck_init_list(tcs, type->arr.base,
                                              cur_expr);
            } else {
                retval &= typecheck_type_assignable(&cur_expr->mark,
                                                    type->arr.base,
                                                    cur_expr->etype);
            }
        }

        if (decl_len >= 0 && decl_len < len) {
            logger_log(&expr->mark, LOG_WARN,
                       "excess elements in array initializer");
        }
        return retval;
    }
    default: {
        if (sl_head(&expr->init_list.exprs) == NULL) {
            logger_log(&expr->arr_idx.index->mark, LOG_ERR,
                       "empty scalar initializer");
            return false;
        }
        if (sl_head(&expr->init_list.exprs) !=
            sl_tail(&expr->init_list.exprs)) {
            logger_log(&expr->arr_idx.index->mark, LOG_WARN,
                       "excess elements in scalar initializer");
        }
        expr_t *first = sl_head(&expr->init_list.exprs);
        retval &= typecheck_expr(tcs, first, TC_NOCONST);
        retval &= typecheck_type_assignable(&first->mark, type, first->etype);
    }
    }
    return retval;
}

bool typecheck_decl_node(tc_state_t *tcs, decl_node_t *decl_node,
                         type_type_t type) {
    bool retval = true;
    retval &= typecheck_type(tcs, decl_node->type);
    type_t *node_type = ast_type_untypedef(decl_node->type);
    type_t *unmod = ast_type_unmod(node_type);

    // TODO1: Incomplete types are allowed for function declarations
    if ((unmod->type == TYPE_STRUCT || unmod->type == TYPE_UNION) &&
        unmod->struct_params.esize == (size_t)-1 &&
        !(node_type->type == TYPE_MOD &&
          node_type->mod.type_mod & TMOD_EXTERN)) {
        logger_log(&decl_node->mark, LOG_ERR,
                   "storage size of '%s' isn't known", decl_node->id);
        return false;
    }

    if (unmod->type == TYPE_VOID) {
        logger_log(&decl_node->mark, LOG_ERR,
                   "variable or field '%s' declared void", decl_node->id);
        return false;
    }
    if ((type == TYPE_STRUCT || type == TYPE_UNION) &&
        (unmod->type == TYPE_STRUCT || unmod->type == TYPE_UNION) &&
        unmod->struct_params.esize == (size_t)-1) {
        logger_log(&decl_node->mark, LOG_ERR, "field '%s' has incomplete type",
                   decl_node->id);
        return false;
    }

    if (type == TYPE_VOID && decl_node->id != NULL) {
        status_t status;
        typetab_entry_t *entry = NULL;
        type_t *type_base = node_type;
        while (type_base->type == TYPE_PTR || type_base->type == TYPE_ARR) {
            type_base = typecheck_get_ptr_base(type_base);
        }

        // Not a definiton if its a variable with extern, or a function
        // declaration not equal to the function we're currently in
        bool is_decl = ((type_base->type == TYPE_MOD &&
                         type_base->mod.type_mod & TMOD_EXTERN) ||
                        (node_type->type == TYPE_FUNC &&
                         (tcs->func == NULL ||
                          decl_node != sl_head(&tcs->func->decl->decls))));

        if (CCC_OK !=
            (status =
             tt_insert(tcs->typetab, node_type, TT_VAR, decl_node->id,
                       &entry))) {
            if (status != CCC_DUPLICATE) {
                logger_log(&decl_node->mark, LOG_ERR,
                           "Failure inserting to type table");
                return false;
            } else {
                entry = tt_lookup(tcs->typetab, decl_node->id);

                // If existing type is extern, compare with the type
                // without extern
                type_t *cmp_type;
                if (entry->type->type == TYPE_MOD &&
                    entry->type->mod.type_mod == TMOD_EXTERN) {
                    cmp_type = entry->type->mod.base;
                } else {
                    cmp_type = entry->type;
                }
                // Redefined if old entry is not a variable, last variable was
                // already defined, and this is a definiton, or if the
                // types are not equal
                if (entry->entry_type != TT_VAR ||
                    (entry->var.var_defined && !is_decl) ||
                    !typecheck_type_equal(cmp_type, node_type)) {
                    logger_log(&decl_node->mark, LOG_ERR,
                               "Redefined symbol %s", decl_node->id);
                    return false;
                }
            }
        } else {
            entry->var.var_defined = !is_decl;
        }
    }
    if (decl_node->expr != NULL) {
        switch (type) {
        case TYPE_VOID:
            retval &= typecheck_expr(tcs, decl_node->expr, TC_NOCONST);
            if (!retval) {
                return false;
            }

            switch (decl_node->expr->type) {
            case EXPR_DESIG_INIT: // This should not parse
                assert(false);
                return false;
            case EXPR_INIT_LIST:
                retval &= typecheck_init_list(tcs, node_type,
                                              decl_node->expr);

                // If we're assigning to an array without a size, set the
                // array's size to the init list length
                if (node_type->type == TYPE_ARR &&
                    node_type->arr.len == NULL) {
                    node_type->arr.nelems =
                        decl_node->expr->init_list.nelems;
                }
                break;
            default:
                retval &= typecheck_type_assignable(&decl_node->mark, node_type,
                                                    decl_node->expr->etype);
            }
            break;
        case TYPE_STRUCT:
        case TYPE_UNION:
        case TYPE_ENUM: {
            retval &= typecheck_expr(tcs, decl_node->expr, TC_CONST);
            if (!retval) {
                return false;
            }

            type_t *type = decl_node->expr->etype;
            type = ast_type_unmod(type);
            if (!TYPE_IS_INTEGRAL(type)) {
                logger_log(&decl_node->mark, LOG_ERR,
                           "bit-field '%s' width not an integer constant",
                           decl_node->id);
                return false;
            }
            break;
        }
        default:
            assert(false);
            return false;
        }
    }
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
        typetab_entry_t *entry = tt_lookup(tcs->typetab, expr->var_id);
        if (entry == NULL ||
            (entry->entry_type != TT_VAR && entry->entry_type != TT_ENUM_ID)) {
            logger_log(&expr->mark, LOG_ERR, "'%s' undeclared.", expr->var_id);
            return false;
        }
        if (constant == TC_CONST && entry->entry_type == TT_VAR) {
            logger_log(&expr->mark, LOG_ERR, "Expected constant value");
            return false;
        }
        expr->etype = entry->type;
        return retval;
    }

    case EXPR_ASSIGN:
        retval &= typecheck_expr(tcs, expr->assign.dest, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->assign.expr, TC_NOCONST);
        if (!retval) {
            return false;
        }
        retval &= typecheck_expr_lvalue(tcs, expr->assign.dest);
        retval &= typecheck_type_assignable(&expr->assign.dest->mark,
                                            expr->assign.dest->etype,
                                            expr->assign.expr->etype);
        if (expr->assign.op != OP_NOP) {
            retval &= typecheck_types_binop(&expr->mark, expr->assign.op,
                                            expr->assign.dest->etype,
                                            expr->assign.expr->etype);
        }
        expr->etype = expr->assign.dest->etype;
        return retval;

    case EXPR_CONST_INT:
    case EXPR_CONST_FLOAT:
    case EXPR_CONST_STR:
        expr->etype = expr->const_val.type;
        return retval;

    case EXPR_BIN:
        retval &= typecheck_expr(tcs, expr->bin.expr1, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->bin.expr2, TC_NOCONST);
        if (!retval) {
            return false;
        }
        retval &= typecheck_types_binop(&expr->mark, expr->bin.op,
                                        expr->bin.expr1->etype,
                                        expr->bin.expr2->etype);
        switch (expr->bin.op) {
        case OP_LT:
        case OP_GT:
        case OP_LE:
        case OP_GE:
        case OP_EQ:
        case OP_NE:
        case OP_LOGICAND:
        case OP_LOGICOR:
            expr->etype = tt_bool;
            break;
        case OP_MINUS:
            // subtracting pointers evaluates to size_t
            if (ast_type_unmod(expr->bin.expr1->etype)->type == TYPE_PTR &&
                ast_type_unmod(expr->bin.expr2->etype)->type == TYPE_PTR) {
                expr->etype = tt_size_t;
                break;
            }
            // FALL THROUGH
        default:
            retval &= typecheck_type_max(&expr->mark, expr->bin.expr1->etype,
                                         expr->bin.expr2->etype,
                                         &expr->etype);
        }
        return retval;

    case EXPR_UNARY:
        if (!typecheck_expr(tcs, expr->unary.expr, TC_NOCONST)) {
            return false;
        }
        if (!typecheck_type_unaryop(&expr->mark, expr->unary.op,
                                    expr->unary.expr->etype)) {
            return false;
        }
        switch (expr->unary.op) {
        case OP_ADDR:
            if (!typecheck_expr_lvalue(tcs, expr->unary.expr)) {
                return false;
            }
            expr->etype = emalloc(sizeof(type_t));
            // If we have a translation unit, save etypes on it because we'll
            // need them later
            if (tcs->tunit != NULL) {
                sl_append(&tcs->tunit->types, &expr->etype->heap_link);
            } else {
                sl_append(&tcs->etypes, &expr->etype->heap_link);
            }
            memcpy(&expr->etype->mark, &expr->mark, sizeof(fmark_t));

            expr->etype->type = TYPE_PTR;
            expr->etype->ptr.type_mod = TMOD_NONE;
            expr->etype->ptr.base = expr->unary.expr->etype;
            break;
        case OP_DEREF: {
            type_t *ptr_type = ast_type_unmod(expr->unary.expr->etype);
            assert(ptr_type->type == TYPE_PTR);
            type_t *unmod = ast_type_unmod(ptr_type->ptr.base);
            if (unmod->type == TYPE_VOID) {
                logger_log(&expr->mark, LOG_WARN,
                           "dereferencing a 'void *' pointer");
            }
            if ((unmod->type == TYPE_STRUCT || unmod->type == TYPE_VOID) &&
                unmod->struct_params.esize == (size_t)-1) {
                logger_log(&expr->mark, LOG_ERR,
                           "dereferencing pointer to incomplete type");
                retval = false;
            }
            expr->etype = ptr_type->ptr.base;
            break;
        }
        case OP_LOGICNOT:
            expr->etype = tt_bool;
            break;
        default:
            expr->etype = expr->unary.expr->etype;
        }
        return retval;

    case EXPR_COND:
        retval &= typecheck_expr_conditional(tcs, expr->cond.expr1);
        retval &= typecheck_expr(tcs, expr->cond.expr2, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->cond.expr3, TC_NOCONST);
        if (!retval) {
            return false;
        }
        if (ast_type_unmod(expr->cond.expr2->etype)->type == TYPE_VOID ||
            ast_type_unmod(expr->cond.expr3->etype)->type == TYPE_VOID) {
            expr->etype = tt_void;
        } else {
            retval &= typecheck_type_max(&expr->mark, expr->cond.expr2->etype,
                                         expr->cond.expr3->etype,
                                         &expr->etype);
        }
        return retval;

    case EXPR_CAST: {
        if (!typecheck_expr(tcs, expr->cast.base, TC_NOCONST)) {
            return false;
        }
        decl_node_t *node = sl_head(&expr->cast.cast->decls);
        if (node == NULL) {
            retval &= typecheck_type_cast(&expr->cast.cast->mark,
                                          expr->cast.cast->type,
                                          expr->cast.base->etype);
            expr->etype = expr->cast.cast->type;
        } else {
            retval &= typecheck_type_cast(&node->mark, node->type,
                                          expr->cast.base->etype);
            expr->etype = node->type;
        }
        return retval;
    }

    case EXPR_CALL: {
        if (!(retval &= typecheck_expr(tcs, expr->call.func, TC_NOCONST))) {
            return false;
        }
        type_t *func_sig = ast_type_unmod(expr->call.func->etype);
        if (func_sig->type == TYPE_PTR) {
            func_sig = ast_type_unmod(func_sig->ptr.base);
        }
        if (func_sig->type != TYPE_FUNC) {
            logger_log(&expr->mark, LOG_ERR,
                       "called object is not a function or function pointer");
            return false;
        }
        int arg_num = 1;
        sl_link_t *cur_sig, *cur_arg;
        cur_sig = func_sig->func.params.head;
        cur_arg = expr->call.params.head;
        while (cur_sig != NULL && cur_arg != NULL) {
            decl_t *decl = GET_ELEM(&func_sig->func.params, cur_sig);
            decl_node_t *param = sl_head(&decl->decls);
            expr_t *arg = GET_ELEM(&expr->call.params, cur_arg);
            retval &= typecheck_expr(tcs, arg, TC_NOCONST);
            type_t *param_type = param == NULL ? decl->type : param->type;
            if (arg->etype != NULL &&
                !typecheck_type_assignable(NULL, param_type,
                                           arg->etype)) {
                logger_log(&arg->mark, LOG_ERR,
                           "incompatible type for argument %d of function",
                           arg_num);
                return false;
            }

            ++arg_num;
            cur_sig = cur_sig->next;
            cur_arg = cur_arg->next;
        }
        if (cur_sig != NULL) {
            decl_t *decl = GET_ELEM(&func_sig->func.params, cur_sig);
            decl_node_t *param = sl_head(&decl->decls);

            // Only report error if parameter isn't (void)
            if (!(arg_num == 1 && param == NULL &&
                  decl->type->type == TYPE_VOID)) {
                logger_log(&expr->mark, LOG_ERR,
                           "too few arguments to function");
                retval = false;
            }
        }
        if (cur_arg != NULL) {
            if (func_sig->func.varargs) {
                while (cur_arg != NULL) {
                    expr_t *arg = GET_ELEM(&expr->call.params, cur_arg);
                    retval &= typecheck_expr(tcs, arg, TC_NOCONST);
                    cur_arg = cur_arg->next;
                }
            } else {
                logger_log(&expr->mark, LOG_ERR,
                           "too many arguments to function");
                retval = false;
            }
        }
        expr->etype = func_sig->func.type;
        return retval;
    }

    case EXPR_CMPD: {
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            retval &= typecheck_expr(tcs, GET_ELEM(&expr->cmpd.exprs, cur),
                                     TC_NOCONST);
        }
        expr_t *tail = sl_tail(&expr->cmpd.exprs);
        expr->etype = tail->etype;
        return retval;
    }

    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        if (expr->sizeof_params.type != NULL) {
            decl_node_t *node = sl_head(&expr->sizeof_params.type->decls);
            type_t *type = node != NULL ? node->type :
                expr->sizeof_params.type->type;
            type = ast_type_unmod(type);
            if (type->type == TYPE_STRUCT || type->type == TYPE_UNION) {
                // If the type hasn't been defined yet, then return an error
                if (type->struct_params.esize == (size_t)-1) {
                    return false;
                }
            }
            retval &= typecheck_decl(tcs, expr->sizeof_params.type, TYPE_VOID);
        }
        if (expr->sizeof_params.expr != NULL) {
            retval &= typecheck_expr(tcs, expr->sizeof_params.expr, TC_NOCONST);
        }
        expr->etype = tt_size_t;
        return retval;

    case EXPR_OFFSETOF:
        retval &= typecheck_decl(tcs, expr->offsetof_params.type, TYPE_VOID);
        decl_node_t *decl = sl_head(&expr->offsetof_params.type->decls);
        type_t *compound = decl == NULL ?
            expr->offsetof_params.type->type : decl->type;
        compound = ast_type_unmod(compound);
        str_node_t *head = sl_head(&expr->offsetof_params.path);
        switch (compound->type) {
        case TYPE_STRUCT:
        case TYPE_UNION:
            break;
        default:
            logger_log(&expr->mark, LOG_ERR,
                       "request for member '%s' in something not a structure "
                       "or union", head->str);
            return false;
        }

        SL_FOREACH(cur, &expr->offsetof_params.path) {
            str_node_t *node = GET_ELEM(&expr->offsetof_params.path, cur);
            type_t *mem_type = ast_type_find_member(compound, node->str,
                                                    NULL, NULL);
            if (mem_type == NULL) {
                logger_log(&expr->mark, LOG_ERR,
                           "compound type has no member '%s'",
                           node->str);
                return false;
            }
        }

        expr->etype = tt_size_t;
        return true;

    case EXPR_MEM_ACC: {
        if (!typecheck_expr(tcs, expr->mem_acc.base, TC_NOCONST)) {
            return false;
        }
        type_t *compound = ast_type_unmod(expr->mem_acc.base->etype);
        switch (compound->type) {
        case TYPE_STRUCT:
        case TYPE_UNION:
            if (expr->mem_acc.op != OP_DOT) {
                logger_log(&expr->mark, LOG_ERR,
                           "invalid type argument of '->'");
                return false;
            }
            break;
        case TYPE_PTR:
            if (expr->mem_acc.op == OP_ARROW) {
                compound = ast_type_unmod(compound->ptr.base);
                if (compound->type == TYPE_STRUCT ||
                    compound->type == TYPE_UNION) {
                    break;
                }
            }
            // FALL THROUGH
        default:
            logger_log(&expr->mark, LOG_ERR,
                       "request for member '%s' in something not a structure "
                       "or union", expr->mem_acc.name);
            return false;
        }
        if (compound->struct_params.esize == (size_t)-1) {
            logger_log(&expr->mark, LOG_ERR,
                       "dereferencing pointer to incomplete type");
            return false;
        }
        type_t *mem_type =
            ast_type_find_member(compound, expr->mem_acc.name, NULL, NULL);
        if (mem_type != NULL) {
            expr->etype = mem_type;
            return true;
        }
        logger_log(&expr->mark, LOG_ERR, "compound type has no member '%s'",
                   expr->mem_acc.name);
        return false;
    }

    case EXPR_ARR_IDX: {
        retval &= typecheck_expr(tcs, expr->arr_idx.array, TC_NOCONST);
        retval &= typecheck_expr(tcs, expr->arr_idx.index, TC_NOCONST);
        if (!retval) {
            return false;
        }
        type_t *umod_arr = ast_type_unmod(expr->arr_idx.array->etype);
        type_t *umod_index = ast_type_unmod(expr->arr_idx.index->etype);

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

        if (umod_arr->type == TYPE_PTR) {
            expr->etype = umod_arr->ptr.base;
        } else { // umod_arr->type == TYPE_ARR
            expr->etype = umod_arr->arr.base;
        }
        return retval;
    }

    case EXPR_INIT_LIST: {
        SL_FOREACH(cur, &expr->init_list.exprs) {
            retval &= typecheck_expr(tcs, GET_ELEM(&expr->init_list.exprs, cur),
                                     TC_NOCONST);
        }
        // Don't know what etype is
        return retval;
    }

    case EXPR_DESIG_INIT:
        retval &= typecheck_expr(tcs, expr->desig_init.val, TC_NOCONST);
        // Don't know what etype is
        return retval;

    case EXPR_VA_START: {
        retval &= typecheck_expr_va_list(tcs, expr->vastart.ap);
        gdecl_t *func = tcs->func;
        bool failed = true;
        if (func != NULL && expr->vastart.last->type == EXPR_VAR) {
            decl_node_t *fun_decl = sl_head(&func->decl->decls);
            assert(fun_decl != NULL);
            type_t *fun_type = fun_decl->type;
            assert(fun_type->type == TYPE_FUNC);
            decl_t *last_param = sl_tail(&fun_type->func.params);
            if (last_param != NULL) {
                decl_node_t *last_param_node = sl_tail(&last_param->decls);
                assert(last_param_node != NULL);
                if (strcmp(expr->vastart.last->var_id, last_param_node->id) ==
                    0) {
                    failed = false;
                }
            }
        }
        if (failed) {
            logger_log(&expr->vastart.last->mark, LOG_ERR,
                       "Expected function parameter name");
            retval = false;
        }
        expr->etype = tt_void;
        return retval;
    }
    case EXPR_VA_ARG: {
        retval &= typecheck_expr_va_list(tcs, expr->vaarg.ap);
        retval &= typecheck_decl(tcs, expr->vaarg.type, TC_NOCONST);
        decl_node_t *node = sl_head(&expr->vaarg.type->decls);
        if (node != NULL) {
            expr->etype = node->type;
        } else {
            expr->etype = expr->vaarg.type->type;
        }
        return retval;
    }
    case EXPR_VA_END:
        retval &= typecheck_expr_va_list(tcs, expr->vaend.ap);
        expr->etype = tt_void;
        return retval;
    case EXPR_VA_COPY:
        retval &= typecheck_expr_va_list(tcs, expr->vacopy.dest);
        retval &= typecheck_expr_va_list(tcs, expr->vacopy.src);
        expr->etype = tt_void;
        return retval;
    default:
        retval = false;
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
    case TYPE_VA_LIST:
        // Primitive types always type check
        return retval;

    case TYPE_STRUCT:
    case TYPE_UNION: {
        // If size exists, already typechecked
        if (type->struct_params.esize != (size_t)-1) {
            return true;
        }
        SL_FOREACH(cur, &type->struct_params.decls) {
            retval &= typecheck_decl(tcs,
                                     GET_ELEM(&type->struct_params.decls, cur),
                                     type->type);
        }

        if (!retval) {
            return false;
        }

        // Make sure there are no duplicate names
        struct_iter_t cur, check;
        struct_iter_init(type, &cur);

        do {
            // Skip anonymous members
            if (cur.node == NULL || cur.node->id == NULL) {
                continue;
            }

            memcpy(&check, &cur, sizeof(cur));

            while (struct_iter_advance(&check)) {
                if (check.node != NULL) {
                    if (check.node->id != NULL &&
                        strcmp(cur.node->id, check.node->id) == 0) {
                        logger_log(&check.node->mark, LOG_ERR,
                                   "duplicate member '%s'", check.node->id);
                        retval = false;
                    }
                } else {
                    // Check anonymous struct/unions
                    assert(check.decl != NULL);
                    if (ast_type_find_member(check.decl->type, cur.node->id,
                                             NULL, NULL)) {
                        logger_log(&cur.node->mark, LOG_ERR,
                                   "duplicate member '%s'", cur.node->id);
                        retval = false;
                    }
                }
            }
        } while (struct_iter_advance(&cur));

        ast_type_size(type); // Take size to mark as being a complete type
        return retval;
    }
    case TYPE_ENUM: {
        retval &= typecheck_type(tcs, type->enum_params.type);
        long long next_val = 0;
        SL_FOREACH(cur, &type->enum_params.ids) {
            decl_node_t *node = GET_ELEM(&type->enum_params.ids, cur);
            retval &= typecheck_decl_node(tcs, node, TYPE_ENUM);
            typetab_entry_t *entry;
            status_t status;
            if (CCC_OK !=
                (status =
                 tt_insert(tcs->typetab, type->enum_params.type, TT_ENUM_ID,
                           node->id, &entry))) {
                if (status == CCC_DUPLICATE) {
                    logger_log(&node->mark, LOG_ERR, "Redefined symbol %s",
                               node->id);
                }
                return false;
            }
            long long cur_val;
            if (node->expr != NULL) {
                typecheck_const_expr_eval(tcs->typetab, node->expr, &cur_val);
                entry->enum_val = cur_val;
                next_val = cur_val + 1;
            } else {
                entry->enum_val = next_val++;
            }
        }
        return retval;
    }

    case TYPE_TYPEDEF:
        // Don't typecheck typedefs to avoid typechecking multiple times
        return retval;

    case TYPE_MOD:
        // If there's a modifier without a base, then assume its int
        if (type->mod.base == NULL) {
            type->mod.base = tt_int;
        }
        retval &= typecheck_type(tcs, type->mod.base);
        if (type->mod.type_mod & TMOD_SIGNED &&
            type->mod.type_mod & TMOD_UNSIGNED) {
            logger_log(&type->mark, LOG_ERR,
                       "both 'signed' and 'unsigned' in declaration"
                       " specifiers");
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

        typetab_t *save_tab = NULL;
        if (tcs->func != NULL) {
            decl_node_t *func_node = sl_head(&tcs->func->decl->decls);
            if (func_node->type == type) {
                // Make sure to enter the scope of the function's body before
                // typechecking the decl so that the variables are added to the
                // correct scope
                save_tab = tcs->typetab;
                assert(tcs->func->fdefn.stmt->type = STMT_COMPOUND);
                tcs->typetab = &tcs->func->fdefn.stmt->compound.typetab;
            }
        }
        // If this is only a function declaration, we don't want to add the
        // parameters to any symbol table
        type_type_t decl_type = save_tab == NULL ? TYPE_FUNC : TYPE_VOID;
        SL_FOREACH(cur, &type->func.params) {
            decl_t *decl = GET_ELEM(&type->func.params, cur);
            decl_node_t *node = sl_head(&decl->decls);
            assert(node == sl_tail(&decl->decls));

            // Make sure param names are not duplicated
            if (node != NULL && node->id != NULL) {
                SL_FOREACH(cur, &type->func.params) {
                    decl_t *cur_decl = GET_ELEM(&type->func.params, cur);
                    decl_node_t *cur_node = sl_head(&cur_decl->decls);
                    if (cur_node == node) {
                        break;
                    }
                    if (cur_node == NULL || cur_node->id == NULL) {
                        continue;
                    }
                    if (strcmp(node->id, cur_node->id) == 0) {
                        logger_log(&node->mark, LOG_ERR,
                                   "redefinition of parameter '%s'", node->id);
                        logger_log(&cur_node->mark, LOG_NOTE,
                                   "previous definition of '%s' was here",
                                   node->id);
                        retval = false;
                    }
                }
            }

            retval &= typecheck_decl(tcs, decl, decl_type);
        }

        if (save_tab != NULL) {
            tcs->typetab = save_tab;
        }
        return retval;
    case TYPE_ARR:
        retval &= typecheck_type(tcs, type->arr.base);
        if (type->arr.len != NULL) {
            retval &= typecheck_expr(tcs, type->arr.len, TC_CONST);
            long long nelems;
            typecheck_const_expr_eval(tcs->typetab, type->arr.len,
                                      &nelems);
            if (nelems < 0) {
                logger_log(&type->arr.len->mark, LOG_ERR,
                           "size of array is negative");
            }
            type->arr.nelems = nelems;
        }
        return retval;
    case TYPE_PTR:
        return typecheck_type(tcs, type->ptr.base);

    default:
        assert(false);
    }

    return retval;
}
