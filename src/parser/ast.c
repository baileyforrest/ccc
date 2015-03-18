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

#define INDENT ("    ")

void ast_print(trans_unit_t *ast) {
    ast_trans_unit_print(ast);
}

void ast_destroy(trans_unit_t *ast) {
    ast_trans_unit_destroy(ast);
}

void ast_trans_unit_print(trans_unit_t *tras_unit) {
    sl_link_t *cur;
    SL_FOREACH(cur, &tras_unit->gdecls) {
        ast_gdecl_print(GET_ELEM(&tras_unit->gdecls, cur));
    }
}

void ast_gdecl_print(gdecl_t *gdecl) {
    ast_decl_print(gdecl->decl, TYPE_VOID);
    printf("\n");
    switch (gdecl->type) {
    case GDECL_FDEFN:
        ast_stmt_print(gdecl->fdefn.stmt, 0);
        break;
    case GDECL_DECL:
        printf(";");
        break;
    case GDECL_NOP: // Should never have GDECL_NOP in properly formed AST
    default:
        assert(false);
    }
    printf("\n");
}

void ast_stmt_print(stmt_t *stmt, int indent) {
    for (int i = 0; i < indent; ++i) {
        printf(INDENT);
    }

    switch(stmt->type) {
    case STMT_NOP:
        printf(";");
        break;

    case STMT_DECL:
        ast_decl_print(stmt->decl, TYPE_VOID);
        printf(";");
        break;

    case STMT_LABEL:
        printf("%s:\n", stmt->label.label->str);
        ast_stmt_print(stmt->label.stmt, indent);
        break;
    case STMT_CASE:
        printf("case ");
        ast_expr_print(stmt->case_params.val);
        printf(":\n");
        ast_stmt_print(stmt->case_params.stmt, indent + 1);
        break;
    case STMT_DEFAULT:
        printf("default:\n");
        ast_stmt_print(stmt->default_params.stmt, indent + 1);
        break;

    case STMT_IF:
        printf("if (");
        ast_expr_print(stmt->if_params.expr);
        printf(")\n");
        ast_stmt_print(stmt->if_params.true_stmt, indent + 1);
        if (stmt->if_params.false_stmt) {
            printf("else");
            ast_stmt_print(stmt->if_params.false_stmt, indent + 1);
        }
        break;
    case STMT_SWITCH:
        printf("switch (");
        ast_expr_print(stmt->switch_params.expr);
        printf(")\n");
        ast_stmt_print(stmt->switch_params.stmt, indent + 1);
        break;

    case STMT_DO:
        printf("do\n");
        ast_stmt_print(stmt->do_params.stmt, indent + 1);
        printf("while (");
        ast_expr_print(stmt->do_params.expr);
        printf(")\n");
        break;
    case STMT_WHILE:
        printf("while (");
        ast_expr_print(stmt->while_params.expr);
        printf(")\n");
        ast_stmt_print(stmt->while_params.stmt, indent + 1);
        break;
    case STMT_FOR:
        printf("for (");
        if (stmt->for_params.expr1) {
            ast_expr_print(stmt->for_params.expr1);
        }
        printf(";");
        if (stmt->for_params.expr2) {
            ast_expr_print(stmt->for_params.expr2);
        }
        printf(";");
        if (stmt->for_params.expr3) {
            ast_expr_print(stmt->for_params.expr3);
        }
        printf(")\n");
        ast_stmt_print(stmt->for_params.stmt, indent + 1);
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
        ast_expr_print(stmt->return_params.expr);
        printf(";");
        break;

    case STMT_COMPOUND: {
        printf("{\n");
        sl_link_t *cur;
        SL_FOREACH(cur, &stmt->compound.stmts) {
            ast_stmt_print(GET_ELEM(&stmt->compound.stmts, cur), indent + 1);
        }
        printf("}\n");
        break;
    }

    case STMT_EXPR:
        ast_expr_print(stmt->expr.expr);
        printf(";");
        break;

    default:
        assert(false);
    }
    printf("\n");
}

void ast_decl_print(decl_t *decl, basic_type_t type) {
    ast_type_print(decl->type);
    printf(" ");

    bool first = true;
    sl_link_t *cur;
    SL_FOREACH(cur, &decl->decls) {
        if (first) {
            first = false;
        } else {
            printf(", ");
        }
        decl_node_t *node = GET_ELEM(&decl->decls, cur);
        ast_decl_node_print(node, node->type);
        if (node->expr == NULL) {
            continue;
        }
        switch (type) {
        case TYPE_STRUCT:
        case TYPE_UNION:
            printf(" : ");
            break;
        default:
            printf(" = ");
            break;
        }
        ast_expr_print(node->expr);
    }
}

