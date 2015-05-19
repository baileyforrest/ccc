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
 * Vector of pointers interface
 */
// TODO1: Documentation for this
// TODO1: Add iterator interface for this and slist

#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct vec_t {
    void **elems;
    size_t size;
    size_t capacity;
} vec_t;

typedef struct vec_iter_t {
    vec_t *vec;
    size_t off;
} vec_iter_t;

void vec_init(vec_t *vec, size_t capacity);

void vec_destroy(vec_t *vec);

#define VEC_DESTROY_FUNC(vec, func)                     \
    do {                                                \
        for (size_t i = 0; i < (vec)->capacity; ++i) {  \
            (func)((vec)->elems[i]);                    \
        }                                               \
        vec_destroy(vec);                               \
    } while (0)

inline void *vec_get(vec_t *vec, size_t idx) {
    return vec->elems[idx];
}

inline void **vec_elems(vec_t *vec) {
    return vec->elems;
}

inline size_t vec_size(vec_t *vec) {
    return vec->size;
}

inline void *vec_front(vec_t *vec) {
    return vec->elems[0];
}

inline void *vec_back(vec_t *vec) {
    return vec->elems[vec->size - 1];
}

void vec_push_back(vec_t *vec, void *elem);

inline void *vec_pop_back(vec_t *vec) {
    return vec->elems[--vec->size];
}

#define VEC_FOREACH(cur_idx, vec) \
    for (size_t cur_idx = 0; cur_idx < (vec)->size; ++cur_idx)   \

void vec_iter_init(vec_iter_t *iter, vec_t *vec);

inline bool vec_iter_has_next(vec_iter_t *iter) {
    return iter->off < vec_size(iter->vec);
}

inline void *vec_iter_get(vec_iter_t *iter) {
    assert(vec_iter_has_next(iter));
    return vec_get(iter->vec, iter->off);
}

inline void *vec_iter_advance(vec_iter_t *iter) {
    assert(vec_iter_has_next(iter));
    return vec_get(iter->vec, iter->off++);
}

inline void *vec_iter_reverse(vec_iter_t *iter) {
    assert(vec_iter_has_next(iter));
    return vec_get(iter->vec, iter->off--);
}

void vec_append(vec_t *dest, vec_t *vec2);

#endif /* _VECTOR_H_ */
