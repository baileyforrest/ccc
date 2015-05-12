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
 */
// TODO1: Missing documentation

#ifndef _AST_H_
#define _AST_H_

#include "parse/type_table.h"

#include <stdarg.h>

#include "util/file_directory.h"
#include "util/slist.h"
#include "util/util.h"

// Forward declarations
typedef struct type_t type_t;
typedef struct expr_t expr_t;
typedef struct stmt_t stmt_t;
typedef struct decl_t decl_t;
typedef struct gdecl_t gdecl_t;
typedef struct ir_label_t ir_label_t;

#define TYPE_IS_NUMERIC(test)                                       \
    ((test)->type >= TYPE_BOOL && (test)->type <= TYPE_LONG_DOUBLE)

#define TYPE_IS_FLOAT(test)                                         \
    ((test)->type >= TYPE_FLOAT && (test)->type <= TYPE_LONG_DOUBLE)

#define TYPE_IS_INTEGRAL(test)                                      \
    ((test)->type >= TYPE_BOOL && (test)->type <= TYPE_LONG_LONG)

#define TYPE_IS_UNSIGNED(test)                                          \
    ((test)->type == TYPE_BOOL ||                                       \
     ((test)->type == TYPE_MOD && (test)->mod.type_mod & TMOD_UNSIGNED))

#define TYPE_HAS_MOD(test, modname)                                 \
     ((test)->type == TYPE_MOD && (test)->mod.type_mod & modname)


#define TYPE_IS_PTR(test)                                   \
    ((test)->type >= TYPE_FUNC && (test)->type <= TYPE_PTR)

#define STMT_LABELED(test)                                          \
    ((test)->type == STMT_CASE ? (test)->case_params.stmt :         \
     (test)->type == STMT_DEFAULT ? (test)->default_params.stmt :   \
     (test)->type == STMT_LABEL ? (test)->label.stmt : NULL)

// TODO: Replace this idiom in the code with this
#define DECL_TYPE(decl) \
    (sl_head(&decl->decls) == NULL ? \
     decl->type : ((decl_node_t *)sl_head(&decl->decls))->type)

#define DECL_MARK(decl) \
    (sl_head(&decl->decls) == NULL ? \
     &decl->mark : &((decl_node_t *)sl_head(&decl->decls))->mark)


/**
 * Modifiers for types. Should be stored in a bitmap
 */
typedef enum type_mod_t {
    TMOD_NONE     = 0,
    TMOD_SIGNED   = 1 << 0,
    TMOD_UNSIGNED = 1 << 1,
    TMOD_AUTO     = 1 << 2,
    TMOD_REGISTER = 1 << 3,
    TMOD_STATIC   = 1 << 4,
    TMOD_EXTERN   = 1 << 5,
    TMOD_TYPEDEF  = 1 << 6,
    TMOD_CONST    = 1 << 7,
    TMOD_VOLATILE = 1 << 8,
    TMOD_INLINE   = 1 << 9,
    TMOD_ALIGNAS  = 1 << 10,
    TMOD_NORETURN = 1 << 11,
} type_mod_t;

/**
 * Basic varieties of types
 *
 * The order of the integral and pointer subgroups is important
 */
typedef enum type_type_t {
    // Primitive types
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_LONG_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LONG_DOUBLE,

    // User defined types
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,

    TYPE_TYPEDEF, /**< Typedef name */

    TYPE_MOD,     /**< Modified Type */

    TYPE_PAREN,   /**< Parens in type */
    TYPE_FUNC,    /**< Function */
    TYPE_ARR,     /**< Array */
    TYPE_PTR,     /**< Pointer */

    TYPE_VA_LIST, /**< va_list */

    TYPE_STATIC_ASSERT, /** Type to hold static assert information */
} type_type_t;

/**
 * Tagged union representing a type
 */
struct type_t {
    sl_link_t heap_link;         /**< Allocation Link */
    // No storage link because there are static types
    fmark_t mark;                /**< File mark */
    type_type_t type;            /**< Type of type type */
    bool typechecked;

    union {
        struct {                 /**< Struct/union params */
            char *name;          /**< Name of struct/union, NULL if anon */
            slist_t decls;       /**< (decl_t) List of struct/union decls */
            void *trans_state;
            size_t esize;        /**< Cached size. -1 if unassigned */
            size_t ealign;       /**< Cached align. -1 if unassigned */
        } struct_params;

