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
 * IR symbol table interface
 */

#ifndef _IR_SYMTAB_H_
#define _IR_SYMTAB_H_

#include "util/htable.h"
#include "util/util.h"

typedef struct ir_expr_t ir_expr_t;

typedef struct ir_symtab_t {
    htable_t table;
} ir_symtab_t;

typedef struct ir_gdecl_t ir_gdecl_t;

typedef enum ir_symtab_entry_type_t {
    IR_SYMTAB_ENTRY_VAR,
} ir_symtab_entry_type_t;

typedef struct ir_symtab_entry_t {
    sl_link_t link;
    ir_symtab_entry_type_t type;
    len_str_t name;

    union {
        ir_expr_t *var; /**< Expression initialized to. NULL if none */
    };
} ir_symtab_entry_t;


void ir_symtab_init(ir_symtab_t *symtab);

void ir_symtab_destroy(ir_symtab_t *symtab);

ir_symtab_entry_t *ir_symtab_entry_create(ir_symtab_entry_type_t type,
                                          len_str_t *name);

status_t ir_symtab_insert(ir_symtab_t *symtab, ir_symtab_entry_t *entry);

ir_symtab_entry_t *ir_symtab_lookup(ir_symtab_t *symtab, len_str_t *name);

#endif /* _IR_SYMTAB_H_ */
