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
 * Initializer typechecking functions
 */
// TODO0: Now NULL is being returned both for errors and zero initializer...

#include "typecheck_priv.h"
#include "typecheck_init_priv.h"

#include "util/logger.h"

bool typecheck_init_list(tc_state_t *tcs, type_t *type, expr_t *expr) {
    assert(expr != NULL);
    assert(expr->type == EXPR_INIT_LIST);
    type = ast_type_unmod(type);
    if (!typecheck_canon_init(tcs, type, expr)) {
        return false;
    }

    return typecheck_init_list_helper(tcs, type, expr);
}

bool typecheck_init_list_helper(tc_state_t *tcs, type_t *type, expr_t *expr) {
    bool retval = true;
    expr->etype = type;
    type = ast_type_unmod(type);

    switch (type->type) {
    case TYPE_UNION: {
        // If no elements, its 0 initalized
        if (vec_size(&expr->init_list.exprs) == 0) {
            return true;
        }

        expr_t *head = vec_front(&expr->init_list.exprs);

        if (head == NULL) { // 0 initializer
            return true;
        }

        if (head->type == EXPR_INIT_LIST) {
            retval &= typecheck_init_list_helper(tcs, head->etype, head);
        } else {
            retval &= typecheck_expr(tcs, head, TC_NOCONST);
        }

        return retval;
    }
    case TYPE_STRUCT: {
        struct_iter_t iter;
        struct_iter_init(type, &iter);

        VEC_FOREACH(cur, &expr->init_list.exprs) {
            expr_t *cur_expr = vec_get(&expr->init_list.exprs, cur);

            // Skip anonymous members that can't be struct/union
            while (!struct_iter_has_node(&iter) &&
                   !struct_iter_has_anon_struct(&iter)) {
                struct_iter_advance(&iter);
            }

            // NULL means 0 initialized
            if (cur_expr == NULL) {
                struct_iter_advance(&iter);
                continue;
            }

            type_t *expr_type = NULL;

            if (struct_iter_has_node(&iter)) {
                expr_type = iter.node->type;
                if (cur_expr->type == EXPR_INIT_LIST) {
                    retval &= typecheck_init_list_helper(tcs, expr_type,
                                                         cur_expr);
                } else {
                    retval &= typecheck_expr(tcs, cur_expr, TC_NOCONST);
                }
            }

            if (struct_iter_has_anon_struct(&iter)) {
                expr_type = iter.decl->type;
                assert(cur_expr->type == EXPR_INIT_LIST);
                retval &= typecheck_init_list_helper(tcs, expr_type, cur_expr);
            }

            assert(expr_type != NULL);
            if (!retval) {
                return false;
            }
            retval &= typecheck_type_assignable(cur_expr->mark, expr_type,
                                                cur_expr->etype);

            struct_iter_advance(&iter);
        }

        return retval;
    }
    case TYPE_ARR: {
        ssize_t decl_len = -1;
        if (type->arr.len != NULL) {
            decl_len = type->arr.nelems;
        }

        long len = 0;
        VEC_FOREACH(cur, &expr->init_list.exprs) {
            ++len;
            expr_t *cur_expr = vec_get(&expr->init_list.exprs, cur);

            // NULL means 0 initializer
            if (cur_expr == NULL) {
                continue;
            }
            if (cur_expr->type == EXPR_INIT_LIST) {
                retval &= typecheck_init_list_helper(tcs, type->arr.base,
                                                     cur_expr);
            } else {
                retval &= typecheck_expr(tcs, cur_expr, TC_NOCONST);
            }
            if (!retval) {
                return false;
            }
            retval &= typecheck_type_assignable(cur_expr->mark,
                                                type->arr.base,
                                                cur_expr->etype);
        }

        // Set array length to the number of elems in the init list
        if (type->arr.len == NULL) {
            type->arr.nelems = len;
        }

        if (decl_len >= 0 && decl_len < len) {
            logger_log(expr->mark, LOG_WARN,
                       "excess elements in array initializer");
            vec_resize(&expr->init_list.exprs, decl_len);
        }
        return retval;
    }
    default: {
        if (vec_size(&expr->init_list.exprs) > 0) {
            expr_t *first = vec_front(&expr->init_list.exprs);
            if (first == NULL) {
                return true;
            }

            if (!(retval &= typecheck_expr(tcs, first, TC_NOCONST))) {
                return false;
            }
            retval &= typecheck_type_assignable(first->mark, type,
                                                first->etype);
        }
    }
    }
    return retval;
}