        struct {
            char *name;          /**< Name of struct/union, NULL if anon */
            type_t *type;        /**< Type of enum elements */
            slist_t ids;         /**< (decl_node_t) List of enum ids/values */
        } enum_params;

        struct {                 /**< Typedef params */
            char *name;          /**< Name of typedef type */
            type_t *base;        /**< Base of typedef type */
            type_type_t type;    /**< (struct/union/enum/void=regular) */
        } typedef_params;

        struct {                 /**< Modified type params */
            type_mod_t type_mod; /**< Bitset of type modifiers */
            decl_t *alignas_type;
            expr_t *alignas_expr;
            size_t alignas_align;
            type_t *base;        /**< Base type */
        } mod;

        type_t *paren_base;      /**< Type in parens */

        struct {                 /**< Function signature */
            type_t *type;        /**< Return type */
            slist_t params;      /**< (decl_t) Paramater signature */
            bool varargs;        /**< Whether or not function has VA */
        } func;

        struct {                 /**< Structure for array info */
            type_t *base;        /**< Base type */
            expr_t *len;         /**< Dimension length */
            size_t nelems;       /**< Number of elems */
        } arr;

        struct {                 /**< Structure for pointer info */
            type_t *base;        /**< Base type pointed to */
            type_mod_t type_mod; /**< Modifiers */
        } ptr;

        struct {
            expr_t *expr;
            char *msg;
        } sa_params;
    };
};

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
    OP_LOGICAND,   // Logical AND
    OP_LOGICOR,    // Logical OR
    OP_BITNOT,     // Bitwise NOT
    OP_PREINC,     // Pre increment
    OP_POSTINC,    // Post increment
    OP_PREDEC,     // Pre decrement
    OP_POSTDEC,    // Post decrement
    OP_ARROW,      // ->
    OP_DOT,        // .
} oper_t;


/**
 * Types of expressions
 */
typedef enum expr_type_t {
    EXPR_VOID,        /**< Void */
    EXPR_PAREN,       /**< Parens */
    EXPR_VAR,         /**< Variable */
    EXPR_ASSIGN,      /**< Assignment */
    EXPR_CONST_INT,   /**< Constant integral type */
    EXPR_CONST_FLOAT, /**< Constant floating point type */
    EXPR_CONST_STR,   /**< Constant string type */
    EXPR_BIN,         /**< Binary Operation */
    EXPR_UNARY,       /**< Unary Operation */
    EXPR_COND,        /**< Conditional Operator */
    EXPR_CAST,        /**< Cast expression */
    EXPR_CALL,        /**< Function call */
    EXPR_CMPD,        /**< Compound expression */
    EXPR_SIZEOF,      /**< Sizeof expression */
    EXPR_ALIGNOF,     /**< alignof expression */
    EXPR_OFFSETOF,    /**< offsetof expression */
    EXPR_MEM_ACC,     /**< Member access */
    EXPR_ARR_IDX,     /**< Array Index */
    EXPR_INIT_LIST,   /**< Initializer list */
    EXPR_DESIG_INIT,  /**< Designated initializer */
    EXPR_VA_START,    /**< __builtin_va_start */
    EXPR_VA_ARG,      /**< __builtin_va_arg */
    EXPR_VA_END,      /**< __builtin_va_end */
    EXPR_VA_COPY,     /**< __builtin_va_copy */
} expr_type_t;

typedef struct mem_acc_list_t {
    slist_t list; /**< (expr_t) List of EXPR_MEM_ACC and EXPR_ARR_IDX  */
} mem_acc_list_t;

/**
 * Tagged union for expressions
 */
struct expr_t {
    sl_link_t heap_link;            /**< Allocation Link */
    sl_link_t link;                 /**< Storage link */
    fmark_t mark;                   /**< File mark */
    expr_type_t type;               /**< Expression type */
    type_t *etype;                  /**< Type of the expression */

    union {
        expr_t *paren_base;         /**< Expression in parens */
        char *var_id;               /**< Variable identifier */

        struct {                    /**< Assignment paramaters */
            expr_t *dest;           /**< Expression to assign to */
            expr_t *expr;           /**< Expression to assign */
            oper_t op;              /**< Operation for expression e.g. (+=) */
        } assign;

