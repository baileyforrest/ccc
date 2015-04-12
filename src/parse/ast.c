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
 * AST function implementation
 */

#include "ast.h"
#include "ast_priv.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "typecheck/typechecker.h"

#define INDENT ("    ")
#define INDENT_LEN (sizeof(INDENT) - 1)

#define PRINT_INDENT(indent)                    \
    do {                                        \
        for (int i = 0; i < indent; ++i) {      \
            printf(INDENT);                     \
        }                                       \
    } while (0)

void ast_directed_print(char **buf, size_t *remain, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (buf == NULL) {
        vprintf(fmt, ap);
    } else {
        size_t printed = vsnprintf(*buf, *remain, fmt, ap);
        *remain -= printed;
        *buf += printed;
    }
}

void ast_print(trans_unit_t *ast) {
    ast_trans_unit_print(ast);
}

void ast_print_type(type_t *type) {
    ast_type_print(type, 0, NULL, NULL);
}

void ast_destroy(trans_unit_t *ast) {
    ast_trans_unit_destroy(ast);
}

size_t ast_type_size(type_t *type) {
    switch (type->type) {
    case TYPE_VOID:        return sizeof(void);
    case TYPE_BOOL:        return sizeof(bool);
    case TYPE_CHAR:        return sizeof(char);
    case TYPE_SHORT:       return sizeof(short);
    case TYPE_INT:         return sizeof(int);
    case TYPE_LONG:        return sizeof(long);
    case TYPE_LONG_LONG:   return sizeof(long long);
    case TYPE_FLOAT:       return sizeof(float);
    case TYPE_DOUBLE:      return sizeof(double);
    case TYPE_LONG_DOUBLE: return sizeof(long double);

        // TODO: Handle bit fields
    case TYPE_STRUCT:
    case TYPE_UNION: {
        size_t size = 0;
        sl_link_t *cur;
        SL_FOREACH(cur, &type->struct_params.decls) {
            decl_t *decl = GET_ELEM(&type->struct_params.decls, cur);

            sl_link_t *icur;
            SL_FOREACH(icur, &decl->decls) {
                decl_node_t *decl_node = GET_ELEM(&decl->decls, cur);
                if (type->type == TYPE_STRUCT) {
                    size += ast_type_size(decl_node->type);
                } else { // type->type == TYPE_UNION
                    size = MAX(size, ast_type_size(decl_node->type));
                }
            }
        }
        return size;
    }
    case TYPE_ENUM:
        return ast_type_size(type->enum_params.type);

    case TYPE_TYPEDEF:
        return ast_type_size(type->typedef_params.base);

    case TYPE_MOD:
        return ast_type_size(type->mod.base);

    case TYPE_PAREN:
        return ast_type_size(type->paren_base);
    case TYPE_FUNC:
        return sizeof(ast_type_size);
    case TYPE_ARR: {
        size_t size = ast_type_size(type->arr.base);
        long long len;
        if (!typecheck_const_expr(type->arr.len, &len)) {
            return -1;
        }
        return size * len;
    }
    case TYPE_PTR:
        return sizeof(void *);
    default:
        assert(false);
    }

    return 0;
}

size_t ast_type_align(type_t *type) {
    switch (type->type) {
    case TYPE_VOID:        return alignof(void);
    case TYPE_BOOL:        return alignof(bool);
    case TYPE_CHAR:        return alignof(char);
    case TYPE_SHORT:       return alignof(short);
    case TYPE_INT:         return alignof(int);
    case TYPE_LONG:        return alignof(long);
    case TYPE_LONG_LONG:   return alignof(long long);
    case TYPE_FLOAT:       return alignof(float);
    case TYPE_DOUBLE:      return alignof(double);
    case TYPE_LONG_DOUBLE: return alignof(long double);

    case TYPE_STRUCT:
    case TYPE_UNION: {
        size_t align = 0;
        sl_link_t *cur;
        SL_FOREACH(cur, &type->struct_params.decls) {
            decl_t *decl = GET_ELEM(&type->struct_params.decls, cur);

            sl_link_t *icur;
            SL_FOREACH(icur, &decl->decls) {
                decl_node_t *decl_node = GET_ELEM(&decl->decls, cur);
                align = MAX(align, ast_type_align(decl_node->type));
            }
        }
        return align;
    }
    case TYPE_ENUM:
        return ast_type_align(type->enum_params.type);

    case TYPE_TYPEDEF:
        return ast_type_align(type->typedef_params.base);

    case TYPE_MOD:
        return ast_type_align(type->mod.base);

    case TYPE_PAREN:
        return ast_type_align(type->paren_base);
    case TYPE_FUNC:
        return alignof(ast_type_align);
    case TYPE_ARR:
        return ast_type_align(type->arr.base);
    case TYPE_PTR:
        return alignof(void *);
    default:
        assert(false);
    }

    return 0;
}

size_t ast_type_offset(type_t *type, slist_t path) {
    (void)type;
    (void)path;
    // TODO: This
    return 0;
}

