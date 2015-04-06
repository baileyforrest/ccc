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
#include "type_table.h"

#include <assert.h>
#include <stdbool.h>
#include <stdalign.h>

#include "parse/ast.h"

#include "util/logger.h"
#include "util/util.h"

static len_str_t s_prim_filename = LEN_STR_LIT("<primitive_type>");

#define TYPE_LITERAL(typename, type) \
    { SL_LINK_LIT, { NULL, &s_prim_filename, "\n", 0, 0 }, typename,    \
      { .ptr = { NULL, 0 } } }

static len_str_t void_str        = LEN_STR_LIT("void");
static len_str_t bool_str        = LEN_STR_LIT("_Bool");
static len_str_t char_str        = LEN_STR_LIT("char");
static len_str_t short_str       = LEN_STR_LIT("short");
static len_str_t int_str         = LEN_STR_LIT("int");
static len_str_t long_str        = LEN_STR_LIT("long");
static len_str_t long_long_str   = LEN_STR_LIT("long long");
static len_str_t float_str       = LEN_STR_LIT("float");
static len_str_t double_str      = LEN_STR_LIT("double");
static len_str_t long_double_str = LEN_STR_LIT("long double");

static type_t stt_void        = TYPE_LITERAL(TYPE_VOID       , void       );
static type_t stt_bool        = TYPE_LITERAL(TYPE_BOOL       , _Bool      );
static type_t stt_char        = TYPE_LITERAL(TYPE_CHAR       , char       );
static type_t stt_short       = TYPE_LITERAL(TYPE_SHORT      , short      );
static type_t stt_int         = TYPE_LITERAL(TYPE_INT        , int        );
static type_t stt_long        = TYPE_LITERAL(TYPE_LONG       , long       );
static type_t stt_long_long   = TYPE_LITERAL(TYPE_LONG_LONG  , long long  );
static type_t stt_float       = TYPE_LITERAL(TYPE_FLOAT      , float      );
static type_t stt_double      = TYPE_LITERAL(TYPE_DOUBLE     , double     );
static type_t stt_long_double = TYPE_LITERAL(TYPE_LONG_DOUBLE, long double);

type_t * const tt_void = &stt_void;
type_t * const tt_bool = &stt_bool;
type_t * const tt_char = &stt_char;
type_t * const tt_short = &stt_short;
type_t * const tt_int = &stt_int;
type_t * const tt_long = &stt_long;
type_t * const tt_long_long = &stt_long_long;
type_t * const tt_float = &stt_float;
type_t * const tt_double = &stt_double;
type_t * const tt_long_double = &stt_long_double;

