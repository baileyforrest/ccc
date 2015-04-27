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
#include <stdlib.h>

#include "typecheck/typechecker.h"

/**
 * Allocate an AST node, add it onto the translation unit's allocation list,
 * set up marks
 *
 * @param tunit The translation unit
 * @param mark The location of the node
 * @param loc location to store the result
 */
#define ALLOC_NODE(list, mark, loc) \
    do { \
        (loc) = ecalloc(1, sizeof(*(loc))); \
        sl_append(list, &(loc)->heap_link); \
        memcpy(&(loc)->mark, mark, sizeof(fmark_t)); \
    } while(0)

type_t *ast_type_create(trans_unit_t *tunit, fmark_t *mark, type_type_t type) {
    type_t *node;
    ALLOC_NODE(&tunit->types, mark, node);
    node->type = type;

    switch (type) {
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
    case TYPE_VA_LIST:
        assert(false && "Use static types");
        break;

    case TYPE_STRUCT:
    case TYPE_UNION:
        sl_init(&node->struct_params.decls, offsetof(decl_t, link));
        break;
    case TYPE_ENUM:
        sl_init(&node->enum_params.ids, offsetof(decl_node_t, link));
        break;

    case TYPE_FUNC:
        sl_init(&node->func.params, offsetof(decl_t, link));
        break;

    case TYPE_TYPEDEF:
    case TYPE_MOD:
    case TYPE_PAREN:
    case TYPE_ARR:
    case TYPE_PTR:
        break;
    default:
        assert(false);
    }

    return node;
}

expr_t *ast_expr_create(trans_unit_t *tunit, fmark_t *mark, expr_type_t type) {
    expr_t *node;
    ALLOC_NODE(&tunit->exprs, mark, node);
    node->type = type;

    switch (type) {
    case EXPR_CALL:
        sl_init(&node->call.params, offsetof(expr_t, link));
        break;
    case EXPR_CMPD:
        sl_init(&node->cmpd.exprs, offsetof(expr_t, link));
        break;
    case EXPR_OFFSETOF:
        sl_init(&node->offsetof_params.path, offsetof(str_node_t, link));
        break;
    case EXPR_INIT_LIST:
        sl_init(&node->init_list.exprs, offsetof(expr_t, link));
        break;

    case EXPR_VOID:
    case EXPR_PAREN:
    case EXPR_VAR:
    case EXPR_ASSIGN:
    case EXPR_CONST_INT:
    case EXPR_CONST_FLOAT:
    case EXPR_CONST_STR:
    case EXPR_BIN:
    case EXPR_UNARY:
    case EXPR_COND:
    case EXPR_CAST:
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
    case EXPR_MEM_ACC:
    case EXPR_ARR_IDX:
    case EXPR_DESIG_INIT:
    case EXPR_VA_START:
    case EXPR_VA_ARG:
    case EXPR_VA_END:
    case EXPR_VA_COPY:
        break;
    default:
        assert(false);
        break;
    }

    return node;
}

decl_node_t *ast_decl_node_create(trans_unit_t *tunit, fmark_t *mark) {
    decl_node_t *node;
    ALLOC_NODE(&tunit->decl_nodes, mark, node);

    return node;
}

decl_t *ast_decl_create(trans_unit_t *tunit, fmark_t *mark) {
    decl_t *node;
    ALLOC_NODE(&tunit->decls, mark, node);

    sl_init(&node->decls, offsetof(decl_node_t, link));

    return node;
}

stmt_t *ast_stmt_create(trans_unit_t *tunit, fmark_t *mark, stmt_type_t type) {
    stmt_t *node;
    ALLOC_NODE(&tunit->stmts, mark, node);
    node->type = type;

    switch (type) {
    case STMT_SWITCH:
        sl_init(&node->switch_params.cases, offsetof(stmt_t, link));
        break;

    case STMT_COMPOUND:
        sl_init(&node->compound.stmts, offsetof(stmt_t, link));
        break;

    case STMT_NOP:
    case STMT_DECL:
    case STMT_LABEL:
    case STMT_CASE:
    case STMT_DEFAULT:
    case STMT_IF:
    case STMT_DO:
    case STMT_WHILE:
    case STMT_FOR:
    case STMT_GOTO:
    case STMT_CONTINUE:
    case STMT_BREAK:
    case STMT_RETURN:
    case STMT_EXPR:
        break;
    default:
        assert(false);
        break;
    }

    return node;
}