int ast_get_member_num(type_t *type, len_str_t *name) {
    // TODO: This
    (void)type;
    (void)name;
    return 0;
}

void ast_trans_unit_print(trans_unit_t *tras_unit) {
    sl_link_t *cur;
    SL_FOREACH(cur, &tras_unit->gdecls) {
        ast_gdecl_print(GET_ELEM(&tras_unit->gdecls, cur));
    }
}

void ast_gdecl_print(gdecl_t *gdecl) {
    ast_decl_print(gdecl->decl, TYPE_VOID, 0, NULL, NULL);
    switch (gdecl->type) {
    case GDECL_FDEFN:
        printf("\n");
        ast_stmt_print(gdecl->fdefn.stmt, 0);
        break;
    case GDECL_DECL:
        printf(";");
        break;
    case GDECL_NOP: // Should never have GDECL_NOP in properly formed AST
    default:
        assert(false);
    }
    printf("\n\n");
}

#define PRINT_CMPD_CUR_INDENT(stmt, indent)                     \
    do {                                                        \
        if ((stmt)->type == STMT_COMPOUND) {                    \
            ast_stmt_print(stmt, indent);                       \
        } else {                                                \
            ast_stmt_print(stmt, (indent) + 1);                 \
        }                                                       \
    } while (0)


void ast_stmt_print(stmt_t *stmt, int indent) {
    switch (stmt->type) {
    case STMT_LABEL:
    case STMT_CASE:
    case STMT_DEFAULT:
        PRINT_INDENT(indent - 1);
        break;
    default:
        PRINT_INDENT(indent);
    }

    bool print_newline = true;

    switch(stmt->type) {
    case STMT_NOP:
        printf(";");
        break;

    case STMT_DECL:
        ast_decl_print(stmt->decl, TYPE_VOID, indent, NULL, NULL);
        printf(";");
        break;

    case STMT_LABEL:
        printf("%s:\n", stmt->label.label->str);
        ast_stmt_print(stmt->label.stmt, indent);
        break;
    case STMT_CASE:
        printf("case ");
        ast_expr_print(stmt->case_params.val, 0, NULL, NULL);
        printf(":\n");
        PRINT_CMPD_CUR_INDENT(stmt->case_params.stmt, indent - 1);
        print_newline = false;
        break;
    case STMT_DEFAULT:
        printf("default:\n");
        ast_stmt_print(stmt->default_params.stmt, indent);
        print_newline = false;
        break;

    case STMT_IF:
        printf("if (");
        ast_expr_print(stmt->if_params.expr, 0, NULL, NULL);
        printf(")\n");
        PRINT_CMPD_CUR_INDENT(stmt->if_params.true_stmt, indent);
        if (stmt->if_params.false_stmt) {
            PRINT_INDENT(indent);
            printf("else\n");
            if (stmt->if_params.false_stmt->type == STMT_COMPOUND ||
                stmt->if_params.false_stmt->type == STMT_IF) {
                ast_stmt_print(stmt->if_params.false_stmt, indent);
            } else {
                ast_stmt_print(stmt->if_params.false_stmt, indent + 1);
            }
        }
        break;
    case STMT_SWITCH:
        printf("switch (");
        ast_expr_print(stmt->switch_params.expr, 0, NULL, NULL);
        printf(")\n");
        PRINT_CMPD_CUR_INDENT(stmt->switch_params.stmt, indent);
        break;

    case STMT_DO:
        printf("do\n");
        PRINT_CMPD_CUR_INDENT(stmt->do_params.stmt, indent);
        PRINT_INDENT(indent);
        printf("while (");
        ast_expr_print(stmt->do_params.expr, 0, NULL, NULL);
        printf(");\n");
        break;
    case STMT_WHILE:
        printf("while (");
        ast_expr_print(stmt->while_params.expr, 0, NULL, NULL);
        printf(")\n");
        PRINT_CMPD_CUR_INDENT(stmt->while_params.stmt, indent);
        break;
    case STMT_FOR:
        printf("for (");
        if (stmt->for_params.expr1 != NULL) {
            ast_expr_print(stmt->for_params.expr1, 0, NULL, NULL);
        }
        if (stmt->for_params.decl1 != NULL) {
            ast_decl_print(stmt->for_params.decl1, TYPE_VOID, 0, NULL, NULL);
        }
        printf("; ");
        if (stmt->for_params.expr2 != NULL) {
            ast_expr_print(stmt->for_params.expr2, 0, NULL, NULL);
        }
        printf("; ");
        if (stmt->for_params.expr3 != NULL) {
            ast_expr_print(stmt->for_params.expr3, 0, NULL, NULL);
        }
        printf(")\n");
        PRINT_CMPD_CUR_INDENT(stmt->for_params.stmt, indent);
        break;

    case STMT_GOTO:
        printf("goto %s;", stmt->goto_params.label->str);
        break;
    case STMT_CONTINUE:
        printf("continue;");
        break;
    case STMT_BREAK:
        printf("break;");
        break;
    case STMT_RETURN:
        printf("return ");
        if (stmt->return_params.expr != NULL) {
            ast_expr_print(stmt->return_params.expr, 0, NULL, NULL);
        }
        printf(";");
        break;

    case STMT_COMPOUND: {
        printf("{\n");
        sl_link_t *cur;
        SL_FOREACH(cur, &stmt->compound.stmts) {
            ast_stmt_print(GET_ELEM(&stmt->compound.stmts, cur), indent + 1);
        }
        PRINT_INDENT(indent);
        printf("}");
        break;
    }

    case STMT_EXPR:
        ast_expr_print(stmt->expr.expr, indent, NULL, NULL);
        printf(";");
        break;

    default:
        assert(false);
    }
    if (print_newline) {
        printf("\n");
    }
}

