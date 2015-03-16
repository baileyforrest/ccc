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

#include "parser/type_table.h"

#include "util/slist.h"
#include "util/util.h"

// Forward declarations
struct stmt_t;
struct type_t;
struct gdecl_t;
struct decl_t;

/**
 * Struct and union declaration entry
 */
typedef struct struct_decl_t {
    sl_link_t link;         /**< List link */
    struct decl_t *decl;    /**< The declaration */
    struct expr_t *bf_bits; /**< Bitfield bits 0 for unused */
} struct_decl_t;

/**
 * Union declaration entry
 */
typedef struct enum_id_t {
    sl_link_t link; /**< List link */
    len_str_t *id;  /**< Name */
    struct expr_t *val;   /**< Value */
} enum_id_t;

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
 * Basic varieties of types
 */
typedef enum basic_type_t {
    // Basic types
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,

    // User defined types
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,

    TYPE_FUNC,   /**< Function */

    TYPE_ARR,    /**< Array */
    TYPE_PTR,    /**< Pointer */
    TYPE_MOD,    /**< Modified Type */
} basic_type_t;

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
            struct type_t *type;        /**< Type (May be function pointer */
            slist_t params;             /**< Paramater signature (decl list) */
            bool varargs;               /**< Whether or not function has VA */
        } func;

        struct {                        /**< Structure for array info */
            struct type_t *base;        /**< Base type */
            struct expr_t *len;         /**< Dimension length */
        } arr;

        struct {                        /**< Structure for pointer info */
            struct type_t *base;        /**< Base type pointed to */
            type_mod_t type_mod;        /**< Modifiers */
        } ptr;

        struct {
            struct type_t *base;
            type_mod_t type_mod;        /**< Bitset of type modifiers */
        } mod;
    };
} type_t;

/**
 * Types of operations
 */
typedef enum oper_t {
    OP_NOP,        // Nop
    OP_PLUS,       // Plus
    OP_UPLUS,      // Unary plus
    OP_MINUS,      // Minus
    OP_UMINUS,     // Unary minus
    OP_TIMES,      // Times
    OP_DEREF,      // Dereference of
    OP_DIV,        // Div
    OP_MOD,        // Mod
    OP_LT,         // Less than
    OP_LE,         // Less than or equal
    OP_GT,         // Greater than
    OP_GE,         // Greater than or equal
    OP_EQ,         // Equal
    OP_NE,         // Non equal
    OP_BITAND,     // Bitwise AND
    OP_ADDR,       // Address of
    OP_BITXOR,     // Bitwise XOR
    OP_BITOR,      // Bitwise OR
    OP_LSHIFT,     // Left Shift
    OP_RSHIFT,     // Right Shift
    OP_LOGICNOT,   // Logical NOT
    OP_BITNOT,     // Bitwise NOT
    OP_ARR_ACC,    // Array access
    OP_PREINC,     // Pre increment
    OP_POSTINC,    // Post increment
    OP_PREDEC,     // Pre decrement
    OP_POSTDEC,    // Post decrement
    OP_ARROW,      // ->
    OP_DOT         // .
} oper_t;


/**
 * Types of expressions
 */
typedef enum expr_type_t {
    EXPR_VOID,        /**< Void */
    EXPR_VAR,         /**< Variable */
    EXPR_ASSIGN,      /**< Assignment */
    EXPR_CONST_INT,   /**< Constant integral type */
    EXPR_CONST_FLOAT, /**< Constant floating point type */
    EXPR_CONST_STR,   /**< Constant string tye */
    EXPR_BIN,         /**< Binary Operation */
    EXPR_UNARY,       /**< Unary Operation */
    EXPR_COND,        /**< Conditional Operator */
    EXPR_CAST,        /**< Cast expression */
    EXPR_CALL,        /**< Function call */
    EXPR_CMPD,        /**< Compound expression */
    EXPR_SIZEOF,      /**< Sizeof expression */
    EXPR_MEM_ACC,     /**< Member access */
    EXPR_INIT_LIST    /**< Initializer list */
} expr_type_t;

/**
 * Tagged union for expressions
 */
