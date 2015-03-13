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

/**
 * Basic varieties of types
 */
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

    TYPE_FUNC,   /**< Function */

    TYPE_ARR,    /**< Array */
    TYPE_PTR,    /**< Pointer */
    TYPE_MOD,    /**< Modified Type */

    TYPE_TYPEDEF /**< Typedef type */
} basic_type_t;

/**
 * Modifiers for types. Should be stored in a bitmap
 */
typedef enum type_mod_t {
    TMOD_SIGNED   = 1 << 0,
    TMOD_UNSIGNED = 1 << 1,
    TMOD_AUTO     = 1 << 2,
    TMOD_REGISTER = 1 << 3,
    TMOD_STATIC   = 1 << 4,
    TMOD_EXTERN   = 1 << 5,
    TMOD_TYPEDEF  = 1 << 6,
    TMOD_CONST    = 1 << 7,
    TMOD_VOLATILE = 1 << 8,
} type_mod_t;

/**
 * Types of operations
 */
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

// Forward declarations
struct stmt_t;
struct type_t;
struct gdecl_t;

/**
 * Struct and union declaration entry
 */
typedef struct struct_decl_t {
    sl_link_t link;      /**< List link */
    stmt_t *decl;
    expr_t *bf_bits;     /**< Bitfield bits 0 for unused */
} struct_decl_t;

/**
 * Union declaration entry
 */
typedef struct enum_id_t {
    sl_link_t link; /**< List link */
    len_str_t *id;  /**< Name */
    intptr_t val;   /**< Value */
} enum_id_t;

/**
 * Tagged union representing a type
 */
typedef struct type_t {
    sl_link_t link;                     /**< Storage Link */
    basic_type_t type;                  /**< Basic type */
    int size;                           /**< Size of this type */
    char align;                         /**< Alignment */

    /** Whether or not this should be freed with containing structure */
    bool dealloc;

    union {
        slist_t struct_decls;           /**< List of struct/union definitions */
        slist_t enum_ids;               /**< List of enum ids/values */

        struct {                        /**< Function signature */
            type_t *type;               /**< Type (May be function pointer */
            stmt_t *params;             /**< Paramater signature */
        } func;

        struct {                        /**< Structure for pointer info */
            struct type_t *base;        /**< Base type pointed to */
            type_mod_t type_mod;        /**< Modifiers */
        } ptr;

        struct {                        /**< Structure for array info */
            struct type_t *base;        /**< Base type */
            expr_t *len;                /**< Dimension length */
        } arr;

        struct {
            struct type_t *base;
            type_mod_t type_mod;        /**< Bitset of type modifiers */
        } mod;

        len_str_t *typedef_name;        /**< New type's name */
    };
} type_t;

typedef enum gdecl_type_t {
    GDECL_FDEFN,    /**< Function definition */
    GDECL_DECL      /**< Declaration */
} gdecl_type_t;

/**
 * Global declaration
 */
typedef struct gdecl_t {
    sl_link_t link;              /**< Storage Link */
    gdecl_type_t type;           /**< Type of gdecl */
    struct stmt_t *decl;         /**< Declaration */
    union {
        struct {                 /**< Function definition parameters */
            struct stmt_t *stmt; /**< Function body */
        } fdefn;
    };
} gdecl_t;

/**
 * Types of expressions
 */
typedef enum expr_type_t {
    EXPR_VOID,    /**< Void */
    EXPR_VAR,     /**< Variable */
    EXPR_CONST,   /**< Constant */
    EXPR_OP,      /**< Operation */
    EXPR_CALL,    /**< Function call */
    EXPR_DEREF,   /**< Pointer dereference */
    EXPR_ARR_ACC, /**< Array access */
    EXPR_CMPD,    /**< Compound expression */
} expr_type_t;

/**
 * Tagged union for expressions
 */
typedef struct expr_t {
    sl_link_t link;               /**< Storage link */
    expr_type_t type;             /**< Expression type */

    union {                       /**< Type specific info */
        len_str_t *var_id;        /**< Variable identifier */
        intptr_t const_val;       /**< Constant value */

        struct {                  /**< Operation paramaters */
            oper_t op;            /**< Type of operation */
            slist_t exprs;        /**< Paramaters for operation */
        } op;

        struct {                  /**< Function call paramaters */
            gdecl_t *func;        /**< The function to call */
            slist_t params;       /**< The function paramaters (expressions) */
        } call;

        struct {                  /**< Pointer dereference paramaters */
            struct expr_t *expr;  /**< The expression to dereference */
        } defref;

        struct {                  /**< Array access parameters */
            struct expr_t *expr;  /**< Expression for array */
            struct expr_t *index; /**< Expression for array index */
        } arr_acc;

        struct {                  /**< Compound expression */
            slist_t exprs;        /**< List of expressions */
        } cmpd;
    };
} expr_t;