void ast_decl_print(decl_t *decl, basic_type_t type, int indent, char **dest,
                    size_t *remain) {
    ast_type_print(decl->type, indent, dest, remain);

    bool first = true;
    sl_link_t *cur;
    SL_FOREACH(cur, &decl->decls) {
        if (first) {
            ast_directed_print(dest, remain, " ");
            first = false;
        } else {
            ast_directed_print(dest, remain, ", ");
        }
        decl_node_t *node = GET_ELEM(&decl->decls, cur);
        ast_decl_node_print(node, node->type, dest, remain);
        if (node->expr != NULL) {
            switch (type) {
            case TYPE_STRUCT:
            case TYPE_UNION:
                ast_directed_print(dest, remain, " : ");
                break;
            default:
                ast_directed_print(dest, remain, " = ");
                break;
            }
            ast_expr_print(node->expr, indent, dest, remain);
        }
    }
    if (type == TYPE_STRUCT || type == TYPE_UNION) {
        ast_directed_print(dest, remain, ";\n");
    }
}

void ast_decl_node_print(decl_node_t *decl_node, type_t *type, char **dest,
                         size_t *remain) {
    switch (type->type) {
    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:
    case TYPE_PAREN:
        break;
    default:
        if (decl_node->id != NULL) {
            // No special decl node modifiers, just print
            ast_directed_print(dest, remain, "%.*s", decl_node->id->len,
                               decl_node->id->str);
        }
        return;
    }
    char print_buf[PRINT_BUF_SIZE];

    slist_t accum, temp;
    sl_init(&accum, offsetof(len_str_node_t, link));
    sl_init(&temp, offsetof(len_str_node_t, link));

    len_str_node_t *node = NULL;
    if (decl_node->id != NULL) {
        node = malloc(sizeof(*node) + decl_node->id->len + 1);
        if (node == NULL) {
            goto fail;
        }
        node->str.str = (char *)node + sizeof(*node);
        strncpy(node->str.str, decl_node->id->str, decl_node->id->len);
        node->str.str[decl_node->id->len] = '\0';
        node->str.len = decl_node->id->len;
        sl_append(&accum, &node->link);
    }

#define OFFSET (PRINT_BUF_SIZE - remain - 1)
#define PUT_CUR(chr) *(cur++) = (chr); remain--

#define ALLOC_COPY_NODE()                                               \
    do {                                                                \
        size_t len = OFFSET;                                            \
        if (NULL ==                                                     \
            (node = malloc(sizeof(*node) + len + 1))) {                 \
            goto fail;                                                  \
        }                                                               \
        node->str.str = (char *)node + sizeof(*node);                   \
        strncpy(node->str.str, print_buf, len);                         \
        node->str.str[len] = '\0';                                      \
        node->str.len = len;                                            \
    } while (0)

    while(type != NULL) {
        size_t remain = PRINT_BUF_SIZE - 1;
        char *cur = print_buf;
        switch (type->type) {
        case TYPE_PAREN:
            PUT_CUR('(');
            ALLOC_COPY_NODE();
            sl_prepend(&accum, &node->link);

            remain = PRINT_BUF_SIZE - 1;
            cur = print_buf;
            PUT_CUR(')');
            ALLOC_COPY_NODE();
            sl_append(&accum, &node->link);
            type = type->paren_base;
            break;
        case TYPE_FUNC: {
            PUT_CUR('(');
            bool first = true;
            sl_link_t *cur_link;
            SL_FOREACH(cur_link, &type->func.params) {
                if (remain == 0) {
                    continue;
                }
                if (first) {
                    first = false;
                } else {
                    PUT_CUR(',');
                    PUT_CUR(' ');
                }
                ast_decl_print(GET_ELEM(&type->func.params, cur_link),
                               TYPE_VOID, 0, &cur, &remain);
            }
            if (type->func.varargs) {
                PUT_CUR(',');
                PUT_CUR(' ');
                PUT_CUR('.');
                PUT_CUR('.');
                PUT_CUR('.');
            }
            if (remain > 0) {
                PUT_CUR(')');
            }

            ALLOC_COPY_NODE();
            sl_append(&accum, &node->link);
            type = type->func.type;
            break;
        }
        case TYPE_ARR:
            PUT_CUR('[');
            if (type->arr.len != NULL) {
                ast_expr_print(type->arr.len, 0, &cur, &remain);
            }
            if (remain > 0) {
                PUT_CUR(']');
            }

            ALLOC_COPY_NODE();
            sl_append(&accum, &node->link);
            type = type->arr.base;

            break;
        case TYPE_PTR:
            for (;type->type == TYPE_PTR; type = type->ptr.base) {
                size_t remain = PRINT_BUF_SIZE - 1;
                char *cur = print_buf;
                PUT_CUR('*');
                ast_type_mod_print(type->ptr.type_mod, &cur, &remain);
                ALLOC_COPY_NODE();
                sl_append(&temp, &node->link);
            }
            sl_concat_front(&accum, &temp);
            break;
        default:
            type = NULL;
        }
    }

    sl_link_t *cur;
    SL_FOREACH(cur, &accum) {
        len_str_node_t *node = GET_ELEM(&accum, cur);
        ast_directed_print(dest, remain, "%.*s", node->str.len,
                           node->str.str);
    }
    SL_DESTROY_FUNC(&accum, free);
    return;
fail:
    printf("%s: Out of memory", __func__);
}

