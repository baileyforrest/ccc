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

#ifndef _TYPECHECK_INIT_PRIV_H_
#define _TYPECHECK_INIT_PRIV_H_

#include "typecheck_init.h"

bool typecheck_init_list_helper(tc_state_t *tcs, type_t *type, expr_t *expr);

expr_t *typecheck_canon_init_struct(tc_state_t *tcs, type_t *type,
                                    vec_iter_t *iter, expr_t *expr);

expr_t *typecheck_canon_init_union(tc_state_t *tcs, type_t *type,
                                   vec_iter_t *iter, expr_t *expr);

expr_t *typecheck_canon_init_arr(tc_state_t *tcs, type_t *type,
                                 vec_iter_t *iter, expr_t *expr);

expr_t *typecheck_canon_init_helper(tc_state_t *tcs, type_t *type,
                                    vec_iter_t *iter, expr_t *expr);

bool typecheck_canon_init(tc_state_t *tcs, type_t *type, expr_t *expr);

#endif /* _TYPECHECK_INIT_PRIV_H_ */
