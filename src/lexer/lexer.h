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

#ifndef _LEXER_H_
#define _LEXER_H_

typedef enum token_t {
    // Delimiters
    LBRACE,        // {
    RBRACE,        // }
    LPAREN,        // (
    RPAREN,        // )
    SEMI,          // ;
    COMMA,         // ,

    COND,          // ?
    COLON,         // :

    // Assignment operators
    ASSIGN,        // =
    PLUSEQ,        // +=
    MINUSEQ,       // -=
    MULEQ,         // *=
    DIVEQ,         // /=
    MODEQ,         // %=
    NEQ,           // !=
    ANDEQ,         // &=
    XOREQ,         // ^=
    OREQ,          // |=
    RSHIFTEQ,      // >>=
    LSHIFTEQ,      // <<=

    // Comparison operators
    EQ,            // ==
    LT,            // <
    GT,            // >
    LTEQ,          // <=
    GTEQ,          // >=

    // Arithmetic
    RSHIFT,        // >>
    LSHIFT,        // <<

    LOGICAND,      // &&
    LOGICOR,       // ||
    LOGICNOT,      // !

    PLUS,          // +
    MINUS,         // -
    MUL,           // *
    DIV,           // /
    MOD,           // %

    BITAND,        // &
    BITOR,         // |
    BITXOR,        // ^
    BITNOT,        // ~

    INC,           // ++
    DEC,           // --

    // Keywords
    AUTO,          // auto
    BREAK,         // break
    CASE,          // case
    CONST,         // const
    CONTINUE,      // continue
    DEFAULT,       // default
    DO,            // do
    ELSE,          // else
    ENUM,          // enum
    EXTERN,        // extern
    FOR,           // for
    GOTO,          // goto
    IF,            // if
    INLINE,        // inline
    REGISTER,      // register
    RESTRICT,      // restrict
    RETURN,        // return
    SIZEOF,        // sizeof
    STATIC,        // static
    STRUCT,        // struct
    SWITCH,        // switch
    TYPEDEF,       // typedef
    UNION,         // union
    VOLATILE,      // volatile
    WHILE,         // while

    // New
    ALIGNAS,       // _Alignas
    ALIGNOF,       // _Alignof
    BOOL,          // _Bool
    COMPLEX,       // _Complex
    GENERIC,       // _Generic
    IMAGINARY,     // _Imaginary
    NORETURN,      // _Noreturn
    STATIC_ASSERT, // _Static_assert
    THREAD_LOCAL,  // _Thread_local

    // Types
    VOID,     // void

    CHAR,     // char
    SHORT,    // short
    INT,      // int
    LONG,     // long
    UNSIGNED, // unsigned
    SIGNED,   // unsigned

    DOUBLE,   // double
    FLOAT,

    // Other
    ID,       // identifier
} token_t;

#endif /* _LEXER_H_ */
