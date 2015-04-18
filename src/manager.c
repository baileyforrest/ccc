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
 * Implementation for compliation manager
 */

#include "manager.h"

#include <assert.h>

#include "ir/translator.h"

#include "parse/ast.h"
#include "parse/lexer.h"
#include "parse/parser.h"
#include "parse/preprocessor.h"
#include "parse/symtab.h"

void man_init(manager_t *manager, htable_t *macros) {
    assert(manager != NULL);

    pp_init(&manager->pp, macros);

    // If macros != NULL, then this is for evaluating preprocessor if, so then
    // don't initialize with symbols. This is important because the static
    // entries can only be in one table at a time
    bool sym = macros == NULL ? IS_SYM : NOT_SYM;
    st_init(&manager->symtab, sym);
    st_init(&manager->string_tab, NOT_SYM);

    lexer_init(&manager->lexer, &manager->pp, &manager->symtab,
               &manager->string_tab);
    manager->ast = NULL;
    manager->ir = NULL;
}

void man_destroy(manager_t *manager) {
    if (manager == NULL) {
        return;
    }

    lexer_destroy(&manager->lexer);
    st_destroy(&manager->string_tab);
    st_destroy(&manager->symtab);
    pp_destroy(&manager->pp);
    man_destroy_ast(manager);
    man_destroy_ir(manager);
}

void man_destroy_ast(manager_t *manager) {
    ast_destroy(manager->ast);
    manager->ast = NULL;
}

void man_destroy_ir(manager_t *manager) {
    ir_trans_unit_destroy(manager->ir);
    manager->ir = NULL;
}

status_t man_parse(manager_t *manager, trans_unit_t **ast) {
    assert(manager != NULL);
    assert(ast != NULL);
    status_t status = parser_parse(&manager->lexer, &manager->ast);
    *ast = manager->ast;
    return status;
}

status_t man_parse_expr(manager_t *manager, expr_t **expr) {
    assert(manager != NULL);
    assert(expr != NULL);
    return parser_parse_expr(&manager->lexer, expr);
}

ir_trans_unit_t *man_translate(manager_t *manager) {
    assert(manager != NULL);
    assert(manager->ast != NULL);
    manager->ir = trans_translate(manager->ast);
    return manager->ir;
}

status_t man_dump_tokens(manager_t *manager) {
    status_t status = CCC_OK;

    lexeme_t cur_token;
    do {
        lexer_next_token(&manager->lexer, &cur_token);
        token_print(&cur_token);
        fmark_chain_free(cur_token.mark.last);
    } while(cur_token.type != TOKEN_EOF);

    return status;
}
