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
    { { NULL }, {"auto"          , sizeof "auto"          - 1}, AUTO          },
    { { NULL }, {"break"         , sizeof "break"         - 1}, BREAK         },
    { { NULL }, {"case"          , sizeof "case"          - 1}, CASE          },
    { { NULL }, {"const"         , sizeof "const"         - 1}, CONST         },
    { { NULL }, {"continue"      , sizeof "continue"      - 1}, CONTINUE      },
    { { NULL }, {"default"       , sizeof "default"       - 1}, DEFAULT       },
    { { NULL }, {"do"            , sizeof "do"            - 1}, DO            },
    { { NULL }, {"else"          , sizeof "else"          - 1}, ELSE          },
    { { NULL }, {"enum"          , sizeof "enum"          - 1}, ENUM          },
    { { NULL }, {"extern"        , sizeof "extern"        - 1}, EXTERN        },
    { { NULL }, {"for"           , sizeof "for"           - 1}, FOR           },
    { { NULL }, {"goto"          , sizeof "goto"          - 1}, GOTO          },
    { { NULL }, {"if"            , sizeof "if"            - 1}, IF            },
    { { NULL }, {"inline"        , sizeof "inline"        - 1}, INLINE        },
    { { NULL }, {"register"      , sizeof "register"      - 1}, REGISTER      },
    { { NULL }, {"restrict"      , sizeof "restrict"      - 1}, RESTRICT      },
    { { NULL }, {"return"        , sizeof "return"        - 1}, RETURN        },
    { { NULL }, {"sizeof"        , sizeof "sizeof"        - 1}, SIZEOF        },
    { { NULL }, {"static"        , sizeof "static"        - 1}, STATIC        },
    { { NULL }, {"struct"        , sizeof "struct"        - 1}, STRUCT        },
    { { NULL }, {"switch"        , sizeof "switch"        - 1}, SWITCH        },
    { { NULL }, {"typedef"       , sizeof "typedef"       - 1}, TYPEDEF       },
    { { NULL }, {"union"         , sizeof "union"         - 1}, UNION         },
    { { NULL }, {"volatile"      , sizeof "volatile"      - 1}, VOLATILE      },
    { { NULL }, {"while"         , sizeof "while"         - 1}, WHILE         },

    // Underscor{e keywords        sizeof e keywords
    { { NULL }, {"_Alignas"      , sizeof "_Alignas"      - 1}, ALIGNAS       },
    { { NULL }, {"_Alignof"      , sizeof "_Alignof"      - 1}, ALIGNOF       },
    { { NULL }, {"_Bool"         , sizeof "_Bool"         - 1}, BOOL          },
    { { NULL }, {"_Complex"      , sizeof "_Complex"      - 1}, COMPLEX       },
    { { NULL }, {"_Generic"      , sizeof "_Generic"      - 1}, GENERIC       },
    { { NULL }, {"_Imaginary"    , sizeof "_Imaginary"    - 1}, IMAGINARY     },
    { { NULL }, {"_Noreturn"     , sizeof "_Noreturn"     - 1}, NORETURN      },
    { { NULL }, {"_Static_assert", sizeof "_Static_assert"- 1}, STATIC_ASSERT },
    { { NULL }, {"_Thread_local" , sizeof "_Thread_local" - 1}, THREAD_LOCAL  },

    // Types
    { { NULL }, {"void"          , sizeof "void"          - 1}, VOID          },

    { { NULL }, {"char"          , sizeof "char"          - 1}, CHAR          },
    { { NULL }, {"short"         , sizeof "short"         - 1}, SHORT         },
    { { NULL }, {"int"           , sizeof "int"           - 1}, INT           },
    { { NULL }, {"long"          , sizeof "long"          - 1}, LONG          },
    { { NULL }, {"unsigned"      , sizeof "unsigned"      - 1}, UNSIGNED      },
    { { NULL }, {"signed"        , sizeof "signed"        - 1}, SIGNED        },

    { { NULL }, {"double"        , sizeof "double"        - 1}, DOUBLE        },
    { { NULL }, {"float"         , sizeof "float"         - 1}, FLOAT         },
};

/**
 * Paramaters for newly constructed symbol tables
 */
status_t st_init(symtab_t *table) {
    status_t status = CCC_OK;

    static const ht_params s_params = {
        sizeof(s_reserved) / sizeof(s_reserved[0]) * 2, // Size estimate
        offsetof(symtab_entry_t, key),                  // Offset of key
        offsetof(symtab_entry_t, link),                 // Offset of ht link
        strhash,                                        // Hash function
        vstrcmp,                                        // void string compare
    };

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
