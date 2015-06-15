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
 * Lexer private interface
 */

#ifndef _LEX_PRIV_H_
#define _LEX_PRIV_H_

#include "lex.h"

typedef enum lex_str_type_t {
    LEX_STR_CHAR,
    LEX_STR_LCHAR,
    LEX_STR_U8,
    LEX_STR_U16,
    LEX_STR_U32,
} lex_str_type_t;

typedef struct lex_state_t {
    lexer_t *lexer;
    vec_t *ostream;
} lex_state_t;

int lex_if_next_eq(tstream_t *stream, int test, token_type_t noeq,
                   token_type_t iseq);

int lex_getc_splice(tstream_t *stream);

status_t lex_next_token(lex_state_t *ls, tstream_t *stream, token_t *result);

status_t lex_id(lex_state_t *ls, tstream_t *stream, int cur, token_t *result);

uint32_t lex_single_char(lex_state_t *ls, tstream_t *stream, token_t *result,
                         lex_str_type_t type);

status_t lex_char_lit(lex_state_t *ls, tstream_t *stream, token_t *result,
                      lex_str_type_t type);

status_t lex_string(lex_state_t *ls, tstream_t *stream, token_t *result,
                    lex_str_type_t type);


/**
 * Lex a number value
 *
 * @param lexer Lexer to use
 * @param result Location to store the result
 */
status_t lex_number(lex_state_t *ls, tstream_t *stream, int cur,
                    token_t *result);


#endif /* _LEX_PRIV_H_ */