void ast_expr_print(expr_t *expr, int indent, char **dest, size_t *remain) {
    switch (expr->type) {
    case EXPR_VOID:
        break;
    case EXPR_PAREN:
        ast_directed_print(dest, remain, "(");
        ast_expr_print(expr->paren_base, 0, dest, remain);
        ast_directed_print(dest, remain, ")");
        break;
    case EXPR_VAR:
        ast_directed_print(dest, remain, "%s", expr->var_id->str);
        break;
    case EXPR_ASSIGN:
        ast_expr_print(expr->assign.dest, 0, dest, remain);
        ast_directed_print(dest, remain, " ");
        ast_oper_print(expr->assign.op, dest, remain);
        ast_directed_print(dest, remain, "= ");
        ast_expr_print(expr->assign.expr, 0, dest, remain);
        break;
    case EXPR_CONST_INT:
        ast_directed_print(dest, remain, "%lld", expr->const_val.int_val);
        switch (expr->const_val.type->type) {
        case TYPE_LONG:
            ast_directed_print(dest, remain, "L");
            break;
        case TYPE_LONG_LONG:
            ast_directed_print(dest, remain, "LL");
            break;
        case TYPE_MOD:
            ast_directed_print(dest, remain, "U");
            if (expr->const_val.type->mod.base->type == TYPE_LONG) {
                ast_directed_print(dest, remain, "L");
            }
            if (expr->const_val.type->mod.base->type == TYPE_LONG_LONG) {
                ast_directed_print(dest, remain, "LL");
            }
            break;
        default:
            break;
        }
        break;
    case EXPR_CONST_FLOAT:
        ast_directed_print(dest, remain, "%f", expr->const_val.float_val);
        if (expr->const_val.type->type == TYPE_MOD) {
            ast_directed_print(dest, remain, "f");
        }
        break;
    case EXPR_CONST_STR:
        ast_directed_print(dest, remain, "\"%s\"",
                           expr->const_val.str_val->str);
        break;
    case EXPR_BIN:
        ast_expr_print(expr->bin.expr1, 0, dest, remain);
        ast_directed_print(dest, remain, " ");
        ast_oper_print(expr->bin.op, dest, remain);
        ast_directed_print(dest, remain, " ");
        ast_expr_print(expr->bin.expr2, 0, dest, remain);
        break;
    case EXPR_UNARY:
        switch (expr->unary.op) {
        case OP_POSTINC:
        case OP_POSTDEC:
            ast_expr_print(expr->unary.expr, 0, dest, remain);
            ast_oper_print(expr->unary.op, dest, remain);
            break;
        default:
            ast_oper_print(expr->unary.op, dest, remain);
            ast_expr_print(expr->unary.expr, 0, dest, remain);
            break;
        }
        break;
    case EXPR_COND:
        ast_expr_print(expr->cond.expr1, 0, dest, remain);
        ast_directed_print(dest, remain, " ? ");
        ast_expr_print(expr->cond.expr2, 0, dest, remain);
        ast_directed_print(dest, remain, " : ");
        ast_expr_print(expr->cond.expr3, 0, dest, remain);
        break;
    case EXPR_CAST:
        ast_directed_print(dest, remain, "(");
        ast_decl_print(expr->cast.cast, TYPE_VOID, 0, dest, remain);
        ast_directed_print(dest, remain, ")");
        ast_expr_print(expr->cast.base, 0, dest, remain);
        break;
    case EXPR_CALL:
        ast_expr_print(expr->call.func, 0, dest, remain);
        ast_directed_print(dest, remain, "(");
        bool first = true;
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->call.params) {
            if (first) {
                first = false;
            } else {
                ast_directed_print(dest, remain, ", ");
            }
            ast_expr_print(GET_ELEM(&expr->call.params, cur), 0, dest, remain);
        }
        ast_directed_print(dest, remain, ")");
        break;
    case EXPR_CMPD: {
        bool first = true;
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            if (first) {
                first = false;
            } else {
                ast_directed_print(dest, remain, ", ");
            }
            ast_expr_print(GET_ELEM(&expr->cmpd.exprs, cur), 0, dest, remain);
        }
        break;
    }
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        if (expr->type == EXPR_SIZEOF) {
            ast_directed_print(dest, remain, "sizeof");
        } else { // expr->type == EXPR_ALIGNOF
            ast_directed_print(dest, remain, "_Alignof");
        }
        if (expr->sizeof_params.type != NULL) {
            ast_directed_print(dest, remain, "(");
            ast_decl_print(expr->sizeof_params.type, TYPE_VOID, 0, dest,
                           remain);
            ast_directed_print(dest, remain, ")");
        } else {
            ast_expr_print(expr->sizeof_params.expr, 0, dest, remain);
        }
        break;
    case EXPR_OFFSETOF: {
        ast_directed_print(dest, remain, "__builtin_offsetof(");
        ast_decl_print(expr->offsetof_params.type, TYPE_VOID, 0, dest, remain);
        ast_directed_print(dest, remain, ", ");
        bool first = true;
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->offsetof_params.path) {
            if (!first) {
                ast_directed_print(dest, remain, ".");
            }
            len_str_node_t *node = GET_ELEM(&expr->offsetof_params.path, cur);
            ast_directed_print(dest, remain, "%.*s", node->str.len,
                               node->str.str);
            first = false;
        }
        ast_directed_print(dest, remain, ")");
        break;
    }
    case EXPR_MEM_ACC:
        ast_expr_print(expr->mem_acc.base, 0, dest, remain);
        ast_oper_print(expr->mem_acc.op, dest, remain);
        ast_directed_print(dest, remain, "%s", expr->mem_acc.name->str, dest,
                    remain);
        break;
    case EXPR_ARR_IDX:
        ast_expr_print(expr->arr_idx.array, 0, dest, remain);
        ast_directed_print(dest, remain, "[");
        ast_expr_print(expr->arr_idx.index, 0, dest, remain);
        ast_directed_print(dest, remain, "]");
        break;
    case EXPR_INIT_LIST: {
        ast_directed_print(dest, remain, "{\n");
        bool first = true;
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            if (first) {
                first = false;
            } else {
                ast_directed_print(dest, remain, ",\n");
            }
            PRINT_INDENT(indent + 1);
            ast_expr_print(GET_ELEM(&expr->cmpd.exprs, cur), indent + 1,
                           dest, remain);
        }
        ast_directed_print(dest, remain, "\n");
        PRINT_INDENT(indent);
        ast_directed_print(dest, remain, "}");
        break;
    }
    case EXPR_DESIG_INIT:
        ast_directed_print(dest, remain, ".%.*s = ", expr->desig_init.name->len,
                           expr->desig_init.name->str);
        ast_expr_print(expr->desig_init.val, indent,
                       dest, remain);
        break;

    default:
        assert(false);
    }
}

