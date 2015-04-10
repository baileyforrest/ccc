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
 * IR tree interface
 *
 * This is designed to be a subset of llvm ir
 *
 * Reference: http://llvm.org/docs/LangRef.html
 */
// TODO: Finalize and doc this

#ifndef _IR_H_
#define _IR_H_

#include "util/slist.h"
#include "util/util.h"

typedef struct ir_label_t {
    sl_link_t link;
    len_str_t str;
    int refcnt;
} ir_label_t;


typedef enum ir_type_type_t {
    IR_VOID,
    IR_FUNC,
    IR_INT,
    IR_FLOAT,
    IR_PTR,
    IR_LABEL,
    IR_ARR,
    IR_STRUCT,
    IR_OPAQUE,
} ir_type_type_t;

typedef enum ir_float_type_t {
    IR_FLOAT_FLOAT,
    IR_FLOAT_DOUBLE,
} ir_float_type_t;

typedef struct ir_type_t ir_type_t;
struct ir_type_t {
    sl_link_t link;
    ir_type_type_t type;

    union {
        struct {
            ir_type_t *type;
            slist_t params;
            int num_params;
            int varargs;
        } func;

        struct {
            int width;
        } int_params;

        struct {
            ir_float_type_t type;
        } float_params;

        struct {
            ir_type_t *base;
        } ptr;

        struct {
            size_t nelems;
            ir_type_t *elem_type;
        } arr;

        struct {
            slist_t types; /**< Types in the structure (ir_type_t) */
        } struct_params;
    };
};

typedef enum ir_const_type_t {
    IR_CONST_BOOL,
    IR_CONST_INT,
    IR_CONST_FLOAT,
    IR_CONST_NULL,
    IR_CONST_GLOBAL,
} ir_const_type_t;

typedef struct ir_const_t ir_const_t;
struct ir_const_t {
    sl_link_t link;
    ir_const_type_t type;

    union {
        bool bool_val;
        long long int_val;
        long double float_val;

        struct {
            ir_type_t *type;
            ir_const_t *val;
        } global;
    };
};

typedef struct ir_expr_t ir_expr_t;
typedef enum ir_oper_t {
    // Binary operations
    IR_OP_ADD,
    IR_OP_FADD,
    IR_OP_SUB,
    IR_OP_FSUB,
    IR_OP_MUL,
    IR_OP_FMUL,
    IR_OP_UDIV,
    IR_OP_SDIV,
    IR_OP_FDIV,
    IR_OP_UREM,
    IR_OP_SREM,
    IR_OP_SHL,
    IR_OP_LSHR,
    IR_OP_ASHR,
    IR_OP_AND,
    IR_OP_OR,
    IR_OP_XOR,
} ir_oper_t;

typedef enum ir_convert_t {
    IR_CONVERT_TRUNC,
    IR_CONVERT_ZEXT,
    IR_CONVERT_SEXT,
    IR_CONVERT_FPTRUNC,
    IR_CONVERT_FPEXT,
    IR_CONVERT_FPTOUI,
    IR_CONVERT_FPTOSI,
    IR_CONVERT_UITOFP,
    IR_CONVERT_SITOFP,
    IR_CONVERT_PTRTOINT,
    IR_CONVERT_INTTOPTR,
} ir_convert_t;

typedef enum ir_icmp_type_t {
    IR_ICMP_EQ,
    IR_ICMP_NE,
    IR_ICMP_UGT,
    IR_ICMP_UGE,
    IR_ICMP_ULT,
    IR_ICMP_ULE,
    IR_ICMP_SGT,
    IR_ICMP_SGE,
    IR_ICMP_SLT,
    IR_ICMP_SLE,
} ir_icmp_type_t;

typedef enum ir_fcmp_type_t {
    IR_FCMP_FALSE,
    IR_FCMP_OEQ,
    IR_FCMP_OGT,
    IR_FCMP_OGE,
    IR_FCMP_OLT,
    IR_FCMP_OLE,
    IR_FCMP_ONE,
    IR_FCMP_ORD,
    IR_FCMP_UEQ,
    IR_FCMP_UGT,
    IR_FCMP_UGE,
    IR_FCMP_ULT,
    IR_FCMP_ULE,
    IR_FCMP_UNE,
    IR_FCMP_UNO,
    IR_FCMP_TRUE,
} ir_fcmp_type_t;

