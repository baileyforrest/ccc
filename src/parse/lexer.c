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
// TODO2: Support unicode literals
#include "lexer.h"

#include <stdio.h>

#include <assert.h>
#include <ctype.h>
#include <uchar.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <wchar.h>

#include "util/logger.h"

/** Initial size of lexer internal buffer */
#define INIT_LEXEME_SIZE 512

/** 1.5 Growth rate */
#define LEXBUF_NEW_SIZE(lexer) (lexer->lexbuf_size + (lexer->lexbuf_size >> 1))

#define LEXBUF_TEST_GROW(lexer, len)                                    \
    do {                                                                \
        if (len == lexer->lexbuf_size) {                                \
            lexer->lexbuf_size = LEXBUF_NEW_SIZE(lexer);                \
            lexer->lexbuf = erealloc(lexer->lexbuf, lexer->lexbuf_size); \
        }                                                               \
    } while (0)

/**
 * Gets next character from preprocessor ignoring errors
 *
 * @param lexer Lexer to get characters from
 * @param dest Variable to assign to
 */
#define NEXT_CHAR_NOERR(lexer, dest)            \
    do {                                        \
        if (lexer->next_char) {                 \
            dest = lexer->next_char;            \
            lexer->next_char = 0;               \
        } else {                                \
            dest = pp_nextchar(lexer->pp);      \
        }                                       \
    } while (dest < 0)

typedef enum lex_str_type_t {
    LEX_STR_CHAR,
    LEX_STR_LCHAR,
    LEX_STR_U8,
    LEX_STR_U16,
    LEX_STR_U32,
} lex_str_type_t;

static status_t lex_id(lexer_t *lexer, int cur, lexeme_t *result);

static char32_t lex_single_char(lexer_t *lexer, int cur, lex_str_type_t type);

static status_t lex_char(lexer_t *lexer, int cur, lexeme_t *result,
                         lex_str_type_t type);

static status_t lex_string(lexer_t *lexer, int cur, lexeme_t *result,
                           lex_str_type_t type);


/**
 * Lex a number value
 *
 * @param lexer Lexer to use
 * @param neg Whether or not the number has a leading hypthen
 * @param cur First input character
 * @param result Location to store the result
 */
static status_t lex_number(lexer_t *lexer, bool neg, int cur, lexeme_t *result);


void lexer_init(lexer_t *lexer, preprocessor_t *pp, symtab_t *symtab,
                    symtab_t *string_tab) {
    assert(lexer != NULL);
    assert(pp != NULL);
    assert(symtab != NULL);
    assert(string_tab != NULL);

    lexer->pp = pp;
    lexer->symtab = symtab;
    lexer->string_tab = string_tab;
    lexer->next_char = 0;
    lexer->lexbuf = emalloc(INIT_LEXEME_SIZE);
    lexer->lexbuf_size = INIT_LEXEME_SIZE;
}

void lexer_destroy(lexer_t *lexer) {
    assert(lexer != NULL);
    free(lexer->lexbuf);
}

