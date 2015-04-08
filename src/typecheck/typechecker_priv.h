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
 * Type checker private interface
 */

#ifndef _TYPECHECKER_PRIV_H_
#define _TYPECHECKER_PRIV_H_

#include "typechecker.h"

#define TC_CONST true
#define TC_NOCONST false

/** Buffer size used for reporting errors */
#define ERR_BUF_SIZE 512

/**
 * Container for type checking context
 */
typedef struct tc_state_t {
    slist_t etypes;
    typetab_t *typetab; /*< Type table on top of the stack */
    gdecl_t *func;
    stmt_t *last_switch;
    stmt_t *last_loop;
    stmt_t *last_break;
} tc_state_t;

#define TYPE_IS_NUMERIC(test)                                       \
    ((test)->type >= TYPE_BOOL && (test)->type <= TYPE_LONG_DOUBLE)

#define TYPE_IS_INTEGRAL(test)                                      \
    ((test)->type >= TYPE_BOOL && (test)->type <= TYPE_LONG_LONG)

#define TYPE_IS_PTR(test)                                   \
    ((test)->type >= TYPE_FUNC && (test)->type <= TYPE_PTR)

/**
 * Initializes a type checker context
 *
 * @param tcs Type checker context to init
 */
void tc_state_init(tc_state_t *tcs);

/**
 * Destroys a type checker context
 *
 * @param tcs Type checker context to destroy
 */
void tc_state_destroy(tc_state_t *tcs);

/**
 * Evaluates a given constant expression expression.
 *
 * Assumes that the expression typechecks
 *
 * @param expr The expression to evaluate
 * @param result Location to store the result
 */
void typecheck_const_expr_eval(expr_t *expr, long long *result);

/**
 * Returns a type with its typedefs removed
 *
 * @param type The type
 * @return The type with its typedefs removed
 */
type_t *typecheck_untypedef(type_t *type);

/**
 * Returns a type with its modifers removed
 *
 * @param type The type
 * @return The type with its modifers removed
 */
type_t *typecheck_unmod(type_t *type);

/**
 * Checks whether an expression is a suitable lvalue. The expression must be
 * typechecked first.
 *
 * @param expr The expression to check
 * @return true if the expression is an lvalue, false otherwise
 */
bool typecheck_expr_lvalue(tc_state_t *tcs, expr_t *expr);

/**
 * Returns true if type from can be assigned to type to, false otherwise
 *
 * @param mark Location of the assignment
 * @param to Type to assign to
 * @param from Type to assign from
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_type_assignable(fmark_t *mark, type_t *to, type_t *from);

/**
 * Returns true if types t1 and t2 can be combined with given binop
 *
 * @param mark Location of the binop
 * @param op The binary operation
 * @param t1 The first type
 * @param t2 The second type
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_types_binop(fmark_t *mark, oper_t op, type_t *t1, type_t *t2);

/**
 * Returns true if given unary op can be applied to given type
 *
 * @param mark Location of the op
 * @param op The unary operation
 * @param type The type
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_type_unaryop(fmark_t *mark, oper_t op, type_t *type);

/**
 * Verifies that t1 and t2 are compatible, and the returns the "higher" type of
 * the two.
 *
 * @param mark Location of usage
 * @param t1 Type 1
 * @param t2 Type 2
 * @param result The wider of t1 and t2 otherwise.
 *     NULL if they are not compatible
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_type_max(fmark_t *mark, type_t *t1, type_t *t2, type_t **result);

/**
 * Returns true if from can be cast to to.
 *
 * @param mark Location of the type cast
 * @param to Type being casted to
 * @param from Type being casted from
 * @return true if the the cast can be completed, false otherwise
 */
bool typecheck_type_cast(fmark_t *mark, type_t *to, type_t *from);

/**
 * Returns true if type can be used in a conditional
 *
 * @param mark Location where the instance of the type occurs
 * @param type The type to check
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_type_conditional(fmark_t *mark, type_t *type);

/**
 * Returns true if type is integral, false otherwise
 *
 * @param mark Location where the instance of the type occurs
 * @param type The type to check
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_type_integral(fmark_t *mark, type_t *type);

/**
 * Typechecks the given expression and ensures its type can be used in a
 * conditional
 *
 * @param type The type to check
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_expr_conditional(tc_state_t *tcs, expr_t *expr);

/**
 * Typechecks expression, and makes sure its an integral type
 *
 * @param tcs The typechecking state
 * @param expr The expression to check
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_expr_integral(tc_state_t *tcs, expr_t *expr);

/**
 * Typechecks expression, and makes sure its a conditional type
 *
 * @param tcs The typechecking state
 * @param expr The expression to check
 * @return true if the node typechecks, false otherwise
 */
bool typecheck_expr_conditional(tc_state_t *tcs, expr_t *expr);

/**
 * Typechecks a trans_unit_t.
 *
 * @param tcs The typechecking state
 * @param tras_unit Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_trans_unit(tc_state_t *tcs, trans_unit_t *trans_unit);

/**
 * Typechecks a gdecl_t.
 *
 * @param tcs The typechecking state
 * @param gdecl Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_gdecl(tc_state_t *tcs, gdecl_t *gdecl);

/**
 * Typechecks a stmt_t.
 *
 * @param tcs The typechecking state
 * @param stmt Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_stmt(tc_state_t *tcs, stmt_t *stmt);

/**
 * Typechecks a decl_t.
 *
 * @param tcs The typechecking state
 * @param decl Object to typecheck
 * @param constant if TC_CONST, make sure the decl has constant expressions
 *     as assignments, and they are integers (for struct bit fields)
 * @return true if the node type checks, false otherwise
 */
bool typecheck_decl(tc_state_t *tcs, decl_t *decl, bool constant);

/**
 * Typechecks a type being assigned to an init list
 *
 * @param tcs The typechecking state
 * @param type The type bein assigned to
 * @param expr The init list expression
 * @return true if the node type checks, false otherwise
 */
bool typecheck_init_list(tc_state_t *tcs, type_t *type, expr_t *expr);

/**
 * Typechecks a decl_node_t.
 *
 * @param tcs The typechecking state
 * @param decl_node Object to typecheck
 * @param constant if TC_CONST, make sure the expression is constant and of type
 *     integer. This is for struct bitfields and enum values
 * @return true if the node type checks, false otherwise
 */
bool typecheck_decl_node(tc_state_t *tcs, decl_node_t *decl_node,
                         bool constant);

/**
 * Typechecks a expr_t.
 *
 * @param tcs The typechecking state
 * @param expr Object to typecheck
 * @param constant if TC_CONST, make sure the expression is constant.
 * @return true if the node type checks, false otherwise
 */
bool typecheck_expr(tc_state_t *tcs, expr_t *expr, bool constant);

/**
 * Typechecks a type_t that is not protected.
 *
 * @param tcs The typechecking state
 * @param type Object to typecheck
 * @return true if the node type checks, false otherwise
 */
bool typecheck_type(tc_state_t *tcs, type_t *type);

#endif /* _TYPECHECKER_PRIV_H_ */
