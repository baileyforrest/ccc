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
 * IR printing functions
 */

#include "ir.h"
#include "ir_priv.h"

#include <assert.h>

#define INDENT "    "
#define DATALAYOUT "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
#define TRIPLE "x86_64-unknown-linux-gnu"

void ir_print(FILE *stream, ir_trans_unit_t *irtree, const char *module_name) {
    assert(stream != NULL);
    assert(irtree != NULL);
    assert(module_name != NULL);

    fprintf(stream, "; ModuleID = '%s'\n", module_name);

    ir_trans_unit_print(stream, irtree);
}

void ir_trans_unit_print(FILE *stream, ir_trans_unit_t *irtree) {
    fprintf(stream, "target datalayout = \"%s\"\n", DATALAYOUT);
    fprintf(stream, "target triple = \"%s\"\n", TRIPLE);
    fprintf(stream, "\n");
    SL_FOREACH(cur, &irtree->id_structs) {
        ir_gdecl_print(stream, GET_ELEM(&irtree->decls, cur));
    }
    fprintf(stream, "\n");
    SL_FOREACH(cur, &irtree->decls) {
        ir_gdecl_print(stream, GET_ELEM(&irtree->decls, cur));
    }
    SL_FOREACH(cur, &irtree->funcs) {
        ir_gdecl_print(stream, GET_ELEM(&irtree->funcs, cur));
    }
}

void ir_gdecl_print(FILE *stream, ir_gdecl_t *gdecl) {
    switch (gdecl->type) {
    case IR_GDECL_GDATA:
        DL_FOREACH(cur, &gdecl->gdata.stmts.list) {
            ir_stmt_print(stream, GET_ELEM(&gdecl->gdata.stmts.list, cur),
                          false);
        }
        break;
    case IR_GDECL_ID_STRUCT:
        fprintf(stream, "%%%s = type ", gdecl->id_struct.name);
        ir_type_print(stream, gdecl->id_struct.type, NULL);
        break;
    case IR_GDECL_FUNC_DECL:
        fprintf(stream, "declare ");
        ir_type_print(stream, gdecl->func_decl.type, gdecl->func_decl.name);
        break;
    case IR_GDECL_FUNC:
        fprintf(stream, "\ndefine ");
        assert(gdecl->func.type->type == IR_TYPE_FUNC);
        ir_type_print(stream, gdecl->func.type->func.type, NULL);
        fprintf(stream, " @%s", gdecl->func.name);
        fprintf(stream, "(");
        SL_FOREACH(cur, &gdecl->func.params) {
            ir_expr_t *expr = GET_ELEM(&gdecl->func.params, cur);
            ir_type_print(stream, ir_expr_type(expr), NULL);
            fprintf(stream, " ");
            ir_expr_print(stream, expr);
            if (expr != sl_tail(&gdecl->func.params)) {
                fprintf(stream, ", ");
            }
        }
        fprintf(stream, ")");

        fprintf(stream, " {\n");
        DL_FOREACH(cur, &gdecl->func.prefix.list) {
            ir_stmt_print(stream, GET_ELEM(&gdecl->func.prefix.list, cur),
                          true);
        }
        DL_FOREACH(cur, &gdecl->func.body.list) {
            ir_stmt_print(stream, GET_ELEM(&gdecl->func.body.list, cur),
                          true);
        }
        fprintf(stream, "}");
        break;
    default:
        assert(false);
    }
    fprintf(stream, "\n");
}

void ir_stmt_print(FILE *stream, ir_stmt_t *stmt, bool indent) {
    if (indent && stmt->type != IR_STMT_LABEL) {
        fprintf(stream, INDENT);
    }
    switch (stmt->type) {
    case IR_STMT_LABEL:
        fprintf(stream, "\n%s:", stmt->label->name);
        break;
    case IR_STMT_EXPR:
        ir_expr_print(stream, stmt->expr);
        break;
    case IR_STMT_RET:
        fprintf(stream, "ret ");
        ir_type_print(stream, stmt->ret.type, NULL);
        if (stmt->ret.val != NULL) {
            fprintf(stream, " ");
            ir_expr_print(stream, stmt->ret.val);
        }
        break;
    case IR_STMT_BR:
        fprintf(stream, "br ");
        if (stmt->br.cond == NULL) {
            fprintf(stream, "label %%%s", stmt->br.uncond->name);
        } else {
            fprintf(stream, "i1 ");
            ir_expr_print(stream, stmt->br.cond);
            fprintf(stream, ", label %%%s, label %%%s",
                    stmt->br.if_true->name, stmt->br.if_false->name);
        }
        break;
    case IR_STMT_SWITCH:
        fprintf(stream, "switch ");
        ir_type_print(stream, &SWITCH_VAL_TYPE, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, stmt->switch_params.expr);
        fprintf(stream, ", label %%%s [ ",
                stmt->switch_params.default_case->name);
        SL_FOREACH(cur, &stmt->switch_params.cases) {
            ir_expr_label_pair_t *pair =
                GET_ELEM(&stmt->switch_params.cases, cur);
            ir_type_print(stream, &SWITCH_VAL_TYPE, NULL);
            ir_expr_print(stream, pair->expr);
            fprintf(stream, " ");
            fprintf(stream, ", label %%%s ", pair->label->name);
        }
        fprintf(stream, "]");
        break;
    case IR_STMT_INDIR_BR:
        // TODO0: Remove if unused
        break;
    case IR_STMT_ASSIGN:
        ir_expr_print(stream, stmt->assign.dest);
        fprintf(stream, " = ");
        ir_expr_print(stream, stmt->assign.src);
        break;
    case IR_STMT_STORE:
        fprintf(stream, "store ");
        ir_type_print(stream, stmt->store.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, stmt->store.val);
        fprintf(stream, ", ");
        ir_type_print(stream, stmt->store.type, NULL);
        fprintf(stream, "* ");
        ir_expr_print(stream, stmt->store.ptr);
        break;
    case IR_STMT_INTRINSIC_FUNC:
        // TODO0: This
        break;
    default:
        assert(false);
    }
    fprintf(stream, "\n");
}