const char *ast_oper_str(oper_t op) {
    switch (op) {
    case OP_NOP:      return "";

    case OP_PLUS:
    case OP_UPLUS:    return "+";

    case OP_MINUS:
    case OP_UMINUS:   return "-";

    case OP_TIMES:
    case OP_DEREF:    return "*";

    case OP_DIV:      return "/";
    case OP_MOD:      return "%%";
    case OP_LT:       return "<";
    case OP_LE:       return "<=";
    case OP_GT:       return ">";
    case OP_GE:       return ">=";
    case OP_EQ:       return "==";
    case OP_NE:       return "!=";

    case OP_BITAND:
    case OP_ADDR:     return "&";

    case OP_BITXOR:   return "^";
    case OP_BITOR:    return "|";
    case OP_LSHIFT:   return "<<";
    case OP_RSHIFT:   return ">>";
    case OP_LOGICNOT: return "!";
    case OP_LOGICAND: return "&&";
    case OP_LOGICOR:  return "||";
    case OP_BITNOT:   return "~";

    case OP_PREINC:
    case OP_POSTINC:  return "++";

    case OP_PREDEC:
    case OP_POSTDEC:  return "--";

    case OP_ARROW:    return "->";
    case OP_DOT:      return ".";
    default:
        assert(false);
    }
    return NULL;
}

void ast_oper_print(oper_t op, char **dest, size_t *remain) {
    ast_directed_print(dest, remain, ast_oper_str(op));
}

const char *ast_basic_type_str(basic_type_t type) {
    switch (type) {
    case TYPE_VOID:        return "void";        break;
    case TYPE_BOOL:        return "_Bool";       break;
    case TYPE_CHAR:        return "char";        break;
    case TYPE_SHORT:       return "short";       break;
    case TYPE_INT:         return "int";         break;
    case TYPE_LONG:        return "long";        break;
    case TYPE_LONG_LONG:   return "long long";   break;
    case TYPE_FLOAT:       return "float";       break;
    case TYPE_DOUBLE:      return "double";      break;
    case TYPE_LONG_DOUBLE: return "long double"; break;

    case TYPE_STRUCT:      return "struct";      break;
    case TYPE_UNION:       return "union";       break;
    case TYPE_ENUM:        return "enum";        break;
    default:
        assert(false);
    }

    return NULL;
}

