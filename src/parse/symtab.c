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
/**
 * Symbol table implementation
 */

#include "symtab.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "util/htable.h"
#include "util/util.h"

/**
 * Symbol table entries for reserved keywords
 */
static symtab_entry_t s_reserved[] = {
    // Keywords
    { { NULL }, LEN_STR_LIT("auto"          ), AUTO          },
    { { NULL }, LEN_STR_LIT("break"         ), BREAK         },
    { { NULL }, LEN_STR_LIT("case"          ), CASE          },
    { { NULL }, LEN_STR_LIT("const"         ), CONST         },
    { { NULL }, LEN_STR_LIT("continue"      ), CONTINUE      },
    { { NULL }, LEN_STR_LIT("default"       ), DEFAULT       },
    { { NULL }, LEN_STR_LIT("do"            ), DO            },
    { { NULL }, LEN_STR_LIT("else"          ), ELSE          },
    { { NULL }, LEN_STR_LIT("enum"          ), ENUM          },
    { { NULL }, LEN_STR_LIT("extern"        ), EXTERN        },
    { { NULL }, LEN_STR_LIT("for"           ), FOR           },
    { { NULL }, LEN_STR_LIT("goto"          ), GOTO          },
    { { NULL }, LEN_STR_LIT("if"            ), IF            },
    { { NULL }, LEN_STR_LIT("inline"        ), INLINE        },
    { { NULL }, LEN_STR_LIT("register"      ), REGISTER      },
    { { NULL }, LEN_STR_LIT("restrict"      ), RESTRICT      },
    { { NULL }, LEN_STR_LIT("return"        ), RETURN        },
    { { NULL }, LEN_STR_LIT("sizeof"        ), SIZEOF        },
    { { NULL }, LEN_STR_LIT("static"        ), STATIC        },
    { { NULL }, LEN_STR_LIT("struct"        ), STRUCT        },
    { { NULL }, LEN_STR_LIT("switch"        ), SWITCH        },
    { { NULL }, LEN_STR_LIT("typedef"       ), TYPEDEF       },
    { { NULL }, LEN_STR_LIT("union"         ), UNION         },
    { { NULL }, LEN_STR_LIT("volatile"      ), VOLATILE      },
    { { NULL }, LEN_STR_LIT("while"         ), WHILE         },

    // Underscore keywords
    { { NULL }, LEN_STR_LIT("_Alignas"      ), ALIGNAS       },
    { { NULL }, LEN_STR_LIT("_Alignof"      ), ALIGNOF       },
    { { NULL }, LEN_STR_LIT("_Bool"         ), BOOL          },
    { { NULL }, LEN_STR_LIT("_Complex"      ), COMPLEX       },
    { { NULL }, LEN_STR_LIT("_Generic"      ), GENERIC       },
    { { NULL }, LEN_STR_LIT("_Imaginary"    ), IMAGINARY     },
    { { NULL }, LEN_STR_LIT("_Noreturn"     ), NORETURN      },
    { { NULL }, LEN_STR_LIT("_Static_assert"), STATIC_ASSERT },
    { { NULL }, LEN_STR_LIT("_Thread_local" ), THREAD_LOCAL  },

    // Types
    { { NULL }, LEN_STR_LIT("void"          ), VOID          },

    { { NULL }, LEN_STR_LIT("char"          ), CHAR          },
    { { NULL }, LEN_STR_LIT("short"         ), SHORT         },
    { { NULL }, LEN_STR_LIT("int"           ), INT           },
    { { NULL }, LEN_STR_LIT("long"          ), LONG          },

    { { NULL }, LEN_STR_LIT("unsigned"      ), UNSIGNED      },
    { { NULL }, LEN_STR_LIT("signed"        ), SIGNED        },

    { { NULL }, LEN_STR_LIT("double"        ), DOUBLE        },
    { { NULL }, LEN_STR_LIT("float"         ), FLOAT         },
};

/**
 * Paramaters for newly constructed symbol tables
 */
status_t st_init(symtab_t *table, bool is_sym) {
    assert(table != NULL);
    status_t status = CCC_OK;

    size_t size = is_sym ? STATIC_ARRAY_LEN(s_reserved) * 2: 0;

    ht_params_t params = {
        size,                           // Size estimate
        offsetof(symtab_entry_t, key),  // Offset of key
        offsetof(symtab_entry_t, link), // Offset of ht link
        strhash,                        // Hash function
        vstrcmp,                        // void string compare
    };

    if (CCC_OK != (status = ht_init(&table->hashtab, &params))) {
        return status;
    }
    table->is_sym = is_sym;

    if (!is_sym) {
        return status;
    }

    // Initalize symbol table with reserved keywords
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_reserved); ++i) {
        if (CCC_OK != (status = ht_insert(&table->hashtab,
                                          &s_reserved[i].link))) {
            goto fail;
        }

    }

    return status;

fail:
    ht_destroy(&table->hashtab);
    return status;
}

void st_destroy(symtab_t *table) {
    if (table->is_sym) {
        // Remove all of the static entries first, because they aren't heap
        // allocated
        for (size_t i = 0; i < STATIC_ARRAY_LEN(s_reserved); ++i) {
            ht_remove(&table->hashtab, &s_reserved[i].key);
        }
    }

    HT_DESTROY_FUNC(&table->hashtab, free);
}

status_t st_lookup(symtab_t *table, char *str, size_t len, token_t type,
                   symtab_entry_t **entry) {
    status_t status = CCC_OK;

    len_str_t tmp = { str, len };

    symtab_entry_t *cur_entry = ht_lookup(&table->hashtab, &tmp);
    if (cur_entry != NULL) {
        *entry = cur_entry;
        return status;
    }

    // Doesn't exist. Need to allocate memory for the string and entry
    // We allocate them together so they are freed together
    cur_entry = malloc(sizeof(*cur_entry) + len + 1);
    if (cur_entry == NULL) {
        status = CCC_NOMEM;
        goto fail;
    }
    cur_entry->key.str = (char *)cur_entry + sizeof(*cur_entry);

    strncpy(cur_entry->key.str, str, len);
    cur_entry->key.str[len] = '\0';

    cur_entry->type = type;
    cur_entry->key.len = len;

    if (CCC_OK != (status = ht_insert(&table->hashtab, &cur_entry->link))) {
        goto fail;
    }

    *entry = cur_entry;
    return status;
fail:
    free(cur_entry);
    return status;
}
