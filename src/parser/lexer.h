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
/**
 * Lexer interface
 */

#ifndef _LEXER_H_
#define _LEXER_H_

#include "parser/preprocessor.h"
#include "parser/symtab.h"
#include "parser/token.h"

#define CHAR_BITS (sizeof(char) * 8);
#define SHORT_BITS (sizeof(short) * 8);
#define INT_BITS (sizeof(int) * 8);
#define LONG_BITS (sizeof(long) * 8);
#define LONG_LONG_BITS (sizeof(long long) * 8);

/** Size of lexer internal buffer */
#define MAX_LEXEME_SIZE 4096

/**
 * Structure represerting a lexer
 */
typedef struct lexer_t {
    preprocessor_t *pp;           /**< Preprocessor to get characters from */
    symtab_t *symtab;             /**< Symbol table */
    symtab_t *string_tab;         /**< String table */
    char lexbuf[MAX_LEXEME_SIZE]; /**< Temporary work buffer */
    int next_char;                /**< Next buffered character */
} lexer_t;

/**
 * Structure representing a single lexeme
 *
 * Acts as a tagged union
 */
typedef struct lexeme_t {
    token_t type;                  /**< Type of lememe */
    fmark_t mark;                  /**< Location of lexeme */

    union {
        symtab_entry_t *tab_entry; /**< For string/id types */
        struct {
            long long int_val;     /**< For integral types */
            bool hasU;             /**< Has U suffix */
            bool hasL;             /**< Has U suffix */
            bool hasLL;            /**< Has LL suffix */
        } int_params;
        struct {
            double float_val;      /**< For floating point types */
            bool hasF;             /**< Has F suffix */
        } float_params;
    };
} lexeme_t;


/**
 * Initializes a lexer object
 *
 * @param lexer The lexer to initialize
 * @param pp Preprocessor to fetch chars from
 * @param symtab Symbol table to use
 * @param string_tab string table to use
 * @return CCC_OK on success, error code or error
 */
status_t lexer_init(lexer_t *lexer, preprocessor_t *pp, symtab_t *symtab,
                    symtab_t *string_tab);

/**
 * Destroys a lexer object
 *
 * @param lexer The lexer to destroy
 */
void lexer_destroy(lexer_t *lexer);

/**
 * Fetches the next lexeme
 *
 * @param lexer The lexer to fetch from
 * @return CCC_OK on success, error code on error
 */
status_t lexer_next_token(lexer_t *lexer, lexeme_t *result);

#endif /* _LEXER_H_ */
