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
 * Token types
 */

#ifndef _TOKEN_H_
#define _TOKEN_H_

#include <stdio.h>

#include "util/file_mark.h"
#include "util/string_set.h"
#include "util/string_builder.h"

typedef enum token_type_t {
    TOKEN_EOF,     // EOF
    HASH,          // #
    HASHHASH,      // ##

    SPACE,         // ' '
    NEWLINE,       // '\n'
    BACKSLASH,     // '\\'

    // Delimiters
    LBRACE,        // {
    RBRACE,        // }
    LPAREN,        // (
    RPAREN,        // )
    SEMI,          // ;
    COMMA,         // ,
    LBRACK,        // [
    RBRACK,        // ]
    DEREF,         // ->
    DOT,           // .
    ELIPSE,        // ...

    COND,          // ?
    COLON,         // :

    // Assignment operators
    ASSIGN,        // =
    PLUSEQ,        // +=
    MINUSEQ,       // -=
    STAREQ,        // *=
    DIVEQ,         // /=
    MODEQ,         // %=
    BITXOREQ,      // ^=
    BITOREQ,       // |=
    BITANDEQ,      // &=
    RSHIFTEQ,      // >>=
    LSHIFTEQ,      // <<=

    // Comparison operators
    EQ,            // ==
    NE,            // !=
    LT,            // <
    GT,            // >
    LE,            // <=
    GE,            // >=

    // Arithmetic
    RSHIFT,        // >>
    LSHIFT,        // <<

    LOGICAND,      // &&
    LOGICOR,       // ||
    LOGICNOT,      // !

    PLUS,          // +
    MINUS,         // -
    STAR,          // *
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

    // Underscore keywords
    ALIGNAS,       // _Alignas
    ALIGNOF,       // _Alignof
    BOOL,          // _Bool
    COMPLEX,       // _Complex
    GENERIC,       // _Generic
    IMAGINARY,     // _Imaginary
    STATIC_ASSERT, // _Static_assert
    THREAD_LOCAL,  // _Thread_local

    // Built in
    OFFSETOF,      // __builtin_offsetof
    VA_LIST,       // __builtin_va_list
    VA_START,      // __builtin_va_start
    VA_ARG,        // __builtin_va_arg
    VA_END,        // __builtin_va_end
    VA_COPY,       // __builtin_va_copy

    // Types
    VOID,          // void

    CHAR,          // char
    SHORT,         // short
    INT,           // int
    LONG,          // long
    UNSIGNED,      // unsigned
    SIGNED,        // signed

    DOUBLE,        // double
    FLOAT,         // float

    // Other
    ID,            // identifier
    STRING,        // string
    INTLIT,        // Integral literal
    FLOATLIT,      // Float literal

    FUNC,          // __func__
} token_type_t;

typedef struct token_int_params_t {
    bool hasU;             /**< Has U suffix */
    bool hasL;             /**< Has L suffix */
    bool hasLL;            /**< Has LL suffix */
    long long int_val;     /**< For integral types */
} token_int_params_t;

typedef struct token_float_params_t {
    bool hasF;             /**< Has F suffix */
    bool hasL;             /**< Has L suffix */
    long double float_val; /**< For floating point types */
} token_float_params_t;

/**
 * Token structure
 */
typedef struct token_t {
    token_type_t type;             /**< Type of token */
    unsigned len;
    char *start;
    fmark_t *mark;                 /**< Location of token */
    str_set_t *hideset;

    union {
        char *id_name;
        char *str_val;
        token_int_params_t *int_params;
        token_float_params_t *float_params;
    };
} token_t;

typedef struct token_man_t {
    slist_t tokens;
} token_man_t;

extern token_t token_int_zero;
extern token_t token_int_one;
extern token_t token_eof;

void token_man_init(token_man_t *tm);
void token_man_destroy(token_man_t *tm);

token_t *token_create(token_man_t *tm);
token_t *token_copy(token_man_t *tm, token_t *token);

bool token_equal(const token_t *t1, const token_t *t2);

/**
 * Prints a token
 *
 * @param token The token to print
 */
void token_print(FILE *file, token_t *token);

char *token_str(token_t *token);

void token_str_append_sb(string_builder_t *sb, token_t *token);

/**
 * Returns a pointer to a token's string representation
 */
const char *token_type_str(token_type_t token);


#endif /* _TOKEN_H_ */
