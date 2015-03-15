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
    free(struct_decl->decl);

    ast_expr_destroy(struct_decl->bf_bits);
    free(struct_decl->bf_bits);
}

void ast_enum_id_destroy(enum_id_t *enum_id) {
    if (enum_id == NULL) {
        return;
    }
    ast_expr_destroy(enum_id->val);
    free(enum_id->val);
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
    case TYPE_UNION: {
        sl_link_t *cur;
        SL_FOREACH(cur, &type->struct_decls) {
            ast_struct_decl_destroy(GET_ELEM(&type->struct_decls, cur));
        }
        sl_destroy(&type->struct_decls, DOFREE);
        break;
    }
    case TYPE_ENUM: {
        sl_link_t *cur;
        SL_FOREACH(cur, &type->enum_ids) {
            ast_enum_id_destroy(GET_ELEM(&type->enum_ids, cur));
        }
        sl_destroy(&type->enum_ids, DOFREE);
        break;
    }

    case TYPE_FUNC: {
        ast_type_destroy(type->func.type, NO_OVERRIDE);
        free(type->func.type);

        sl_link_t *cur;
        SL_FOREACH(cur, &type->func.params) {
            ast_stmt_destroy(GET_ELEM(&type->func.params, cur));
        }
        sl_destroy(&type->func.params, DOFREE);
        break;
    }

    case TYPE_ARR: {
        ast_type_destroy(type->arr.base, NO_OVERRIDE);
        free(type->arr.base);

        ast_expr_destroy(type->arr.len);
        free(type->arr.len);
        break;
    }
    case TYPE_PTR: {
        ast_type_destroy(type->ptr.base, NO_OVERRIDE);
        free(type->ptr.base);
        break;
    }
    case TYPE_MOD: {
        ast_type_destroy(type->mod.base, NO_OVERRIDE);
        free(type->mod.base);
        break;
    }
    default:
        assert(false);
    }
}

void ast_gdecl_destroy(gdecl_t *gdecl) {
    if (gdecl == NULL) {
        return;
    }
    ast_stmt_destroy(gdecl->decl);
    free(gdecl->decl);

    switch (gdecl->type) {
    case GDECL_FDEFN:
        ast_stmt_destroy(gdecl->fdefn.stmt);
        free(gdecl->fdefn.stmt);
        break;
    case GDECL_DECL:
        break;
    default:
        assert(false);
    }
}

void ast_expr_destroy(expr_t *expr) {
    if (expr == NULL) {
        return;
    }
    switch (expr->type) {
    case EXPR_VOID:
    case EXPR_VAR:
        break;

    case EXPR_ASSIGN: {
        ast_expr_destroy(expr->assign.dest);
        free(expr->assign.dest);

        ast_expr_destroy(expr->assign.expr);
        free(expr->assign.expr);
        break;
    }
    case EXPR_CONST_INT:
    case EXPR_CONST_FLOAT:
    case EXPR_CONST_STR: {
        ast_type_destroy(expr->const_val.type, NO_OVERRIDE);
        free(expr->const_val.type);
        break;
    }
    case EXPR_BIN: {
        ast_expr_destroy(expr->bin.expr1);
        free(expr->bin.expr1);

        ast_expr_destroy(expr->bin.expr2);
        free(expr->bin.expr2);
        break;
    }
    case EXPR_UNARY: {
        ast_expr_destroy(expr->unary.expr);
        free(expr->unary.expr);
        break;
    }
    case EXPR_COND: {
        ast_expr_destroy(expr->cond.expr1);
        free(expr->cond.expr1);

        ast_expr_destroy(expr->cond.expr2);
        free(expr->cond.expr2);

        ast_expr_destroy(expr->cond.expr3);
        free(expr->cond.expr3);
        break;
    }
    case EXPR_CAST: {
        ast_expr_destroy(expr->cast.base);
        free(expr->cast.base);

        ast_stmt_destroy(expr->cast.cast);
        free(expr->cast.cast);
        break;
    }
    case EXPR_CALL: {
        ast_expr_destroy(expr->call.func);
        free(expr->call.func);

        sl_link_t *cur;
        SL_FOREACH(cur, &expr->call.params) {
            ast_expr_destroy(GET_ELEM(&expr->call.params, cur));
        }
        sl_destroy(&expr->call.params, DOFREE);
        break;
    }
    case EXPR_CMPD: {
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->cmpd.exprs) {
            ast_expr_destroy(GET_ELEM(&expr->cmpd.exprs, cur));
        }
        sl_destroy(&expr->cmpd.exprs, DOFREE);
        break;
    }
    case EXPR_SIZEOF: {
        ast_stmt_destroy(expr->sizeof_params.type);
        free(expr->sizeof_params.type);

        ast_expr_destroy(expr->sizeof_params.expr);
        free(expr->sizeof_params.expr);
        break;
    }
    case EXPR_MEM_ACC: {
        ast_expr_destroy(expr->mem_acc.base);
        free(expr->mem_acc.base);
        break;
    }
    case EXPR_INIT_LIST: {
        sl_link_t *cur;
        SL_FOREACH(cur, &expr->init_list.exprs) {
            ast_expr_destroy(GET_ELEM(&expr->init_list.exprs, cur));
        }
        sl_destroy(&expr->init_list.exprs, DOFREE);
        break;
    }
    default:
        assert(false);
    }
}