#define CHECK_NEXT_EQ(noeq, iseq)               \
    do {                                        \
        NEXT_CHAR_NOERR(lexer, next);       \
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

    do { // Skip spaces and backlash
        NEXT_CHAR_NOERR(lexer, cur);
        if (!(isspace(cur) || cur == '\\')) {
            break;
        }
    } while (true);

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
        NEXT_CHAR_NOERR(lexer, next);
        if (next != '.') {
            result->type = DOT;
            lexer->next_char = next;
            break;
        }
        NEXT_CHAR_NOERR(lexer, next);
        if (next == '.') {
            result->type = ELIPSE;
            break;
        }
        logger_log(&result->mark, LOG_ERR, "Unexpected token: ..");
        status = CCC_ESYNTAX;
        break;
    }
    case '+': {
        NEXT_CHAR_NOERR(lexer, next);
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
        NEXT_CHAR_NOERR(lexer, next);
        switch(next) {
        case '-': result->type = DEC; break;
        case '=': result->type = MINUSEQ; break;
        case '>': result->type = DEREF; break;
            /* TODO0: Handle negatives elsewhere
        case ASCII_DIGIT: // Negative number
            status = lex_number(lexer, true, next, result);
            break;
            */
        default:
            result->type = MINUS;
            lexer->next_char = next;
        }
        break;
    }
    case '|': {
        NEXT_CHAR_NOERR(lexer, next);
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
        NEXT_CHAR_NOERR(lexer, next);
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
        NEXT_CHAR_NOERR(lexer, next);
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
        NEXT_CHAR_NOERR(lexer, next);
        switch (next) {
        case '=': result->type = LE; break;
        case '<': CHECK_NEXT_EQ(LSHIFT, LSHIFTEQ); break;
        default:
            result->type = LT;
            lexer->next_char = next;
        }
        break;
    }

    case 'L':
        NEXT_CHAR_NOERR(lexer, next);
        switch (next) {
        case '"':
            cur = next;
            status = lex_string(lexer, cur, result, LEX_STR_LCHAR);
            break;
        case '\'':
            cur = next;
            status = lex_char(lexer, cur, result, LEX_STR_LCHAR);
            break;
        default:
            lexer->next_char = next;
            status = lex_id(lexer, cur, result);
        }
        break;
    case 'U':
        /*
          NEXT_CHAR_NOERR(lexer, next);
          switch (next) {
          case '"':
          break;
          case '\'':
          break;
          default:
          lexer->next_char = next;
          status = lex_id(lexer, cur, result);
          }
          break;
        */
    case 'u':
        /*
          NEXT_CHAR_NOERR(lexer, next);
          switch (next) {
          case '"':
          break;
          case '\'':
          break;
          case '8':
          break;
          default:
          lexer->next_char = next;
          status = lex_id(lexer, cur, result);
          }
          break;
        */
    // Identifiers
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': case 'M': case 'N': case 'O':
    case 'P': case 'Q': case 'R': case 'S': case 'T': case 'V': case 'W':
    case 'X': case 'Y': case 'Z':

    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
    case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'v':
    case 'w': case 'x': case 'y': case 'z':

    case '_':
        status = lex_id(lexer, cur, result);
        break;

    case '"': // String Literals
        status = lex_string(lexer, cur, result, LEX_STR_CHAR);
        break;
    case '\'': // Character literals
        status = lex_char(lexer, cur, result, LEX_STR_CHAR);
        break;
    case ASCII_DIGIT:
        status = lex_number(lexer, false, cur, result);
        // Skip whitespace
        break;
    default:
        logger_log(&result->mark, LOG_ERR, "Unexpected character: %c", cur);
        status = CCC_ESYNTAX;
    } // switch (cur)

    return status;
}

