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

#include <assert.h>

void ast_print(trans_unit_t *ast) {
    // TODO: Implement this
    (void)ast;
}

void ast_destroy(trans_unit_t *ast) {
    ast_trans_unit_destroy(ast);
}

void ast_struct_decl_destroy(struct_decl_t *struct_decl) {
    if (struct_decl == NULL) {
        return;
    }
    ast_stmt_destroy(struct_decl->decl);
    ast_expr_destroy(struct_decl->bf_bits);
    free(struct_decl);
}

void ast_enum_id_destroy(enum_id_t *enum_id) {
    if (enum_id == NULL) {
        return;
    }
    ast_expr_destroy(enum_id->val);
    free(enum_id);
}

void ast_type_destroy(type_t *type, bool override) {
    // Do nothing if marked as not being deallocated
    if (type == NULL || (!override && !type->dealloc)) {
        return;
    }

    switch (type->type) {
    case TYPE_VOID:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
        break;

    case TYPE_STRUCT:
    case TYPE_UNION:
        SL_DESTROY_FUNC(&type->struct_decls, ast_struct_decl_destroy);
        break;
    case TYPE_ENUM:
        SL_DESTROY_FUNC(&type->enum_ids, ast_enum_id_destroy);
        break;

    case TYPE_FUNC:
        ast_type_destroy(type->func.type, NO_OVERRIDE);
        SL_DESTROY_FUNC(&type->func.params, ast_stmt_destroy);
        break;

    case TYPE_ARR:
        ast_type_destroy(type->arr.base, NO_OVERRIDE);
        ast_expr_destroy(type->arr.len);
        break;
    case TYPE_PTR:
        ast_type_destroy(type->ptr.base, NO_OVERRIDE);
        break;
    case TYPE_MOD:
        ast_type_destroy(type->mod.base, NO_OVERRIDE);
        break;
    default:
        assert(false);
    }
    free(type);
}

void ast_gdecl_destroy(gdecl_t *gdecl) {
    if (gdecl == NULL) {
        return;
    }
    ast_stmt_destroy(gdecl->decl);

    switch (gdecl->type) {
    case GDECL_FDEFN:
        ast_stmt_destroy(gdecl->fdefn.stmt);
        break;
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
    case EXPR_VAR:
        break;

    case EXPR_ASSIGN:
        ast_expr_destroy(expr->assign.dest);
        ast_expr_destroy(expr->assign.expr);
        break;
    case EXPR_CONST_INT:
    case EXPR_CONST_FLOAT:
    case EXPR_CONST_STR:
        ast_type_destroy(expr->const_val.type, NO_OVERRIDE);
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
        ast_stmt_destroy(expr->cast.cast);
        break;
    case EXPR_CALL:
        ast_expr_destroy(expr->call.func);
        SL_DESTROY_FUNC(&expr->call.params, ast_expr_destroy);
        break;
    case EXPR_CMPD:
        SL_DESTROY_FUNC(&expr->cmpd.exprs, ast_expr_destroy);
        break;
    case EXPR_SIZEOF:
        ast_stmt_destroy(expr->sizeof_params.type);
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
    ast_type_destroy(decl_node->type, NO_OVERRIDE);
    ast_expr_destroy(decl_node->expr);
    free(decl_node);
}

void ast_stmt_destroy(stmt_t *stmt) {
    if (stmt == NULL) {
        return;
    }

    switch (stmt->type) {
    case STMT_NOP:
        break;

    case STMT_DECL: {
        // Make sure decl_nodes don't free base
        bool dealloc_base;
        if (stmt->decl.type->dealloc) {
            dealloc_base = true;
            stmt->decl.type->dealloc = false;
        } else {
            dealloc_base = false;
        }
        SL_DESTROY_FUNC(&stmt->decl.decls, ast_decl_node_destroy);

        if (dealloc_base) {
            stmt->decl.type->dealloc = true;
            ast_type_destroy(stmt->decl.type, NO_OVERRIDE);
        }
        break;
    }

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

    case STMT_GOTO:
        ast_stmt_destroy(stmt->goto_params.target);
        break;
    case STMT_CONTINUE:
        ast_stmt_destroy(stmt->continue_params.parent);
        break;
    case STMT_BREAK:
        ast_stmt_destroy(stmt->break_params.parent);
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
