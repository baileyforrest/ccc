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
 * Parser implementation
 *
 * Recursive descent style parser
 */

#include "parser.h"
#include "parser_priv.h"

#include <assert.h>
#include <stdlib.h>

#include "util/logger.h"

status_t parser_parse(lexer_t *lexer, trans_unit_t **result) {
    assert(lexer != NULL);
    assert(result != NULL);
    status_t status = CCC_OK;

    lex_wrap_t lex;
    lex.lexer = lexer;
    LEX_ADVANCE(&lex);

    status = par_trans_unit(&lex, result);
fail:
    return status;
}

status_t par_trans_unit(lex_wrap_t *lex, trans_unit_t **result) {
    status_t status = CCC_OK;
    trans_unit_t *tunit = NULL;

    // Setup new translation unit
    ALLOC_NODE(tunit, trans_unit_t);
    tunit->path = lex->cur.mark.filename;
    if (!CCC_OK ==
        (status = sl_init(&tunit->gdecls, offsetof(gdecl_t, link)))) {
        goto fail0;
    }

    while (true) {
        // We're done when we reach EOF
        if (lex->cur.type == TOKEN_EOF) {
            *result = tunit;
            return status;
        }

        gdecl_t *gdecl;
        if (CCC_OK != (status = par_gdecl(lex, &gdecl))) {
            goto fail;
        }
        sl_append(&tunit->gdecls, &gdecl->link);
    }

fail: // General failure
    // TODO: call destructors on gdecls
    sl_destroy(&tunit->gdecls, NOFREE);
fail0: // Failed allocation
    free(tunit);
    return status;
}

status_t par_gdecl(lex_wrap_t *lex, gdecl_t **result) {
    status_t status = CCC_OK;
    // TODO: Implement
    (void)lex;
    (void)result;
    return status;
}

status_t par_type(lex_wrap_t *lex, type_t **result) {
    status_t status = CCC_OK;
    // TODO: Implement
    (void)lex;
    (void)result;
    return status;
}

status_t par_expr(lex_wrap_t *lex, type_t **result) {
    status_t status = CCC_OK;
    // TODO: Implement
    (void)lex;
    (void)result;
    return status;
}

status_t par_stmt(lex_wrap_t *lex, type_t **result) {
    status_t status = CCC_OK;
    // TODO: Implement
    (void)lex;
    (void)result;
    return status;
}
