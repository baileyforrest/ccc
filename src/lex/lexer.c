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
 * Lexer implementation
 */
// TODO2: Support unicode literals
#include "lexer.h"
#include "lexer_priv.h"

#include <stdio.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <wchar.h>

#include "util/char_class.h"
#include "util/logger.h"
#include "util/string_store.h"

#define INIT_LEXBUF_SIZE 128

void lexer_init(lexer_t *lexer, token_man_t *token_man, symtab_t *symtab) {
    assert(lexer != NULL);
    assert(symtab != NULL);

    lexer->symtab = symtab;
    lexer->token_man = token_man;
    sb_init(&lexer->lexbuf, INIT_LEXBUF_SIZE);
}

void lexer_destroy(lexer_t *lexer) {
    assert(lexer != NULL);
    sb_destroy(&lexer->lexbuf);
}

status_t lexer_lex_stream(lexer_t *lexer, tstream_t *stream, vec_t *result) {
    assert(lexer != NULL);
    assert(stream != NULL);
    assert(result != NULL);
    status_t status = CCC_OK;

    token_t *last = NULL;
    while (ts_peek(stream) != EOF) {
        token_t *token = token_create(lexer->token_man);
        token->type = TOKEN_EOF;
        token->start = ts_pos(stream);

        if (CCC_OK != (status = lex_next_token(lexer, stream, token))) {
            return status;
        }
        token->len = ts_pos(stream) - token->start;

        // If we encounter two # in a row, combine them. This is necessary
        // to lex the %:%: digraph with only getc and ungetc operations
        if (token->type == HASH && last != NULL && last->type == HASH) {
            last->type = HASHHASH;
            continue;
        }

        vec_push_back(result, token);
        last = token;
    }

    return status;
}

int lex_if_next_eq(tstream_t *stream, int test, token_type_t noeq,
                   token_type_t iseq) {
    int next = lex_getc_splice(stream);
    if (next == test) {
        return iseq;
    }
    ts_ungetc(next, stream);
    return noeq;
}

int lex_getc_splice(tstream_t *stream) {
    while (true) {
        int cur = ts_getc(stream);
        if (cur != '\\') {
            return cur;
        }

        int next = ts_getc(stream);
        if (next != '\n') {
            ts_ungetc(next, stream);
            return cur;
        }
    }
}