static status_t lex_id(lexer_t *lexer, int cur, lexeme_t *result) {
    status_t status = CCC_OK;
    result->type = ID;

    size_t len = 0;
    lexer->lexbuf[len++] = cur;

    bool done = false;
    while (!done) {
        LEXBUF_TEST_GROW(lexer, len);

        NEXT_CHAR_NOERR(lexer, cur);
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
            NEXT_CHAR_NOERR(lexer, cur);
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
    lexer->lexbuf[len] = '\0';

    if (CCC_OK !=
        (status = st_lookup(lexer->symtab, lexer->lexbuf, ID,
                            &result->tab_entry))) {
        logger_log(&result->mark, LOG_ERR, "Failed to add identifier!");
        status = CCC_ESYNTAX;
        goto fail;
    }
    result->type = result->tab_entry->type;

fail:
    return status;
}

static char32_t lex_single_char(lexer_t *lexer, int cur, lex_str_type_t type) {
    if (cur != '\\') {
        return cur;
    }
    bool is_oct = false;
    NEXT_CHAR_NOERR(lexer, cur);
    switch (cur) {
    case 'a':  return '\a';
    case 'b':  return '\b';
    case 'f':  return '\f';
    case 'n':  return '\n';
    case 'r':  return '\r';
    case 't':  return '\t';
    case 'v':  return '\v';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"':  return '\"';
    case '?':  return '\?';

        // Hex /oct digit
    case OCT_DIGIT:
        is_oct = true;
        // FALL THROUGH
    case 'x': {
        size_t offset = 0;
        lexer->lexbuf[offset++] = '0';
        if (!is_oct) {
            lexer->lexbuf[offset++] = 'x';
        }

        bool overflow = false;
        bool done = false;
        do {
            NEXT_CHAR_NOERR(lexer, cur);
            switch (cur) {
            case '8': case '9':
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                if (!is_oct) {
                    lexer->next_char = cur;
                    done = true;
                    break;
                }
            case OCT_DIGIT:
                LEXBUF_TEST_GROW(lexer, offset);
                lexer->lexbuf[offset++] = cur;
                break;
            default:
                lexer->next_char = cur;
                done = true;
            }
        } while(!done);
        lexer->lexbuf[offset] = '\0';

        errno = 0;

        long long hexnum = strtol(lexer->lexbuf, NULL, 0);
        if (errno == ERANGE) {
            overflow = true;
        }
        switch (type) {
        case LEX_STR_CHAR:
            overflow = hexnum > UCHAR_MAX;
            break;
        case LEX_STR_LCHAR:
            overflow = hexnum > (long long)WCHAR_MAX - (long long)WCHAR_MIN;
            break;
        case LEX_STR_U8:
            overflow = hexnum > UINT8_MAX;
            break;
        case LEX_STR_U16:
            overflow = hexnum > UINT16_MAX;
            break;
        case LEX_STR_U32:
            overflow = hexnum > UINT32_MAX;
            break;
        }

        if (overflow) {
            if (is_oct) {
                logger_log(&lexer->pp->last_mark, LOG_WARN,
                           "Overflow in character constant '\\%s'",
                           lexer->lexbuf + 1);
            } else {
                logger_log(&lexer->pp->last_mark, LOG_WARN,
                           "Overflow in character constant '\\x%s'",
                           lexer->lexbuf + 2);
            }
        }
        return hexnum;
    }

    case 'u':
        // TODO2: Handle utf8
    case 'U':
        // TODO2: Handle utf16

    default:
        logger_log(&lexer->pp->last_mark, LOG_WARN,
                   "Unknown escape sequence: '\\%c'", cur);
        return cur;
    }
}

static status_t lex_char(lexer_t *lexer, int cur, lexeme_t *result,
                         lex_str_type_t type) {
    status_t status = CCC_OK;
    result->type = INTLIT;

    NEXT_CHAR_NOERR(lexer, cur);
    result->int_params.hasU = false;
    result->int_params.hasL = false;
    result->int_params.hasLL = false;

    result->int_params.int_val = lex_single_char(lexer, cur, type);

    NEXT_CHAR_NOERR(lexer, cur);
    if (cur != '\'') {
        logger_log(&result->mark, LOG_ERR,
                   "Unexpected junk in character literal");
        status = CCC_ESYNTAX;
    }

    // skip over junk in character literal
    while (cur != '\'') {
        NEXT_CHAR_NOERR(lexer, cur);
    }

    return status;
}

static status_t lex_string(lexer_t *lexer, int cur, lexeme_t *result,
                           lex_str_type_t type) {
    // TODO2: Make sure wide character literals take up more space
    (void)type;
    status_t status = CCC_OK;
    result->type = STRING;

    size_t len = 0;

    bool done = false;
    do {
        LEXBUF_TEST_GROW(lexer, len);

        NEXT_CHAR_NOERR(lexer, cur);

        // Reached the end, an unescaped quote
        if (cur == '"' &&
            (len == 0 || lexer->lexbuf[len - 1] != '\\')) {

            // Concatenate strings. Skip until non whitespace character,
            // then if we find another quote, skip that quote
            do {
                NEXT_CHAR_NOERR(lexer, cur);
                if (!isspace(cur)) {
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
    } while (!done);

    if (!done) {
        logger_log(&result->mark, LOG_ERR, "String too long!");
        status = CCC_ESYNTAX;

        // Skip over the rest of the String
        while (!done) {
            NEXT_CHAR_NOERR(lexer, cur);
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
    lexer->lexbuf[len] = '\0';

    if (CCC_OK !=
        (status = st_lookup(lexer->symtab, lexer->lexbuf, STRING,
                            &result->tab_entry))) {
        logger_log(&result->mark, LOG_ERR, "Failed to add String!");
        status = CCC_ESYNTAX;
        goto fail;
    }

fail:
    return status;
}


static status_t lex_number(lexer_t *lexer, bool neg, int cur,
                           lexeme_t *result) {
    status_t status = CCC_OK;

    bool has_e = false;
    bool has_dot = false;
    bool has_f = false;
    bool has_u = false;
    bool has_l = false;
    bool has_ll = false;
    bool is_hex = false;

    size_t offset = 0;

    if (neg) {
        lexer->lexbuf[offset++] = '-';
    }

    int last = -1;
    bool done = false;
    bool err = false;
    while (!done && !err) {
        LEXBUF_TEST_GROW(lexer, offset);
        switch (cur) {
        case 'e':
        case 'E':
            if (!is_hex) {
                if (has_e) {
                    err = true;
                }
                has_e = true;
            }
            break;
        case '.':
            if (has_dot) {
                err = true;
            }
            has_dot = true;
            break;
        case 'f':
        case 'F':
            if (!is_hex) {
                if (has_f || has_u || has_ll) {
                    err = true;
                }
                has_f = true;
            }
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
            if (has_f || has_ll || (has_l && cur != last)) {
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
        case 'a': case 'b': case 'c': case 'd':
        case 'A': case 'B': case 'C': case 'D':
            if (!is_hex) {
                err = true;
            }
            break;

            // _ and all letters other than X, U, L, E, F and hex letters
        case 'g': case 'h':
        case 'i': case 'j': case 'k': case 'm': case 'n': case 'o': case 'p':
        case 'q': case 'r': case 's': case 't': case 'v': case 'w': case 'y':
        case 'z':
        case 'G': case 'H':
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
            NEXT_CHAR_NOERR(lexer, cur);
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
            NEXT_CHAR_NOERR(lexer, cur);
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

                // Sign extend literal if necessary
                if (result->int_params.int_val &
                    (1 << (sizeof(int) * CHAR_BIT - 1))) {
                    result->int_params.int_val |=
                        ~0LL << (sizeof(int) * CHAR_BIT - 1);
                }
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
