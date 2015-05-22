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

#include "lex/cpp.h"
#include "lex/lexer.h"
#include "lex/symtab.h"

#include "parse/ast.h"
#include "parse/parser.h"

void man_init(manager_t *manager, htable_t *macros) {
    assert(manager != NULL);

    vec_init(&manager->tokens, 0);

    bool preload = macros != NULL;
    st_init(&manager->symtab, preload);

    token_man_init(&manager->token_man);
    lexer_init(&manager->lexer, &manager->token_man, &manager->symtab);

    manager->ast = NULL;
    manager->ir = NULL;
    manager->parse_destroyed = false;
}

void man_destroy(manager_t *manager) {
    if (manager == NULL) {
        return;
    }

    if (!manager->parse_destroyed) {
        lexer_destroy(&manager->lexer);
        ast_destroy(manager->ast);
    }

    st_destroy(&manager->symtab);
    man_destroy_ir(manager);
}

void man_destroy_parse(manager_t *manager) {
    assert(!manager->parse_destroyed);
    manager->parse_destroyed = true;

    lexer_destroy(&manager->lexer);
    ast_destroy(manager->ast);
}

void man_destroy_ir(manager_t *manager) {
    ir_trans_unit_destroy(manager->ir);
    manager->ir = NULL;
}

status_t man_lex(manager_t *manager, char *filepath) {
    return cpp_process(&manager->token_man, &manager->lexer, filepath,
                       &manager->tokens);
}

status_t man_parse(manager_t *manager, trans_unit_t **ast) {
    assert(manager != NULL);
    assert(ast != NULL);

    status_t status = parser_parse(&manager->tokens, &manager->ast);
    *ast = manager->ast;
    return status;
}

status_t man_parse_expr(manager_t *manager, expr_t **expr) {
    assert(manager != NULL);
    assert(expr != NULL);
    manager->ast = ast_trans_unit_create(true);
    return parser_parse_expr(&manager->tokens, manager->ast, expr);
}

ir_trans_unit_t *man_translate(manager_t *manager) {
    assert(manager != NULL);
    assert(manager->ast != NULL);
    manager->ir = trans_translate(manager->ast);
    return manager->ir;
}

status_t man_dump_tokens(manager_t *manager) {
    status_t status = CCC_OK;

    VEC_FOREACH(cur, &manager->tokens) {
        token_print(stdout, vec_get(&manager->tokens, cur));
    }

    return status;
}