void ast_decl_node_print(decl_node_t *decl_node, type_t *type) {
    switch (type->type) {
    case TYPE_FUNC: {
        ast_decl_node_print(decl_node, type->func.type);

        bool first = true;
        printf("(");
        sl_link_t *cur;
        SL_FOREACH(cur, &decl_node->type->func.params) {
            if (first) {
                first = false;
            } else {
                printf(", ");
            }
            ast_decl_print(GET_ELEM(&decl_node->type->func.params, cur),
                           TYPE_VOID);
        }
        printf(")");
        break;
    }
    case TYPE_ARR:
        ast_decl_node_print(decl_node, type->arr.base);
        printf("[");
        ast_expr_print(type->arr.len);
        printf("]");
        break;
    case TYPE_PTR:
        printf(" * ");
        ast_type_mod_print(type->ptr.type_mod);
        ast_decl_node_print(decl_node, type->ptr.base);
        break;
    default:
        printf("%s", decl_node->id->str);
    }
}

void ast_expr_print(expr_t *expr) {
    switch (expr->type) {
    case EXPR_VOID:
        break;
    case EXPR_PAREN:
        printf("(");
        ast_expr_print(expr->paren_base);
        printf(")");
        break;
    case EXPR_VAR:
        printf("%s", expr->var_id->str);
        break;
    case EXPR_ASSIGN:
        ast_expr_print(expr->assign.dest);
        printf(" ");
        ast_oper_print(expr->assign.op);
        printf("= ");
        ast_expr_print(expr->assign.expr);
        break;
    case EXPR_CONST_INT:
        printf("%lld", expr->const_val.int_val);
        switch (expr->const_val.type->type) {
        case TYPE_LONG:
            printf("L");
            break;
        case TYPE_MOD:
            printf("U");
            if (expr->const_val.type->mod.base->type == TYPE_LONG) {
                printf("L");
            }
            break;
        default:
            break;
        }
        break;
    case EXPR_CONST_FLOAT:
        printf("%f", expr->const_val.float_val);
        if (expr->const_val.type->type == TYPE_MOD) {
            printf("f");
        }
        break;
    case EXPR_CONST_STR:
        printf("%s", expr->const_val.str_val->str);
        break;
    case EXPR_BIN:
        if (expr->bin.op == OP_ARR_ACC) {
            ast_expr_print(expr->bin.expr1);
            printf("[");
            ast_expr_print(expr->bin.expr2);
            printf("]");
        } else {
            ast_expr_print(expr->bin.expr1);
            printf(" ");
            ast_oper_print(expr->bin.op);
            printf(" ");
            ast_expr_print(expr->bin.expr2);
        }
        break;
    case EXPR_UNARY:
        switch (expr->unary.op) {
        case OP_POSTINC:
        case OP_POSTDEC:
            ast_expr_print(expr->unary.expr);
            ast_oper_print(expr->unary.op);
            break;
        default:
            ast_oper_print(expr->unary.op);
            ast_expr_print(expr->unary.expr);
            break;
        }
        break;
    case EXPR_COND:
        ast_expr_print(expr->cond.expr1);
        printf(" ? ");
        ast_expr_print(expr->cond.expr2);
        printf(" : ");
        ast_expr_print(expr->cond.expr3);
        break;
    case EXPR_CAST:
        printf("(");
        ast_decl_print(expr->cast.cast, TYPE_VOID);
        printf(")");
        ast_expr_print(expr->cast.base);
        break;
    case EXPR_CALL:
        ast_expr_print(expr->call.func);
        printf("(");
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->call.params) {
            ast_expr_print(GET_ELEM(&expr->call.params, cur));
        }
        printf(")");
        break;
    case EXPR_CMPD: {
        bool first = true;
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            if (first) {
                first = false;
            } else {
                printf(", ");
            }
            ast_expr_print(GET_ELEM(&expr->cmpd.exprs, cur));
        }
        break;
    }
    case EXPR_SIZEOF:
        printf("sizeof (");
        if (expr->sizeof_params.type != NULL) {
            ast_decl_print(expr->sizeof_params.type, TYPE_VOID);
        } else {
            ast_expr_print(expr->sizeof_params.expr);
        }
        printf(")");
        break;
    case EXPR_MEM_ACC:
        ast_expr_print(expr->mem_acc.base);
        ast_oper_print(expr->mem_acc.op);
        printf("%s", expr->mem_acc.name->str);
        break;
    case EXPR_INIT_LIST: {
        printf("{ ");
        bool first = true;
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            if (first) {
                first = false;
            } else {
                printf(", ");
            }
            ast_expr_print(GET_ELEM(&expr->cmpd.exprs, cur));
        }
        printf("}");
        break;
    }

    default:
        assert(false);
    }
}

