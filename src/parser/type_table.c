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

#include "parser/ast.h"

#include "util/util.h"

#define TYPE_LITERAL(typename, type) \
    { { NULL }, typename, sizeof(type), alignof(type), false, \
      { .ptr = { NULL, 0 } } }

static type_t stt_void   = TYPE_LITERAL(TYPE_VOID  , void  );
static type_t stt_char   = TYPE_LITERAL(TYPE_CHAR  , char  );
static type_t stt_short  = TYPE_LITERAL(TYPE_SHORT , short );
static type_t stt_int    = TYPE_LITERAL(TYPE_INT   , int   );
static type_t stt_long   = TYPE_LITERAL(TYPE_LONG  , long  );
static type_t stt_float  = TYPE_LITERAL(TYPE_FLOAT , float );
static type_t stt_double = TYPE_LITERAL(TYPE_DOUBLE, double);

type_t * const tt_void = &stt_void;
type_t * const tt_char = &stt_char;
type_t * const tt_short = &stt_short;
type_t * const tt_int = &stt_int;
type_t * const tt_long = &stt_long;
type_t * const tt_float = &stt_float;
type_t * const tt_double = &stt_double;

#define TYPE_TAB_LITERAL_ENTRY(type) \
    { { NULL }, { LEN_STR_LITERAL(#type), TT_PRIM }, &stt_ ## type }

/**
 * Table of primative types
 */
static typetab_entry_t s_prim_types[] = {
    TYPE_TAB_LITERAL_ENTRY(void),
    TYPE_TAB_LITERAL_ENTRY(char),
    TYPE_TAB_LITERAL_ENTRY(short),
    TYPE_TAB_LITERAL_ENTRY(int),
    TYPE_TAB_LITERAL_ENTRY(long),
    TYPE_TAB_LITERAL_ENTRY(float),
    TYPE_TAB_LITERAL_ENTRY(double)
};

uint32_t typetab_key_hash(const void *void_key) {
    const tt_key_t *key = (tt_key_t *)void_key;
    uint32_t hash = strhash(&key->name);

    return hash * 33 + (int)key->type;
}

bool typetab_key_cmp(const void *void_key1, const void *void_key2) {
    const tt_key_t *key1 = (tt_key_t *)void_key1;
    const tt_key_t *key2 = (tt_key_t *)void_key2;

    if (key1->type != key2->type) {
        return false;
    }

    return vstrcmp(&key1->name, &key2->name);
}

status_t tt_init(typetab_t *tt, typetab_t *last) {
    assert(tt != NULL);
    status_t status = CCC_OK;
    tt->last = last;

    static ht_params params = {
        0,                               // Size estimate
        offsetof(typetab_entry_t, key),  // Offset of key
        offsetof(typetab_entry_t, link), // Offset of ht link
        typetab_key_hash,                // Hash function
        typetab_key_cmp,                 // void string compare
    };

    if (CCC_OK != (status = ht_init(&tt->hashtab, &params))) {
        goto fail;
    }

    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_prim_types); ++i) {
        if (CCC_OK != (status = ht_insert(&tt->hashtab,
                                          &s_prim_types[i].link))) {
            goto fail1;
        }
    }
    return status;

fail1:
    ht_destroy(&tt->hashtab, NOFREE);
fail:
    return status;
}

void tt_destroy(typetab_t *tt) {
    assert(tt != NULL);
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_prim_types); ++i) {
        ht_remove(&tt->hashtab, &s_prim_types[i].key, NOFREE);
    }
    // TODO: Free all the types
    ht_destroy(&tt->hashtab, NOFREE);
}

status_t tt_insert(typetab_t *tt, type_t *type, tt_type_t tt_type,
                   len_str_t *name, typetab_entry_t **entry) {
    assert(tt != NULL);
    assert(type != NULL);
    assert(name != NULL);

    status_t status = CCC_OK;
    typetab_entry_t *new_entry = malloc(sizeof(typetab_entry_t));
    if (entry == NULL) {
        status = CCC_NOMEM;
        goto fail;
    }

    new_entry->type = type;
    new_entry->key.type = tt_type;
    new_entry->key.name.str = name->str;
    new_entry->key.name.len = name->len;

    if (NULL != tt_lookup(tt, &new_entry->key)) {
        status = CCC_DUPLICATE;
        goto fail1;
    }
    if (CCC_OK != (status = ht_insert(&tt->hashtab, &new_entry->link))) {
        goto fail1;
    }

    if (entry) {
        *entry = new_entry;
    }
    return status;

fail1:
    free(new_entry);
fail:
    return status;
}

typetab_entry_t *tt_lookup(typetab_t *tt, tt_key_t *key) {
    for (;tt != NULL; tt = tt->last) {
        typetab_entry_t *result = ht_lookup(&tt->hashtab, key);
        if (result != NULL) {
            return result;
        }
    }

    return NULL;
}