        struct {                    /**< Constast paramaters */
            type_t *type;           /**< The type of the constant */
            union {                 /**< Constant value */
                long long int_val;  /**< Int constant */
                long double float_val; /**< Float constant */
                char *str_val;      /**< String constant */
            };
        } const_val;

        struct {                    /**< Binary operation */
            oper_t op;              /**< Type of operation */
            expr_t *expr1;          /**< Expr 1 */
            expr_t *expr2;          /**< Expr 2 */
        } bin;

        struct {                    /**< Unary operation */
            oper_t op;              /**< Type of operation */
            expr_t *expr;           /**< Expression  */
        } unary;

        struct {                    /**< Operation paramaters */
            expr_t *expr1;          /**< Expr 1 */
            expr_t *expr2;          /**< Expr 2 */
            expr_t *expr3;          /**< Expr 3 */
        } cond;

        struct {                    /**< Cast parameters */
            decl_t *cast;           /**< Casted type */
            expr_t *base;           /**< Base expression */
        } cast;

        struct {                    /**< Function call paramaters */
            expr_t *func;           /**< The function to call */
            slist_t params;         /**< The function paramaters (expr_t) */
        } call;

        struct {                    /**< Compound expression */
            slist_t exprs;          /**< (expr_t) List of expressions */
        } cmpd;

        struct {                    /**< Sizeof and alignof paramaters */
            decl_t *type;           /**< Type to get sizeof. NULL if expr */
            expr_t *expr;           /**< Expr to get sizeof. NULL if type */
        } sizeof_params;

        struct {                    /**< Offsetof parameters */
            decl_t *type;           /**< Type to get offsetof */
            mem_acc_list_t path;    /**< Accesses is path */
        } offsetof_params;

        struct {                    /**< Member access of a compound type */
            expr_t *base;           /**< Expression to get type */
            char *name;             /**< Name of member */
            oper_t op;              /**< Operation (., ->) */
        } mem_acc;

        struct {                    /**< Array index */
            expr_t *array;          /**< Array */
            expr_t *index;          /**< index */
            size_t const_idx;
        } arr_idx;

        struct {                    /**< Initalizer list */
            slist_t exprs;          /**< List of expressions */
            size_t nelems;
        } init_list;

        struct {
            expr_t *val;
            char *name;
        } desig_init;

        struct {
            expr_t *ap;
            expr_t *last;
        } vastart;

        struct {
            expr_t *ap;
            decl_t *type;
        } vaarg;

        struct {
            expr_t *ap;
        } vaend;

        struct {
            expr_t *dest;
            expr_t *src;
        } vacopy;

    };
};

/**
 * One declaration on a given type.
 *
 * e.g. int foo, *bar; foo and *bar are the decl nodes
 */
typedef struct decl_node_t {
    sl_link_t heap_link; /**< Allocation Link */
    sl_link_t link;      /**< Storage link */
    fmark_t mark;        /**< File mark */
    type_t *type;        /**< Type of variable */
    char *id;            /**< Name of variable */
    expr_t *expr;        /**< Expression to assign, bitfield bits for struct/union */
} decl_node_t;

/**
 * A declaration
 */
struct decl_t {
    sl_link_t heap_link; /**< Allocation Link */
    sl_link_t link;      /**< Storage link */
    fmark_t mark;        /**< File mark */
    type_t *type;        /**< Type of variable */
    slist_t decls;       /**< List of declarations (decl_node_t) */
};


/**
 * Types of statements
 */
typedef enum stmt_type_t {
    STMT_NOP,      /**< No op statement */

    STMT_DECL,     /**< Declaration only allowed in compound statement */

    // Labeled Statements
    STMT_LABEL,    /**< Label */
    STMT_CASE,     /**< case */
    STMT_DEFAULT,  /**< default */

    // Selection statements
    STMT_IF,       /**< if */
    STMT_SWITCH,   /**< switch */

    // Iteration statements
    STMT_DO,       /**< do while */
    STMT_WHILE,    /**< while */
    STMT_FOR,      /**< for */

    // Jump statements
    STMT_GOTO,     /**< goto */
    STMT_CONTINUE, /**< continue */
    STMT_BREAK,    /**< break */
    STMT_RETURN,   /**< Return */

    STMT_COMPOUND, /**< block of statements */

    STMT_EXPR,     /**< expression */
} stmt_type_t;