void ast_oper_print(oper_t op) {
    switch (op) {
    case OP_NOP:
        break;
    case OP_PLUS:
    case OP_UPLUS:
        printf("+");
        break;
    case OP_MINUS:
    case OP_UMINUS:
        printf("-");
        break;
    case OP_TIMES:
    case OP_DEREF:
        printf("*");
        break;
    case OP_DIV:
        printf("/");
        break;
    case OP_MOD:
        printf("%%");
        break;
    case OP_LT:
        printf("<");
        break;
    case OP_LE:
        printf("<=");
        break;
    case OP_GT:
        printf(">");
        break;
    case OP_GE:
        printf(">=");
        break;
    case OP_EQ:
        printf("==");
        break;
    case OP_NE:
        printf("!=");
        break;
    case OP_BITAND:
    case OP_ADDR:
        printf("&");
        break;
    case OP_BITXOR:
        printf("^");
        break;
    case OP_BITOR:
        printf("|");
        break;
    case OP_LSHIFT:
        printf("<<");
        break;
    case OP_RSHIFT:
        printf(">>");
        break;
    case OP_LOGICNOT:
        printf("!");
        break;
    case OP_BITNOT:
        printf("~");
        break;
    case OP_ARR_ACC:
        printf("[]");
        break;
    case OP_PREINC:
    case OP_POSTINC:
        printf("++");
        break;
    case OP_PREDEC:
    case OP_POSTDEC:
        printf("--");
        break;
    case OP_ARROW:
        printf("->");
        break;
    case OP_DOT:
        printf(".");
        break;
    default:
        assert(false);
    }
}

const char *ast_basic_type_str(basic_type_t type) {
    switch (type) {
    case TYPE_VOID:   return "void";   break;
    case TYPE_CHAR:   return "char";   break;
    case TYPE_SHORT:  return "short";  break;
    case TYPE_INT:    return "int";    break;
    case TYPE_LONG:   return "long";   break;
    case TYPE_FLOAT:  return "float";  break;
    case TYPE_DOUBLE: return "double"; break;

    case TYPE_STRUCT: return "struct"; break;
    case TYPE_UNION:  return "union";  break;
    case TYPE_ENUM:   return "enum";   break;
    default:
        assert(false);
    }

    return NULL;
}

void ast_type_print(type_t *type) {
    switch (type->type) {
    case TYPE_VOID:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
        printf(ast_basic_type_str(type->type));
        break;

    case TYPE_STRUCT:
    case TYPE_UNION: {
        printf(ast_basic_type_str(type->type));
        printf(" {\n");
        sl_link_t *cur;
        SL_FOREACH(cur, &type->struct_params.decls) {
            ast_decl_print(GET_ELEM(&type->struct_params.decls, cur),
                           TYPE_STRUCT);
        }
        printf("}");
        break;
    }
    case TYPE_ENUM: {
        printf(ast_basic_type_str(type->type));
        printf(" {\n");
        sl_link_t *cur;
        SL_FOREACH(cur, &type->enum_params.ids) {
            enum_id_t *enum_id = GET_ELEM(&type->enum_params.ids, cur);
            ast_enum_id_print(enum_id);
            if (enum_id != sl_tail(&type->enum_params.ids)) {
                printf(",");
            }
            printf("\n");
        }
        printf("}");
        break;
    }

    case TYPE_TYPEDEF:
        printf("%.*s", (int)type->typedef_params.name->len,
               type->typedef_params.name->str);
        break;

    case TYPE_MOD:
        ast_type_mod_print(type->mod.type_mod);
        ast_type_print(type->mod.base);
        break;

    case TYPE_PAREN:
        printf("(");
        ast_type_print(type->paren_base);
        printf(")");
        break;
    case TYPE_FUNC: {
        ast_type_print(type->func.type);
        printf("(");
        bool first = true;
        sl_link_t *cur;
        SL_FOREACH(cur, &type->func.params) {
            if (first) {
                first = false;
            } else {
                printf(", ");
            }
            ast_enum_id_print(GET_ELEM(&type->func.params, cur));
        }
        printf(")");
        break;
    }
    case TYPE_ARR:
        ast_type_print(type->arr.base);
        printf("[");
        ast_expr_print(type->arr.len);
        printf("]");
        break;
    case TYPE_PTR:
        ast_type_print(type->ptr.base);
        printf(" * ");
        ast_type_mod_print(type->ptr.type_mod);
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
    default:
        assert(false);
    }
    return NULL;
}