void ast_type_print(type_t *type, int indent, char **dest, size_t *remain) {
    switch (type->type) {
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
        ast_directed_print(dest, remain, ast_basic_type_str(type->type));
        break;

    case TYPE_STRUCT:
    case TYPE_UNION: {
        ast_directed_print(dest, remain, ast_basic_type_str(type->type));

        if (type->struct_params.name != NULL) {
            ast_directed_print(dest, remain, " %.*s",
                               type->struct_params.name->len,
                               type->struct_params.name->str);
        }

        ast_directed_print(dest, remain, " {\n");
        sl_link_t *cur;
        SL_FOREACH(cur, &type->struct_params.decls) {
            PRINT_INDENT(indent + 1);
            ast_decl_print(GET_ELEM(&type->struct_params.decls, cur),
                           TYPE_STRUCT, indent + 1, dest, remain);
        }
        PRINT_INDENT(indent);
        ast_directed_print(dest, remain, "}");
        break;
    }
    case TYPE_ENUM: {
        ast_directed_print(dest, remain, ast_basic_type_str(type->type));
        if (type->enum_params.name != NULL) {
            ast_directed_print(dest, remain, " %.*s",
                               type->struct_params.name->len,
                               type->struct_params.name->str);
        }

        ast_directed_print(dest, remain, " {\n");
        sl_link_t *cur;
        SL_FOREACH(cur, &type->enum_params.ids) {
            PRINT_INDENT(indent + 1);
            decl_node_t *enum_id = GET_ELEM(&type->enum_params.ids, cur);
            ast_directed_print(dest, remain, "%.*s", enum_id->id->len,
                    enum_id->id->str);
            if (enum_id->expr != NULL) {
                ast_directed_print(dest, remain, " = ");
                ast_expr_print(enum_id->expr, 0, dest, remain);
            }
            if (enum_id != sl_tail(&type->enum_params.ids)) {
                ast_directed_print(dest, remain, ",");
            }
            ast_directed_print(dest, remain, "\n");
        }
        ast_directed_print(dest, remain, "}");
        break;
    }

    case TYPE_TYPEDEF:
        switch (type->typedef_params.type) {
        case TYPE_VOID:
            break;
        case TYPE_STRUCT:
        case TYPE_UNION:
        case TYPE_ENUM:
            ast_directed_print(dest, remain, "%s ",
                               ast_basic_type_str(type->typedef_params.type));
            break;
        default:
            assert(false);
        }
        ast_directed_print(dest, remain, "%.*s",
                           type->typedef_params.name->len,
                           type->typedef_params.name->str);
        break;

    case TYPE_MOD:
        ast_type_mod_print(type->mod.type_mod, dest, remain);
        ast_type_print(type->mod.base, 0, dest, remain);
        break;

    default:
        assert(false);
    }
}

const char *ast_type_mod_str(type_mod_t type_mod) {
    switch (type_mod) {
    case TMOD_SIGNED:   return "signed";
    case TMOD_UNSIGNED: return "unsigned";
    case TMOD_AUTO:     return "auto";
    case TMOD_REGISTER: return "register";
    case TMOD_STATIC:   return "static";
    case TMOD_EXTERN:   return "extern";
    case TMOD_TYPEDEF:  return "typedef";
    case TMOD_CONST:    return "const";
    case TMOD_VOLATILE: return "volatile";
    case TMOD_INLINE:   return "inline";
    default:
        assert(false);
    }
    return NULL;
}

void ast_type_mod_print(type_mod_t type_mod, char **dest, size_t *remain) {
    if (type_mod & TMOD_TYPEDEF) {
        ast_directed_print(dest, remain, "%s ", ast_type_mod_str(TMOD_TYPEDEF));
    }
    if (type_mod & TMOD_INLINE) {
        ast_directed_print(dest, remain, "%s ", ast_type_mod_str(TMOD_INLINE));
    }
    if (type_mod & TMOD_SIGNED) {
        ast_directed_print(dest, remain, "%s ", ast_type_mod_str(TMOD_SIGNED));
    }
    if (type_mod & TMOD_UNSIGNED) {
        ast_directed_print(dest, remain, "%s ",
                    ast_type_mod_str(TMOD_UNSIGNED));
    }
    if (type_mod & TMOD_AUTO) {
        ast_directed_print(dest, remain, "%s ", ast_type_mod_str(TMOD_AUTO));
    }
    if (type_mod & TMOD_REGISTER) {
        ast_directed_print(dest, remain, "%s ",
                    ast_type_mod_str(TMOD_REGISTER));
    }
    if (type_mod & TMOD_STATIC) {
        ast_directed_print(dest, remain, "%s ", ast_type_mod_str(TMOD_STATIC));
    }
    if (type_mod & TMOD_EXTERN) {
        ast_directed_print(dest, remain, "%s ", ast_type_mod_str(TMOD_EXTERN));
    }
    if (type_mod & TMOD_CONST) {
        ast_directed_print(dest, remain, "%s ", ast_type_mod_str(TMOD_CONST));
    }
    if (type_mod & TMOD_VOLATILE) {
        ast_directed_print(dest, remain, "%s ",
                    ast_type_mod_str(TMOD_VOLATILE));
    }
}

