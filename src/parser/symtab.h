/*
  Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>

  This file is part of CCC.

  CCC is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  CCC is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with CCC.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SYMTAB_H_
#define _SYMTAB_H_

#include "parser/token.h"

#include "util/status.h"
#include "util/htable.h"
#include "util/slist.h"
#include "util/util.h"

/**
 * Symbol table
 */
typedef struct symtab_t {
    htable_t hashtab; /**< Hash table backing store */
    /** If true, used for symbols, otherwise used for something else
      * (e.g. strings)
      */
    bool is_sym;
} symtab_t;

/**
 * Type and value of a string lexeme
 */
typedef struct symtab_entry_t {
    sl_link_t link;   /**< Hashtable link */
    len_str_t key;    /**< Hashtable key value */
    token_t type;     /**< Denotes the type of the symbol table entry */
} symtab_entry_t;

#define IS_SYM true
#define NOT_SYM false

/**
 * Initalizes a symbol table
 *
 * @param sym_tab Symbol table to initialize
 * @param is_sym IS_SYM if symbol table. else NOT_SYM. if IS_SYM, then table
 *     will be populated with reserved words
 * @return CCC_OK on success, error code on error
 */
status_t st_init(symtab_t *table, bool is_sym);

/**
 * Does not destroy sym_tab. Destroys a symbol table.
 * Frees all of its heap memory.
 *
 * @param sym_tab Symbol table to destroy
 * @return CCC_OK on success, error code on error
 */
void st_destroy(symtab_t *table);

/**
 * Looks up a string in the symbol table, if it exists returns the existing
 * entry. Otherwise, it adds its own entry is added.
 *
 * @param table The table to lookup
 * @param str The string to lookup and or add
 * @param len The length of the string to looku and or add
 * @param type The type of entry to add if it doesn't already exist
 * @param Pointer to an entry pointer if success, otherwise unchanged
 * @return CCC_OK on success, error code on failure
 */
status_t st_lookup(symtab_t *table, char *str, size_t len, token_t type,
                   symtab_entry_t **entry);


#endif /* _SYMTAB_H_ */
