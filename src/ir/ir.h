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
// TODO0: Finalize and doc this

#ifndef _IR_H_
#define _IR_H_

#include <stdio.h>

#include "util/dlist.h"
#include "util/htable.h"
#include "util/slist.h"
#include "util/util.h"
#include "util/vector.h"

#include "ir/ir_symtab.h"

typedef struct ir_label_t {
    sl_link_t link;
    char *name;
} ir_label_t;


// order is important here, must be lowest to greatest width
typedef enum ir_float_type_t {
    IR_FLOAT_FLOAT,
    IR_FLOAT_DOUBLE,
    IR_FLOAT_X86_FP80,
} ir_float_type_t;

typedef enum ir_type_type_t {
    IR_TYPE_VOID,
    IR_TYPE_FUNC,
    IR_TYPE_INT,
    IR_TYPE_FLOAT,
    IR_TYPE_PTR,
    IR_TYPE_ARR,
    IR_TYPE_STRUCT,
    IR_TYPE_ID_STRUCT,
    IR_TYPE_OPAQUE,
} ir_type_type_t;

typedef struct ir_type_t ir_type_t;
struct ir_type_t {
    sl_link_t heap_link;
    // No storage link because there are static types
    ir_type_type_t type;

    union {
        struct {
            ir_type_t *type;
            vec_t params; /**< (ir_type_t) */
            bool varargs;
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
            vec_t types; /**< (ir_type_t) Types in the structure */
        } struct_params;

        struct {
            char *name;
            ir_type_t *type;
        } id_struct;
    };
};

typedef struct ir_expr_t ir_expr_t;

typedef enum ir_const_type_t {
    IR_CONST_BOOL,
    IR_CONST_INT,
    IR_CONST_FLOAT,
    IR_CONST_NULL,
    IR_CONST_STRUCT,
    IR_CONST_STR,
    IR_CONST_ARR,
    IR_CONST_ZERO,
} ir_const_type_t;

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
    IR_OP_FREM,
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
    IR_CONVERT_BITCAST,
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

typedef struct ir_expr_label_pair_t {
    sl_link_t link;
    ir_expr_t *expr;
    ir_label_t *label;
} ir_expr_label_pair_t;

typedef enum ir_expr_type_t {
    IR_EXPR_VAR,
    IR_EXPR_CONST,
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
    IR_EXPR_VAARG,
} ir_expr_type_t;

struct ir_expr_t {
    sl_link_t heap_link;
    sl_link_t link;
    ir_expr_type_t type;

    union {
        struct {
            ir_type_t *type;
            char *name;
            bool local;
        } var;

        struct {
            ir_const_type_t ctype;
            ir_type_t *type;
            union {
                bool bool_val;
                long long int_val;
                long double float_val;
                slist_t struct_val; /**< (ir_expr_t) */
                char *str_val;
                slist_t arr_val; /**< (ir_expr_t) */
            };
        } const_params;

        struct {
            ir_oper_t op;
            ir_type_t *type;
            ir_expr_t *expr1;
            ir_expr_t *expr2;
        } binop;

        struct {
            ir_type_t *type;
            ir_type_t *elem_type;
            ir_type_t *nelem_type;
            int nelems;
            int align;
        } alloca;

        struct {
            ir_type_t *type;
            ir_expr_t *ptr;
        } load;

        struct {
            ir_type_t *type;
            ir_type_t *ptr_type;
            ir_expr_t *ptr_val;
            slist_t idxs; /**< ir_expr_t */
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
            slist_t preds; /**< (ir_expr_label_pair_t) */
        } phi;

        struct {
            ir_expr_t *cond;
            ir_type_t *type;
            ir_expr_t *expr1;
            ir_expr_t *expr2;
        } select;

        struct {
            ir_type_t *func_sig;
            ir_expr_t *func_ptr;
            slist_t arglist; /**< (ir_expr_t) */
        } call;

        struct {
            ir_type_t *va_list_type;
            slist_t arglist; /**< (ir_expr_t) */
            ir_type_t *arg_type;
        } vaarg;
    };
};

typedef struct ir_label_node_t {
    sl_link_t node;
    ir_label_t *label;
} ir_label_node_t;

typedef enum ir_stmt_type_t {
    IR_STMT_LABEL,
    IR_STMT_EXPR,
    IR_STMT_RET,
    IR_STMT_BR,
    IR_STMT_SWITCH,
    IR_STMT_INDIR_BR,
    IR_STMT_ASSIGN,
    IR_STMT_STORE,
} ir_stmt_type_t;

typedef struct ir_stmt_t {
    sl_link_t heap_link;
    dl_link_t link;
    ir_stmt_type_t type;

    union {
        ir_label_t *label;
        ir_expr_t *expr;

        struct {
            ir_type_t *type;
            ir_expr_t *val;
        } ret;

        struct {
            ir_expr_t *cond; /**< Condition. NULL if direct branch */
            union {
                struct {
                    ir_label_t *if_true;
                    ir_label_t *if_false;
                };
                ir_label_t *uncond;
            };
        } br;

        struct {
            ir_expr_t *expr;
            slist_t cases; /**< (ir_expr_label_pair_t) */
            ir_label_t *default_case;
        } switch_params;

        // TODO0: Remove if unused
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
            ir_expr_t *ptr;
        } store;
    };
} ir_stmt_t;

