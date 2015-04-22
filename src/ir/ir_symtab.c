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
 * IR symbol table implementation
 */

#include "ir_symtab.h"

#include "ir/ir.h"

#include <assert.h>

void ir_symtab_entry_destroy(ir_symtab_entry_t *entry);

void ir_symtab_init(ir_symtab_t *symtab) {
    assert(symtab != NULL);

    static const ht_params_t ht_params = {
        0,                                 // Size estimate
        offsetof(ir_symtab_entry_t, name), // Offset of key
        offsetof(ir_symtab_entry_t, link), // Offset of ht link
        ind_str_hash,                      // Hash function
        ind_str_eq,                        // void string compare
    };

    ht_init(&symtab->table, &ht_params);
}

void ir_symtab_entry_destroy(ir_symtab_entry_t *entry) {
    switch (entry->type) {
    case IR_SYMTAB_ENTRY_VAR:
        break;
    default:
        assert(false);
    }
    free(entry);
}

void ir_symtab_destroy(ir_symtab_t *symtab) {
    HT_DESTROY_FUNC(&symtab->table, ir_symtab_entry_destroy);
}

ir_symtab_entry_t *ir_symtab_entry_create(ir_symtab_entry_type_t type,
                                          char *name) {
    ir_symtab_entry_t *entry = emalloc(sizeof(*entry));
    entry->type = type;
    entry->name = name;
    return entry;
}

status_t ir_symtab_insert(ir_symtab_t *symtab, ir_symtab_entry_t *entry) {
    return ht_insert(&symtab->table, &entry->link);
}

ir_symtab_entry_t *ir_symtab_lookup(ir_symtab_t *symtab, char *name) {
    return ht_lookup(&symtab->table, &name);
}
