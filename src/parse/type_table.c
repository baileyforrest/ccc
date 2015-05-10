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

#define PRIM_TYPE_FILE "<primitive_type>"

#define TYPE_LITERAL(typename, type) \
    { SL_LINK_LIT, { NULL, PRIM_TYPE_FILE, "\n", 0, 0 }, \
      typename, true, { } }

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

// TODO1: This isn't portable
// size_t is unsigned long.
static type_t stt_size_t = {
    SL_LINK_LIT, { NULL, PRIM_TYPE_FILE, "\n", 0, 0 }, TYPE_MOD, true,
    { .mod = { TMOD_UNSIGNED, NULL, NULL, 0, &stt_long } }
};

static type_t stt_va_list = TYPE_LITERAL(TYPE_VA_LIST, va_list);


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

type_t * const tt_size_t = &stt_size_t;

type_t * const tt_va_list = &stt_va_list;

#define TYPE_TAB_LITERAL_ENTRY(type, type_str)                  \
    { SL_LINK_LIT, type_str, NULL, TT_PRIM , &stt_ ## type, { } }

/**
 * Table of primative types
 */
static typetab_entry_t s_prim_types[] = {
    TYPE_TAB_LITERAL_ENTRY(void,        "void"),
    TYPE_TAB_LITERAL_ENTRY(bool,        "_Bool"),
    TYPE_TAB_LITERAL_ENTRY(char,        "char"),
    TYPE_TAB_LITERAL_ENTRY(short,       "short"),
    TYPE_TAB_LITERAL_ENTRY(int,         "int"),
    TYPE_TAB_LITERAL_ENTRY(long,        "long"),
    TYPE_TAB_LITERAL_ENTRY(long_long,   "long long"),
    TYPE_TAB_LITERAL_ENTRY(float,       "float"),
    TYPE_TAB_LITERAL_ENTRY(double,      "double"),
    TYPE_TAB_LITERAL_ENTRY(long_double, "long double"),
    TYPE_TAB_LITERAL_ENTRY(size_t,      "____size_t__"),
    TYPE_TAB_LITERAL_ENTRY(va_list,     "__builtin_va_list"),
};

void tt_init(typetab_t *tt, typetab_t *last) {
    assert(tt != NULL);
    tt->last = last;

    static const ht_params_t params = {
        0,                               // Size estimate
        offsetof(typetab_entry_t, key),  // Offset of key
        offsetof(typetab_entry_t, link), // Offset of ht link
        ind_str_hash,                        // Hash function
        ind_str_eq                           // void string compare
    };

    ht_init(&tt->types, &params);
    ht_init(&tt->compound_types, &params);

    // Initialize top level table with primitive types
    if (last == NULL) {
        for (size_t i = 0; i < STATIC_ARRAY_LEN(s_prim_types); ++i) {
            status_t status = ht_insert(&tt->types, &s_prim_types[i].link);

            assert(status == CCC_OK);
            s_prim_types[i].typetab = tt;
        }
    }
}

static void typetab_entry_destroy(typetab_entry_t *entry) {
    switch (entry->entry_type) {
    case TT_PRIM:
        // Ignore primitive types, they are in static memory
        return;
    case TT_COMPOUND:
    case TT_TYPEDEF:
    case TT_VAR:
    case TT_ENUM_ID:
        break;
    default:
        assert(false);
    }
    free(entry);
}

void tt_destroy(typetab_t *tt) {
    assert(tt != NULL);
    // This order is important. types may point to typedef_bases,
    // typedef_bases may point to compound types
    HT_DESTROY_FUNC(&tt->types, typetab_entry_destroy);
    HT_DESTROY_FUNC(&tt->compound_types, typetab_entry_destroy);
}

status_t tt_insert(typetab_t *tt, type_t *type, tt_type_t tt_type, char *name,
                   typetab_entry_t **entry) {
    assert(tt != NULL);
    assert(type != NULL);
    assert(name != NULL);

    status_t status = CCC_OK;
    typetab_entry_t *new_entry = ecalloc(1, sizeof(typetab_entry_t));

    new_entry->type = type;
    new_entry->entry_type = tt_type;
    new_entry->key = name;
    new_entry->typetab = tt;

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

typetab_entry_t *tt_lookup(typetab_t *tt, char *key) {
    for (;tt != NULL; tt = tt->last) {
        typetab_entry_t *result;
        if (NULL != (result = ht_lookup(&tt->types, &key))) {
            return result;
        }
    }

    return NULL;
}

typetab_entry_t *tt_lookup_compound(typetab_t *tt, char *key) {
    for (;tt != NULL; tt = tt->last) {
        typetab_entry_t *result;
        if (NULL != (result = ht_lookup(&tt->compound_types, &key))) {
            return result;
        }
    }

    return NULL;
}