typedef struct expr_t {
    sl_link_t link;               /**< Storage link */
    expr_type_t type;             /**< Expression type */

    union {                       /**< Type specific info */
        len_str_t *var_id;        /**< Variable identifier */

        struct {                  /**< Assignment paramaters */
            struct expr_t *dest;  /**< Expression to assign to */
            struct expr_t *expr;  /**< Expression to assign */
            oper_t op;            /**< Operation for expression (+=) */
        } assign;

        struct {
            type_t *type;
            union {               /**< Constant value */
                long long int_val;/**< Int constant */
                double float_val; /**< Float constant */
                len_str_t *str_val;/**< String constant */
            };
        } const_val;

        struct {                  /**< Binary operation */
            oper_t op;            /**< Type of operation */
            struct expr_t *expr1; /**< Expr 1 */
            struct expr_t *expr2; /**< Expr 2 */
        } bin;

        struct {                  /**< Binary operation */
            oper_t op;            /**< Type of operation */
            struct expr_t *expr;  /**< Expression  */
        } unary;

        struct {                  /**< Operation paramaters */
            oper_t op;            /**< Type of operation */
            struct expr_t *expr1; /**< Expr 1 */
            struct expr_t *expr2; /**< Expr 2 */
            struct expr_t *expr3; /**< Expr 3 */
        } cond;

        struct {                  /**< Cast parameters */
            struct decl_t *cast;  /**< Casted type */
            struct expr_t *base;  /**< Base expression */
        } cast;

        struct {                  /**< Function call paramaters */
            struct expr_t *func;  /**< The function to call */
            slist_t params;       /**< The function paramaters (expressions) */
        } call;

        struct {                  /**< Compound expression */
            slist_t exprs;        /**< List of expressions */
        } cmpd;

        struct {
            struct decl_t *type;
            struct expr_t *expr;
        } sizeof_params;

        struct {
            struct expr_t *base;
            len_str_t *name;
            oper_t op;
        } mem_acc;

        struct {
            slist_t exprs;        /**< List of expressions */
        } init_list;
    };
} expr_t;

typedef struct decl_node_t {
    sl_link_t link; /**< Storage link */
    type_t *type;   /**< Type of variable */
    len_str_t *id;  /**< Name of variable */
    expr_t *expr;   /**< Expression to assign */
} decl_node_t;

typedef struct decl_t {
    sl_link_t link;              /**< Storage link */
    type_t *type;                /**< Type of variable */
    slist_t decls;               /**< List of declarations (decl_node_t) */
} decl_t;


/**
 * Types of statements
 */
typedef enum stmt_type_t {
    STMT_NOP,            /**< No op statement */

    STMT_DECL,           /**< Declaration */

    // Labeled Statements
    STMT_LABEL,          /**< Label */
    STMT_CASE,           /**< case */
    STMT_DEFAULT,        /**< default */

    // Selection statements
    STMT_IF,             /**< if */
    STMT_SWITCH,         /**< switch */

    // Iteration statements
    STMT_DO,             /**< do while */
    STMT_WHILE,          /**< while */
    STMT_FOR,            /**< for */

    // Jump statements
    STMT_GOTO,           /**< goto */
    STMT_CONTINUE,       /**< continue */
    STMT_BREAK,          /**< break */
    STMT_RETURN,         /**< Return */

    STMT_COMPOUND,       /**< block of statements */

    STMT_EXPR            /**< expression */
} stmt_type_t;

/**
 * Tagged union representing a statement
 */