/**
 * Types of statements
 */
typedef enum stmt_type_t {
    STMT_NOP,            /**< No op statement */
    STMT_DECL,           /**< Declaration */
    STMT_ASSIGN,         /**< Assignment */
    STMT_RETURN,         /**< Return */
    STMT_IF,             /**< if */
    STMT_WHILE,          /**< while */
    STMT_FOR,            /**< for */
    STMT_DO,             /**< do while */
    STMT_SEQ,            /**< sequence of statements */
    STMT_BLOCK,          /**< block of statements */
    STMT_EXP,            /**< expression */
    STMT_ASSN_INIT_LIST, /**< Assign to an initializer list */
    STMT_LABEL,          /**< Label */
    STMT_GOTO,           /**< goto */
    STMT_CONTINUE,       /**< continue */
    STMT_SWITCH,         /**< switch */
    STMT_BREAK,          /**< break */
} stmt_type_t;

/**
 * Object representing a single switch case
 */
typedef struct switch_case_t {
    sl_link_t link;      /**< Storage Link */
    intptr_t val;        /**< case value */
    struct stmt_t *stmt; /**< Statement to execute */
} switch_case_t;

typedef struct decl_node_t {
    sl_link_t link; /**< Storage link */
    type_t *type;   /**< Type of variable */
    len_str_t *id;  /**< Name of variable */
    expr_t *expr;   /**< Expression to assign */
} decl_node_t;

/**
 * Tagged union representing a statement
 */
typedef struct stmt_t {
    sl_link_t link;                      /**< Storage link */
    stmt_type_t type;                    /**< Type of statement */

    union {
        struct {                         /**< Declaration parameters */
            type_t *type;                /**< Type of variable */
            slist_t decls;               /**< List of declarations */
        } decl;

        struct {                         /**< Assignment paramaters */
            expr_t *dest;                /**< Expression to assign to */
            expr_t *expr;                /**< Expression to assign */
            oper_t op;                   /**< Operation for expression (+=) */
        } assign;

        struct {                         /**< Return paramaters */
            expr_t *expr;                /**< Expression to return */
        } ret;

        struct {                         /**< if paramaters */
            expr_t *expr;                /**< Conditional expression */
            struct stmt_t *true_stmt;    /**< Statement to execute if true */
            struct stmt_t *false_stmt;   /**< Statement to execute if false */
        } if_params;

        struct {                         /**< While parameters */
            expr_t *expr;                /**< Conditional expression */
            struct stmt_t *stmt;         /**< Statement in loop */
        } while_params;

        struct {                         /**< For paramaters */
            expr_t *expr1;               /**< Expression 1 */
            expr_t *expr2;               /**< Expression 2 */
            expr_t *expr3;               /**< Expression 3 */
            struct stmt_t *stmt;         /**< Statement in loop */
        } for_params;

        struct {                         /**< Do while paramaters */
            struct stmt_t *stmt;         /**< Statement in loop */
            expr_t *expr;                /**< Conditional expression */
        } do_params;

        struct {                         /**< Sequence paramaters */
            slist_t stmts;               /**< List of statements */
        } seq;

        struct {                         /**< Block paramaters */
            slist_t stmts;               /**< List of statements */
        } block;

        struct {                         /**< Expression parameters */
            expr_t *expr;                /**< Expression to execute */
        } expr;

        struct {                         /**< Initializer list parameters */
            slist_t exprs;               /**< Expressions in initializer list */
        } assn_init_list;

        struct {                         /**< Label parameters */
            len_str_t *label;            /**< Label value */
        } label;

        struct {                         /**< Goto parameters */
            struct stmt_t *label;               /**< Label to goto */
        } goto_params;

        struct {                         /**< Continue parameters */
            struct stmt_t *parent;       /**< Parent statement to continue */
        } continue_params;

        struct {                         /**< Switch paramaters */
            expr_t *expr;                /**< Expression to switch on */
            slist_t cases;               /**< List of cases */
            struct stmt_t *default_case; /**< Default case statement */
        } switch_params;

        struct {                         /**< Break parameters */
            struct stmt_t *parent;       /**< Parent statement */
        } break_params;
    };
} stmt_t;

/**
 * Translation unit - Top level AST structure
 */
typedef struct trans_unit_t {
    sl_link_t link;  /**< Storage link */
    len_str_t *path; /**< Path of compilation unit */
    slist_t gdecls;  /**< List of gdecl in compilation unit */
} trans_unit_t;

/**
 * Print an AST
 *
 * @param cu The AST to print
 */
void ast_print(trans_unit_t *tu);

#endif /* _AST_H_ */