void ir_expr_print(FILE *stream, ir_expr_t *expr) {
    switch (expr->type) {
    case IR_EXPR_VAR:
        expr->var.local ? fprintf(stream, "%%") : fprintf(stream, "@");
        fprintf(stream, "%s", expr->var.name);
        break;
    case IR_EXPR_CONST:
        switch (expr->const_params.ctype) {
        case IR_CONST_BOOL:
            if (expr->const_params.bool_val) {
                fprintf(stream, "true");
            } else {
                fprintf(stream, "false");
            }
            break;
        case IR_CONST_INT:
            fprintf(stream, "%lld", expr->const_params.int_val);
            break;
        case IR_CONST_FLOAT:
            fprintf(stream, "%Lf", expr->const_params.float_val);
            break;
        case IR_CONST_NULL:
            fprintf(stream, "null");

            break;
        case IR_CONST_STRUCT:
            fprintf(stream, "{ ");
            SL_FOREACH(cur, &expr->const_params.struct_val) {
                ir_type_expr_pair_t *pair =
                    GET_ELEM(&expr->const_params.struct_val, cur);
                ir_type_print(stream, pair->type, NULL);
                fprintf(stream, " ");
                ir_expr_print(stream, pair->expr);
                if (pair != sl_tail(&expr->const_params.struct_val)) {
                    fprintf(stream, " ,");
                }
            }
            fprintf(stream, " }");
            break;
        case IR_CONST_STR: {
            // TODO1 Add these as type properties
            fprintf(stream, "private unnamed_addr constant ");
            ir_type_print(stream, expr->const_params.type, NULL);

            // TODO1 Add align as type property
            fprintf(stream, " c\"%s\\00\", align 1",
                    expr->const_params.str_val);
            break;
        }
        case IR_CONST_ARR: {
            fprintf(stream, "[ ");
            assert(expr->const_params.type->type == IR_TYPE_ARR);
            ir_type_t *elem_type = expr->const_params.type->arr.elem_type;
            SL_FOREACH(cur, &expr->const_params.struct_val) {
                ir_expr_t *elem = GET_ELEM(&expr->const_params.arr_val, cur);
                ir_type_print(stream, elem_type, NULL);
                fprintf(stream, " ");
                ir_expr_print(stream, elem);
                if (elem != sl_tail(&expr->const_params.arr_val)) {
                    fprintf(stream, " ,");
                }
            }
            fprintf(stream, " ]");
            break;
        }
        case IR_CONST_ZERO:
            fprintf(stream, "zeroinitializer");
            break;
        default:
            assert(false);
        }
        break;
    case IR_EXPR_BINOP: {
        fprintf(stream, "%s ", ir_oper_str(expr->binop.op));
        ir_type_print(stream, expr->binop.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->binop.expr1);
        fprintf(stream, ", ");
        ir_expr_print(stream, expr->binop.expr2);
        break;
    }
    case IR_EXPR_ALLOCA:
        fprintf(stream, "alloca ");
        ir_type_print(stream, expr->alloca.type, NULL);
        if (expr->alloca.nelem_type != NULL) {
            fprintf(stream, ", ");
            ir_type_print(stream, expr->alloca.nelem_type, NULL);
            fprintf(stream, " %d", expr->alloca.nelems);
        }
        if (expr->alloca.align != 0) {
            fprintf(stream, ", align %d", expr->alloca.align);
        }
        break;
    case IR_EXPR_LOAD:
        fprintf(stream, "load ");
        ir_type_print(stream, expr->load.type, NULL);
        fprintf(stream, "* ");
        ir_expr_print(stream, expr->load.ptr);
        break;
    case IR_EXPR_GETELEMPTR:
        fprintf(stream, "getelementptr ");
        ir_type_print(stream, expr->getelemptr.ptr_type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->getelemptr.ptr_val);
        fprintf(stream, ", ");
        SL_FOREACH(cur, &expr->getelemptr.idxs) {
            ir_type_expr_pair_t *pair = GET_ELEM(&expr->getelemptr.idxs, cur);
            ir_type_print(stream, pair->type, NULL);
            fprintf(stream, " ");
            ir_expr_print(stream, pair->expr);
            if (pair != sl_tail(&expr->getelemptr.idxs)) {
                fprintf(stream, ", ");
            }
        }
        break;
    case IR_EXPR_CONVERT:
        fprintf(stream, "%s ", ir_convert_str(expr->convert.type));
        ir_type_print(stream, expr->convert.src_type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->convert.val);
        fprintf(stream, " to ");
        ir_type_print(stream, expr->convert.dest_type, NULL);
        break;
    case IR_EXPR_ICMP:
        fprintf(stream, "icmp %s ", ir_icmp_str(expr->icmp.cond));
        ir_type_print(stream, expr->icmp.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->icmp.expr1);
        fprintf(stream, ", ");
        ir_expr_print(stream, expr->icmp.expr2);
        break;
    case IR_EXPR_FCMP:
        fprintf(stream, "fcmp %s ", ir_icmp_str(expr->fcmp.cond));
        ir_type_print(stream, expr->fcmp.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->fcmp.expr1);
        fprintf(stream, ", ");
        ir_expr_print(stream, expr->fcmp.expr2);
        break;
    case IR_EXPR_PHI: {
        fprintf(stream, "phi ");
        ir_type_print(stream, expr->phi.type, NULL);
        fprintf(stream, " ");
        SL_FOREACH(cur, &expr->phi.preds) {
            ir_expr_label_pair_t *pair = GET_ELEM(&expr->phi.preds, cur);
            fprintf(stream, "[ ");
            ir_expr_print(stream, pair->expr);
            fprintf(stream, ", %%%s", pair->label->name);
            fprintf(stream, " ]");
            if (pair != sl_tail(&expr->phi.preds)) {
                fprintf(stream, ", ");
            }
        }
        break;
    }
    case IR_EXPR_SELECT:
        fprintf(stream, "select i1 ");
        ir_expr_print(stream, expr->select.cond);
        fprintf(stream, ", ");
        ir_type_print(stream, expr->select.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->select.expr1);
        fprintf(stream, ", ");
        ir_type_print(stream, expr->select.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->select.expr1);
        break;
    case IR_EXPR_CALL: {
        assert(expr->call.func_sig->type == IR_TYPE_FUNC);
        ir_type_t *func_sig = expr->call.func_sig;
        fprintf(stream, "call ");
        ir_type_print(stream, func_sig->func.type, NULL);
        fprintf(stream, " ");
        ir_expr_print(stream, expr->call.func_ptr);
        fprintf(stream, "(");

        SL_FOREACH(cur, &expr->call.arglist) {
            ir_type_expr_pair_t *pair = GET_ELEM(&expr->call.arglist, cur);
            ir_type_print(stream, pair->type, NULL);
            fprintf(stream, " ");
            ir_expr_print(stream, pair->expr);
            if (pair != sl_tail(&expr->call.arglist)) {
                fprintf(stream, ", ");
            }
        }
        fprintf(stream, ")");
        break;
    }
    case IR_EXPR_VAARG:
        // TODO0: This
        break;
    default:
        assert(false);
    }
}

