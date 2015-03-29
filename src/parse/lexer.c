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
 * Lexer implementation
 */

#include "lexer.h"

#include <stdio.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

#include "util/logger.h"

/**
 * Gets next character from preprocessor ignoring errors
 *
 * @param dest Variable to assign to
 * @param pp Preprocessor to get characters from
 */
#define NEXT_CHAR_NOERR(pp, dest)               \
    do {                                        \
    dest = pp_nextchar(pp);                     \
    } while (dest < 0)

/**
 * Lex a number value
 *
 * @param lexer Lexer to use
 * @param cur First input character
 */
static status_t lex_number(lexer_t *lexer, int cur, lexeme_t *result);


status_t lexer_init(lexer_t *lexer, preprocessor_t *pp, symtab_t *symtab,
                    symtab_t *string_tab) {
    assert(lexer != NULL);
    assert(pp != NULL);
    assert(symtab != NULL);
    assert(string_tab != NULL);

    lexer->pp = pp;
    lexer->symtab = symtab;
    lexer->string_tab = string_tab;
    lexer->next_char = 0;
    return CCC_OK;
}

void lexer_destroy(lexer_t *lexer) {
    assert(lexer != NULL);
    // noop
    (void)lexer;
}

#define CHECK_NEXT_EQ(noeq, iseq)               \
    do {                                        \
        NEXT_CHAR_NOERR(lexer->pp, next);       \
        if (next == '=') {                      \
            result->type = iseq;                \
        } else {                                \
            result->type = noeq;                \
            lexer->next_char = next;            \
        }                                       \
    } while (0)

