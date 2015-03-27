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
 * Destroy a symbol table entry
 *
 * @param entry The entry to destroy
 */
void st_entry_destroy(symtab_entry_t *entry);

/**
 * Symbol table entries for reserved keywords
 */
static symtab_entry_t s_reserved[] = {
    // Keywords
    { SL_LINK_LIT, LEN_STR_LIT("auto"          ), AUTO          },
    { SL_LINK_LIT, LEN_STR_LIT("break"         ), BREAK         },
    { SL_LINK_LIT, LEN_STR_LIT("case"          ), CASE          },
    { SL_LINK_LIT, LEN_STR_LIT("const"         ), CONST         },
    { SL_LINK_LIT, LEN_STR_LIT("continue"      ), CONTINUE      },
    { SL_LINK_LIT, LEN_STR_LIT("default"       ), DEFAULT       },
    { SL_LINK_LIT, LEN_STR_LIT("do"            ), DO            },
    { SL_LINK_LIT, LEN_STR_LIT("else"          ), ELSE          },
    { SL_LINK_LIT, LEN_STR_LIT("enum"          ), ENUM          },
    { SL_LINK_LIT, LEN_STR_LIT("extern"        ), EXTERN        },
    { SL_LINK_LIT, LEN_STR_LIT("for"           ), FOR           },
    { SL_LINK_LIT, LEN_STR_LIT("goto"          ), GOTO          },
    { SL_LINK_LIT, LEN_STR_LIT("if"            ), IF            },
    { SL_LINK_LIT, LEN_STR_LIT("inline"        ), INLINE        },
    { SL_LINK_LIT, LEN_STR_LIT("register"      ), REGISTER      },
    { SL_LINK_LIT, LEN_STR_LIT("restrict"      ), RESTRICT      },
    { SL_LINK_LIT, LEN_STR_LIT("return"        ), RETURN        },
    { SL_LINK_LIT, LEN_STR_LIT("sizeof"        ), SIZEOF        },
    { SL_LINK_LIT, LEN_STR_LIT("static"        ), STATIC        },
    { SL_LINK_LIT, LEN_STR_LIT("struct"        ), STRUCT        },
    { SL_LINK_LIT, LEN_STR_LIT("switch"        ), SWITCH        },
    { SL_LINK_LIT, LEN_STR_LIT("typedef"       ), TYPEDEF       },
    { SL_LINK_LIT, LEN_STR_LIT("union"         ), UNION         },
    { SL_LINK_LIT, LEN_STR_LIT("volatile"      ), VOLATILE      },
    { SL_LINK_LIT, LEN_STR_LIT("while"         ), WHILE         },

    // Underscore keywords
    { SL_LINK_LIT, LEN_STR_LIT("_Alignas"      ), ALIGNAS       },
    { SL_LINK_LIT, LEN_STR_LIT("_Alignof"      ), ALIGNOF       },
    { SL_LINK_LIT, LEN_STR_LIT("_Bool"         ), BOOL          },
    { SL_LINK_LIT, LEN_STR_LIT("_Complex"      ), COMPLEX       },
    { SL_LINK_LIT, LEN_STR_LIT("_Generic"      ), GENERIC       },
    { SL_LINK_LIT, LEN_STR_LIT("_Imaginary"    ), IMAGINARY     },
    { SL_LINK_LIT, LEN_STR_LIT("_Noreturn"     ), NORETURN      },
    { SL_LINK_LIT, LEN_STR_LIT("_Static_assert"), STATIC_ASSERT },
    { SL_LINK_LIT, LEN_STR_LIT("_Thread_local" ), THREAD_LOCAL  },

    // Types
    { SL_LINK_LIT, LEN_STR_LIT("void"          ), VOID          },

    { SL_LINK_LIT, LEN_STR_LIT("char"          ), CHAR          },
    { SL_LINK_LIT, LEN_STR_LIT("short"         ), SHORT         },
    { SL_LINK_LIT, LEN_STR_LIT("int"           ), INT           },
    { SL_LINK_LIT, LEN_STR_LIT("long"          ), LONG          },

    { SL_LINK_LIT, LEN_STR_LIT("unsigned"      ), UNSIGNED      },
    { SL_LINK_LIT, LEN_STR_LIT("signed"        ), SIGNED        },

    { SL_LINK_LIT, LEN_STR_LIT("double"        ), DOUBLE        },
    { SL_LINK_LIT, LEN_STR_LIT("float"         ), FLOAT         },
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

void st_entry_destroy(symtab_entry_t *entry) {
    // Ignore static entries
    if (entry >= s_reserved && entry <
        s_reserved + STATIC_ARRAY_LEN(s_reserved)) {
        return;
    }

    free(entry);
}

void st_destroy(symtab_t *table) {
    HT_DESTROY_FUNC(&table->hashtab, st_entry_destroy);
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
