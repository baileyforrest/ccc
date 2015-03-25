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

#include "parse/ast.h"
#include "parse/lexer.h"
#include "parse/parser.h"
#include "parse/preprocessor.h"
#include "parse/symtab.h"
#include "parse/token.h"

#include "util/file_directory.h"

#include "manager.h"

static manager_t manager;

int main(int argc, char **argv) {
    status_t status = CCC_OK;
    if (argc < 2) {
        return -1;
    }

    if (CCC_OK != (status = fdir_init())) {
        goto fail0;
    }

    if (CCC_OK != (status = man_init(&manager, NULL))) {
        goto fail1;
    }

    if (CCC_OK != (status = pp_open(&manager.pp, argv[1]))) {
        goto fail2;
    }

    /*
    // Print tokens
    lexeme_t cur_token;

    do {
        lexer_next_token(&lexer, &cur_token);
        token_print(&cur_token);
    } while(cur_token.type != TOKEN_EOF);
    */
    ///*
    trans_unit_t *ast;
    if (CCC_OK != (status = man_parse(&manager, &ast))) {
        goto fail2;
    }
    ast_print(ast);
    ast_destroy(ast);
    //*/

fail2:
    man_destroy(&manager);
fail1:
    fdir_destroy();
fail0:
    return status;
}
