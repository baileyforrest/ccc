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
    { SL_LINK_LIT, "auto"          , AUTO          },
    { SL_LINK_LIT, "break"         , BREAK         },
    { SL_LINK_LIT, "case"          , CASE          },
    { SL_LINK_LIT, "const"         , CONST         },
    { SL_LINK_LIT, "continue"      , CONTINUE      },
    { SL_LINK_LIT, "default"       , DEFAULT       },
    { SL_LINK_LIT, "do"            , DO            },
    { SL_LINK_LIT, "else"          , ELSE          },
    { SL_LINK_LIT, "enum"          , ENUM          },
    { SL_LINK_LIT, "extern"        , EXTERN        },
    { SL_LINK_LIT, "for"           , FOR           },
    { SL_LINK_LIT, "goto"          , GOTO          },
    { SL_LINK_LIT, "if"            , IF            },
    { SL_LINK_LIT, "inline"        , INLINE        },
    { SL_LINK_LIT, "register"      , REGISTER      },
    { SL_LINK_LIT, "restrict"      , RESTRICT      },
    { SL_LINK_LIT, "return"        , RETURN        },
    { SL_LINK_LIT, "sizeof"        , SIZEOF        },
    { SL_LINK_LIT, "static"        , STATIC        },
    { SL_LINK_LIT, "struct"        , STRUCT        },
    { SL_LINK_LIT, "switch"        , SWITCH        },
    { SL_LINK_LIT, "typedef"       , TYPEDEF       },
    { SL_LINK_LIT, "union"         , UNION         },
    { SL_LINK_LIT, "volatile"      , VOLATILE      },
    { SL_LINK_LIT, "while"         , WHILE         },

    // Underscore keywords
    { SL_LINK_LIT, "_Alignas"      , ALIGNAS       },
    { SL_LINK_LIT, "_Alignof"      , ALIGNOF       },
    { SL_LINK_LIT, "_Bool"         , BOOL          },
    { SL_LINK_LIT, "_Complex"      , COMPLEX       },
    { SL_LINK_LIT, "_Generic"      , GENERIC       },
    { SL_LINK_LIT, "_Imaginary"    , IMAGINARY     },
    { SL_LINK_LIT, "_Noreturn"     , NORETURN      },
    { SL_LINK_LIT, "_Static_assert", STATIC_ASSERT },
    { SL_LINK_LIT, "_Thread_local" , THREAD_LOCAL  },

    // __builtin
    { SL_LINK_LIT, "__builtin_offsetof", OFFSETOF  },
    { SL_LINK_LIT, "__builtin_va_list" , VA_LIST   },
    { SL_LINK_LIT, "__builtin_va_start", VA_START  },
    { SL_LINK_LIT, "__builtin_va_arg"  , VA_ARG    },
    { SL_LINK_LIT, "__builtin_va_end"  , VA_END    },
    { SL_LINK_LIT, "__builtin_va_copy" , VA_COPY   },

    // Types
    { SL_LINK_LIT, "void"          , VOID          },

    { SL_LINK_LIT, "char"          , CHAR          },
    { SL_LINK_LIT, "short"         , SHORT         },
    { SL_LINK_LIT, "int"           , INT           },
    { SL_LINK_LIT, "long"          , LONG          },

    { SL_LINK_LIT, "unsigned"      , UNSIGNED      },
    { SL_LINK_LIT, "signed"        , SIGNED        },

    { SL_LINK_LIT, "double"        , DOUBLE        },
    { SL_LINK_LIT, "float"         , FLOAT         },
};

/**
 * Paramaters for newly constructed symbol tables
 */
void st_init(symtab_t *table, bool is_sym) {
    assert(table != NULL);

    size_t size = is_sym ? STATIC_ARRAY_LEN(s_reserved) * 2: 0;

    ht_params_t params = {
        size,                           // Size estimate
        offsetof(symtab_entry_t, key),  // Offset of key
        offsetof(symtab_entry_t, link), // Offset of ht link
        ind_str_hash,                   // Hash function
        ind_str_eq,                     // void string compare
    };

    ht_init(&table->hashtab, &params);
    table->is_sym = is_sym;

    // Initalize symbol table with reserved keywords
    if (is_sym) {
        for (size_t i = 0; i < STATIC_ARRAY_LEN(s_reserved); ++i) {
            status_t status = ht_insert(&table->hashtab, &s_reserved[i].link);
            assert(status == CCC_OK);
        }
    }

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

status_t st_lookup(symtab_t *table, char *str, token_t type,
                   symtab_entry_t **entry) {
    status_t status = CCC_OK;

    symtab_entry_t *cur_entry = ht_lookup(&table->hashtab, &str);
    if (cur_entry != NULL) {
        *entry = cur_entry;
        return status;
    }

    // Doesn't exist. Need to allocate memory for the string and entry
    // We allocate them together so they are freed together
    cur_entry = emalloc(sizeof(*cur_entry) + strlen(str) + 1);
    cur_entry->key = (char *)cur_entry + sizeof(*cur_entry);

    strcpy(cur_entry->key, str);

    cur_entry->type = type;

    if (CCC_OK != (status = ht_insert(&table->hashtab, &cur_entry->link))) {
        goto fail;
    }

    *entry = cur_entry;
    return status;
fail:
    free(cur_entry);
    return status;
}
