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
#include <limits.h>
#include <stdlib.h>

#include "util/logger.h"

/**
 * Allocate an AST node, add it onto the translation unit's allocation list,
 * set up marks
 *
 * @param tunit The translation unit
 * @param mark The location of the node
 * @param loc location to store the result
 */
#define ALLOC_NODE(list, mark, loc)             \
    do {                                        \
        (loc) = ecalloc(1, sizeof(*(loc)));     \
        sl_append(list, &(loc)->heap_link);     \
        (loc)->mark = mark;                     \
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
        node->struct_params.esize = -1;
        node->struct_params.ealign = -1;
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
    case TYPE_STATIC_ASSERT:
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
        sl_init(&node->offsetof_params.list.list, offsetof(expr_t, link));
        break;
    case EXPR_DESIG_INIT:
        sl_init(&node->desig_init.list.list, offsetof(expr_t, link));
        break;
    case EXPR_INIT_LIST:
        vec_init(&node->init_list.exprs, 0);
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
        sl_init(&node->switch_params.cases, offsetof(stmt_t, case_params.link));
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
    case TYPE_STATIC_ASSERT:
        break;
    default:
        assert(false);
    }
    free(type);
}

void ast_expr_destroy(expr_t *expr) {
    switch (expr->type) {
    case EXPR_INIT_LIST:
        vec_destroy(&expr->init_list.exprs);
        break;
    case EXPR_OFFSETOF:
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

void struct_iter_init(type_t *type, struct_iter_t *iter) {
    assert(type->type == TYPE_STRUCT || type->type == TYPE_UNION);
    iter->type = type;
    struct_iter_reset(iter);
}

void struct_iter_reset(struct_iter_t *iter) {
    iter->cur_decl = iter->type->struct_params.decls.head;
    iter->decl = iter->cur_decl == NULL ?
        NULL : GET_ELEM(&iter->type->struct_params.decls, iter->cur_decl);
    iter->cur_node = iter->decl == NULL ? NULL : iter->decl->decls.head;
    iter->node = iter->cur_node == NULL ?
        NULL : GET_ELEM(&iter->decl->decls, iter->cur_node);
}

bool struct_iter_advance(struct_iter_t *iter) {
    if (iter->cur_node != NULL) {
        iter->cur_node = iter->cur_node->next;
    }
    if (iter->cur_node == NULL) {
        if (iter->cur_decl != NULL) {
            iter->cur_decl = iter->cur_decl->next;
        }
        if (iter->cur_decl != NULL) {
            iter->decl = GET_ELEM(&iter->type->struct_params.decls,
                                  iter->cur_decl);
            iter->cur_node = iter->decl->decls.head;
        }
    }
    if (iter->cur_node != NULL) {
        iter->node = GET_ELEM(&iter->decl->decls, iter->cur_node);
    } else {
        iter->node = NULL;
    }

    return !struct_iter_end(iter);
}

bool struct_iter_end(struct_iter_t *iter) {
    return iter->cur_decl == NULL && iter->cur_node == NULL;
}

extern bool struct_iter_has_node(struct_iter_t *iter);
extern bool struct_iter_has_anon_struct(struct_iter_t *iter);

status_t ast_canonicalize_init_list_check_mem_acc(expr_t *expr) {
    if (expr->type == EXPR_MEM_ACC) {
        return CCC_OK;
    }

    logger_log(expr->mark, LOG_ERR, "array index in non-array initializer");
    return CCC_ESYNTAX;
}

bool ast_canonicalize_mem_acc(trans_unit_t *tunit, expr_t *expr, type_t *type,
                              expr_t **tail) {
    if (expr->type != EXPR_MEM_ACC || expr->mem_acc.op != OP_DOT) {
        return false;
    }
    type = ast_type_unmod(type);

    struct_iter_t iter;
    struct_iter_init(type, &iter);
    do {
        // No anonymous members, return
        if (iter.node != NULL && iter.node->id != NULL &&
            strcmp(iter.node->id, expr->mem_acc.name) == 0) {
            if (tail != NULL) {
                *tail = expr;
            }
            return true;
        }
        if (iter.node == NULL && iter.decl != NULL &&
            (iter.decl->type->type == TYPE_STRUCT ||
             iter.decl->type->type == TYPE_UNION)) {
            expr_t *new_tail;
            if (ast_canonicalize_mem_acc(tunit, expr, iter.decl->type,
                                         &new_tail)) {
                assert(new_tail != NULL && new_tail->type == EXPR_MEM_ACC &&
                       new_tail->mem_acc.op == OP_DOT);

                // Member is in anonymous
                // Make a new member access to get it
                expr_t *new_expr = ast_expr_create(tunit, new_tail->mark,
                                                   EXPR_MEM_ACC);
                new_expr->etype = iter.decl->type;
                new_expr->mem_acc.base = new_tail->mem_acc.base;
                new_expr->mem_acc.name = new_tail->mem_acc.name;
                new_expr->mem_acc.op = OP_DOT;
                new_tail->mem_acc.base = new_expr;
                if (tail != NULL) {
                    *tail = new_expr;
                }
                return true;
            }
        }
    } while (struct_iter_advance(&iter));

    return false;
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

    case TYPE_UNION: {
        if (type->struct_params.esize != (size_t)-1) {
            return type->struct_params.esize;
        }
        size_t size = 0;
        struct_iter_t iter;
        struct_iter_init(type, &iter);
        do {
            if (iter.node != NULL && iter.node->id != NULL) {
                size_t cur_size = ast_type_size(iter.node->type);
                size = MAX(size, cur_size);
            } else if (iter.node == NULL && iter.decl != NULL &&
                       (iter.decl->type->type == TYPE_STRUCT ||
                        iter.decl->type->type == TYPE_UNION)) {
                // Anonymous struct/union
                size_t cur_size = ast_type_size(iter.decl->type);
                size = MAX(size, cur_size);
            }
        } while (struct_iter_advance(&iter));

        // Add pending between unions
        size_t align = ast_type_align(type);
        size_t remain = size % align;
        if (remain != 0) {
            size += align - remain;
        }

        return type->struct_params.esize = size;
    }

    case TYPE_STRUCT: {
        if (type->struct_params.esize != (size_t)-1) {
            return type->struct_params.esize;
        }
        size_t size = 0;
        ast_type_find_member(type, NULL, &size);

        return type->struct_params.esize = size;
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
        return size * type->arr.nelems;
    }
    case TYPE_PTR:
        return sizeof(void *);
    case TYPE_VA_LIST:
#ifdef __x86_64__
        return sizeof(int) * 2 + sizeof(char *) * 2;
#else
#error "Unsupported platform"
#endif
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
        if (type->struct_params.ealign != (size_t)-1) {
            return type->struct_params.ealign;
        }
        size_t align = 1;
        struct_iter_t iter;
        struct_iter_init(type, &iter);
        do {
            if (iter.node != NULL && iter.node->id != NULL) {
                size_t cur_align = ast_type_align(iter.node->type);
                align = MAX(align, cur_align);
            } else if (iter.node == NULL && iter.decl != NULL &&
                       (iter.decl->type->type == TYPE_STRUCT ||
                        iter.decl->type->type == TYPE_UNION)) {
                // Anonymous struct/union
                size_t cur_align = ast_type_align(iter.decl->type);
                align = MAX(align, cur_align);
            }
        } while (struct_iter_advance(&iter));

        return type->struct_params.ealign = align;
    }
    case TYPE_ENUM:
        return ast_type_align(type->enum_params.type);

    case TYPE_TYPEDEF:
        return ast_type_align(type->typedef_params.base);

    case TYPE_MOD: {
        size_t alignas_align = 0;
        if (type->mod.type_mod & TMOD_ALIGNAS) {
            alignas_align = type->mod.alignas_align;
        }
        size_t base_align = ast_type_align(type->mod.base);

        return MAX(base_align, alignas_align);
    }

    case TYPE_PAREN:
        return ast_type_align(type->paren_base);

    case TYPE_FUNC:
        return 1; // Functions have an "align" of 1

    case TYPE_ARR:
        return ast_type_align(type->arr.base);
    case TYPE_PTR:
        return alignof(void *);
    case TYPE_VA_LIST:
#ifdef __x86_64__
        return alignof(char *);
#else
#error "Unsupported platform"
#endif
    default:
        assert(false);
    }

    return 0;
}

