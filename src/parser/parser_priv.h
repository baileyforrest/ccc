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
 * Parser private interface
 */

#ifndef _PARSER_PRIV_H_
#define _PARSER_PRIV_H_

#include "util/status.h"

#define ALLOC_NODE(loc, type)                                       \
    do {                                                            \
        loc = malloc(sizeof(type));                                 \
        if (loc == NULL) {                                          \
            logger_log(&lex->cur.mark, "Out of memory in parser",   \
                       LOG_ERR);                                    \
            status = CCC_NOMEM;                                     \
            goto fail;                                              \
        }                                                           \
    } while (0)

/**
 * Wrapper for lexer
 */
typedef struct lex_wrap_t {
    lexer_t *lexer;
    lexeme_t cur;
} lex_wrap_t;

/**
 * Advance lexer wrapper to next token
 *
 * @param wrap Wrapper to advance
 */
#define LEX_ADVANCE(wrap)                                               \
    do {                                                                \
        if (CCC_OK !=                                                   \
            (status = lexer_next_token((wrap)->lexer, &(wrap)->cur))) { \
            goto fail;                                                  \
        }                                                               \
    } while (0)

/**
 * Match lexer wrapper with specified token, then advance to next token
 *
 * @param wrap Wrapper to match with
 * @param token Token to match
 */
#define LEX_MATCH(wrap, token)                                          \
    do {                                                                \
        if ((wrap)->cur.type != (token)) {                              \
            /* TODO: Change this to print out token found, expected */  \
            snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,                  \
                     "Parser Error: Expected %d", token);               \
            logger_log(&(wrap)->cur.mark, logger_fmt_buf, LOG_ERR);     \
            status = CCC_ESYNTAX;                                       \
            goto fail;                                                  \
        }                                                               \
        LEX_WRAP_ADVANCE(wrap);                                         \
    } while (0)

status_t par_trans_unit(lex_wrap_t *lex, trans_unit_t **result);

status_t par_gdecl(lex_wrap_t *lex, gdecl_t **result);
//status_t par_gdecl_param(lex_wrap_t *lex, param_t **result);
//status_t par_gdecl_param_list(lex_wrap_t *lex, slist__t **result);

status_t par_type(lex_wrap_t *lex, type_t **result);
//status_t par_type_struct_decls(lex_wrap_t *lex, type_t **result);
//status_t par_type_arr_decl(lex_wrap_t *lex, slist_t **result);

status_t par_expr(lex_wrap_t *lex, type_t **result);

status_t par_stmt(lex_wrap_t *lex, type_t **result);

#endif /* _PARSER_PRIV_H_ */