void ast_type_mod_print(type_mod_t type_mod) {
    if (type_mod & TMOD_TYPEDEF) {
        ast_type_mod_str(TMOD_TYPEDEF);
    }
    if (type_mod & TMOD_SIGNED) {
        ast_type_mod_str(TMOD_SIGNED);
    }
    if (type_mod & TMOD_UNSIGNED) {
        ast_type_mod_str(TMOD_UNSIGNED);
    }
    if (type_mod & TMOD_AUTO) {
        ast_type_mod_str(TMOD_AUTO);
    }
    if (type_mod & TMOD_REGISTER) {
        ast_type_mod_str(TMOD_REGISTER);
    }
    if (type_mod & TMOD_STATIC) {
        ast_type_mod_str(TMOD_STATIC);
    }
    if (type_mod & TMOD_EXTERN) {
        ast_type_mod_str(TMOD_EXTERN);
    }
    if (type_mod & TMOD_CONST) {
        ast_type_mod_str(TMOD_CONST);
    }
    if (type_mod & TMOD_VOLATILE) {
        ast_type_mod_str(TMOD_VOLATILE);
    }
}

void ast_enum_id_print(enum_id_t *enum_id) {
    printf("%s", enum_id->id->str);
    if (enum_id->val != NULL) {
        printf(" = ");
        ast_expr_print(enum_id->val);
    }
}

void ast_enum_id_destroy(enum_id_t *enum_id) {
    if (enum_id == NULL) {
        return;
    }
    ast_expr_destroy(enum_id->val);
    free(enum_id);
}

void ast_type_protected_destroy(type_t *type) {
    switch (type->type) {
    case TYPE_VOID:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
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
        SL_DESTROY_FUNC(&type->enum_params.ids, ast_enum_id_destroy);
        break;
    default:
        assert(false);
    }
    free(type);
}

void ast_type_destroy(type_t *type) {
    // Do nothing if marked as not being deallocated
    if (type == NULL) {
        return;
    }

    type_t *declarator_type = NULL;

    switch (type->type) {
    case TYPE_VOID:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
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
        SL_DESTROY_FUNC(&type->enum_params.ids, ast_enum_id_destroy);
        break;

    case TYPE_TYPEDEF:
        // Do nothing, base is stored in the type table
        break;

    case TYPE_MOD:
        ast_type_destroy(type->mod.base);
        break;

    case TYPE_PAREN:
        declarator_type = type->paren_base;
        break;
    case TYPE_FUNC:
        declarator_type = type->func.type;
        SL_DESTROY_FUNC(&type->func.params, ast_stmt_destroy);
        break;
    case TYPE_ARR:
        declarator_type = type->arr.base;
        ast_expr_destroy(type->arr.len);
        break;
    case TYPE_PTR:
        declarator_type = type->ptr.base;
        ast_type_destroy(type->ptr.base);
        break;

    default:
        assert(false);
    }

    // Avoid freeing base of a declarator type so the base isn't freed multiple
    // times
    if (declarator_type != NULL) {
        switch (declarator_type->type) {
        case TYPE_PAREN:
        case TYPE_FUNC:
        case TYPE_ARR:
        case TYPE_PTR:
            ast_type_destroy(declarator_type);
            break;
        default:
            break;
        }
    }

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
        break;
    case GDECL_NOP:
    case GDECL_DECL:
        break;
    default:
        assert(false);
    }
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
        ast_decl_destroy(expr->sizeof_params.type);
        ast_expr_destroy(expr->sizeof_params.expr);
        break;
    case EXPR_MEM_ACC:
        ast_expr_destroy(expr->mem_acc.base);
        break;
    case EXPR_INIT_LIST:
        SL_DESTROY_FUNC(&expr->init_list.exprs, ast_expr_destroy);
        break;
    default:
        assert(false);
    }
    free(expr);
}

void ast_decl_node_destroy(decl_node_t *decl_node) {
    if (decl_node == NULL) {
        return;
    }
    ast_type_destroy(decl_node->type);
    ast_expr_destroy(decl_node->expr);
    free(decl_node);
}

void ast_decl_destroy(decl_t *decl) {
    // Make sure decl_nodes don't free base
    SL_DESTROY_FUNC(&decl->decls, ast_decl_node_destroy);
    ast_type_destroy(decl->type);
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
        ast_expr_destroy(stmt->for_params.expr2);
        ast_expr_destroy(stmt->for_params.expr3);
        ast_stmt_destroy(stmt->for_params.stmt);
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
    free(stmt);
}

void ast_trans_unit_destroy(trans_unit_t *trans_unit) {
    if (trans_unit == NULL) {
        return;
    }
    SL_DESTROY_FUNC(&trans_unit->gdecls, ast_gdecl_destroy);
    tt_destroy(&trans_unit->typetab); // Must free after trans units
    free(trans_unit);
}
