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
 * Declaration translator functions
 */

#include "trans_decl.h"

#include "trans_expr.h"
#include "trans_init.h"
#include "trans_type.h"

#include "util/string_store.h"

#define MAX_NUM_LEN 21

void trans_gdecl_node(trans_state_t *ts, decl_node_t *node) {
    ir_gdecl_t *ir_gdecl;
    if (node->type->type == TYPE_FUNC) {
        ir_gdecl = ir_gdecl_create(IR_GDECL_FUNC_DECL);
        ir_gdecl->func_decl.type = trans_decl_node(ts, node, IR_DECL_NODE_FDEFN,
                                                   NULL);
        ir_gdecl->func_decl.name = node->id;
    } else {
        ir_gdecl = ir_gdecl_create(IR_GDECL_GDATA);
        trans_decl_node(ts, node, IR_DECL_NODE_GLOBAL, ir_gdecl);
    }
    sl_append(&ts->tunit->decls, &ir_gdecl->link);
}

char *trans_decl_node_name(ir_symtab_t *symtab, char *name) {
    ir_symtab_entry_t *entry = ir_symtab_lookup(symtab, name);
    if (entry == NULL) {
        return name;
    }

    size_t name_len = strlen(name);
    size_t patch_len = name_len + MAX_NUM_LEN;
    char *patch_name = emalloc(patch_len);
    int number = ++entry->number;
    sprintf(patch_name, "%s%d", name, number);

    do { // Keep trying to increment number until we find unused name
        ir_symtab_entry_t *test = ir_symtab_lookup(symtab, patch_name);
        if (test == NULL) {
            break;
        }
        ++number;
        sprintf(patch_name + name_len, "%d", number);
    } while(true);

    // Record next number to try from
    entry->number = number;

    return sstore_insert(patch_name);
}