expr_t *typecheck_canon_init_struct(tc_state_t *tcs, type_t *type,
                                    vec_iter_t *iter, expr_t *expr) {
    assert(type->type == TYPE_STRUCT);
    size_t nmembers = ast_type_num_members(type);

    // New init list vector
    vec_t new_vec;
    vec_init(&new_vec, nmembers);

    // Initalize map to all NULL
    vec_resize(&new_vec, nmembers);
    for (size_t i = 0; i < nmembers; ++i) {
        vec_set(&new_vec, i, NULL);
    }

    // Current member number
    size_t cur_off = 0;
    size_t iters = 0;

    // Current member we're working on in the struct
    struct_iter_t mem_iter;
    struct_iter_init(type, &mem_iter);

    while (vec_iter_has_next(iter)) {
        // Skip non struct/union anonymous members
        while (!struct_iter_end(&mem_iter) &&
               !struct_iter_has_node(&mem_iter) &&
               !struct_iter_has_anon_struct(&mem_iter)) {
            struct_iter_advance(&mem_iter);
        }

        type_t *elem_type = NULL;
        if (struct_iter_has_node(&mem_iter)) {
            elem_type = mem_iter.node->type;
        } else if (struct_iter_has_anon_struct(&mem_iter)) {
            elem_type = mem_iter.decl->type;
        }

        expr_t *cur = vec_iter_get(iter);

        // If this isn't this type's init list, and we already matched one
        // element, then ignore the next element if its a designated initializer
        if (expr == NULL && iters > 0 &&
            (cur->type == EXPR_ARR_IDX || cur->type == EXPR_MEM_ACC)) {
            break;
        }

        // Handle designated initalizer to change index
        if (cur->type == EXPR_MEM_ACC) {
            struct_iter_init(type, &mem_iter);
            cur_off = 0;
            bool in_anon = false;

            do {
                if (mem_iter.node != NULL && mem_iter.node->id != NULL &&
                    strcmp(mem_iter.node->id, cur->mem_acc.name) == 0) {
                    elem_type = mem_iter.node->type;
                    // Found the member
                    break;
                }
                if (struct_iter_has_anon_struct(&mem_iter) &&
                    ast_type_find_member(mem_iter.decl->type,
                                         cur->mem_acc.name, NULL) != NULL) {
                    elem_type = mem_iter.decl->type;
                    in_anon = true;
                    break;
                }
                ++cur_off;
            } while (struct_iter_advance(&mem_iter));

            if (struct_iter_end(&mem_iter)) {
                logger_log(cur->mark, LOG_ERR,
                           "unknown field '%s' specified in initializer",
                           cur->mem_acc.name);
                goto fail;
            }

            if (!vec_iter_has_next(iter)) {
                logger_log(cur->mark, LOG_ERR,
                           "Expected expression for designated initializer");
                goto fail;
            }
            // If its in an anonymous member, then we don't want to consume
            // the designated initializer
            if (!in_anon) {
                vec_iter_advance(iter);
                cur = vec_iter_get(iter);
            }
        }

        // We reached the end of the struct
        if (struct_iter_end(&mem_iter)) {
            break;
        }

        expr_t *val = NULL;

        // If we encounter an initlist, use that as the new iterator
        if (cur->type == EXPR_INIT_LIST) {
            vec_t temp;
            vec_move(&temp, &cur->init_list.exprs, false);
            vec_iter_t inner_iter = { &temp, 0 };
            val = typecheck_canon_init_helper(tcs, elem_type, &inner_iter, cur);
            vec_destroy(&temp);
            vec_iter_advance(iter);
        } else {
            val = typecheck_canon_init_helper(tcs, elem_type, iter, NULL);
        }

        if (val == NULL) {
            goto fail;
        }

        assert(cur_off < nmembers);
        vec_set(&new_vec, cur_off, val);

        struct_iter_advance(&mem_iter);
        ++cur_off;
        ++iters;
    }

    if (expr == NULL) {
        fmark_t *mark = NULL;
        if (vec_size(&new_vec) > 0) {
            expr_t *front = vec_front(&new_vec);
            if (front != NULL) {
                mark = front->mark;
            }
        }

        expr = ast_expr_create(tcs->tunit, mark, EXPR_INIT_LIST);
        vec_move(&expr->init_list.exprs, &new_vec, true);
    } else {
        vec_move(&expr->init_list.exprs, &new_vec, false);
    }

    return expr;

fail:
    vec_destroy(&new_vec);
    return NULL;
}

