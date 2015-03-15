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
 * Table for storing named types
 */

#ifndef _TYPE_TABLE_H_
#define _TYPE_TABLE_H_

#include "util/htable.h"
#include "util/util.h"

struct type_t type;

typedef struct typetab_t {
    struct typetab_t *last;
    htable_t hashtab;
} typetab_t;

typedef enum tt_type_t {
    TT_PRIM,
    TT_TYPEDEF,
    TT_COMPOUND /* struct, union, enum */
} tt_type_t;

typedef struct tt_key_t {
    len_str_t name;
    tt_type_t type;
} tt_key_t;

typedef struct typetab_entry_t {
    sl_link_t link;
    tt_key_t key;
    struct type_t *type;
} typetab_entry_t;

extern struct type_t * const tt_void;
extern struct type_t * const tt_char;
extern struct type_t * const tt_short;
extern struct type_t * const tt_int;
extern struct type_t * const tt_long;
extern struct type_t * const tt_float;
extern struct type_t * const tt_double;

uint32_t typetab_key_hash(const void *key);
bool typetab_key_cmp(const void *key1, const void *key2);

/**
 * Initalizes a type table
 *
 * @param tt Type table to initialize
 * @param Last Typetable in previous scope. NULL if none (top level). If NULL,
 * the typetable will get initialized with primitive types
 *
 * @return CCC_OK on success, error code on error
 */
status_t tt_init(typetab_t *tt, typetab_t *last);

/**
 * Destroys a type table
 *
 * @param tt Type table to destroy
 * @return CCC_OK on success, error code on error
 */
void tt_destroy(typetab_t *tt);

/**
 * Looks up a type in the type table
 *
 * @param tt Type table to lookup in
 * @param key Key to lookup with
 * @return Returns pointer to type table entry, or NULL if it doesn't exist
 */
typetab_entry_t *tt_lookup(typetab_t *tt, tt_key_t *key);

/**
 * Inserts a type into the type table
 *
 * @param tt Type table to insert into
 * @param type The type to insert
 * @param tt_type Type table type to insert
 * @param name The name of the item to insert
 * @param entry The added entry, NULL if it isn't needed
 * @return CCC_OK on success, error code on error
 */
status_t tt_insert(typetab_t *tt, struct type_t *type, tt_type_t tt_type,
                   len_str_t *name, typetab_entry_t **entry);

#endif /* _TYPE_TABLE_H_ */
