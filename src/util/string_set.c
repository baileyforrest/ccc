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

#define STR_SET_ELEM(list_link) \
    (str_node_t *)((char *)(list_link) - offsetof(str_node_t, link));

void str_set_init(str_set_t *set) {
    set->head = NULL;
}

void str_set_destroy(str_set_t *set) {
    for (sl_link_t *cur = set->head, *next; cur != NULL; cur = next) {
        str_node_t *elem = STR_SET_ELEM(cur);
        next = cur->next;
        free(elem);
    }
}

void str_set_copy(str_set_t *dest, str_set_t *set) {
    str_set_init(dest);
    sl_link_t **ptail = &dest->head;

    for (sl_link_t *cur = set->head; cur != NULL; cur = cur->next) {
        str_node_t *elem = STR_SET_ELEM(cur);

        str_node_t *new_elem = emalloc(sizeof(str_node_t));
        new_elem->str = elem->str;
        *ptail = &new_elem->link;
        ptail = &new_elem->link.next;
    }
    *ptail = NULL;
}

bool str_set_mem(str_set_t *set, char *str) {
    for (sl_link_t *cur = set->head; cur != NULL; cur = cur->next) {
        str_node_t *elem = STR_SET_ELEM(cur);
        if (strcmp(str, elem->str) == 0) {
            return true;
        }
    }

    return false;
}

void str_set_add(str_set_t *set, char *str) {
    sl_link_t **pcur;
    for (pcur = &set->head; *pcur != NULL; pcur = &(*pcur)->next) {
        str_node_t *elem = STR_SET_ELEM(*pcur);
        if (strcmp(str, elem->str) == 0) {
            return;
        }
    }

    str_node_t *new_elem = emalloc(sizeof(str_node_t));
    new_elem->str = str;
    *pcur = &new_elem->link;
    new_elem->link.next = NULL;
}

void str_set_union(str_set_t *set1, str_set_t *set2, str_set_t *dest) {
    str_set_init(dest);

    for (sl_link_t *cur = set1->head; cur != NULL; cur = cur->next) {
        str_node_t *elem = STR_SET_ELEM(cur);
        str_set_add(dest, elem->str);
    }

    for (sl_link_t *cur = set2->head; cur != NULL; cur = cur->next) {
        str_node_t *elem = STR_SET_ELEM(cur);
        str_set_add(dest, elem->str);
    }
}

void str_set_union_inplace(str_set_t *dest, str_set_t *other) {
    for (sl_link_t *cur = other->head; cur != NULL; cur = cur->next) {
        str_node_t *elem = STR_SET_ELEM(cur);
        str_set_add(dest, elem->str);
    }
}

void str_set_intersect(str_set_t *set1, str_set_t *set2, str_set_t *dest) {
    str_set_init(dest);

    for (sl_link_t *cur = set1->head; cur != NULL; cur = cur->next) {
        str_node_t *elem = STR_SET_ELEM(cur);
        if (str_set_mem(set2, elem->str)) {
            str_set_add(dest, elem->str);
        }
    }
}