void ir_type_print(FILE *stream, ir_type_t *type, char *func_name) {
    switch (type->type) {
    case IR_TYPE_VOID:
        fprintf(stream, "void");
        break;
    case IR_TYPE_FUNC:
        ir_type_print(stream, type->func.type, NULL);
        if (func_name != NULL) {
            fprintf(stream, " @%s", func_name);
        }
        fprintf(stream, "(");
        VEC_FOREACH(cur, &type->func.params) {
            ir_type_t *arg = vec_get(&type->func.params, cur);
            ir_type_print(stream, arg, NULL);
            if (cur != vec_size(&type->func.params) - 1) {
                fprintf(stream, ", ");
            }
        }
        fprintf(stream, ")");
        break;
    case IR_TYPE_INT:
        fprintf(stream, "i%d", type->int_params.width);
        break;
    case IR_TYPE_FLOAT:
        fprintf(stream, "%s", ir_float_type_str(type->float_params.type));
        break;
    case IR_TYPE_PTR:
        ir_type_print(stream, type->ptr.base, NULL);
        fprintf(stream, "*");
        break;
    case IR_TYPE_ARR:
        fprintf(stream, "[%zu x ", type->arr.nelems);
        ir_type_print(stream, type->arr.elem_type, NULL);
        fprintf(stream, "]");
        break;
    case IR_TYPE_STRUCT: {
        fprintf(stream, "{ ");
        VEC_FOREACH(cur, &type->struct_params.types) {
            ir_type_t *elem = vec_get(&type->struct_params.types, cur);
            ir_type_print(stream, elem, NULL);
            if (cur != vec_size(&type->struct_params.types) - 1) {
                fprintf(stream, ", ");
            }
        }
        fprintf(stream, " }");
        break;
    }
    case IR_TYPE_ID_STRUCT: {
        fprintf(stream, "%%%s", type->id_struct.name);
        break;
    }
    case IR_TYPE_OPAQUE:
        fprintf(stream, "type opaque");
        break;
    default:
        assert(false);
    }
}

