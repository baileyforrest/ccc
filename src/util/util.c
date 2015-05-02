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
 * Implementations of misc utilities
 */

#include "util.h"

#include <stdlib.h>
#include <assert.h>

#include "util/logger.h"
#include "util/string_store.h"

extern uint32_t ind_str_hash(const void *vstr);
extern bool ind_str_eq(const void *vstr1, const void *vstr2);
extern uint32_t len_str_hash(const void *vstr);
extern bool len_str_eq(const void *vstr1, const void *vstr2);

static const char *const mem_err = "out of memory, giving up";

char *ccc_basename(char *path) {
    size_t path_len = strlen(path);
    while (path_len > 0 && path[path_len - 1] != '/') {
        --path_len;
    }

    return path + path_len;
}

void *emalloc(size_t size) {
    void *result = malloc(size);
    if (result == NULL) {
        logger_log(NULL, LOG_ERR, mem_err);
        exit(EXIT_FAILURE);
    }
    return result;
}

void *ecalloc(size_t nmemb, size_t size) {
    void *result = calloc(nmemb, size);
    if (result == NULL) {
        logger_log(NULL, LOG_ERR, mem_err);
        exit(EXIT_FAILURE);
    }
    return result;
}

void *erealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    if (result == NULL) {
        logger_log(NULL, LOG_ERR, mem_err);
        exit(EXIT_FAILURE);
    }
    return result;
}

char *unescape_str(char *str) {
    if (strchr(str, '\\') == NULL) {
        return str;
    }

    // Will have slight over allocation depending on # of escapes
    char *unescaped = emalloc(strlen(str) + 1);
    char *dest = unescaped;

    // FSM
    typedef enum state_t {
        NORMAL,
        FOUND_FSLASH,
        OCT1,
        OCT2,
        HEX0,
        HEX1,
    } state_t;

    int val;
    state_t state = NORMAL;

    int cur;
    while ((cur = *(str++))) {
        switch (state) {
        case NORMAL:
            if (cur == '\\') {
                state = FOUND_FSLASH;
            } else {
                *(dest++) = cur;
            }
            break;
        case FOUND_FSLASH:
            switch (cur) {
            case 'a': state = NORMAL; *(dest++) = 0x07; break;
            case 'b': state = NORMAL; *(dest++) = 0x08; break;
            case 'f': state = NORMAL; *(dest++) = 0x0c; break;
            case 'n': state = NORMAL; *(dest++) = 0x0a; break;
            case 'r': state = NORMAL; *(dest++) = 0x0d; break;
            case 't': state = NORMAL; *(dest++) = 0x09; break;
            case 'v': state = NORMAL; *(dest++) = 0x0b; break;

            case 'x':
                state = HEX0;
                break;

            case OCT_DIGIT:
                val = cur - '0';
                state = OCT1;
                break;

            default:
                state = NORMAL;
                *(dest++) = cur;
            }
            break;
        case OCT1:
            switch (cur) {
            case OCT_DIGIT:
                val = val << 3 | (cur - '0');
                state = OCT2;
                break;
            default:
                *(dest++) = val;
                *(dest++) = cur;
                state = NORMAL;
                break;
            }
            break;
        case OCT2:
            switch (cur) {
            case OCT_DIGIT:
                val = val << 3 | (cur - '0');
                *(dest++) = val;
                break;
            default:
                *(dest++) = val;
                *(dest++) = cur;
                break;
            }
            state = NORMAL;
            break;
        case HEX0:
            switch (cur) {
            case HEX_DIGIT: {
                int hex_val;
                if (cur >= '0' && cur <= '9') {
                    hex_val = cur - '0';
                } else if (cur >= 'A' && cur <= 'F') {
                    hex_val = cur - 'A' + 10;
                } else { // cur >= 'a' && cur <= 'f'
                    hex_val = cur - 'a' + 10;
                }
                val = hex_val;
                break;
            }
            default:
                // Should have failed to lex
                assert(false);
            }
            state = HEX1;
            break;
        case HEX1:
            switch (cur) {
            case HEX_DIGIT: {
                int hex_val;
                if (cur >= '0' && cur <= '9') {
                    hex_val = cur - '0';
                } else if (cur >= 'A' && cur <= 'F') {
                    hex_val = cur - 'A' + 10;
                } else { // cur >= 'a' && cur <= 'f'
                    hex_val = cur - 'a' + 10;
                }
                val = val << 4 | hex_val;
                *(dest++) = val;
                break;
            }
            default:
                *(dest++) = val;
                *(dest++) = cur;
            }
            state = NORMAL;
            break;
        default:
            assert(false);
        }
    }
    *dest = '\0';

    return sstore_insert(unescaped);
}