void ast_decl_node_destroy(decl_node_t *decl_node) {
    if (decl_node == NULL) {
        return;
    }
    ast_type_destroy(decl_node->type, NO_OVERRIDE);
    free(decl_node->type);

    ast_expr_destroy(decl_node->expr);
    free(decl_node->expr);
}

void ast_stmt_destroy(stmt_t *stmt) {
    if (stmt == NULL) {
        return;
    }

    switch (stmt->type) {
    case STMT_NOP:
        break;

    case STMT_DECL: {
        // Make sure decls don't free this
        bool dealloc_base;
        if (stmt->decl.type->dealloc) {
            dealloc_base = true;
            stmt->decl.type->dealloc = false;
        } else {
            dealloc_base = false;
        }
        sl_link_t *cur;
        SL_FOREACH(cur, &stmt->decl.decls) {
            ast_decl_node_destroy(GET_ELEM(&stmt->decl.decls, cur));
        }
        sl_destroy(&stmt->decl.decls, DOFREE);

        if (dealloc_base) {
            stmt->decl.type->dealloc = true;
            ast_type_destroy(stmt->decl.type, NO_OVERRIDE);
            free(stmt->decl.type);
        }
        break;
    }

    case STMT_LABEL: {
        ast_stmt_destroy(stmt->label.stmt);
        free(stmt->label.stmt);
        break;
    }
    case STMT_CASE: {
        ast_expr_destroy(stmt->case_params.val);
        free(stmt->case_params.val);

        ast_stmt_destroy(stmt->case_params.stmt);
        free(stmt->case_params.stmt);
        break;
    }
    case STMT_DEFAULT: {
        ast_stmt_destroy(stmt->default_params.stmt);
        free(stmt->default_params.stmt);
        break;
    }

    case STMT_IF: {
        ast_expr_destroy(stmt->if_params.expr);
        free(stmt->if_params.expr);

        ast_stmt_destroy(stmt->if_params.true_stmt);
        free(stmt->if_params.true_stmt);

        ast_stmt_destroy(stmt->if_params.false_stmt);
        free(stmt->if_params.false_stmt);
        break;
    }
    case STMT_SWITCH: {
        ast_expr_destroy(stmt->switch_params.expr);
        free(stmt->switch_params.expr);

        ast_stmt_destroy(stmt->switch_params.stmt);
        free(stmt->switch_params.stmt);
        break;
    }

    case STMT_DO: {
        ast_stmt_destroy(stmt->do_params.stmt);
        free(stmt->do_params.stmt);

        ast_expr_destroy(stmt->do_params.expr);
        free(stmt->do_params.expr);
        break;
    }
    case STMT_WHILE: {
        ast_expr_destroy(stmt->while_params.expr);
        free(stmt->while_params.expr);

        ast_stmt_destroy(stmt->while_params.stmt);
        free(stmt->while_params.stmt);
        break;
    }
    case STMT_FOR: {
        ast_expr_destroy(stmt->for_params.expr1);
        free(stmt->for_params.expr1);

        ast_expr_destroy(stmt->for_params.expr2);
        free(stmt->for_params.expr2);

        ast_expr_destroy(stmt->for_params.expr3);
        free(stmt->for_params.expr3);

        ast_stmt_destroy(stmt->for_params.stmt);
        free(stmt->for_params.stmt);
        break;
    }

    case STMT_GOTO:
        break;

    case STMT_CONTINUE: {
        ast_stmt_destroy(stmt->continue_params.parent);
        free(stmt->continue_params.parent);
        break;
    }
    case STMT_BREAK: {
        ast_stmt_destroy(stmt->break_params.parent);
        free(stmt->break_params.parent);
        break;
    }
    case STMT_RETURN: {
        ast_expr_destroy(stmt->return_params.expr);
        free(stmt->return_params.expr);
        break;
    }

    case STMT_COMPOUND: {
        tt_destroy(&stmt->compound.typetab);

        sl_link_t *cur;
        SL_FOREACH(cur, &stmt->compound.stmts) {
            ast_stmt_destroy(GET_ELEM(&stmt->compound.stmts, cur));
        }
        sl_destroy(&stmt->compound.stmts, DOFREE);
        break;
    }

    case STMT_EXPR: {
        ast_expr_destroy(stmt->expr.expr);
        free(stmt->expr.expr);
        break;
    }
    default:
        assert(false);
    }
}

void ast_trans_unit_destroy(trans_unit_t *trans_unit) {
    if (trans_unit == NULL) {
        return;
    }
    tt_destroy(&trans_unit->typetab);

    sl_link_t *cur;
    SL_FOREACH(cur, &trans_unit->gdecls) {
        ast_gdecl_destroy(GET_ELEM(&trans_unit->gdecls, cur));
    }
    sl_destroy(&trans_unit->gdecls, DOFREE);
}