typedef struct stmt_t {
    sl_link_t link;                      /**< Storage link */
    stmt_type_t type;                    /**< Type of statement */

    union {
        decl_t *decl;                    /**< Declaration parameters */

        struct {                         /**< Label parameters */
            len_str_t *label;            /**< Label value */
            struct stmt_t *stmt;         /**< Statement labeled */
        } label;

        struct {                         /**< case parameters */
            expr_t *val;                 /**< Expression value */
            struct stmt_t *stmt;         /**< Statement labeled */
        } case_params;

        struct {                         /**< default parameters */
            struct stmt_t *stmt;         /**< Statement labeled */
        } default_params;

        struct {                         /**< if paramaters */
            expr_t *expr;                /**< Conditional expression */
            struct stmt_t *true_stmt;    /**< Statement to execute if true */
            struct stmt_t *false_stmt;   /**< Statement to execute if false */
        } if_params;

        struct {                         /**< Switch paramaters */
            expr_t *expr;                /**< Expression to switch on */
            struct stmt_t *stmt;         /**< Statement */
        } switch_params;

        struct {                         /**< Do while paramaters */
            struct stmt_t *stmt;         /**< Statement in loop */
            expr_t *expr;                /**< Conditional expression */
        } do_params;

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

        struct {                         /**< Goto parameters */
            struct stmt_t *target;       /**< Parent statement to continue */
            struct len_str_t *label;     /**< Label to goto */
        } goto_params;

        struct {                         /**< Continue parameters */
            struct stmt_t *parent;       /**< Parent statement to continue */
        } continue_params;

        struct {                         /**< Break parameters */
            struct stmt_t *parent;       /**< Parent statement */
        } break_params;

        struct {                         /**< Return paramaters */
            expr_t *expr;                /**< Expression to return */
        } return_params;;

        struct {                         /**< Block paramaters */
            slist_t stmts;               /**< List of statements */
            typetab_t typetab;           /**< Types defined in this scope */
        } compound;

        struct {                         /**< Expression parameters */
            expr_t *expr;                /**< Expression to execute */
        } expr;
    };
} stmt_t;


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
    struct decl_t *decl;         /**< Declaration */
    union {
        struct {                 /**< Function definition parameters */
            struct stmt_t *stmt; /**< Function body */
        } fdefn;
    };
} gdecl_t;


/**
 * Translation unit - Top level AST structure
 */
typedef struct trans_unit_t {
    sl_link_t link;     /**< Storage link */
    len_str_t *path;    /**< Path of compilation unit */
    slist_t gdecls;     /**< List of gdecl in compilation unit */
    typetab_t typetab; /**< Types defined at top level */
} trans_unit_t;

/**
 * Print an AST
 *
 * @param cu The AST to print
 */
void ast_print(trans_unit_t *tu);

/**
 * Destroys an AST
 *
 * @param ast The ast to destroy
 */
void ast_destroy(trans_unit_t *ast);

/**
 * Destroys a struct_decl_t. Does not free, but does free child nodes
 *
 * @param struct_decl Object to destroy
 */
void ast_struct_decl_destroy(struct_decl_t *struct_decl);

/**
 * Destroys an enum_id_t. Does not free, but does free child nodes
 *
 * @param enum_id Object to destroy
 */
void ast_enum_id_destroy(enum_id_t *enum_id);


#define OVERRIDE true
#define NO_OVERRIDE false

/**
 * Destroys a type_t.
 *
 * @param type Object to destroy
 * @param override If OVERRIDE, frees even if the dealloc flag is set.
 * If NOOVERRIDE ignores an object with the dealloc flag set.
 * This paramater is not recursively applied
 */
void ast_type_destroy(type_t *type, bool override);

/**
 * Destroys a gdecl_t.
 *
 * @param gdecl Object to destroy
 */
void ast_gdecl_destroy(gdecl_t *gdecl);

/**
 * Destroys a expr_t.
 *
 * @param expr Object to destroy
 */
void ast_expr_destroy(expr_t *expr);

/**
 * Destroys a decl_node_t.
 *
 * @param decl_node Object to destroy
 */
void ast_decl_node_destroy(decl_node_t *decl_node);

/**
 * Destroys a decl_t.
 *
 * @param decl Object to destroy
 */
void ast_decl_destroy(decl_t *decl);

/**
 * Destroys a stmt_t.
 *
 * @param stmt Object to destroy
 */
void ast_stmt_destroy(stmt_t *stmt);

/**
 * Destroys a trans_unit_t.
 *
 * @param tras_unit Object to destroy
 */
void ast_trans_unit_destroy(trans_unit_t *trans_unit);

#endif /* _AST_H_ */
