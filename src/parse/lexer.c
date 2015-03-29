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

// TODO: Change this to use strtoll, strtod family of functions
status_t lex_number(lexer_t *lexer, int cur, lexeme_t *result) {
    status_t status = CCC_OK;

    // Unsigned so modular overflow is defined
    unsigned long long accum = 0;
    unsigned bits = 0;

    // 0 is handled separately for hex/octal
    switch (cur) {
    case '0': {
        NEXT_CHAR_NOERR(lexer->pp, cur);
        switch(cur) {
        case 'x':
        case 'X': { // Hexidecmal
            bool done = false;
            do {
                NEXT_CHAR_NOERR(lexer->pp, cur);
                switch (cur) {
                case ASCII_DIGIT:
                    accum = accum << 4 | (cur - '0');
                    break;
                case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                    accum = accum << 4 | (cur - 'a' + 10);
                    break;
                case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                    accum = accum << 4 | (cur - 'A' + 10);
                    break;
                default:
                    done = true;
                }
                if (!done) {
                    bits += 4;
                }
            } while (!done);
            break;
        }
        case '0': case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': { // Octal
            bool done = false;
            do {
                switch (cur) {
                case '0': case '1': case '2': case '3': case '4': case '5':
                case '6': case '7':
                    accum = accum << 3 | (cur - '0');
                    break;
                case '8': case '9': case '.': case 'e': case 'E':
                    goto handle_float;
                default:
                    done = true;
                }
                if (!done) {
                    bits += 3;
                    NEXT_CHAR_NOERR(lexer->pp, cur);
                }
            } while (!done);
            break;
        }
            // Floats can have a leading 0
        case '8': case '9': case '.': case 'e': case 'E':
            goto handle_float;
        } // switch (cur)
    }
        // Try to parse as a decimal number
    case '1': case '2': case '3': case '4': case '5': case '6': case '7':
    case '8': case '9': {
        bool done = false;
        do {
            switch (cur) {
            case ASCII_DIGIT: {
                unsigned long long last = accum;
                accum = accum * 10 + (cur - '0');
                if (accum < last) {
                    // Overflow, just add bits to mark it
                    bits += sizeof(long long) * 8;
                }
                break;
            }
            case '.': case 'e': case 'E':
                goto handle_float;
            default:
                done = true;
            }
            if (!done) {
                NEXT_CHAR_NOERR(lexer->pp, cur);
            }
        } while(!done);
        break;
    }
    default:
        // Can't end up here
        assert(false);
    } // case (cur)

    // Finished parsing body of int

    bool hasU = false;
    bool hasL = false;
    bool hasLL = false;
    bool junk = false;
    bool done = false;
    int offset = 0;
    // Scan characters at end
    do {
        lexer->lexbuf[offset++] = cur;
        switch (cur) {
        case 'U':
        case 'u':
            if (hasU) { // Can't repeat suffix
                junk = true;
                break;
            }
            hasU = true;
            break;
        case 'L':
        case 'l':
            if (hasL) {
                // Don't need to check 0 because if hasL is true, offset
                // must be greater than 1
                if (lexer->lexbuf[offset - 2] != cur) {
                    junk = true;
                } else {
                    hasLL = true;
                }
                break;
            }
            hasL = true;
            break;
            // _ and all letters other than U, L, E
        case 'a': case 'b': case 'c': case 'd': case 'f':
        case 'g': case 'h': case 'i': case 'j': case 'k': case 'm':
        case 'n': case 'o': case 'p': case 'q': case 'r': case 's':
        case 't': case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'M':
        case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S':
        case 'T': case 'V': case 'W': case 'X': case 'Y': case 'Z':
        case '_':
            junk = true;
            done = true;
            break;
        default:
            done = true;
        } // switch (cur)

        if (!done) {
            NEXT_CHAR_NOERR(lexer->pp, cur);
        }
    } while(!done);

    if (junk) {
        logger_log(&result->mark, LOG_ERR,
                   "Unexpected junk in integer literal");
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

    if (bits && bits > sizeof(long long) * 8) {
        logger_log(&result->mark, LOG_WARN, "Integer constant too large");
    }

    lexer->next_char = cur;

    result->type = INTLIT;
    result->int_params.hasU = hasU;
    result->int_params.hasL = hasL;
    result->int_params.hasLL = hasLL;
    result->int_params.int_val = accum;
    return status;

handle_float:
    ; // Empty statement to allow compilation

    double double_accum = (double)accum;
    double frac_val = 0.1;
    bool decimal_point = false;

    bool has_exp = false;
    bool exp_neg = false;
    int exp = 0;

    bool hasF = false;

    do {
        switch(cur) {
        case '.':
            if (decimal_point || exp != 0) {
                junk = true;
                done = true;
                break;
            }
            decimal_point = true;
            break;
        case 'E':
        case 'e':
            if (has_exp) {
                junk = true;
                done = true;
                break;
            }
            break;
        case '-': { // Negative exponent
            if (!has_exp || exp != 0) {
                junk = true;
                done = true;
                break;
            }
            exp_neg = true;
            break;
        }
        case 'F': // F suffix
        case 'f': {
            hasF = true;
            done = true;
            break;
        }
        case ASCII_DIGIT: {
            if (!decimal_point) { // Accum integral part
                double_accum = double_accum * 10 + (cur - '0');
            } else if (!exp) { // Accum fractional part
                double_accum = double_accum + (cur - '0') * frac_val;
                frac_val = frac_val / 10.0;
            } else { // Accumulate exponent
                exp = exp * 10 + (cur - '0');
            }
        }
            // _ and any letter other than E or F
        case 'a': case 'b': case 'c': case 'd': case 'g': case 'h':
        case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
        case 'o': case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'G': case 'H':
        case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
        case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
        case '_':
            junk = true;
            done = true;
            break;
        default:
            done = true;
        } // switch (cur)
        if (!done) {
            NEXT_CHAR_NOERR(lexer->pp, cur);
        }
    } while(!done);

    exp = exp_neg ? -exp : exp;

    if (junk) {
        logger_log(&result->mark, LOG_ERR,
                   "Unexpected junk in floating point literal");
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

    result->type = FLOATLIT;
    result->float_params.hasF = hasF;
    result->float_params.float_val = double_accum * pow(10.0, exp);

    return status;
}