status_t lex_next_token(lexer_t *lexer, tstream_t *stream, token_t *result) {
    assert(lexer != NULL);
    assert(stream != NULL);
    assert(result != NULL);

    status_t status = CCC_OK;

    memcpy(&result->mark, &stream->mark, sizeof(fmark_t));
    int cur = lex_getc_splice(stream);

    // Combine spaces
    if (isspace(cur) && cur != '\n') {
        while (isspace(cur) && cur != '\n') {
            cur = lex_getc_splice(stream);
        }

        ts_ungetc(cur, stream);

        result->type = SPACE;
        return status;
    }

    int next;
    switch (cur) {
    case '\n': result->type = NEWLINE; break;
    case '{': result->type = LBRACE; break;
    case '}': result->type = RBRACE; break;
    case '(': result->type = LPAREN; break;
    case ')': result->type = RPAREN; break;
    case ';': result->type = SEMI; break;
    case ',': result->type = COMMA; break;
    case '[': result->type = LBRACK; break;
    case ']': result->type = RBRACK; break;
    case '?': result->type = COND; break;
    case '~': result->type = BITNOT; break;
    case '=': result->type = lex_if_next_eq(stream, '=', ASSIGN, EQ); break;
    case '*': result->type = lex_if_next_eq(stream, '=', STAR, STAREQ); break;
    case '!': result->type = lex_if_next_eq(stream, '=', LOGICNOT, NE); break;
    case '^': result->type = lex_if_next_eq(stream, '=', BITXOR, BITXOREQ);
        break;
        // digraph: :> = ]
    case ':': result->type = lex_if_next_eq(stream, '>', COLON, RBRACK); break;
    case '#': result->type = lex_if_next_eq(stream, '#', HASH, HASHHASH); break;

    case '/': {
        next = lex_getc_splice(stream);
        switch (next) {
            // Single line comment
        case '/':
            while ((next = lex_getc_splice(stream)) != '\n')
                continue;
            result->type = SPACE;
            break;

            // Multi line comment
        case '*': {
            while (true) {
                if (lex_getc_splice(stream) == '*' &&
                    lex_getc_splice(stream) == '/') {
                    break;
                }
            }
            result->type = SPACE;
            break;
        }

        case '=': result->type = DIVEQ; break;
        default:
            ts_ungetc(next, stream);
            result->type = DIV;
        }
        break;
    }
    case '.': {
        next = lex_getc_splice(stream);
        bool done = false;
        switch (next) {
        case ASCII_DIGIT:
            ts_ungetc(next, stream);
            status = lex_number(lexer, stream, cur, result);
            done = true;
            break;
        case '.':
            break;
        default:
            ts_ungetc(next, stream);
            result->type = DOT;
            done = true;
            break;
        }
        if (done) {
            break;
        }

        next = lex_getc_splice(stream);
        if (next == '.') {
            result->type = ELIPSE;
            break;
        }
        logger_log(&result->mark, LOG_ERR, "Invalid token: ..");
        status = CCC_ESYNTAX;
        break;
    }
    case '%':
        next = lex_getc_splice(stream);
        switch (next) {
        case '=': result->type = MODEQ; break;
        case '>': result->type = RBRACE; break; // digraph %> = }
        case ':': result->type = HASH; break; // digraph %: = #
        default:
            ts_ungetc(next, stream);
            result->type = MOD;
        }
        break;
    case '+': {
        next = lex_getc_splice(stream);
        switch(next) {
        case '+': result->type = INC; break;
        case '=': result->type = PLUSEQ; break;
        default:
            ts_ungetc(next, stream);
            result->type = PLUS;
        }
        break;
    }
    case '-': {
        next = lex_getc_splice(stream);
        switch(next) {
        case '-': result->type = DEC; break;
        case '=': result->type = MINUSEQ; break;
        case '>': result->type = DEREF; break;
        default:
            ts_ungetc(next, stream);
            result->type = MINUS;
        }
        break;
    }
    case '|': {
        next = lex_getc_splice(stream);
        switch(next) {
        case '|': result->type = LOGICOR; break;
        case '=': result->type = BITOREQ; break;
        default:
            ts_ungetc(next, stream);
            result->type = BITOR;
            break;
        }
        break;
    }
    case '&': {
        next = lex_getc_splice(stream);
        switch(next) {
        case '&': result->type = LOGICAND; break;
        case '=': result->type = BITANDEQ; break;
        default:
            ts_ungetc(next, stream);
            result->type = BITAND;
            break;
        }
        break;
    }
    case '>': {
        next = lex_getc_splice(stream);
        switch (next) {
        case '=': result->type = GE; break;
        case '>': result->type = lex_if_next_eq(stream, '=', RSHIFT, RSHIFTEQ);
            break;
        default:
            ts_ungetc(next, stream);
            result->type = GT;
        }
        break;
    }
    case '<': {
        next = lex_getc_splice(stream);
        switch (next) {
        case '=': result->type = LE; break;
        case ':': result->type = LBRACK; break; // digraph <: = [
        case '%': result->type = LBRACE; break; // digraph <% = {
        case '<': result->type = lex_if_next_eq(stream, '=', LSHIFT, LSHIFTEQ);
            break;
        default:
            ts_ungetc(next, stream);
            result->type = LT;
        }
        break;
    }

    case 'L':
        next = lex_getc_splice(stream);
        switch (next) {
        case '"':
            cur = next;
            status = lex_string(lexer, stream, result, LEX_STR_LCHAR);
            break;
        case '\'':
            cur = next;
            status = lex_char_lit(lexer, stream, result, LEX_STR_LCHAR);
            break;
        default:
            ts_ungetc(next, stream);
            status = lex_id(lexer, stream, result);
        }
        break;
    case 'U':
        /*
          next = lex_getc_splice(stream);
          switch (next) {
          case '"':
          break;
          case '\'':
          break;
          default:
          ts_ungetc(next, stream);
          status = lex_id(lexer, cur, result);
          }
          break;
        */
    case 'u':
        /*
          next = lex_getc_splice(stream);
          switch (next) {
          case '"':
          break;
          case '\'':
          break;
          case '8':
          break;
          default:
          ts_ungetc(next, stream);
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

    case '$':
    case '_':
        ts_ungetc(cur, stream);
        status = lex_id(lexer, stream, result);
        break;

    case '"': // String Literals
        status = lex_string(lexer, stream, result, LEX_STR_CHAR);
        break;
    case '\'': // Character literals
        status = lex_char_lit(lexer, stream, result, LEX_STR_CHAR);
        break;
    case ASCII_DIGIT:
        status = lex_number(lexer, stream, cur, result);
        break;
    default:
        logger_log(&result->mark, LOG_ERR, "Unexpected character: %c", cur);
        status = CCC_ESYNTAX;
    }

    return status;
}

status_t lex_id(lexer_t *lexer, tstream_t *stream, token_t *result) {
    status_t status = CCC_OK;
    sb_clear(&lexer->lexbuf);

    bool done = false;
    while (!done) {
        int cur = lex_getc_splice(stream);

        switch (cur) {
        case ID_CHARS:
            sb_append_char(&lexer->lexbuf, cur);
            break;
        default:
            ts_ungetc(cur, stream);
            done = true;
        }
    }

    symtab_entry_t *entry =
        st_lookup(lexer->symtab, sb_buf(&lexer->lexbuf), ID);
    result->id_name = entry->key;
    result->type = entry->type;

    return status;
}

char32_t lex_single_char(lexer_t *lexer, tstream_t *stream,
                         lex_str_type_t type) {
    int cur = lex_getc_splice(stream);
    if (cur != '\\') {
        return cur;
    }
    bool is_oct = false;

    cur = lex_getc_splice(stream);
    sb_clear(&lexer->lexbuf);

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
        sb_append_char(&lexer->lexbuf, '0');
        if (!is_oct) {
            sb_append_char(&lexer->lexbuf, 'x');
        }

        bool overflow = false;
        bool done = false;
        do {
            cur = lex_getc_splice(stream);
            switch (cur) {
            case '8': case '9':
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                if (is_oct) {
                    done = true;
                    break;
                }
            case OCT_DIGIT:
                sb_append_char(&lexer->lexbuf, cur);
                break;
            default:
                done = true;
            }
        } while(!done);
        ts_ungetc(cur, stream);

        errno = 0;

        long long hexnum = strtol(sb_buf(&lexer->lexbuf), NULL, 0);
        if (errno == ERANGE) {
            overflow = true;
        }
        switch (type) {
        case LEX_STR_CHAR:
            overflow |= hexnum > UCHAR_MAX;
            break;
        case LEX_STR_LCHAR:
            overflow |= hexnum > (long long)WCHAR_MAX - (long long)WCHAR_MIN;
            break;
        case LEX_STR_U8:
            overflow |= hexnum > UINT8_MAX;
            break;
        case LEX_STR_U16:
            overflow |= hexnum > UINT16_MAX;
            break;
        case LEX_STR_U32:
            overflow |= hexnum > UINT32_MAX;
            break;
        }

        if (overflow) {
            if (is_oct) {
                logger_log(&stream->mark, LOG_WARN,
                           "Overflow in character constant '\\%s'",
                           sb_buf(&lexer->lexbuf));
            } else {
                logger_log(&stream->mark, LOG_WARN,
                           "Overflow in character constant '\\x%s'",
                           sb_buf(&lexer->lexbuf));
            }
        }
        return hexnum;
    }

    case 'u':
        // TODO2: Handle utf8
    case 'U':
        // TODO2: Handle utf16

    default:
        logger_log(&stream->mark, LOG_WARN,
                   "Unknown escape sequence: '\\%c'", cur);
        return cur;
    }
}

status_t lex_char_lit(lexer_t *lexer, tstream_t *stream, token_t *result,
                      lex_str_type_t type) {
    status_t status = CCC_OK;
    result->type = INTLIT;
    result->int_params = emalloc(sizeof(token_int_params_t));
    result->int_params->hasU = false;
    result->int_params->hasL = false;
    result->int_params->hasLL = false;

    result->int_params->int_val = lex_single_char(lexer, stream, type);

    int cur = lex_getc_splice(stream);
    if (cur != '\'') {
        logger_log(&result->mark, LOG_ERR,
                   "Unexpected junk in character literal");
        status = CCC_ESYNTAX;
    }

    // skip over junk in character literal
    while (cur != '\'' && cur != EOF) {
        cur = lex_getc_splice(stream);
    }

    return status;
}

status_t lex_string(lexer_t *lexer, tstream_t *stream, token_t *result,
                    lex_str_type_t type) {
    // TODO2: Make sure wide character literals take up more space
    (void)type;
    status_t status = CCC_OK;
    result->type = STRING;

    sb_clear(&lexer->lexbuf);

    bool done = false;
    while (!done) {
        int cur = lex_getc_splice(stream);
        if (cur == EOF) {
            break;
        }

        // Reached the end, an unescaped quote
        if (cur == '"' &&
            (sb_len(&lexer->lexbuf) == 0 ||
             sb_buf(&lexer->lexbuf)[sb_len(&lexer->lexbuf) - 1] != '\\')) {

            // Concatenate strings. Skip until non whitespace character,
            // then if we find another quote, skip that quote
            do {
                cur = lex_getc_splice(stream);
                if (!isspace(cur) || cur == '\n') {
                    done = true;
                }
            } while(!done);
            if (cur == '"') {
                done = false;
            } else {
                ts_ungetc(cur, stream);
            }
        } else {
            sb_append_char(&lexer->lexbuf, cur);
        }
    }

    result->str_val = sstore_lookup(sb_buf(&lexer->lexbuf));

    return status;
}

status_t lex_number(lexer_t *lexer, tstream_t *stream, int cur,
                    token_t *result) {
    status_t status = CCC_OK;

    bool has_e = false;
    bool has_f = false;
    bool has_u = false;
    bool has_l = false;
    bool has_ll = false;
    bool is_hex = false;

    size_t dot_off = (size_t)-1;
    size_t p_off = (size_t)-1;

    int last = -1;
    bool done = false;
    bool err = false;

    sb_clear(&lexer->lexbuf);
    while (!done && !err) {
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
            if (dot_off != (size_t)-1) {
                err = true;
            }
            dot_off = sb_len(&lexer->lexbuf);
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
            if (last == '0' && (sb_len(&lexer->lexbuf) == 1)) {
                is_hex = true;
            } else {
                err = true;
            }
            break;
        case 'p':
        case 'P':
            if (p_off != (size_t)-1) {
                err = true;
            }

            p_off = sb_len(&lexer->lexbuf);
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
        case '+':
        case '-':
            if (last == 'e' || last == 'E') {
                break;
            }
            // FALL THROUGH
        default:
            done = true;
        }
        if (!done) {
            last = cur;
            sb_append_char(&lexer->lexbuf, cur);
            cur = lex_getc_splice(stream);
        }
    }

    bool is_float = has_e || (dot_off != (size_t)-1) || has_f;

    if ((is_float && (has_u || has_ll || (is_hex && p_off == (size_t)-1))) ||
        (!is_float && p_off != (size_t)-1)) {
        err = true;
    }

    if (err || !done) {
        err = true;
        logger_log(&result->mark, LOG_ERR, "Invalid numeric literal");
        status = CCC_ESYNTAX;

        // Skip over junk at end (identifier characters)
        done = false;
        do {
            cur = lex_getc_splice(stream);
            switch (cur) {
            case ID_CHARS:
                break;
            default:
                done = true;
            }
        } while(!done);
    }
    ts_ungetc(cur, stream);

    if (err) {
        return status;
    }

    char *buf = sb_buf(&lexer->lexbuf);
    char *end;
    errno = 0;
    if (is_float && is_hex) {
        assert(p_off != (size_t)-1 && dot_off != (size_t)-1);

        char *dot_loc = buf + dot_off;
        char *p_loc = buf + p_off;
        result->type = FLOATLIT;
        result->float_params = emalloc(sizeof(token_float_params_t));
        result->float_params->hasF = has_f;
        result->float_params->hasL = has_l;
        result->float_params->float_val = 0;

        long long head;
        if (dot_off == 2) {
            // Account for the case 0x.NNNpN
            head = 0;
            end = dot_loc;
        } else {
            head = strtoll(buf, &end, 16);
        }
        if (errno == 0 && end == dot_loc) {
            long long frac;
            if (dot_loc + 1 == p_loc) {
                // account for 0xNN.pN
                frac = 0;
                end = p_loc;
            } else {
                frac = strtoll(dot_loc + 1, &end, 16);
            }
            if (errno == 0 && end == p_loc) {
                ptrdiff_t digits = end - (dot_loc + 1);

                long long exp = strtoll(p_loc + 1, &end, 10);
                if (errno == 0) {

                    long double dfrac = frac;

                    // Each hex digit is 4 bits of precision
                    dfrac /= pow(2.0, digits * 4);

                    long double val = (long double)head + dfrac;

                    val *= powl(2.0, exp);
                    result->float_params->float_val = val;
                }
            }
        }
    } else if (is_float) {
        result->type = FLOATLIT;
        result->float_params = emalloc(sizeof(token_float_params_t));
        result->float_params->hasF = has_f;
        result->float_params->hasL = has_l;
        result->float_params->float_val = strtold(buf, &end);
    } else {
        result->type = INTLIT;
        result->int_params = emalloc(sizeof(token_int_params_t));
        result->int_params->hasU = has_u;
        result->int_params->hasL = has_l;
        result->int_params->hasLL = has_ll;
        result->int_params->int_val = strtoull(buf, &end, 0);
    }
    if (errno == ERANGE) {
        logger_log(&result->mark, LOG_WARN, "Overflow in numeric literal", cur);
    }

    // End is allowed to be NULL or an integral literal suffix
    switch (*end) {
    case '\0':
    case 'l':
    case 'L':
    case 'U':
    case 'u':
    case 'f':
    case 'F':
        break;
    default:
        logger_log(&result->mark, LOG_ERR, "Invalid integral constant", cur);
        status = CCC_ESYNTAX;
    }

    return status;
}
