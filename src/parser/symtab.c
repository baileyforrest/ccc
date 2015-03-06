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

#include <string.h>
#include <stddef.h>

#include "util/htable.h"
#include "util/util.h"

/**
 * Symbol table entries for reserved keywords
 */
static symtab_entry_t s_reserved[] = {
    // Keywords
    { { NULL }, LEN_STR_LITERAL("auto"          ), AUTO          },
    { { NULL }, LEN_STR_LITERAL("break"         ), BREAK         },
    { { NULL }, LEN_STR_LITERAL("case"          ), CASE          },
    { { NULL }, LEN_STR_LITERAL("const"         ), CONST         },
    { { NULL }, LEN_STR_LITERAL("continue"      ), CONTINUE      },
    { { NULL }, LEN_STR_LITERAL("default"       ), DEFAULT       },
    { { NULL }, LEN_STR_LITERAL("do"            ), DO            },
    { { NULL }, LEN_STR_LITERAL("else"          ), ELSE          },
    { { NULL }, LEN_STR_LITERAL("enum"          ), ENUM          },
    { { NULL }, LEN_STR_LITERAL("extern"        ), EXTERN        },
    { { NULL }, LEN_STR_LITERAL("for"           ), FOR           },
    { { NULL }, LEN_STR_LITERAL("goto"          ), GOTO          },
    { { NULL }, LEN_STR_LITERAL("if"            ), IF            },
    { { NULL }, LEN_STR_LITERAL("inline"        ), INLINE        },
    { { NULL }, LEN_STR_LITERAL("register"      ), REGISTER      },
    { { NULL }, LEN_STR_LITERAL("restrict"      ), RESTRICT      },
    { { NULL }, LEN_STR_LITERAL("return"        ), RETURN        },
    { { NULL }, LEN_STR_LITERAL("sizeof"        ), SIZEOF        },
    { { NULL }, LEN_STR_LITERAL("static"        ), STATIC        },
    { { NULL }, LEN_STR_LITERAL("struct"        ), STRUCT        },
    { { NULL }, LEN_STR_LITERAL("switch"        ), SWITCH        },
    { { NULL }, LEN_STR_LITERAL("typedef"       ), TYPEDEF       },
    { { NULL }, LEN_STR_LITERAL("union"         ), UNION         },
    { { NULL }, LEN_STR_LITERAL("volatile"      ), VOLATILE      },
    { { NULL }, LEN_STR_LITERAL("while"         ), WHILE         },

    // Underscore keywords
    { { NULL }, LEN_STR_LITERAL("_Alignas"      ), ALIGNAS       },
    { { NULL }, LEN_STR_LITERAL("_Alignof"      ), ALIGNOF       },
    { { NULL }, LEN_STR_LITERAL("_Bool"         ), BOOL          },
    { { NULL }, LEN_STR_LITERAL("_Complex"      ), COMPLEX       },
    { { NULL }, LEN_STR_LITERAL("_Generic"      ), GENERIC       },
    { { NULL }, LEN_STR_LITERAL("_Imaginary"    ), IMAGINARY     },
    { { NULL }, LEN_STR_LITERAL("_Noreturn"     ), NORETURN      },
    { { NULL }, LEN_STR_LITERAL("_Static_assert"), STATIC_ASSERT },
    { { NULL }, LEN_STR_LITERAL("_Thread_local" ), THREAD_LOCAL  },

    // Types
    { { NULL }, LEN_STR_LITERAL("void"          ), VOID          },

    { { NULL }, LEN_STR_LITERAL("char"          ), CHAR          },
    { { NULL }, LEN_STR_LITERAL("short"         ), SHORT         },
    { { NULL }, LEN_STR_LITERAL("int"           ), INT           },
    { { NULL }, LEN_STR_LITERAL("long"          ), LONG          },
    { { NULL }, LEN_STR_LITERAL("unsigned"      ), UNSIGNED      },
    { { NULL }, LEN_STR_LITERAL("signed"        ), SIGNED        },

    { { NULL }, LEN_STR_LITERAL("double"        ), DOUBLE        },
    { { NULL }, LEN_STR_LITERAL("float"         ), FLOAT         },
};

/**
 * Paramaters for newly constructed symbol tables
 */
status_t st_init(symtab_t *table) {
    status_t status = CCC_OK;

    static const ht_params s_params = {
        STATIC_ARRAY_LEN(s_reserved) * 2, // Size estimate
        offsetof(symtab_entry_t, key),                  // Offset of key
        offsetof(symtab_entry_t, link),                 // Offset of ht link
        strhash,                                        // Hash function
        vstrcmp,                                        // void string compare
    };

    if (CCC_OK != (status = ht_init(&table->hashtab, &s_params))) {
        return status;
    }

    // Initalize symbol table with reserved keywords
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_reserved); i++) {
        if (CCC_OK != (status = ht_insert(&table->hashtab,
                                          &s_reserved[i].link))) {
            ht_destroy(&table->hashtab, NOFREE);
            break;
        }

    }

    return status;
}

void st_destroy(symtab_t *table) {
    // Remove all of the static entries first, because they aren't heap
    // allocated
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_reserved); i++) {
        ht_remove(&table->hashtab, &s_reserved[i].link);
    }

    ht_destroy(&table->hashtab, DOFREE);
}

status_t st_lookup(symtab_t *table, char *str, size_t len,
                   symtab_entry_t **entry) {
    status_t status = CCC_OK;

    len_str_t tmp = { str, len };

    symtab_entry_t *cur_entry = ht_lookup(&table->hashtab, &tmp);
    if (NULL != entry) {
        *entry = cur_entry;
        return status;
    }

    // Doesn't exist. Need to allocate memory for the string and entry
    char *new_str = malloc(len + 1);
    if (NULL == new_str) {
        status = CCC_NOMEM;
        goto fail;
    }

    strncpy(new_str, str, len);
    new_str[len] = '\0';


    cur_entry = malloc(sizeof(*cur_entry));
    if (NULL == cur_entry) {
        status = CCC_NOMEM;
        goto fail;
    }

    // If its not in symbol table already, must be identifier type
    cur_entry->type = ID;
    cur_entry->key.str = new_str;
    cur_entry->key.len = len;

    if (CCC_OK != (status = ht_insert(&table->hashtab, &cur_entry->link))) {
        goto fail;
    }

    return status;
fail:
    free(new_str);
    free(cur_entry);
    return status;
}