typedef struct ir_inst_stream_t {
    dlist_t list; /**< (ir_stmt) */
} ir_inst_stream_t;

typedef enum ir_linkage_t {
    IR_LINKAGE_DEFAULT = 0,
    IR_LINKAGE_PRIVATE,
    IR_LINKAGE_INTERNAL,
    IR_LINKAGE_LINKONCE,
    IR_LINKAGE_WEAK,
    IR_LINKAGE_LINKONCE_ODR,
    IR_LINKAGE_WEAK_ODR,
    IR_LINKAGE_EXTERNAL,
} ir_linkage_t;

typedef enum ir_gdata_flags_t {
    IR_GDATA_CONSTANT     = 1 << 0, // If false, then global
    IR_GDATA_UNNAMED_ADDR = 1 << 1,
} ir_gdata_flags_t;

typedef enum ir_gdecl_type_t {
    IR_GDECL_GDATA,
    IR_GDECL_ID_STRUCT,
    IR_GDECL_FUNC_DECL,
    IR_GDECL_FUNC,
} ir_gdecl_type_t;

typedef struct ir_gdecl_t {
    sl_link_t link;
    ir_gdecl_type_t type;
    ir_linkage_t linkage;

    union {
        struct {
            ir_gdata_flags_t flags;
            ir_type_t *type;
            ir_expr_t *var;
            ir_expr_t *init;
            size_t align;
            ir_inst_stream_t setup;
        } gdata;

        struct {
            char *name;
            ir_type_t *type;
            ir_type_t *id_type;
        } id_struct;

        struct {
            ir_type_t *type;
            char *name;
        } func_decl;

        struct {
            ir_type_t *type;
            char *name;
            slist_t params; /**< (ir_expr_t) List of parameters (not owned) */
            ir_inst_stream_t prefix; /**<  Prefix statements */
            ir_inst_stream_t body;
            ir_symtab_t locals;
            int next_temp; /**< Next temp name */
            int next_label; /**< Next label name */
            ir_label_t *last_label;
        } func;
    };
} ir_gdecl_t;

typedef struct ir_trans_unit_t {
    sl_link_t link;
    slist_t id_structs;
    slist_t decls;
    slist_t funcs;
    ir_symtab_t globals;
    htable_t labels;
    htable_t global_decls; /* (char * -> decl_node_t *) */
    htable_t strings; /* (char * -> ir_expr_t *) */
    int static_num;

    slist_t stmts;
    slist_t exprs;
    slist_t types;
} ir_trans_unit_t;

// Built in types
extern ir_type_t ir_type_void;
extern ir_type_t ir_type_i1;
extern ir_type_t ir_type_i8;
extern ir_type_t ir_type_i16;
extern ir_type_t ir_type_i32;
extern ir_type_t ir_type_i64;
extern ir_type_t ir_type_float;
extern ir_type_t ir_type_double;
extern ir_type_t ir_type_x86_fp80;

extern ir_type_t ir_type_i8_ptr;

#define SWITCH_VAL_TYPE ir_type_i64
#define NELEM_TYPE ir_type_i64
#define BOOL_TYPE ir_type_i8

inline ir_stmt_t *ir_inst_stream_head(ir_inst_stream_t *stream) {
    return dl_head(&stream->list);
}

inline ir_stmt_t *ir_inst_stream_tail(ir_inst_stream_t *stream) {
    return dl_tail(&stream->list);
}

inline void ir_inst_stream_append(ir_inst_stream_t *stream, ir_stmt_t *stmt) {
    dl_append(&stream->list, &stmt->link);
}

void ir_print(FILE *stream, ir_trans_unit_t *irtree, const char *module_name);

ir_type_t *ir_expr_type(ir_expr_t *expr);

bool ir_type_equal(ir_type_t *t1, ir_type_t *t2);

ir_label_t *ir_label_create(ir_trans_unit_t *tunit, char *str);

ir_label_t *ir_numlabel_create(ir_trans_unit_t *tunit, int num);

ir_expr_t *ir_temp_create(ir_trans_unit_t *tunit, ir_gdecl_t *func,
                          ir_type_t *type, int num);

ir_trans_unit_t *ir_trans_unit_create(void);

ir_gdecl_t *ir_gdecl_create(ir_gdecl_type_t type);

ir_stmt_t *ir_stmt_create(ir_trans_unit_t *tunit, ir_stmt_type_t type);

ir_expr_t *ir_expr_create(ir_trans_unit_t *tunit, ir_expr_type_t type);

ir_type_t *ir_type_create(ir_trans_unit_t *tunit, ir_type_type_t type);

void ir_trans_unit_destroy(ir_trans_unit_t *trans_unit);

ir_expr_t *ir_int_const(ir_trans_unit_t *tunit, ir_type_t *type,
                        long long value);

ir_expr_t *ir_expr_zero(ir_trans_unit_t *tunit, ir_type_t *type);

#endif /* _IR_H_ */
