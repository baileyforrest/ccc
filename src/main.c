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

#include "parser/lexer.h"
#include "parser/preprocessor.h"
#include "parser/symtab.h"
#include "parser/token.h"

#include "util/file_directory.h"

static preprocessor_t pp;
static symtab_t symtab;
static symtab_t string_tab;
static lexer_t lexer;

int main(int argc, char **argv) {
    if (argc < 2) {
        return -1;
    }

    status_t status;

    if (CCC_OK != (status = fdir_init())) {
        goto fail0;
    }

    if (CCC_OK != (status = pp_init(&pp))) {
        goto fail1;
    }
    if (CCC_OK != (status = st_init(&symtab, IS_SYM))) {
        goto fail2;
    }
    if (CCC_OK != (status = st_init(&string_tab, NOT_SYM))) {
        goto fail3;
    }
    if (CCC_OK != (status = lexer_init(&lexer, &pp, &symtab, &string_tab))) {
        goto fail4;
    }

    if (CCC_OK != (status = pp_open(&pp, argv[1]))) {
        goto fail5;

    }

    lexeme_t cur_token;

    do {
        lexer_next_token(&lexer, &cur_token);
        token_print(&cur_token);
    } while(cur_token.type != TOKEN_EOF);

fail5:
    lexer_destroy(&lexer);
fail4:
    st_destroy(&string_tab);
fail3:
    st_destroy(&symtab);
fail2:
    pp_destroy(&pp);
fail1:
    fdir_destroy();
fail0:
    return status;
}
