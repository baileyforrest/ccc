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
 * AST Interface
 *
 * TODO: Finalize and document this
 */

#ifndef _AST_H_
#define _AST_H_

#include "util/slist.h"
#include "util/util.h"

typedef enum basic_type_t {
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_FUNC,
    TYPE_PTR,
    TYPE_ARR
} basic_type_t;

typedef enum type_mod_t {
    TMOD_SIGNED   = 1 << 0,
    TMOD_UNSIGNED = 1 << 1,
    TMOD_AUTO     = 1 << 2,
    TMOD_REGISTER = 1 << 3,
    TMOD_STATIC   = 1 << 4,
    TMOD_EXTERN   = 1 << 5,
    TMOD_CONST    = 1 << 6,
    TMOD_VOLATILE = 1 << 7,
} type_mod_t;

typedef enum oper_t {
    OP_NOP,        // Nop
    OP_PLUS,       // Plus
    OP_MINUS,      // Minus
    OP_TIMES,      // Times
    OP_DIV,        // Div
    OP_MOD,        // Mod
    OP_LT,         // Less than
    OP_LE,         // Less than or equal
    OP_GT,         // Greater than
    OP_GE,         // Greater than or equal
    OP_EQ,         // Equal
    OP_NE,         // Non equal
    OP_BITAND,     // Bitwise AND
    OP_BITXOR,     // Bitwise XOR
    OP_BITOR,      // Bitwise OR
    OP_LSHIFT,     // Left Shift
    OP_RSHIFT,     // Right Shift
    OP_LNOT,       // Logical NOT
    OP_BNOT,       // Bitwise NOT
    OP_NEGATIVE,   // unary minus
    OP_PINC,       // Post increment
    OP_PDEC,       // Post Decrement
    OP_TERN,       // Ternary Operator
} oper_t;

struct type_t;

typedef struct struct_decl_t {
    sl_link_t link;
    struct type_t *type;
    len_str_t id;
    int bf_bits;
} struct_decl_t;

typedef struct enum_id_t {
    sl_link_t link;
    len_str_t id;
    intptr_t val;
} enum_id_t;

typedef struct arr_dim_t {
    sl_link_t link;
    int dim_len;
} arr_dim_t;

struct gdecl_t;

typedef struct type_t {
    basic_type_t type;
    type_mod_t type_mod;
    int size;
    char align;
    union {
        slist_t struct_decls;
        slist_t union_decls;
        slist_t enum_ids;
        struct gdecl_t *func;
        struct {
            struct type_t *pointed_to;
            int indirections;
        } ptr;
        struct {
            struct type_t *base;
            slist_t dims;
        } arr;
    };
} type_t;


typedef struct param_t {
    sl_link_t link;
    type_t type;
    len_str_t id;
} param_t;

struct stmt_t;
typedef struct gdecl_t {
    sl_link_t link;
    type_t ret;
    len_str_t id;
    slist_t params;
    struct stmt_t *stmt;
} gdecl_t;

typedef enum expr_type_t {
    EXPR_VOID,
    EXPR_VAR,
    EXPR_CONST,
    EXPR_OP,
    EXPR_CALL,
    EXPR_DEREF,
    EXPR_ARR_ACC,
    EXPR_CMPD,
} expr_type_t;

typedef struct expr_t {
    sl_link_t link;
    expr_type_t type;

    union {
        len_str_t var_id;
        intptr_t const_val;
        struct {
            oper_t op;
            slist_t exprs;
        } op;
        struct {
            gdecl_t *func;
            slist_t params;
        } call;
        struct {
            struct expr_t *expr;
        } defref;
        struct {
            struct expr_t *expr;
            struct expr_t *index;
        } arr_acc;
        struct {
            slist_t exprs;
        } cmpd;
    };
} expr_t;

typedef enum stmt_type_t {
    STMT_NOP,
    STMT_DECL,
    STMT_ASSIGN,
    STMT_RETURN,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR,
    STMT_DO,
    STMT_SEQ,
    STMT_BLOCK,
    STMT_EXP,
    STMT_ASSN_INIT_LIST,
    STMT_LABEL,
    STMT_GOTO,
    STMT_CONTINUE,
    STMT_SWITCH,
} stmt_type_t;

struct stmt_t;

typedef struct switch_case_t {
    sl_link_t link;
    intptr_t val;
    struct stmt_t *stmt;
} switch_case_t;

typedef struct stmt_t {
    sl_link_t link;
    stmt_type_t type;

    union {
        struct {
            len_str_t id;
            type_t *type;
            expr_t *expr;
        } decl;
        struct {
            len_str_t id;
            expr_t *expr;
            oper_t op;
        } assign;
        struct {
            expr_t *expr;
        } ret;
        struct {
            expr_t *expr;
            struct stmt_t *true_stmt;
            struct stmt_t *false_stmt;
        } if_params;
        struct {
            expr_t *expr;
            struct stmt_t *stmt;
        } while_params;
        struct {
            expr_t *expr1;
            expr_t *expr2;
            expr_t *expr3;
            struct stmt_t *stmt;
        } for_params;
        struct {
            expr_t *expr;
            struct stmt_t *stmt;
        } do_params;
        struct {
            slist_t stmts;
        } seq;
        struct {
            slist_t stmts;
        } block;
        struct {
            expr_t *expr;
        } expr;
        struct {
            slist_t exprs;
        } assn_init_list;
        struct {
            len_str_t label;
        } label;
        struct {
            len_str_t label;
        } goto_params;
        struct {
            struct stmt_t *parent;
        } continue_params;
        struct {
            expr_t *expr;
            slist_t cases;
            struct stmt_t *default_case;
        } switch_params;
    };
} stmt_t;

#endif /* _AST_H_ */
