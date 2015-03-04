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
    { { NULL }, "auto"          , AUTO          },
    { { NULL }, "break"         , BREAK         },
    { { NULL }, "case"          , CASE          },
    { { NULL }, "const"         , CONST         },
    { { NULL }, "continue"      , CONTINUE      },
    { { NULL }, "default"       , DEFAULT       },
    { { NULL }, "do"            , DO            },
    { { NULL }, "else"          , ELSE          },
    { { NULL }, "enum"          , ENUM          },
    { { NULL }, "extern"        , EXTERN        },
    { { NULL }, "for"           , FOR           },
    { { NULL }, "goto"          , GOTO          },
    { { NULL }, "if"            , IF            },
    { { NULL }, "inline"        , INLINE        },
    { { NULL }, "register"      , REGISTER      },
    { { NULL }, "restrict"      , RESTRICT      },
    { { NULL }, "return"        , RETURN        },
    { { NULL }, "sizeof"        , SIZEOF        },
    { { NULL }, "static"        , STATIC        },
    { { NULL }, "struct"        , STRUCT        },
    { { NULL }, "switch"        , SWITCH        },
    { { NULL }, "typedef"       , TYPEDEF       },
    { { NULL }, "union"         , UNION         },
    { { NULL }, "volatile"      , VOLATILE      },
    { { NULL }, "while"         , WHILE         },

    // Underscore keywords
    { { NULL }, "_Alignas"      , ALIGNAS       },
    { { NULL }, "_Alignof"      , ALIGNOF       },
    { { NULL }, "_Bool"         , BOOL          },
    { { NULL }, "_Complex"      , COMPLEX       },
    { { NULL }, "_Generic"      , GENERIC       },
    { { NULL }, "_Imaginary"    , IMAGINARY     },
    { { NULL }, "_Noreturn"     , NORETURN      },
    { { NULL }, "_Static_assert", STATIC_ASSERT },
    { { NULL }, "_Thread_local" , THREAD_LOCAL  },

    // Types
    { { NULL }, "void"          , VOID          },

    { { NULL }, "char"          , CHAR          },
    { { NULL }, "short"         , SHORT         },
    { { NULL }, "int"           , INT           },
    { { NULL }, "long"          , LONG          },
    { { NULL }, "unsigned"      , UNSIGNED      },
    { { NULL }, "signed"        , SIGNED        },

    { { NULL }, "double"        , DOUBLE        },
    { { NULL }, "float"         , FLOAT         },
};

/**
 * Paramaters for newly constructed symbol tables
 */
static ht_params s_params = {
    sizeof(s_reserved) / sizeof(s_reserved[0]) * 2, // Size estimate
    0,                                              // No specified key length
    offsetof(symtab_entry_t, str),                  // Offset of key (string)
    offsetof(symtab_entry_t, link),                 // Offset of ht link
    strhash,                                        // Hash function
    vstrcmp,                                        // void string compare
};

status_t st_init(symtab_t *table) {
    status_t status = CCC_OK;

    if (CCC_OK != (status = ht_init(&table->hashtab, &s_params))) {
        return status;
    }

    // Initalize symbol table with reserved keywords
    for (size_t i = 0; i < sizeof(s_reserved) / sizeof(s_reserved[0]); i++) {
        if (CCC_OK != (status = ht_insert(&table->hashtab,
                                          &s_reserved[i].link))) {
            ht_destroy(&table->hashtab);
            break;
        }

    }

    return status;
}

void st_destroy(symtab_t *table) {
    ht_destroy(&table->hashtab);
}

status_t st_lookup(symtab_t *table, const char *str, size_t len,
                   symtab_entry_t **entry) {
    status_t status = CCC_OK;

    symtab_entry_t *cur_entry = ht_lookup(&table->hashtab, str);
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
    cur_entry->str = new_str;

    if (CCC_OK != (status = ht_insert(&table->hashtab, &cur_entry->link))) {
        goto fail;
    }

    return status;
fail:
    free(new_str);
    free(cur_entry);
    return status;
}
