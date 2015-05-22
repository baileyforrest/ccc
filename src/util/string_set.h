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
 * String set interface
 */

#ifndef _STRING_SET_H_
#define _STRING_SET_H_

#include "util/slist.h"

typedef struct str_set_t {
    sl_link_t *head; /**< str_node_t */
} str_set_t;

#define STR_SET_LIT { NULL }

void str_set_init(str_set_t *set);

void str_set_destroy(str_set_t *set);

void str_set_copy(str_set_t *dest, str_set_t *set);

bool str_set_mem(str_set_t *set, char *str);

void str_set_add(str_set_t *set, char *str);

void str_set_union(str_set_t *set1, str_set_t *set2, str_set_t *dest);

void str_set_union_inplace(str_set_t *dest, str_set_t *other);

void str_set_intersect(str_set_t *set1, str_set_t *set2, str_set_t *dest);

#endif /* _STRING_SET_H_ */