void ast_type_protected_destroy(type_t *type) {
    switch (type->type) {
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
        return; // These are statically allocated

    case TYPE_STRUCT:
    case TYPE_UNION:
        // Must be unananymous
        assert(type->struct_params.name != NULL);
        SL_DESTROY_FUNC(&type->struct_params.decls, ast_decl_destroy);
        break;
    case TYPE_ENUM:
        // Must be unananymous
        assert(type->enum_params.name != NULL);
        SL_DESTROY_FUNC(&type->enum_params.ids, ast_decl_node_destroy);
        break;
    default:
        assert(false);
    }
    fmark_chain_free(type->mark.last);
    free(type);
}

void ast_decl_node_type_destroy(type_t *type) {
    if (type == NULL) {
        return;
    }

    type_t *declarator_type = NULL;

    switch (type->type) {
    case TYPE_PAREN:
        declarator_type = type->paren_base;
        break;
    case TYPE_FUNC:
        declarator_type = type->func.type;
        SL_DESTROY_FUNC(&type->func.params, ast_decl_destroy);
        break;
    case TYPE_ARR:
        declarator_type = type->arr.base;
        ast_expr_destroy(type->arr.len);
        break;
    case TYPE_PTR:
        declarator_type = type->ptr.base;
        break;
    default:
        return;
    }

    ast_decl_node_type_destroy(declarator_type);
    fmark_chain_free(type->mark.last);
    free(type);
}

void ast_type_destroy(type_t *type) {
    if (type == NULL) {
        return;
    }

    switch (type->type) {
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
        return; // These are statically allocated

    case TYPE_STRUCT:
    case TYPE_UNION:
        // Only free anonymous struct/union
        if (type->struct_params.name != NULL) {
            return;
        }
        SL_DESTROY_FUNC(&type->struct_params.decls, ast_decl_destroy);
        break;
    case TYPE_ENUM:
        // Only free anonymous enum
        if (type->enum_params.name != NULL) {
            return;
        }
        SL_DESTROY_FUNC(&type->enum_params.ids, ast_decl_node_destroy);
        break;

    case TYPE_TYPEDEF:
        // Do nothing, base is stored in the type table
        break;

    case TYPE_MOD:
        ast_type_destroy(type->mod.base);
        break;

    case TYPE_PAREN:
    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:
        ast_decl_node_type_destroy(type);
        return;

    default:
        assert(false);
    }

    fmark_chain_free(type->mark.last);
    free(type);
}

void ast_gdecl_destroy(gdecl_t *gdecl) {
    if (gdecl == NULL) {
        return;
    }
    ast_decl_destroy(gdecl->decl);

    switch (gdecl->type) {
    case GDECL_FDEFN:
        ast_stmt_destroy(gdecl->fdefn.stmt);
        ht_destroy(&gdecl->fdefn.labels);
        break;
    case GDECL_NOP:
    case GDECL_DECL:
        break;
    default:
        assert(false);
    }
    fmark_chain_free(gdecl->mark.last);
    free(gdecl);
}

void ast_expr_destroy(expr_t *expr) {
    if (expr == NULL) {
        return;
    }
    switch (expr->type) {
    case EXPR_VOID:
        break;
    case EXPR_PAREN:
        ast_expr_destroy(expr->paren_base);
        break;
    case EXPR_VAR:
        break;

    case EXPR_ASSIGN:
        ast_expr_destroy(expr->assign.dest);
        ast_expr_destroy(expr->assign.expr);
        break;
    case EXPR_CONST_INT:
    case EXPR_CONST_FLOAT:
    case EXPR_CONST_STR:
        ast_type_destroy(expr->const_val.type);
        break;
    case EXPR_BIN:
        ast_expr_destroy(expr->bin.expr1);
        ast_expr_destroy(expr->bin.expr2);
        break;
    case EXPR_UNARY:
        ast_expr_destroy(expr->unary.expr);
        break;
    case EXPR_COND:
        ast_expr_destroy(expr->cond.expr1);
        ast_expr_destroy(expr->cond.expr2);
        ast_expr_destroy(expr->cond.expr3);
        break;
    case EXPR_CAST:
        ast_expr_destroy(expr->cast.base);
        ast_decl_destroy(expr->cast.cast);
        break;
    case EXPR_CALL:
        ast_expr_destroy(expr->call.func);
        SL_DESTROY_FUNC(&expr->call.params, ast_expr_destroy);
        break;
    case EXPR_CMPD:
        SL_DESTROY_FUNC(&expr->cmpd.exprs, ast_expr_destroy);
        break;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
        ast_decl_destroy(expr->sizeof_params.type);
        ast_expr_destroy(expr->sizeof_params.expr);
        break;
    case EXPR_OFFSETOF:
        ast_decl_destroy(expr->offsetof_params.type);
        SL_DESTROY_FUNC(&expr->offsetof_params.path, free);
        break;
    case EXPR_MEM_ACC:
        ast_expr_destroy(expr->mem_acc.base);
        break;
    case EXPR_ARR_IDX:
        ast_expr_destroy(expr->arr_idx.array);
        ast_expr_destroy(expr->arr_idx.index);
        break;
    case EXPR_INIT_LIST:
        SL_DESTROY_FUNC(&expr->init_list.exprs, ast_expr_destroy);
        break;
    case EXPR_DESIG_INIT:
        ast_expr_destroy(expr->desig_init.val);
        break;
    default:
        assert(false);
    }

    fmark_chain_free(expr->mark.last);
    free(expr);
}