ir_type_t *trans_decl_node(trans_state_t *ts, decl_node_t *node,
                           ir_decl_node_type_t type,
                           void *context) {
    type_t *node_type = ast_type_untypedef(node->type);
    ir_expr_t *var_expr = ir_expr_create(ts->tunit, IR_EXPR_VAR);
    ir_type_t *expr_type = trans_type(ts, node_type);
    ir_type_t *ptr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
    ptr_type->ptr.base = expr_type;


    ir_symtab_t *symtab;
    ir_expr_t *access;

    switch (type) {
    case IR_DECL_NODE_FDEFN: {
        var_expr->var.type = ptr_type;
        var_expr->var.name = node->id;
        var_expr->var.local = false;

        symtab = &ts->tunit->globals;
        access = var_expr;
        break;
    }
    case IR_DECL_NODE_GLOBAL: {
        ir_gdecl_t *gdecl = context;
        assert(gdecl->type == IR_GDECL_GDATA);

        // Set up correct linkage and modifiers
        if ((node_type->type == TYPE_MOD &&
             node_type->mod.type_mod & TMOD_CONST) ||
            (node_type->type == TYPE_PTR &&
             node_type->ptr.type_mod & TMOD_CONST)) {
            gdecl->gdata.flags |= IR_GDATA_CONSTANT;
        }

        // storage class specifers (auto/register/static/extern) are attached
        // to the base type, need to remove pointers
        type_t *mod_check = node_type;
        while (mod_check->type == TYPE_PTR) {
            mod_check = ast_type_untypedef(mod_check->ptr.base);
        }
        bool external = false;
        if (mod_check->type == TYPE_MOD) {
            if (mod_check->mod.type_mod & TMOD_STATIC) {
                gdecl->linkage = IR_LINKAGE_INTERNAL;
            } else if (mod_check->mod.type_mod & TMOD_EXTERN) {
                gdecl->linkage = IR_LINKAGE_EXTERNAL;
                external = true;
            }

        }

        if (external) {
            gdecl->gdata.init = NULL;
        } else {
            ir_expr_t *init;
            if (node->expr != NULL) {
                if (node->expr->type == EXPR_CONST_STR &&
                    node_type->type == TYPE_ARR) {
                    init = ir_expr_create(ts->tunit, IR_EXPR_CONST);
                    init->const_params.ctype = IR_CONST_STR;
                    init->const_params.type = trans_type(ts, node->expr->etype);
                    init->const_params.str_val =
                        unescape_str(node->expr->const_val.str_val);

                } else {
                    init = trans_expr(ts, false, node->expr, NULL);
                    init = trans_type_conversion(ts, node_type,
                                                 node->expr->etype, init, NULL);
                }
            } else {
                init = ir_expr_zero(ts->tunit, expr_type);
            }
            gdecl->gdata.init = init;
            expr_type = ir_expr_type(init);
            ptr_type->ptr.base = expr_type;
        }

        var_expr->var.type = ptr_type;
        var_expr->var.name = node->id;
        var_expr->var.local = false;

        gdecl->gdata.type = expr_type;
        gdecl->gdata.var = var_expr;

        gdecl->gdata.align = ast_type_align(node_type);

        symtab = &ts->tunit->globals;
        access = var_expr;
        break;
    }
    case IR_DECL_NODE_LOCAL: {
        // storage class specifers (auto/register/static/extern) are attached
        // to the base type, need to remove pointers
        type_t *mod_check = node_type;
        while (mod_check->type == TYPE_PTR) {
            mod_check = ast_type_untypedef(mod_check->ptr.base);
        }
        ir_linkage_t linkage = IR_LINKAGE_DEFAULT;
        if (mod_check->type == TYPE_MOD) {
            if (mod_check->mod.type_mod & TMOD_STATIC) {
                linkage = IR_LINKAGE_INTERNAL;
            } else if (mod_check->mod.type_mod & TMOD_EXTERN) {
                linkage = IR_LINKAGE_EXTERNAL;
            }
        }
        // TODO1: Handle extern, need to not translate this, add it to the
        // gdecls hashtable

        symtab = &ts->func->func.locals;
        access = var_expr;

        if (linkage == IR_LINKAGE_INTERNAL) {
            char namebuf[MAX_GLOBAL_NAME];
            snprintf(namebuf, sizeof(namebuf), "%s.%s",
                     ts->func->func.name, node->id);
            var_expr->var.type = ptr_type;
            var_expr->var.name = sstore_lookup(namebuf);
            var_expr->var.local = false;

            ir_expr_t *init;
            // TODO1: This is copied from GDATA, move to function
            if (node->expr != NULL) {
                if (node->expr->type == EXPR_CONST_STR &&
                    node_type->type == TYPE_ARR) {
                    init = ir_expr_create(ts->tunit, IR_EXPR_CONST);
                    init->const_params.ctype = IR_CONST_STR;
                    init->const_params.type = trans_type(ts, node->expr->etype);
                    init->const_params.str_val =
                        unescape_str(node->expr->const_val.str_val);

                } else {
                    init = trans_expr(ts, false, node->expr, NULL);
                    init = trans_type_conversion(ts, node_type,
                                                 node->expr->etype, init, NULL);
                }
            } else {
                init = ir_expr_zero(ts->tunit, expr_type);
            }

            ir_gdecl_t *global = ir_gdecl_create(IR_GDECL_GDATA);
            global->linkage = linkage;
            global->gdata.flags = IR_GDATA_NOFLAG;
            global->gdata.type = expr_type;
            global->gdata.var = var_expr;
            global->gdata.init = init;
            global->gdata.align = ast_type_align(node->type);
            sl_append(&ts->tunit->decls, &global->link);

            break;
        }

        ir_inst_stream_t *ir_stmts = context;

        var_expr->var.type = ptr_type;
        var_expr->var.name = trans_decl_node_name(symtab, node->id);
        var_expr->var.local = true;

        ir_expr_t *src;
        if (node_type->type == TYPE_VA_LIST) {
            // TODO1: Something similar to this is needed for the global
            // case, possibly move this to a function
            // va lists need to be handled separately
            var_expr->var.type = expr_type;

            assert(expr_type->type == IR_TYPE_PTR);

            // Allocate the actual va_list, then set the variable's value to
            // the allocated object
            ir_type_t *va_tag_type = expr_type->ptr.base;
            ir_type_t *arr_type = ir_type_create(ts->tunit, IR_TYPE_ARR);
            arr_type->arr.elem_type = va_tag_type;
            arr_type->arr.nelems = 1;
            ir_type_t *p_arr_type = ir_type_create(ts->tunit, IR_TYPE_PTR);
            p_arr_type->ptr.base = arr_type;

            ir_expr_t *alloc = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
            alloc->alloca.type = p_arr_type;
            alloc->alloca.elem_type = arr_type;
            alloc->alloca.nelem_type = NULL;
            alloc->alloca.align = ast_type_align(node_type);

            ir_expr_t *temp = trans_temp_create(ts, expr_type);
            ir_stmt_t *assign = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
            assign->assign.dest = temp;
            assign->assign.src = alloc;
            trans_add_stmt(ts, &ts->func->func.prefix, assign);

            src = ir_expr_create(ts->tunit, IR_EXPR_GETELEMPTR);
            src->getelemptr.type = expr_type;
            src->getelemptr.ptr_type = p_arr_type;
            src->getelemptr.ptr_val = temp;

            ir_expr_t *zero = ir_expr_zero(ts->tunit, &ir_type_i32);
            sl_append(&src->getelemptr.idxs, &zero->link);
            zero = ir_expr_zero(ts->tunit, &ir_type_i32);
            sl_append(&src->getelemptr.idxs, &zero->link);
        } else {
            // Have to allocate variable on the stack
            src = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
            src->alloca.type = ptr_type;
            src->alloca.elem_type = var_expr->var.type->ptr.base;
            src->alloca.nelem_type = NULL;
            src->alloca.align = ast_type_align(node_type);
        }

        // Assign the named variable to the allocation
        ir_stmt_t *stmt = ir_stmt_create(ts->tunit, IR_STMT_ASSIGN);
        stmt->assign.dest = var_expr;
        stmt->assign.src = src;
        trans_add_stmt(ts, &ts->func->func.prefix, stmt);

        if (node->expr != NULL && !ts->ignore_until_label) {
            trans_initializer(ts, ir_stmts, node_type, expr_type, var_expr,
                              node->expr);
        }

        break;
    }
    case IR_DECL_NODE_FUNC_PARAM: {
        symtab = &ts->func->func.locals;

        var_expr->var.type = expr_type;
        var_expr->var.name = trans_decl_node_name(symtab, node->id);
        var_expr->var.local = true;

        ir_expr_t *alloca = ir_expr_create(ts->tunit, IR_EXPR_ALLOCA);
        alloca->alloca.type = ptr_type;
        alloca->alloca.elem_type = var_expr->var.type;
        alloca->alloca.nelem_type = NULL;
        alloca->alloca.align = ast_type_align(node_type);

        // Stack variable to refer to paramater by
        ir_expr_t *temp = trans_assign_temp(ts, &ts->func->func.prefix, alloca);

        // Record the function parameter
        sl_append(&ts->func->func.params, &var_expr->link);

        // Store the paramater's value into the stack allocated space
        ir_stmt_t *store = ir_stmt_create(ts->tunit, IR_STMT_STORE);
        store->store.type = var_expr->var.type;
        store->store.val = var_expr;
        store->store.ptr = temp;
        trans_add_stmt(ts, &ts->func->func.body, store);

        access = temp;
        break;
    }
    default:
        assert(false);
    }

    // Create the symbol table entry
    ir_symtab_entry_t *entry =
        ir_symtab_entry_create(IR_SYMTAB_ENTRY_VAR, var_expr->var.name);
    entry->var.expr = var_expr;
    entry->var.access = access;
    status_t status = ir_symtab_insert(symtab, entry);
    assert(status == CCC_OK);

    // Associate the given variable with the created entry
    typetab_entry_t *tt_ent = tt_lookup(ts->typetab, node->id);
    assert(tt_ent != NULL && tt_ent->entry_type == TT_VAR);
    tt_ent->var.ir_entry = entry;

    return expr_type;
}