size_t ast_type_offset(type_t *type, designator_list_t *list) {
    size_t offset = 0;
    SL_FOREACH(cur, &list->list) {
        type = ast_type_unmod(type);
        size_t inner_offset;
        expr_t *cur_expr = GET_ELEM(&list->list, cur);

        switch (cur_expr->type) {
        case EXPR_MEM_ACC: {
            decl_node_t *node = ast_type_find_member(type,
                                                     cur_expr->mem_acc.name,
                                                     &inner_offset);
            type = node->type;
            break;
        }
        case EXPR_ARR_IDX:
            inner_offset = ast_type_size(type->arr.base) *
                cur_expr->arr_idx.const_idx;
            type = type->arr.base;
            break;
        default:
            assert(false);
        }
        offset += inner_offset;
    }

    return offset;
}

size_t ast_type_num_members(type_t *type) {
    size_t result = 0;

    struct_iter_t iter;
    struct_iter_init(type, &iter);
    do {
        // Count named members
        if (iter.node != NULL && iter.node->id != NULL) {
            ++result;
        }

        // Count anonymous struct/union
        if (iter.node == NULL && iter.decl != NULL &&
            (iter.decl->type->type == TYPE_STRUCT ||
             iter.decl->type->type == TYPE_UNION)) {
            ++result;
        }
    } while (struct_iter_advance(&iter));

    return result;
}

