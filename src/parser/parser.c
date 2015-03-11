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
        goto fail;
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
    trans_unit_destroy(tunit);
fail0: // Failed allocation
    free(tunit);
    return status;
}

void trans_unit_destroy(trans_unit *tunit) {
    sl_link_t *cur;
    SL_FOR_EACH(cur, &tunit->gdecls) {
        gdecl_t *gdecl = GET_ELEM(&tunit->gdecls, cur);
        gdecl_destroy(gdecl);
    }
    sl_destroy(&tunit->gdecls, NOFREE);
}

status_t par_gdecl(lex_wrap_t *lex, gdecl_t **result) {
    status_t status = CCC_OK;
    gdecl_t *gdecl = NULL;

    // Setup new gdecl
    ALLOC_NODE(gdecl, gdecl_t);
    gdecl->type = GDECL_NULL; // Unknown type of gdecl

    if (lex->cur.type == TYPEDEF) {
        gdecl->type = GDECL_TYPEDEF;
        LEX_ADVANCE(lex);
        type_t *type;
        if (CCC_OK != (status = par_type(lex, &gdecl->typedef_params.type))) {
            goto fail;
        }

        if (lex->cur.type != ID) {
            // TODO: Report error here
            assert(false);
            status = CCC_ESYNTAX;
            goto fail;
        }
        gdecl->typdef_params.name = &lex->cur.tab_entry.key;
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, SEMI);

        return status;
    }

    type_t *type;
    if (CCC_OK != (status = par_type(lex, &type))) {
        goto fail;
    }

    if (type->type_mod & TMOD_TYPEDEF) {
    }

    if (lex->cur.type == SEMI) {
        gdecl->type = TYPE;
        gdecl->type.type = type;
        return status;
    }

    // TODO: This isn't correct because parens can wrap around identifiers
    if (lex->cur.type != ID) {
        // TODO: report
        assert(false);
    }
    len_str_t *id = &cur->type.tab_entry.key;
    LEX_ADVANCE(lex);
    if (lex->cur.type == EQ) { // match assignments
        // TODO: should match a statement
        return status;
    }

    // Match function decls and defns
    if (lex->cur.type != LPAREN) {
        // TODO: report
        assert(false);
    }

    slist_t list;
    LEX_ADVANCE(lex);
    while (lex->cur.type != RPAREN) {
        param_t *param;
        ALLOC_NODE(param, param_t);
        slist_append(&list, &param->link);
        if (CCC_OK != (status = par_type(lex, &param->type))) {
            // TODO: Destroy list here on failure
            goto fail;
        }
        switch (lex->cur.type) {
        case COMMA:  // No param name provided
            param->id = NULL;
            break;
        case ID:
            param->id = &lex->cur.tab_entry.key;
            break;
        default:
            // TODO: Handle error
            assert(false);
        }
    }
    LEX_ADVANCE(lex); // Advance past RPAREN

    if (lex->cur.type == SEMI) {
        gdecl->type = GDECL_FDECL;
        gdecl->fdecl.ret = type;
        gdecl->fdecl.id = id;
        memcpy(&gdecl->fdecl.params, &list);

        return status;
    }

    if (lex->cur.type != LBRACE) {
        // TODO: Handle this
        assert(false);
    }

    gdecl->type = GDECL_FDEFN;
    gdecl->fdefn.ret = type;
    gdecl->fdefn.id = id;
    memcpy(&gdecl->fdefn.params, &list);

    if (CCC_OK != (status = par_stmt(gdecl->fdefn.stmt))) {
        goto fail;
    }

    return status;

fail:
    gdecl_destroy(gdecl);
    return status;
}

void gdecl_destroy(gdecl_t *gdecl) {
    if (gdecl == NULL) {
        return;
    }
    sl_link_t cur;

    switch (gdecl->type) {
    case GDECL_NULL:
        break;
    case GDECL_FDEFN:
        type_destroy(gdecl->fdefn.ret);
        SL_FOREACH(cur, &gdecl->fdefn.params) {
            param_t *param = GET_ELEM(&gdecl->fdefn.params, cur);
            destroy_type(&param->type);
        }
        slist_destroy(&gdecl->fdefn.params, DOFREE);
        stmt_destroy(gdecl->fdefn.stmt);
        break;
    case GDECL_FDECL:
        type_destroy(gdecl->fdecl.ret);
        SL_FOREACH(cur, &gdecl->fdecl.params) {
            param_t *param = GET_ELEM(&gdecl->fdecl.params, cur);
            destroy_type(&param->type);
        }
        slist_destroy(&gdecl->fdecl.params, DOFREE);
        break;
    case GDECL_VDECL:
        destroy_stmt(&gdecl->vdecl);
        break;
    case GDECL_TYPE:
        destroy_type(&gdecl->type);
    case GDECL_TYPEDEF:
    }
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