expr_t *typecheck_canon_init_union(tc_state_t *tcs, type_t *type,
                                   vec_iter_t *iter, expr_t *expr) {
    struct_iter_t mem_iter;
    struct_iter_init(type, &mem_iter);
    type_t *dest_type;

    // Skip anonymous members that can't be struct/union
    while (!struct_iter_has_node(&mem_iter) &&
           !struct_iter_has_anon_struct(&mem_iter)) {
        struct_iter_advance(&mem_iter);
    }
    dest_type = mem_iter.node == NULL ?
        mem_iter.decl->type : mem_iter.node->type;

    expr_t *head = NULL;
    if (vec_iter_has_next(iter)) {
        head = vec_iter_get(iter);
        vec_iter_advance(iter);
    }

    if (head != NULL && head->type == EXPR_MEM_ACC) {
        // Find the member
        do {
            if (struct_iter_has_node(&mem_iter) &&
                strcmp(mem_iter.node->id, head->mem_acc.name) == 0) {
                dest_type = mem_iter.node->type;
                break;
            }
            if (struct_iter_has_anon_struct(&mem_iter) &&
                ast_type_find_member(mem_iter.decl->type,
                                     head->mem_acc.name, NULL) != NULL) {
                dest_type = mem_iter.decl->type;
                break;
            }
        } while (struct_iter_advance(&mem_iter));

        if (struct_iter_end(&mem_iter)) {
            logger_log(head->mark, LOG_ERR,
                       "unknown field '%s' specified in initializer",
                       head->mem_acc.name);
            return NULL;
        }

        if (!vec_iter_has_next(iter)) {
            logger_log(head->mark, LOG_ERR,
                       "expected expression for designated initializer");
            return NULL;
        }
        head = vec_iter_get(iter);
        vec_iter_advance(iter);
    }

    // Set type to first member's type if init list
    if (head != NULL && head->type == EXPR_INIT_LIST) {
        head->etype = dest_type;
    }

    if (expr == NULL) {
        fmark_t *mark = head == NULL ? NULL : head->mark;
        expr = ast_expr_create(tcs->tunit, mark, EXPR_INIT_LIST);
        vec_push_back(&expr->init_list.exprs, head);
    } else {
        if (vec_iter_has_next(iter)) {
            logger_log(expr->mark, LOG_WARN,
                       "excess elements in union initializer");
        }

        vec_resize(iter->vec, 1);
        vec_set(iter->vec, 0, head);

        vec_move(&expr->init_list.exprs, iter->vec, false);
    }

    return expr;
}