decl_node_t *ast_type_find_member(type_t *type, char *name, size_t *offset) {
    assert(type->type == TYPE_STRUCT || type->type == TYPE_UNION);
    size_t cur_offset = 0;
    decl_node_t *result = NULL;

    size_t bitfield_bits = 0;

#define BITFIELD_FINALIZER()                                    \
    (cur_offset += (bitfield_bits + (CHAR_BIT - 1)) / CHAR_BIT, \
     bitfield_bits = 0)

    struct_iter_t iter;
    struct_iter_init(type, &iter);
    do {
        // If we were processing a bitfield, and we found a non bitfield member,
        // then finalize bitfied offset
        if (bitfield_bits != 0 &&
            !(iter.node != NULL && iter.node->expr != NULL)) {
            BITFIELD_FINALIZER();
        }

        type_t *cur_type = NULL; // Current member's type

        if (iter.node != NULL) {
            if (name != NULL && iter.node->id != NULL &&
                strcmp(iter.node->id, name) == 0) {
                result = iter.node;
            }

            if (iter.node->expr != NULL) {
                // We don't need to align the member if its a bitfield,
                // so exit loop
                if (result != NULL) {
                    break;
                }

                // Handle Bitfield bits
                assert(iter.node->expr->type == EXPR_CONST_INT);
                int decl_bf_bits = iter.node->expr->const_val.int_val;

                // 0 bitfield bits causes next field to be aligned at
                // byte boundary
                if (decl_bf_bits == 0) {
                    size_t remain = bitfield_bits % CHAR_BIT;
                    if (remain != 0) {
                        bitfield_bits += CHAR_BIT - remain;
                    }
                    continue;
                }

                // If starting bitfield, align to the bitfield's type
                if (bitfield_bits == 0) {
                    size_t align = ast_type_align(iter.node->type);
                    size_t remain = cur_offset % align;
                    if (remain != 0) {
                        cur_offset += align - remain;
                    }
                }

                bitfield_bits += decl_bf_bits;
            } else if (iter.node->id != NULL) {
                cur_type = iter.node->type;
            }
        }

        // Anonymous struct/union
        if (iter.node == NULL && iter.decl != NULL &&
            (iter.decl->type->type == TYPE_STRUCT ||
             iter.decl->type->type == TYPE_UNION)) {
            cur_type = iter.decl->type;

            if (name != NULL) { // Don't recursively search if no name
                size_t inner_offset;
                decl_node_t *inner_node = ast_type_find_member(cur_type, name,
                                                               &inner_offset);
                if (inner_node != NULL) {
                    result = inner_node;
                    cur_offset += inner_offset;
                }
            }
        }

        if (cur_type != NULL) {
            // Align current offset to the current type
            size_t align = ast_type_align(cur_type);
            size_t remain = cur_offset % align;
            if (remain != 0) {
                cur_offset += align - remain;
            }

            // Don't add on the current type's size if we found the result
            if (result != NULL) {
                break;
            }

            cur_offset += ast_type_size(cur_type);
        }
    } while (struct_iter_advance(&iter));

    // Add the size for trailing bitfields
    if (bitfield_bits != 0) {
        BITFIELD_FINALIZER();
    }

    // If name is NULL, we're finding type size. Add alignment padding
    if (name == NULL) {
        size_t align = ast_type_align(type);
        size_t remain = cur_offset % align;
        if (remain != 0) {
            cur_offset += align - remain;
        }
    }

    if (offset != NULL) {
        if (type->type == TYPE_UNION) { // Unions always have 0 offset
            cur_offset = 0;
        }
        *offset = cur_offset;
    }

    return result;
}

bool ast_is_mem_acc_bitfield(expr_t *expr) {
    if (expr->type != EXPR_MEM_ACC) {
        return false;
    }
    type_t *type = ast_type_unmod(expr->mem_acc.base->etype);
    if (type->type == TYPE_PTR) {
        type = ast_type_unmod(type->ptr.base);
    }
    assert(type->type == TYPE_STRUCT || type->type == TYPE_UNION);

    struct_iter_t iter;
    struct_iter_init(type, &iter);
    do {
        if (iter.node != NULL && iter.node->id != NULL) {
            if (strcmp(iter.node->id, expr->mem_acc.name) == 0) {
                return iter.node->expr != NULL;
            }
        }
    } while (struct_iter_advance(&iter));

    return false;
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

type_t *ast_type_ptr_base(type_t *t1) {
    switch (t1->type) {
    case TYPE_FUNC: return t1;
    case TYPE_PTR:  return t1->ptr.base;
    case TYPE_ARR:  return t1->arr.base;
    default:
        assert(false);
    }
    return NULL;
}

type_t *ast_type_decl_base(type_t *type) {
    while (true) {
        switch (type->type) {
        case TYPE_FUNC: type = type->func.type;
        case TYPE_PTR:  type = type->ptr.base;
        case TYPE_ARR:  type = type->arr.base;
        default:
            return type;
        }
    }

    assert(false);
    return NULL;
}
