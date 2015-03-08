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
 * Token function implmenetation
 */

#include "token.h"

#include <assert.h>
#include <stdio.h>

#include "parser/symtab.h"

#define CASE_BASIC_PRINT(token) \
    case token: printf(#token "\n"); break

void token_print(lexeme_t *token) {
    assert(token != NULL);

    switch (token->type) {
        CASE_BASIC_PRINT(TOKEN_EOF);     // end of file
        // Delimiters
        CASE_BASIC_PRINT(LBRACE);        // {
        CASE_BASIC_PRINT(RBRACE);        // }
        CASE_BASIC_PRINT(LPAREN);        // (
        CASE_BASIC_PRINT(RPAREN);        // )
        CASE_BASIC_PRINT(SEMI);          // ;
        CASE_BASIC_PRINT(COMMA);         // );

        CASE_BASIC_PRINT(COND);          // ?
        CASE_BASIC_PRINT(COLON);         // :

        // Assignment operators
        CASE_BASIC_PRINT(ASSIGN);        // =
        CASE_BASIC_PRINT(PLUSEQ);        // +=
        CASE_BASIC_PRINT(MINUSEQ);       // -=
        CASE_BASIC_PRINT(STAREQ);        // *=
        CASE_BASIC_PRINT(DIVEQ);         // /=
        CASE_BASIC_PRINT(MODEQ);         // %=
        CASE_BASIC_PRINT(BITXOREQ);      // ^=
        CASE_BASIC_PRINT(BITOREQ);       // |=
        CASE_BASIC_PRINT(BITANDEQ);      // &=
        CASE_BASIC_PRINT(RSHIFTEQ);      // >>=
        CASE_BASIC_PRINT(LSHIFTEQ);      // <<=

        // Comparison operators
        CASE_BASIC_PRINT(EQ);            // ==
        CASE_BASIC_PRINT(NE);            // !=
        CASE_BASIC_PRINT(LT);            // <
        CASE_BASIC_PRINT(GT);            // >
        CASE_BASIC_PRINT(LE);            // <=
        CASE_BASIC_PRINT(GE);            // >=

        // Arithmetic
        CASE_BASIC_PRINT(RSHIFT);        // >>
        CASE_BASIC_PRINT(LSHIFT);        // <<

        CASE_BASIC_PRINT(LOGICAND);      // &&
        CASE_BASIC_PRINT(LOGICOR);       // ||
        CASE_BASIC_PRINT(LOGICNOT);      // !

        CASE_BASIC_PRINT(PLUS);          // +
        CASE_BASIC_PRINT(MINUS);         // -
        CASE_BASIC_PRINT(STAR);          // *
        CASE_BASIC_PRINT(DIV);           // /
        CASE_BASIC_PRINT(MOD);           // %

        CASE_BASIC_PRINT(BITAND);        // &
        CASE_BASIC_PRINT(BITOR);         // |
        CASE_BASIC_PRINT(BITXOR);        // ^
        CASE_BASIC_PRINT(BITNOT);        // ~

        CASE_BASIC_PRINT(INC);           // ++
        CASE_BASIC_PRINT(DEC);           // --

        // Keywords
        CASE_BASIC_PRINT(AUTO);          // auto
        CASE_BASIC_PRINT(BREAK);         // break
        CASE_BASIC_PRINT(CASE);          // case
        CASE_BASIC_PRINT(CONST);         // const
        CASE_BASIC_PRINT(CONTINUE);      // continue
        CASE_BASIC_PRINT(DEFAULT);       // default
        CASE_BASIC_PRINT(DO);            // do
        CASE_BASIC_PRINT(ELSE);          // else
        CASE_BASIC_PRINT(ENUM);          // enum
        CASE_BASIC_PRINT(EXTERN);        // extern
        CASE_BASIC_PRINT(FOR);           // for
        CASE_BASIC_PRINT(GOTO);          // goto
        CASE_BASIC_PRINT(IF);            // if
        CASE_BASIC_PRINT(INLINE);        // inline
        CASE_BASIC_PRINT(REGISTER);      // register
        CASE_BASIC_PRINT(RESTRICT);      // restrict
        CASE_BASIC_PRINT(RETURN);        // return
        CASE_BASIC_PRINT(SIZEOF);        // sizeof
        CASE_BASIC_PRINT(STATIC);        // static
        CASE_BASIC_PRINT(STRUCT);        // struct
        CASE_BASIC_PRINT(SWITCH);        // switch
        CASE_BASIC_PRINT(TYPEDEF);       // typedef
        CASE_BASIC_PRINT(UNION);         // union
        CASE_BASIC_PRINT(VOLATILE);      // volatile
        CASE_BASIC_PRINT(WHILE);         // while

        // Underscore keywords
        CASE_BASIC_PRINT(ALIGNAS);       // _Alignas
        CASE_BASIC_PRINT(ALIGNOF);       // _Alignof
        CASE_BASIC_PRINT(BOOL);          // _Bool
        CASE_BASIC_PRINT(COMPLEX);       // _Complex
        CASE_BASIC_PRINT(GENERIC);       // _Generic
        CASE_BASIC_PRINT(IMAGINARY);     // _Imaginary
        CASE_BASIC_PRINT(NORETURN);      // _Noreturn
        CASE_BASIC_PRINT(STATIC_ASSERT); // _Static_assert
        CASE_BASIC_PRINT(THREAD_LOCAL);  // _Thread_local

        // Types
        CASE_BASIC_PRINT(VOID);     // void

        CASE_BASIC_PRINT(CHAR);     // char
        CASE_BASIC_PRINT(SHORT);    // short
        CASE_BASIC_PRINT(INT);      // int
        CASE_BASIC_PRINT(LONG);     // long
        CASE_BASIC_PRINT(UNSIGNED); // unsigned
        CASE_BASIC_PRINT(SIGNED);   // signed

        CASE_BASIC_PRINT(DOUBLE);   // double
        CASE_BASIC_PRINT(FLOAT);    // float

        // Other
    case ID:
        printf("ID: %s\n", token->tab_entry->key.str);
        break;
    case STRING:
        printf("STRING: %s\n", token->tab_entry->key.str);
        break;
    case INTLIT:
        printf("INTLIT: %lld, U:%d L:%d LL:%d\n", token->int_params.int_val,
               token->int_params.hasU, token->int_params.hasL,
               token->int_params.hasLL);
        break;
    case FLOATLIT:
        printf("FLOATLIT: %f, F:%d", token->float_params.float_val,
               token->float_params.hasF);
        break;
    }
}