status_t lexer_next_token(lexer_t *lexer, lexeme_t *result) {
    assert(lexer != NULL);
    assert(result != NULL);
    status_t status = CCC_OK;

    int cur;
    bool done;
    do { // Repeat loop until we find a token
        done = true;

        if (lexer->next_char) {
            cur = lexer->next_char;
            lexer->next_char = 0;
        } else {
            NEXT_CHAR_NOERR(lexer->pp, cur);
        }

        pp_last_mark(lexer->pp, &result->mark);

        int next;
        switch (cur) {
        case PP_EOF: result->type = TOKEN_EOF; break;
        case '{': result->type = LBRACE; break;
        case '}': result->type = RBRACE; break;
        case '(': result->type = LPAREN; break;
        case ')': result->type = RPAREN; break;
        case ';': result->type = SEMI; break;
        case ',': result->type = COMMA; break;
        case '[': result->type = LBRACK; break;
        case ']': result->type = RBRACK; break;
        case '?': result->type = COND; break;
        case ':': result->type = COLON; break;
        case '~': result->type = BITNOT; break;
        case '=': CHECK_NEXT_EQ(ASSIGN, EQ); break;
        case '*': CHECK_NEXT_EQ(STAR, STAREQ); break;
        case '/': CHECK_NEXT_EQ(DIV, DIVEQ); break;
        case '%': CHECK_NEXT_EQ(MOD, MODEQ); break;
        case '!': CHECK_NEXT_EQ(LOGICNOT, NE); break;
        case '^': CHECK_NEXT_EQ(BITXOR, BITXOREQ); break;
        case '.': {
            NEXT_CHAR_NOERR(lexer->pp, next);
            if (next != '.') {
                result->type = DOT;
                lexer->next_char = next;
                break;
            }
            NEXT_CHAR_NOERR(lexer->pp, next);
            if (next == '.') {
                result->type = ELIPSE;
                break;
            }
            logger_log(&result->mark, LOG_ERR, "Unexpected token: ..");
            status = CCC_ESYNTAX;
            break;
        }
        case '+': {
            NEXT_CHAR_NOERR(lexer->pp, next);
            switch(next) {
            case '+': result->type = INC; break;
            case '=': result->type = PLUSEQ; break;
            default:
                result->type = PLUS;
                lexer->next_char = next;
            }
            break;
        }
        case '-': {
            NEXT_CHAR_NOERR(lexer->pp, next);
            switch(next) {
            case '-': result->type = DEC; break;
            case '=': result->type = MINUSEQ; break;
            case '>': result->type = DEREF; break;
            case ASCII_DIGIT: // Negative number
                lexer->next_char = next;
                status = lex_number(lexer, cur, result);
                break;
            default:
                result->type = MINUS;
                lexer->next_char = next;
            }
            break;
        }
        case '|': {
            NEXT_CHAR_NOERR(lexer->pp, next);
            switch(next) {
            case '|': result->type = LOGICOR; break;
            case '=': result->type = BITOREQ; break;
            default:
                result->type = BITOR;
                lexer->next_char = next;
                break;
            }
            break;
        }
        case '&': {
            NEXT_CHAR_NOERR(lexer->pp, next);
            switch(next) {
            case '&': result->type = LOGICAND; break;
            case '=': result->type = BITANDEQ; break;
            default:
                result->type = BITAND;
                lexer->next_char = next;
                break;
            }
            break;
        }
        case '>': {
            NEXT_CHAR_NOERR(lexer->pp, next);
            switch (next) {
            case '=': result->type = GE; break;
            case '>': CHECK_NEXT_EQ(RSHIFT, RSHIFTEQ); break;
            default:
                result->type = GT;
                lexer->next_char = next;
            }
            break;
        }
        case '<': {
            NEXT_CHAR_NOERR(lexer->pp, next);
            switch (next) {
            case '=': result->type = LE; break;
            case '<': CHECK_NEXT_EQ(LSHIFT, LSHIFTEQ); break;
            default:
                result->type = LT;
                lexer->next_char = next;
            }
            break;
        }
        case ASCII_LOWER: // Identifiers
        case ASCII_UPPER:
        case '_': {
            result->type = ID;

            int len = 0;
            lexer->lexbuf[len++] = cur;

            bool done = false;
            while (!done && len < MAX_LEXEME_SIZE) {
                NEXT_CHAR_NOERR(lexer->pp, cur);
                switch (cur) {
                case ASCII_LOWER:
                case ASCII_UPPER:
                case ASCII_DIGIT:
                case '_':
                    lexer->lexbuf[len++] = cur;
                    break;
                default:
                    done = true;
                }
            }

            if (!done) {
                logger_log(&result->mark, LOG_ERR, "Identifer too long!");
                status = CCC_ESYNTAX;

                // Skip over the rest of the identifier
                while (!done) {
                    NEXT_CHAR_NOERR(lexer->pp, cur);
                    switch (cur) {
                    case ASCII_LOWER:
                    case ASCII_UPPER:
                    case ASCII_DIGIT:
                    case '_':
                        break;
                    default:
                        done = true;
                    }
                }
            }
            lexer->next_char = cur;

            if (CCC_OK !=
                (status = st_lookup(lexer->symtab, lexer->lexbuf, len, ID,
                                    &result->tab_entry))) {
                logger_log(&result->mark, LOG_ERR, "Failed to add identifier!");
                status = CCC_ESYNTAX;
                goto end;
            }
            result->type = result->tab_entry->type;

            break;
        }
        case '"': { // String Literals
            result->type = STRING;

            int len = 0;

            bool done = false;
            do {
                NEXT_CHAR_NOERR(lexer->pp, cur);

                // Reached the end, an unescaped quote
                if (cur == '"' &&
                    (len == 0 || lexer->lexbuf[len - 1] != '\\')) {

                    // Concatenate strings. Skip until non whitespace character,
                    // then if we find another quote, skip that quote
                    do {
                        NEXT_CHAR_NOERR(lexer->pp, cur);
                        switch (cur) {
                        case ' ':
                        case '\t':
                            break;
                        default:
                            done = true;
                        }
                    } while(!done);
                    if (cur == '"') {
                        done = false;
                    } else {
                        lexer->next_char = cur;
                    }
                } else {
                    lexer->lexbuf[len++] = cur;
                }
            } while (!done && len < MAX_LEXEME_SIZE);

            if (!done) {
                logger_log(&result->mark, LOG_ERR, "String too long!");
                status = CCC_ESYNTAX;

                // Skip over the rest of the String
                while (!done) {
                    NEXT_CHAR_NOERR(lexer->pp, cur);
                    if (cur == PP_EOF) {
                        logger_log(&result->mark, LOG_ERR,
                                   "Unterminated String!");
                        status = CCC_ESYNTAX;
                        break;
                    }
                    if (cur == '"' &&
                        (len == 0 || lexer->lexbuf[len - 1] != '\\')) {
                        done = true;
                    }
                }
            }

            if (CCC_OK !=
                (status = st_lookup(lexer->symtab, lexer->lexbuf, len, STRING,
                                    &result->tab_entry))) {
                logger_log(&result->mark, LOG_ERR, "Failed to add String!");
                status = CCC_ESYNTAX;
                goto end;
            }

            break;
        }
        case '\'': { // Character literals
            result->type = INTLIT;

            NEXT_CHAR_NOERR(lexer->pp, cur);
            result->int_params.int_val = cur;
            result->int_params.hasU = false;
            result->int_params.hasL = false;
            result->int_params.hasLL = false;

            NEXT_CHAR_NOERR(lexer->pp, cur);

            if (cur != '\'') {
                logger_log(&result->mark, LOG_ERR,
                           "Unexpected junk in character literal");
                status = CCC_ESYNTAX;
            }

            // skip over junk in character literal
            while (cur != '\'') {
                NEXT_CHAR_NOERR(lexer->pp, cur);
            }
            break;
        }
        case ASCII_DIGIT:
            status = lex_number(lexer, cur, result);
            // Skip whitespace
            break;
        case '\n': case ' ': case '\t': case '\\':
            done = false;
            break;
        default:
            logger_log(&result->mark, LOG_ERR, "Unexpected character: %c", cur);
            status = CCC_ESYNTAX;
        } // switch (cur)
    } while (!done);

end:
    return status;
}

