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
 * Lexer interface
 */

#ifndef _LEXER_H_
#define _LEXER_H_

#include "lex/symtab.h"
#include "util/string_builder.h"
#include "util/text_stream.h"
#include "util/vector.h"

/**
 * Structure represerting a lexer
 */
typedef struct lexer_t {
    symtab_t *symtab;         /**< Symbol table */
    token_man_t *token_man;   /**< Symbol table */
    string_builder_t lexbuf;
} lexer_t;

/**
 * Initializes a lexer object
 *
 * @param lexer The lexer to initialize
 */
void lexer_init(lexer_t *lexer, token_man_t *token_man, symtab_t *symtab);

/**
 * Destroys a lexer object
 *
 * @param lexer The lexer to destroy
 */
void lexer_destroy(lexer_t *lexer);

/**
 * Creates token stream from text stream
 *
 * @param lexer The lexer to fetch from
 * @param stream text stream
 * @param result Location to store tokens
 * @return CCC_OK on success, error code on error
 */
status_t lexer_lex_stream(lexer_t *lexer, tstream_t *stream, vec_t *result);

#endif /* _LEXER_H_ */