expr_t *typecheck_canon_init_arr(tc_state_t *tcs, type_t *type,
                                 vec_iter_t *iter, expr_t *expr) {
    assert(type->type == TYPE_ARR);
    size_t nelems = type->arr.nelems;
    type_t *elem_type = type->arr.base;


    // Holds map from index to array value
    vec_t idx_map;
    vec_init(&idx_map, nelems);

    // Initalize map to all NULL
    vec_resize(&idx_map, nelems);
    for (size_t i = 0; i < nelems; ++i) {
        vec_set(&idx_map, i, NULL);
    }

    // Current index we're working on
    size_t index = 0;

    size_t max_idx = 0; // Max index we encountered
    size_t iters = 0; // Number of elements successfully set

    while (vec_iter_has_next(iter)) {
        expr_t *cur = vec_iter_get(iter);

        // If this isn't this type's init list, and we already matched one
        // element, then ignore the next element if its a designated initializer
        if (expr == NULL && iters > 0 &&
            (cur->type == EXPR_ARR_IDX || cur->type == EXPR_MEM_ACC)) {
            break;
        }

        // Handle designated initalizer to change index
        if (cur->type == EXPR_ARR_IDX) {
            long long idx_val;
            if (!typecheck_const_expr(cur->arr_idx.index, &idx_val, false)) {
                goto fail;
            }

            if (idx_val < 0 ||
                (nelems > 0 && (unsigned long long)idx_val > nelems)) {
                logger_log(cur->mark, LOG_ERR,
                           "array index in initializer exceeds array bounds");
                goto fail;
            }

            index = idx_val;

            if (!vec_iter_has_next(iter)) {
                logger_log(cur->mark, LOG_ERR,
                           "Expected expression for designated initializer");
                goto fail;
            }
            vec_iter_advance(iter);
            cur = vec_iter_get(iter);
        }

        // Reached the end of the array
        if (nelems > 0 && index >= nelems) {
            break;
        }

        if (index > max_idx) {
            max_idx = index;
        }

        expr_t *val = NULL;

        // Grow vector if its not big enough. This is needed when array size is
        // not specified
        if (vec_capacity(&idx_map) < index + 1) {
            size_t old_size = vec_size(&idx_map);
            vec_resize(&idx_map, (index + 1) * 2);
            for (size_t i = old_size; i < vec_size(&idx_map); ++i) {
                vec_set(&idx_map, i, NULL);
            }
        }

        // If we encounter an initlist, use that as the new iterator
        if (cur->type == EXPR_INIT_LIST) {
            vec_t temp;
            vec_move(&temp, &cur->init_list.exprs, false);
            vec_iter_t inner_iter = { &temp, 0 };
            val = typecheck_canon_init_helper(tcs, elem_type, &inner_iter, cur);
            vec_destroy(&temp);
            vec_iter_advance(iter);
        } else {
            val = typecheck_canon_init_helper(tcs, elem_type, iter, NULL);
        }

        if (val == NULL) {
            goto fail;
        }

        vec_set(&idx_map, index, val);

        ++index;
        ++iters;
    }

    vec_resize(&idx_map, max_idx + 1);

    if (expr == NULL) {
        fmark_t *mark = NULL;
        if (vec_size(&idx_map) > 0) {
            expr_t *front = vec_front(&idx_map);
            if (front != NULL) {
                mark = front->mark;
            }
        }

        expr = ast_expr_create(tcs->tunit, mark, EXPR_INIT_LIST);
        vec_move(&expr->init_list.exprs, &idx_map, true);
    } else {
        vec_move(&expr->init_list.exprs, &idx_map, false);
    }

    return expr;

fail:
    vec_destroy(&idx_map);
    return NULL;
}

expr_t *typecheck_canon_init_helper(tc_state_t *tcs, type_t *type,
                                    vec_iter_t *iter, expr_t *expr) {
    assert(expr == NULL || expr->type == EXPR_INIT_LIST);

    type = ast_type_unmod(type);
    switch (type->type) {
    case TYPE_UNION:
        return typecheck_canon_init_union(tcs, type, iter, expr);
    case TYPE_STRUCT:
        return typecheck_canon_init_struct(tcs, type, iter, expr);
    case TYPE_ARR:
        return typecheck_canon_init_arr(tcs, type, iter, expr);

        // Not a compound type, if there's an element, return the first item
        // Otherwise, just return NULL indicating zero initalizer
    default: {
        if (!vec_iter_has_next(iter)) {
            return NULL;
        }
        expr_t *result = vec_iter_get(iter);
        vec_iter_advance(iter);

        if (result->type == EXPR_INIT_LIST) {
            logger_log(result->mark, LOG_WARN,
                       "braces around scalar initializer");
        }

        if (result->type == EXPR_INIT_LIST) {
            vec_t *vec = &result->init_list.exprs;
            switch (vec_size(vec)) {
            case 0:
                logger_log(result->mark, LOG_ERR, "empty scalar initializer");
                return NULL;
            case 1:
                break;
            default:
                logger_log(result->mark, LOG_WARN,
                           "excess elements in scalar initializer");
            }
        }

        return result;
    }
    }

    assert(false);
    return NULL;
}

bool typecheck_canon_init(tc_state_t *tcs, type_t *type, expr_t *expr) {
    assert(expr->type == EXPR_INIT_LIST);
    vec_t temp;
    vec_move(&temp, &expr->init_list.exprs, false);
    vec_iter_t iter = { &temp, 0 };
    expr_t *result = typecheck_canon_init_helper(tcs, type, &iter, expr);
    vec_destroy(&temp);

    // Handle compound literals. We want to preserve the outer init list for
    // scalars
    if (type->type != TYPE_STRUCT && type->type != TYPE_UNION &&
        type->type != TYPE_ARR) {
        assert(result != expr);
        vec_init(&expr->init_list.exprs, 1);
        vec_push_back(&expr->init_list.exprs, result);
    }

    return result != NULL;
}