gdecl_t *ast_gdecl_create(trans_unit_t *tunit, fmark_t *mark,
                          gdecl_type_t type) {
    gdecl_t *node;
    ALLOC_NODE(&tunit->gdecl_nodes, mark, node);
    node->type = type;

    static const ht_params_t s_gdecl_ht_params = {
        0,                              // Size estimate
        offsetof(stmt_t, label.label),  // Offset of key
        offsetof(stmt_t, label.link),   // Offset of ht link
        ind_str_hash,                   // Hash function
        ind_str_eq,                     // void string compare
    };

    switch (type) {
    case GDECL_FDEFN:
        sl_init(&node->fdefn.gotos, offsetof(stmt_t, goto_params.link));
        ht_init(&node->fdefn.labels, &s_gdecl_ht_params);
        break;
    case GDECL_NOP:
    case GDECL_DECL:
        break;
    default:
        break;
    }

    return node;
}

trans_unit_t *ast_trans_unit_create(bool dummy) {
    trans_unit_t *node = emalloc(sizeof(*node));
    sl_init(&node->gdecls, offsetof(gdecl_t, link));
    if (dummy) {
        // Don't insert primitive types if dummy
        tt_init(&node->typetab, (void *)node);
        node->typetab.last = NULL;
    } else {
        tt_init(&node->typetab, NULL);
    }

    sl_init(&node->gdecl_nodes, offsetof(gdecl_t, heap_link));
    sl_init(&node->stmts, offsetof(stmt_t, heap_link));
    sl_init(&node->decls, offsetof(decl_t, heap_link));
    sl_init(&node->decl_nodes, offsetof(decl_node_t, heap_link));
    sl_init(&node->exprs, offsetof(expr_t, heap_link));
    sl_init(&node->types, offsetof(type_t, heap_link));

    return node;
}

void ast_destroy(trans_unit_t *trans_unit) {
    if (trans_unit == NULL) {
        return;
    }
    tt_destroy(&trans_unit->typetab);
    SL_DESTROY_FUNC(&trans_unit->gdecl_nodes, ast_gdecl_destroy);
    SL_DESTROY_FUNC(&trans_unit->stmts, ast_stmt_destroy);
    SL_DESTROY_FUNC(&trans_unit->decls, ast_decl_destroy);
    SL_DESTROY_FUNC(&trans_unit->decl_nodes, ast_decl_node_destroy);
    SL_DESTROY_FUNC(&trans_unit->exprs, ast_expr_destroy);
    SL_DESTROY_FUNC(&trans_unit->types, ast_type_destroy);
    free(trans_unit);
}

void ast_type_destroy(type_t *type) {
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
    case TYPE_VA_LIST:
        assert(false && "These should be statically allocated");
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_ENUM:
    case TYPE_TYPEDEF:
    case TYPE_MOD:
    case TYPE_PAREN:
    case TYPE_FUNC:
    case TYPE_ARR:
    case TYPE_PTR:
        break;
    default:
        assert(false);
    }
    free(type);
}

void ast_expr_destroy(expr_t *expr) {
    switch (expr->type) {
    case EXPR_OFFSETOF:
        SL_DESTROY_FUNC(&expr->offsetof_params.path, free);
        break;
    case EXPR_VOID:
    case EXPR_PAREN:
    case EXPR_VAR:
    case EXPR_ASSIGN:
    case EXPR_CONST_INT:
    case EXPR_CONST_FLOAT:
    case EXPR_CONST_STR:
    case EXPR_BIN:
    case EXPR_UNARY:
    case EXPR_COND:
    case EXPR_CAST:
    case EXPR_CALL:
    case EXPR_CMPD:
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
    case EXPR_MEM_ACC:
    case EXPR_ARR_IDX:
    case EXPR_INIT_LIST:
    case EXPR_DESIG_INIT:
    case EXPR_VA_START:
    case EXPR_VA_ARG:
    case EXPR_VA_END:
    case EXPR_VA_COPY:
        break;
    default:
        assert(false);
    }
    free(expr);
}

void ast_decl_node_destroy(decl_node_t *decl_node) {
    free(decl_node);
}

void ast_decl_destroy(decl_t *decl) {
    free(decl);
}

void ast_stmt_destroy(stmt_t *stmt) {
    switch (stmt->type) {
    case STMT_FOR:
        if (stmt->for_params.typetab != NULL) {
            tt_destroy(stmt->for_params.typetab);
            free(stmt->for_params.typetab);
        }
        break;
    case STMT_COMPOUND:
        tt_destroy(&stmt->compound.typetab);
        break;

    case STMT_NOP:
    case STMT_DECL:
    case STMT_LABEL:
    case STMT_CASE:
    case STMT_DEFAULT:
    case STMT_IF:
    case STMT_SWITCH:
    case STMT_DO:
    case STMT_WHILE:
    case STMT_GOTO:
    case STMT_CONTINUE:
    case STMT_BREAK:
    case STMT_RETURN:
    case STMT_EXPR:
        break;
    default:
        assert(false);
    }
    free(stmt);
}