#define TYPE_TAB_LITERAL_ENTRY(type) \
    { SL_LINK_LIT, &type##_str, TT_PRIM , &stt_ ## type }

/**
 * Table of primative types
 */
static typetab_entry_t s_prim_types[] = {
    TYPE_TAB_LITERAL_ENTRY(void),
    TYPE_TAB_LITERAL_ENTRY(bool),
    TYPE_TAB_LITERAL_ENTRY(char),
    TYPE_TAB_LITERAL_ENTRY(short),
    TYPE_TAB_LITERAL_ENTRY(int),
    TYPE_TAB_LITERAL_ENTRY(long),
    TYPE_TAB_LITERAL_ENTRY(long_long),
    TYPE_TAB_LITERAL_ENTRY(float),
    TYPE_TAB_LITERAL_ENTRY(double),
    TYPE_TAB_LITERAL_ENTRY(long_double)
};

status_t tt_init(typetab_t *tt, typetab_t *last) {
    assert(tt != NULL);
    status_t status = CCC_OK;
    tt->last = last;

    sl_init(&tt->typedef_bases, offsetof(typedef_base_t, link));

    static const ht_params_t params = {
        0,                               // Size estimate
        offsetof(typetab_entry_t, key),  // Offset of key
        offsetof(typetab_entry_t, link), // Offset of ht link
        ind_strhash,                     // Hash function
        ind_vstrcmp,                     // void string compare
    };

    if (CCC_OK != (status = ht_init(&tt->types, &params))) {
        goto fail;
    }

    if (CCC_OK != (status = ht_init(&tt->compound_types, &params))) {
        goto fail;
    }

    // Initialize top level table with primitive types
    if (last == NULL) {
        for (size_t i = 0; i < STATIC_ARRAY_LEN(s_prim_types); ++i) {
            if (CCC_OK != (status = ht_insert(&tt->types,
                                              &s_prim_types[i].link))) {
                goto fail1;
            }
        }
    }
    return status;

fail1:
    ht_destroy(&tt->types);
    ht_destroy(&tt->compound_types);
fail:
    return status;
}

static void typetab_typedef_base_destroy(typedef_base_t *entry) {
    ast_type_destroy(entry->type);
    free(entry);
}

static void typetab_entry_destroy(typetab_entry_t *entry) {
    switch (entry->entry_type) {
    case TT_PRIM:
        // Ignore primitive types, they are in static memory
        return;
    case TT_COMPOUND:
        ast_type_protected_destroy(entry->type);
        break;
    case TT_TYPEDEF:
        ast_decl_node_type_destroy(entry->type);
        break;
    case TT_VAR:
        // Ignore variables, the type is in the decl node
        break;
    }
    free(entry);
}

void tt_destroy(typetab_t *tt) {
    assert(tt != NULL);
    // This order is important. types may point to typedef_bases,
    // typedef_bases may point to compound types
    HT_DESTROY_FUNC(&tt->types, typetab_entry_destroy);
    SL_DESTROY_FUNC(&tt->typedef_bases, typetab_typedef_base_destroy);
    HT_DESTROY_FUNC(&tt->compound_types, typetab_entry_destroy);
}

/**
 * For typedefs, multiple typedefs may share the same base type. So we
 * store the base type in a different hash table which is freed after
 * the typedef hashtable
 */
status_t tt_insert_typedef(typetab_t *tt, decl_t *decl,
                           decl_node_t *decl_node) {
    status_t status = CCC_OK;
    typetab_entry_t *new_entry = malloc(sizeof(typetab_entry_t));
    typedef_base_t *base = NULL;
    if (new_entry == NULL) {
        status = CCC_NOMEM;
        goto fail;
    }

    new_entry->type = decl_node->type;
    new_entry->entry_type = TT_TYPEDEF;
    new_entry->key = decl_node->id;

    // Only create an entry in the second table if its the first decl, to make
    // sure it is only removed once
    if (decl_node == sl_head(&decl->decls)) {
        base = malloc(sizeof(typedef_base_t));
        if (base == NULL) {
            status = CCC_NOMEM;
            goto fail;
        }
        base->type = decl->type;
    }

    // Add base, and don't free on failure because the typedef may fail because
    // of a duplicate typedef. We do this because multiple typedefs may share
    // same base, with only one of them failing
    if (base != NULL) {
        sl_append(&tt->typedef_bases, &base->link);
    }

    if (CCC_OK != (status = ht_insert(&tt->types, &new_entry->link))) {
        goto fail;
    }

    return status;

fail:
    free(new_entry);
    return status;
}

status_t tt_insert(typetab_t *tt, type_t *type, tt_type_t tt_type,
                   len_str_t *name, typetab_entry_t **entry) {
    assert(tt != NULL);
    assert(type != NULL);
    assert(name != NULL);
    assert(tt_type != TT_TYPEDEF && "Use tt_insert_typedef");

    status_t status = CCC_OK;
    typetab_entry_t *new_entry = malloc(sizeof(typetab_entry_t));
    if (new_entry == NULL) {
        status = CCC_NOMEM;
        goto fail;
    }

    new_entry->type = type;
    new_entry->entry_type = tt_type;
    new_entry->key = name;

    if (tt_type == TT_COMPOUND) {
        status = ht_insert(&tt->compound_types, &new_entry->link);
    } else {
        status = ht_insert(&tt->types, &new_entry->link);
    }
    if (status != CCC_OK) {
        goto fail;
    }

    if (entry) {
        *entry = new_entry;
    }
    return status;

fail:
    free(new_entry);
    return status;
}

typetab_entry_t *tt_lookup(typetab_t *tt, len_str_t *key) {
    for (;tt != NULL; tt = tt->last) {
        typetab_entry_t *result;
        if (NULL != (result = ht_lookup(&tt->types, &key))) {
            return result;
        }
    }

    return NULL;
}

typetab_entry_t *tt_lookup_compound(typetab_t *tt, len_str_t *key) {
    for (;tt != NULL; tt = tt->last) {
        typetab_entry_t *result;
        if (NULL != (result = ht_lookup(&tt->compound_types, &key))) {
            return result;
        }
    }

    return NULL;
}