/**
 * Tagged union representing a statement
 */
struct stmt_t {
    sl_link_t heap_link;          /**< Allocation Link */
    sl_link_t link;               /**< Storage link */
    fmark_t mark;                 /**< File mark */
    stmt_type_t type;             /**< Type of statement */

    union {
        decl_t *decl;             /**< Declaration parameters */

        struct {                  /**< Label parameters */
            sl_link_t link;       /**< Link for label hash table */
            char *label;          /**< Label value */
            stmt_t *stmt;         /**< Statement labeled */
        } label;

        struct {                  /**< case parameters */
            sl_link_t link;       /**< Link in case statement */
            expr_t *val;          /**< Expression value */
            stmt_t *stmt;         /**< Statement labeled */
            ir_label_t *label;    /**< Label of this case */
        } case_params;

        struct {                  /**< default parameters */
            stmt_t *stmt;         /**< Statement labeled */
            ir_label_t *label;    /**< Label of default */
        } default_params;

        struct {                  /**< if paramaters */
            expr_t *expr;         /**< Conditional expression */
            stmt_t *true_stmt;    /**< Statement to execute if true */
            stmt_t *false_stmt;   /**< False branch, NULL if none  */
        } if_params;

        struct {                  /**< Switch paramaters */
            expr_t *expr;         /**< Expression to switch on */
            stmt_t *stmt;         /**< Statement */
            slist_t cases;        /**< (stmt_t) List of cast params */
            stmt_t *default_stmt; /**< Default statement */
        } switch_params;

        struct {                  /**< Do while paramaters */
            stmt_t *stmt;         /**< Statement in loop */
            expr_t *expr;         /**< Conditional expression */
        } do_params;

        struct {                  /**< While parameters */
            expr_t *expr;         /**< Conditional expression */
            stmt_t *stmt;         /**< Statement in loop */
        } while_params;

        struct {                  /**< For paramaters */
            decl_t *decl1;        /**< Declaration 1 */
            typetab_t *typetab;   /**< vars defined in for loop decl */
            expr_t *expr1;        /**< Expression 1 */
            expr_t *expr2;        /**< Expression 2 */
            expr_t *expr3;        /**< Expression 3 */
            stmt_t *stmt;         /**< Statement in loop */
        } for_params;

        struct {                  /**< Goto parameters */
            sl_link_t link;       /**< Link for GOTO list */
            char *label;     /**< Label to goto */
        } goto_params;

        struct {                  /**< Continue parameters */
            stmt_t *parent;       /**< Loop to continue */
        } continue_params;

        struct {                  /**< Break parameters */
            stmt_t *parent;       /**< Parent statement */
        } break_params;

        struct {                  /**< Return paramaters */
            type_t *type;         /**< Return type for the function */
            expr_t *expr;         /**< Expression to return */
        } return_params;

        struct {                  /**< Compound paramaters */
            slist_t stmts;        /**< List of statements */
            typetab_t typetab;    /**< Types and vars defined in scope */
        } compound;

        struct {                  /**< Expression parameters */
            expr_t *expr;         /**< Expression to execute */
        } expr;
    };
};

/**
 * Global declaration
 */
typedef enum gdecl_type_t {
    GDECL_NOP,   /**< No operation, shouldn't exist in valid AST */
    GDECL_FDEFN, /**< Function definition */
    GDECL_DECL,  /**< Declaration */
} gdecl_type_t;

/**
 * Global declaration
 */
struct gdecl_t {
    sl_link_t heap_link;     /**< Allocation Link */
    sl_link_t link;          /**< Storage Link */
    fmark_t mark;            /**< File mark */
    gdecl_type_t type;       /**< Type of gdecl */
    struct decl_t *decl;     /**< Declaration */
    union {
        struct {             /**< Function definition parameters */
            stmt_t *stmt;    /**< Function body */
            htable_t labels; /**< Labels in function */
            slist_t gotos;   /**< Goto statements in function */
        } fdefn;
    };
};


/**
 * Translation unit - Top level AST structure
 */