status_t lex_number(lexer_t *lexer, int cur, lexeme_t *result) {
    status_t status = CCC_OK;

    bool has_e = false;
    bool has_dot = false;
    bool has_f = false;
    bool has_u = false;
    bool has_l = false;
    bool has_ll = false;
    bool is_hex = false;

    size_t offset = 0;

    if (cur == '-') {
        lexer->lexbuf[offset++] = cur;
        cur = lexer->next_char;
    }

    int last = -1;
    bool done = false;
    bool err = false;
    while (!done && !err && offset < sizeof(lexer->lexbuf)) {
        switch (cur) {
        case 'e':
        case 'E':
            if (has_e) {
                err = true;
            }
            has_e = true;
            break;
        case '.':
            if (has_dot) {
                err = true;
            }
            has_dot = true;
            break;
        case 'f':
        case 'F':
            if (has_f || has_u || has_ll) {
                err = true;
            }
            has_f = true;
            break;
        case 'u':
        case 'U':
            if (has_f || has_u) {
                err = true;
            }
            has_u = true;
            break;
        case 'l':
        case 'L':
            if (has_f || has_u || has_ll || (has_l && cur != last)) {
                err = true;
            }
            if (has_l) {
                has_ll = true;
            }
            has_l = true;
            break;
        case 'x':
        case 'X':
            if (last == '0' &&
                (offset == 1 || (offset == 2 && lexer->lexbuf[0] == '-'))) {
                is_hex = true;
            } else {
                err = true;
            }
            break;
        case ASCII_DIGIT:
            if (has_f || has_u || has_l || has_ll) {
                err = true;
            }
            break;

            // _ and all letters other than X, U, L, E, F
        case 'a': case 'b': case 'c': case 'd': case 'g': case 'h':
        case 'i': case 'j': case 'k': case 'm': case 'n': case 'o': case 'p':
        case 'q': case 'r': case 's': case 't': case 'v': case 'w': case 'y':
        case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'G': case 'H':
        case 'I': case 'J': case 'K': case 'M': case 'N': case 'O': case 'P':
        case 'Q': case 'R': case 'S': case 'T': case 'V': case 'W': case 'Y':
        case 'Z':
        case '_':
            err = true;
            break;
        default:
            done = true;
        }
        if (!done) {
            last = cur;
            lexer->lexbuf[offset++] = cur;
            NEXT_CHAR_NOERR(lexer->pp, cur);
        } else {
            lexer->lexbuf[offset] = '\0';
        }
    }

    bool is_float = has_e || has_dot || has_f;

    if (!err && is_float && (has_u || has_ll || is_hex)) {
        err = true;
    }

    if (err || !done) {
        err = true;
        logger_log(&result->mark, LOG_ERR, "Invalid numeric literal");
        status = CCC_ESYNTAX;

        // Skip over junk at end (identifier characters)
        done = false;
        do {
            NEXT_CHAR_NOERR(lexer->pp, cur);
            switch (cur) {
            case ASCII_DIGIT:
            case ASCII_LOWER:
            case ASCII_UPPER:
            case '_':
                break;
            default:
                done = true;
            }
        } while(!done);
    }

    lexer->next_char = cur;
    if (err) {
        return status;
    }

    errno = 0;
    if (is_float) {
        result->type = FLOATLIT;
        result->float_params.hasF = has_f;
        result->float_params.hasL = has_l;
        if (has_f) {
            result->float_params.float_val = strtof(lexer->lexbuf, NULL);
        } else if (has_l) {
            result->float_params.float_val = strtold(lexer->lexbuf, NULL);
        } else {
            result->float_params.float_val = strtod(lexer->lexbuf, NULL);
        }
    } else {
        result->type = INTLIT;
        result->int_params.hasU = has_u;
        result->int_params.hasL = has_l;
        result->int_params.hasLL = has_ll;
        if (has_u) {
            if (has_l) {
                result->int_params.int_val = strtoul(lexer->lexbuf, NULL, 0);
            } else if (has_ll) {
                result->int_params.int_val = strtoull(lexer->lexbuf, NULL, 0);
            } else {
                result->int_params.int_val = strtoul(lexer->lexbuf, NULL, 0);
                if (result->int_params.int_val > UINT_MAX) {
                    errno = ERANGE;
                }
            }
        } else {
            if (has_l) {
                result->int_params.int_val = strtol(lexer->lexbuf, NULL, 0);
            } else if (has_ll) {
                result->int_params.int_val = strtoll(lexer->lexbuf, NULL, 0);
            } else {
                result->int_params.int_val = strtol(lexer->lexbuf, NULL, 0);
                if (result->int_params.int_val < INT_MIN ||
                    result->int_params.int_val > INT_MAX) {
                    errno = ERANGE;
                }
            }
        }
    }
    if (errno == ERANGE) {
        logger_log(&result->mark, LOG_ERR, "Overflow in numeric literal", cur);
        status = CCC_ESYNTAX;
    }

    return status;
}
