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
 * String set implementation
 */

#include "string_set.h"

#include "util/util.h"

str_set_t *str_set_empty(void) {
    return NULL;
}

void str_set_destroy(str_set_t *set) {
    for (str_set_t *next; set != NULL; set = next) {
        next = set->next;
        free(set);
    }
}

str_set_t *str_set_copy(str_set_t *set) {
    str_set_t *dest = NULL;
    str_set_t **ptail = &dest;

    for (; set != NULL; set = set->next) {
        str_set_t *new_node = emalloc(sizeof(str_set_t));
        new_node->str = set->str;
        *ptail = new_node;
        ptail = &new_node->next;
    }

    *ptail = NULL;

    return dest;
}

bool str_set_mem(str_set_t *set, char *str) {
    for (; set != NULL; set = set->next) {
        if (strcmp(str, set->str) == 0) {
            return true;
        }
    }

    return false;
}

str_set_t *str_set_add(str_set_t *set, char *str) {
    str_set_t **pset;
    for (pset = &set; *pset != NULL; pset = &(*pset)->next) {
        if (strcmp((*pset)->str, str) == 0) {
            return set;
        }
    }

    str_set_t *new_elem = emalloc(sizeof(str_set_t));
    new_elem->str = str;
    new_elem->next = NULL;
    *pset = new_elem;

    return set;
}

str_set_t *str_set_union(str_set_t *set1, str_set_t *set2) {
    str_set_t *dest = str_set_empty();

    for (; set1 != NULL; set1 = set1->next) {
        str_set_add(dest, set1->str);
    }

    for (; set2 != NULL; set2 = set2->next) {
        str_set_add(dest, set2->str);
    }

    return dest;
}

str_set_t *str_set_union_inplace(str_set_t *dest, str_set_t *other) {
    for (; other != NULL; other = other->next) {
        str_set_add(dest, other->str);
    }

    return dest;
}

str_set_t *str_set_intersect(str_set_t *set1, str_set_t *set2) {
    str_set_t *dest = str_set_empty();

    for (; set1 != NULL; set1 = set1->next) {
        if (str_set_mem(set2, set1->str)) {
            str_set_add(dest, set1->str);
        }
    }

    return dest;
}