typedef struct ir_type_expr_pair_t {
    ir_type_t *type;
    ir_expr_t *expr;
} ir_type_expr_pair_t;


typedef struct ir_val_label_pair_t {
    sl_link_t link;
    ir_expr_t *val;
    ir_label_t *label;
} ir_case_t;

typedef enum ir_expr_type_t {
    IR_EXPR_BINOP,
    IR_EXPR_ALLOCA,
    IR_EXPR_LOAD,
    IR_EXPR_GETELEMPTR,
    IR_EXPR_CONVERT,
    IR_EXPR_ICMP,
    IR_EXPR_FCMP,
    IR_EXPR_PHI,
    IR_EXPR_SELECT,
    IR_EXPR_CALL,
    IR_EXPR_VA_ARG,
} ir_expr_type_t;

struct ir_expr_t {
    sl_link_t link;
    ir_expr_type_t type;

    union {
        struct {
            ir_oper_t op;
            ir_type_t *type;
            ir_expr_t *expr1;
            ir_expr_t *expr2;
        } binop;

        struct {
            ir_type_t *type;
            ir_type_t *nelem_type;
            int nelems;
            int align;
        } alloca;

        struct {
            ir_type_t *type;
            ir_type_t *ptr_type;
            ir_expr_t *ptr;
        } load;

        struct {
            ir_type_t *type;
            ir_type_t *ptr_type;
            ir_expr_t *ptr_val;
            slist_t idxs; /**< ir_type_expr_pair_t */
        } getelemptr;

        struct {
            ir_convert_t type;
            ir_type_t *src_type;
            ir_expr_t *val;
            ir_type_t *dest_type;
        } convert;

        struct {
            ir_icmp_type_t cond;
            ir_type_t *type;
            ir_expr_t *expr1;
            ir_expr_t *expr2;
        } icmp;

        struct {
            ir_fcmp_type_t cond;
            ir_type_t *type;
            ir_expr_t *expr1;
            ir_expr_t *expr2;
        } fcmp;

        struct {
            ir_type_t *type;
            slist_t preds; /**< (ir_val_label_pair_t) */
        } phi;

        struct {
            ir_type_t *selty;
            ir_expr_t *cond;
            ir_type_t *type1;
            ir_expr_t *expr1;
            ir_type_t *type2;
            ir_expr_t *expr2;
        } select;

        struct {
            ir_type_t *ret_type;
            ir_type_t *func_sig;
            ir_expr_t *func_ptr;
            slist_t arglist; /**< (ir_type_expr_pair_t) */
        } call;

        struct {
            ir_type_t *va_list_type;
            slist_t arglist; /**< (ir_type_expr_pair_t) */
            ir_type_t *arg_type;
        } va_arg;
    };
};

typedef struct ir_label_node_t {
    sl_link_t node;
    ir_label_t *label;
} ir_label_node_t;

typedef enum ir_stmt_type_t {
    IR_STMT_RET,
    IR_STMT_BR,
    IR_STMT_SWITCH,
    IR_STMT_INDIR_BR,
    IR_STMT_ASSIGN,
    IR_STMT_STORE,
    IR_STMT_INTRINSIC_FUNC,
} ir_stmt_type_t;

typedef struct ir_stmt_t {
    sl_link_t link;
    ir_stmt_type_t type;

    union {
        struct {
            ir_type_t *type;
            ir_expr_t *val;
        } ret;

        struct {
            ir_expr_t *cond;
            union {
                struct {
                    ir_label_t *if_true;
                    ir_label_t *if_false;
                };
                ir_label_t *uncond;
            };
        } br;

        struct {
            ir_label_t *label;
            slist_t cases; /**< (ir_val_label_pair_t) */
        } switch_params;

        struct {
            ir_type_t *type;
            ir_expr_t *addr;
            slist_t labels; /**< List of label_node_t */
        } indirectbr;

        struct {
            ir_expr_t *dest;
            ir_expr_t *src;
        } assign;

        struct {
            ir_type_t *type;
            ir_expr_t *val;
            ir_type_t *ptr_type;
            ir_expr_t *ptr;
        } store;

        struct {
            ir_type_t *func_sig;
            len_str_t name;
        } intrinsic_func;
    };
} ir_stmt_t;

#endif /* _IR_H_ */