const char *ir_oper_str(ir_oper_t op) {
    switch (op) {
    case IR_OP_ADD:  return "add";
    case IR_OP_FADD: return "fadd";
    case IR_OP_SUB:  return "sub";
    case IR_OP_FSUB: return "fsub";
    case IR_OP_MUL:  return "mul";
    case IR_OP_FMUL: return "fmul";
    case IR_OP_UDIV: return "udiv";
    case IR_OP_SDIV: return "sdiv";
    case IR_OP_FDIV: return "fdiv";
    case IR_OP_UREM: return "urem";
    case IR_OP_SREM: return "srem";
    case IR_OP_FREM: return "frem";
    case IR_OP_SHL:  return "shl";
    case IR_OP_LSHR: return "lshr";
    case IR_OP_ASHR: return "ashr";
    case IR_OP_AND:  return "and";
    case IR_OP_OR:   return "or";
    case IR_OP_XOR:  return "xor";
    default:
        assert(false);
    }
    return NULL;
}

const char *ir_convert_str(ir_convert_t conv) {
    switch (conv) {
    case IR_CONVERT_TRUNC:    return "trunc";
    case IR_CONVERT_ZEXT:     return "zext";
    case IR_CONVERT_SEXT:     return "sext";
    case IR_CONVERT_FPTRUNC:  return "fptruc";
    case IR_CONVERT_FPEXT:    return "fpext";
    case IR_CONVERT_FPTOUI:   return "fptoui";
    case IR_CONVERT_FPTOSI:   return "fptosi";
    case IR_CONVERT_UITOFP:   return "uitopf";
    case IR_CONVERT_SITOFP:   return "sitofp";
    case IR_CONVERT_PTRTOINT: return "ptrtoint";
    case IR_CONVERT_INTTOPTR: return "inttoptr";
    case IR_CONVERT_BITCAST:  return "bitcast";
    default:
        assert(false);
    }

    return NULL;
}

const char *ir_icmp_str(ir_icmp_type_t conv) {
    switch (conv) {
    case IR_ICMP_EQ:  return "eq";
    case IR_ICMP_NE:  return "ne";
    case IR_ICMP_UGT: return "ugt";
    case IR_ICMP_UGE: return "uge";
    case IR_ICMP_ULT: return "ult";
    case IR_ICMP_ULE: return "ule";
    case IR_ICMP_SGT: return "sgt";
    case IR_ICMP_SGE: return "sge";
    case IR_ICMP_SLT: return "slt";
    case IR_ICMP_SLE: return "sle";
    default:
        assert(false);
    }
    return NULL;
}

const char *ir_fcmp_str(ir_fcmp_type_t conv) {
    switch (conv) {
    case IR_FCMP_FALSE: return "false";
    case IR_FCMP_OEQ:   return "oeq";
    case IR_FCMP_OGT:   return "ogt";
    case IR_FCMP_OGE:   return "oge";
    case IR_FCMP_OLT:   return "olt";
    case IR_FCMP_OLE:   return "ole";
    case IR_FCMP_ONE:   return "one";
    case IR_FCMP_ORD:   return "ord";
    case IR_FCMP_UEQ:   return "ueq";
    case IR_FCMP_UGT:   return "ugt";
    case IR_FCMP_UGE:   return "uge";
    case IR_FCMP_ULT:   return "ule";
    case IR_FCMP_ULE:   return "ule";
    case IR_FCMP_UNE:   return "une";
    case IR_FCMP_UNO:   return "uno";
    case IR_FCMP_TRUE:  return "true";
    default:
        assert(false);
    }

    return NULL;
}

const char *ir_float_type_str(ir_float_type_t ftype) {
    switch (ftype) {
    case IR_FLOAT_FLOAT:    return "float";
    case IR_FLOAT_DOUBLE:   return "double";
    case IR_FLOAT_X86_FP80: return "x86_fp80";
    default:
        assert(false);
    }
    return NULL;
}
