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
 * Interface for compliation manager
 */

#ifndef _MANAGER_H_
#define _MANAGER_H_

#include "ast/ast.h"
#include "ir/ir.h"
#include "lex/lex.h"
#include "lex/symtab.h"
#include "util/status.h"

/**
 * Compilation manager structure. Manages data structures necessary for compiles
 */
typedef struct manager_t {
    vec_t tokens;
    symtab_t symtab;
    lexer_t lexer;
    token_man_t token_man;
    fmark_man_t mark_man;
    trans_unit_t *ast;
    ir_trans_unit_t *ir;
    bool parse_destroyed;
} manager_t;

/**
 * Initialize a compilation mananger
 *
 * @param manager The compilation mananger to initialize
 */
void man_init(manager_t *manager);

/**
 * Destroy a compilation mananger
 *
 * @param manager The compilation mananger to destroy
 */
void man_destroy(manager_t *manager);

/**
 * Destroy a compilation mananger parsing data structures
 *
 * Calling this is optional because destructor destroys these. May be used to
 * reduce memory usage however.
 *
 * @param manager The compilation mananger to destroy ast
 */
void man_destroy_parse(manager_t *manager);

/**
 * Destroy a compilation mananger's ir
 *
 * Calling this is optional because destructor destroys ir. May be used to
 * reduce memory usage however.
 *
 * @param manager The compilation mananger to destroy ir
 */
void man_destroy_ir(manager_t *manager);

status_t man_lex(manager_t *manager, char *filepath);

/**
 * Parse a translation unit from a compilation manager.
 *
 * The manager's preprocessor must be set up first
 *
 * @param manager The compilation mananger to parse
 * @param filepath Path to file to parse
 * @param ast The parsed ast
 * @return CCC_OK on success, error code on error.
 */
status_t man_parse(manager_t *manager, trans_unit_t **ast);

/**
 * Parse an expression from a compilation manager.
 *
 * The manager's preprocessor must be set up first
 *
 * @param manager The compilation mananger to parse
 * @param expr The parsed expression
 * @return CCC_OK on success, error code on error.
 */
status_t man_parse_expr(manager_t *manager, expr_t **expr);

/**
 * Translate the ast in a manager
 *
 * The manager must have a vaild ast from man_parse
 *
 * @param manager The compilation mananger to use
 * @return Returns the ir tree
 */
ir_trans_unit_t *man_translate(manager_t *manager);

/**
 * Print the tokens from a compilation manager
 *
 * The manager's preprocessor must be set up first
 *
 * @param manager The compilation mananger to use
 * @return CCC_OK on success, error code on error.
 */
status_t man_dump_tokens(manager_t *manager);

#endif /* _MANAGER_H_ */