typedef struct trans_unit_t {
    slist_t gdecls;     /**< List of gdecl in compilation unit */
    typetab_t typetab;  /**< Types defined at top level */
    slist_t gdecl_nodes; /**< (gdecl_t) */
    slist_t stmts;      /**< (stmt_t) */
    slist_t decls;      /**< (decl_t) */
    slist_t decl_nodes; /**< (decl_node_t) */
    slist_t exprs;      /**< (exprs_t) */
    slist_t types;      /**< (types_t) */
} trans_unit_t;

typedef struct struct_iter_t {
    type_t *type;
    sl_link_t *cur_decl;
    decl_t *decl;
    sl_link_t *cur_node;
    decl_node_t *node;
} struct_iter_t;

type_t *ast_type_create(trans_unit_t *tunit, fmark_t *mark, type_type_t type);

expr_t *ast_expr_create(trans_unit_t *tunit, fmark_t *mark, expr_type_t type);

decl_node_t *ast_decl_node_create(trans_unit_t *tunit, fmark_t *mark);

decl_t *ast_decl_create(trans_unit_t *tunit, fmark_t *mark);

stmt_t *ast_stmt_create(trans_unit_t *tunit, fmark_t *mark, stmt_type_t type);

gdecl_t *ast_gdecl_create(trans_unit_t *tunit, fmark_t *mark,
                          gdecl_type_t type);

trans_unit_t *ast_trans_unit_create(bool dummy);

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
 * Print a type
 *
 * @param type The type to print
 */
void ast_print_type(type_t *type);

void struct_iter_init(type_t *type, struct_iter_t *iter);

void struct_iter_reset(struct_iter_t *iter);

bool struct_iter_advance(struct_iter_t *iter);

status_t ast_canonicalize_init_list(trans_unit_t *tunit, type_t *type,
                                    expr_t *expr);

type_t *ast_get_union_type(type_t *type, expr_t *expr, expr_t **head_loc);

/**
 * Gets the size of a type
 *
 * @param type Type to get the size of
 * @return Returns the size of a type. Returns -1 if variable sized array,
 */
size_t ast_type_size(type_t *type);

/**
 * Gets the alignment of a type
 *
 * @param type Type to get the alignment of
 * @return Returns the alignment of the type
 */
size_t ast_type_align(type_t *type);

/**
 * Gets the offset of a member in a type
 *
 * @param type Type to get offset in
 * @param path list of member names
 * @return Returns the offset of the member in the type, or -1 if it isn't a
 *     member
 */
size_t ast_type_offset(type_t *type, mem_acc_list_t *path);

/**
 * Gets the number of a member in a struct/union
 *
 * @param type Type to use
 * @param name Name of the member
 * @return Returns the member number or -1 if it doesn't exist
 */
size_t ast_get_member_num(type_t *type, char *name);

/**
 * Finds the type of a member in a struct or union type
 *
 * @param type Type to find member in
 * @param name Name of the member
 * @param offset location to store offset, NULL if not needed
 * @param mem_num location to store the member number, NULL if not needed
 * @return Return the type of the member, of NULL if doesn't exist
 */
type_t *ast_type_find_member(type_t *type, char *name, size_t *offset,
                             size_t *mem_num);

/**
 * Returns a type with its typedefs removed
 *
 * @param type The type
 * @return The type with its typedefs removed
 */
type_t *ast_type_untypedef(type_t *type);

/**
 * Returns a type with its modifers removed
 *
 * @param type The type
 * @return The type with its modifers removed
 */
type_t *ast_type_unmod(type_t *type);

/**
 * For a type where TYPECHECK_IS_PTR(t) == true, find the base of the pointer
 * type
 *
 * @param t1 pointer type to get base of
 * @return the base of the pointer type
 */
type_t *ast_type_ptr_base(type_t *t1);

/**
 * Get a string of a type modifier
 *
 * @param type_mod The type to get string for
 * @return Returns static string of type_mod
 */
const char *ast_type_mod_str(type_mod_t type_mod);

/**
 * Get a string of a basic type
 *
 * @param type The type to get string for
 * @return Returns static string of basic type
 */
const char *ast_basic_type_str(type_type_t type);

/**
 * Get a string of an operator
 *
 * @param op the string of the given operation
 * @return Returns static string of basic type
 */
const char *ast_oper_str(oper_t op);

#endif /* _AST_H_ */