void ast_gdecl_destroy(gdecl_t *gdecl) {
    switch (gdecl->type) {
    case GDECL_FDEFN:
        ht_destroy(&gdecl->fdefn.labels);
        break;
    case GDECL_DECL:
        break;
    default:
        assert(false);
    }
    free(gdecl);
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

        // TODO1: Handle bit fields
    case TYPE_STRUCT:
    case TYPE_UNION: {
        size_t size = 0;
        SL_FOREACH(cur, &type->struct_params.decls) {
            decl_t *decl = GET_ELEM(&type->struct_params.decls, cur);

            SL_FOREACH(icur, &decl->decls) {
                decl_node_t *decl_node = GET_ELEM(&decl->decls, icur);
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
        SL_FOREACH(cur, &type->struct_params.decls) {
            decl_t *decl = GET_ELEM(&type->struct_params.decls, cur);

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

size_t ast_type_offset(type_t *type, slist_t *path) {
    size_t offset = 0;
    SL_FOREACH(cur, path) {
        size_t inner_offset;
        str_node_t *node = GET_ELEM(path, cur);
        if (type->type != TYPE_STRUCT || type->type != TYPE_UNION) {
            return -1;
        }
        type = ast_type_find_member(type, node->str, &inner_offset, NULL);
        if (type == NULL) {
            return -1;
        }
        offset += inner_offset;
    }

    return offset;
}

size_t ast_get_member_num(type_t *type, char *name) {
    size_t mem_num;
    type = ast_type_unmod(type);
    type = ast_type_find_member(type, name, NULL, &mem_num);
    if (type == NULL) {
        return -1;
    }

    return mem_num;
}

type_t *ast_type_find_member(type_t *type, char *name, size_t *offset,
                             size_t *mem_num) {
    assert(type->type == TYPE_STRUCT || type->type == TYPE_UNION);
    size_t cur_offset = 0;
    size_t cur_mem_num = 0;

    SL_FOREACH(cur, &type->struct_params.decls) {
        decl_t *decl = GET_ELEM(&type->struct_params.decls, cur);

        // Search in anonymous structs and unions too
        if (sl_head(&decl->decls) == NULL) {
            type_t *decl_type = ast_type_unmod(decl->type);
            switch (decl_type->type) {
            case TYPE_STRUCT:
            case TYPE_UNION: {
                size_t inner_offset;
                size_t inner_memnum;
                size_t *offset_p = offset == NULL ? NULL : &inner_offset;
                size_t *mem_num_p = mem_num == NULL ? NULL : &inner_memnum;
                type_t *inner_type =
                    ast_type_find_member(decl_type, name, offset_p, mem_num_p);
                if (inner_type != NULL) {
                    if (offset != NULL) {
                        *offset = cur_offset + inner_offset;
                    }
                    if (mem_num != NULL) {
                        *mem_num = cur_mem_num + inner_memnum;
                    }
                    return inner_type;
                }
                break;
            }
            default:
                break;
            }
        } else {
            SL_FOREACH(cur_node, &decl->decls) {
                decl_node_t *node = GET_ELEM(&decl->decls, cur_node);
                if (strcmp(node->id, name) == 0) {
                    if (offset != NULL) {
                        *offset = cur_offset;
                    }
                    if (mem_num != NULL) {
                        *mem_num = cur_mem_num;
                    }
                    return node->type;
                }

                // Only add offsets for struct types
                if (type->type == TYPE_STRUCT) {
                    if (offset != NULL) {
                        cur_offset += ast_type_size(node->type);
                    }
                    if (mem_num != NULL) {
                        ++cur_mem_num;
                    }
                }
            }
        }
    }

    return NULL;
}

type_t *ast_type_untypedef(type_t *type) {
    bool done = false;
    while (!done) {
        switch (type->type) {
        case TYPE_TYPEDEF:
            type = type->typedef_params.base;
            break;
        case TYPE_PAREN:
            type = type->paren_base;
            break;
        case TYPE_MOD:
            // If the modifier is only typedef, then remove it
            if (type->mod.type_mod & ~TMOD_TYPEDEF) {
                done = true;
            } else {
                type = type->mod.base;
            }
            break;
        default:
            done = true;
        }
    }

    return type;
}

type_t *ast_type_unmod(type_t *type) {
    type = ast_type_untypedef(type);
    while (type->type == TYPE_MOD) {
        type = type->mod.base;
        type = ast_type_untypedef(type);
    }

    return type;
}