void ast_decl_node_destroy(decl_node_t *decl_node) {
    if (decl_node == NULL) {
        return;
    }
    ast_decl_node_type_destroy(decl_node->type);
    ast_expr_destroy(decl_node->expr);
    fmark_chain_free(decl_node->mark.last);
    free(decl_node);
}

void ast_decl_destroy(decl_t *decl) {
    if (decl == NULL) {
        return;
    }
    bool is_typedef = decl->type != NULL && decl->type->type == TYPE_MOD &&
        decl->type->mod.type_mod & TMOD_TYPEDEF;

    if (is_typedef) {
        // If its a typedef, just free the decl nodes, because the types are
        // stored in a typetable, and there shouldn't be any expressions
        SL_DESTROY_FUNC(&decl->decls, free);
    } else {
        // Make sure decl_nodes don't free base
        SL_DESTROY_FUNC(&decl->decls, ast_decl_node_destroy);
        ast_type_destroy(decl->type);
    }
    fmark_chain_free(decl->mark.last);
    free(decl);
}


void ast_stmt_destroy(stmt_t *stmt) {
    if (stmt == NULL) {
        return;
    }

    switch (stmt->type) {
    case STMT_NOP:
        break;

    case STMT_DECL:
        ast_decl_destroy(stmt->decl);
        break;

    case STMT_LABEL:
        ast_stmt_destroy(stmt->label.stmt);
        break;
    case STMT_CASE:
        ast_expr_destroy(stmt->case_params.val);
        ast_stmt_destroy(stmt->case_params.stmt);
        break;
    case STMT_DEFAULT:
        ast_stmt_destroy(stmt->default_params.stmt);
        break;

    case STMT_IF:
        ast_expr_destroy(stmt->if_params.expr);
        ast_stmt_destroy(stmt->if_params.true_stmt);
        ast_stmt_destroy(stmt->if_params.false_stmt);
        break;
    case STMT_SWITCH:
        ast_expr_destroy(stmt->switch_params.expr);
        ast_stmt_destroy(stmt->switch_params.stmt);
        break;

    case STMT_DO:
        ast_stmt_destroy(stmt->do_params.stmt);
        ast_expr_destroy(stmt->do_params.expr);
        break;
    case STMT_WHILE:
        ast_expr_destroy(stmt->while_params.expr);
        ast_stmt_destroy(stmt->while_params.stmt);
        break;
    case STMT_FOR:
        ast_expr_destroy(stmt->for_params.expr1);
        ast_decl_destroy(stmt->for_params.decl1);
        ast_expr_destroy(stmt->for_params.expr2);
        ast_expr_destroy(stmt->for_params.expr3);
        ast_stmt_destroy(stmt->for_params.stmt);
        if (stmt->for_params.typetab != NULL) {
            tt_destroy(stmt->for_params.typetab); // Must free last
            free(stmt->for_params.typetab);
        }
        break;

        // For goto, continue, break target/parent is supposed to be duplicated
    case STMT_GOTO:
        break;
    case STMT_CONTINUE:
        break;
    case STMT_BREAK:
        break;
    case STMT_RETURN:
        ast_expr_destroy(stmt->return_params.expr);
        break;

    case STMT_COMPOUND:
        SL_DESTROY_FUNC(&stmt->compound.stmts, ast_stmt_destroy);
        tt_destroy(&stmt->compound.typetab); // Must free last
        break;

    case STMT_EXPR:
        ast_expr_destroy(stmt->expr.expr);
        break;
    default:
        assert(false);
    }
    fmark_chain_free(stmt->mark.last);
    free(stmt);
}

void ast_trans_unit_destroy(trans_unit_t *trans_unit) {
    if (trans_unit == NULL) {
        return;
    }
    // Use free rather than ast_type_destroy because we don't want to
    // recursively free other nodes
    SL_DESTROY_FUNC(&trans_unit->etypes, free);
    SL_DESTROY_FUNC(&trans_unit->gdecls, ast_gdecl_destroy);
    tt_destroy(&trans_unit->typetab); // Must free after trans units
    fmark_chain_free(trans_unit->mark.last);
    free(trans_unit);
}
