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
 * Misc Utilities
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#include "util/slist.h"

#define PTR_SIZE (sizeof(void *));
#define PTR_ALIGN (alignof(void *));

/**
 * Length of static array
 *
 * @param Array to get length of
 */
#define STATIC_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * max macro
 */
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif /* MAX */

/**
 * min macro
 */
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif /* MIN */

/**
 * String with length attached
 */
typedef struct len_str_t {
    char *str; /**< String. Possibly non null terminated */
    size_t len;      /**< Length of the string (not including NULL) */
} len_str_t;

/**
 * Macro for creating a len_str_t literal from a literal string
 *
 * @param str The string to create the literal it with
 */
#define LEN_STR_LIT(str) { str, sizeof(str) - 1 }


/**
 * String slist node
 */
typedef struct len_str_node_t {
    sl_link_t link; /**< Singly linked list node */
    len_str_t str;  /**< Stored string object */
} len_str_node_t;

/**
 * Structure for storage of len_str_node_t
 */
typedef struct len_str_node_node_t {
    sl_link_t link;
    len_str_node_t node;
} len_str_node_node_t;

// TODO: Replace calls to malloc with this
/**
 * Malloc that reports error and exits if memory cannot be obtained
 */
void *emalloc(size_t size);

/**
 * djb2 String hash function for len_str_t.
 * Must be void * for hashtable interface
 *
 * Source: http://www.cse.yorku.ca/~oz/hash.html
 *
 * @param vstr The len_str_t to hash
 */
inline uint32_t strhash(const void *vstr) {
    const len_str_t *len_str = (const len_str_t *)vstr;
    const char *str = len_str->str;
    size_t len = len_str->len;
    uint32_t hash = 5381;
    int c;

    while (len-- > 0 && (c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

/**
 * Same as str_hash, except the keys are pointers to a len_str_t
 *
 * @param vstrp Pointer to the len_str_t to hash
 */
inline uint32_t ind_strhash(const void *vstrp) {
    const len_str_t **len_strp = (const len_str_t **)vstrp;
    return strhash(*len_strp);
}

/**
 * String compare with void pointers to be compatible with the hash table
 * interface
 *
 * @param vstr1 First string
 * @param vstr2 Second string
 */
inline bool vstrcmp(const void *vstr1, const void *vstr2) {
    const len_str_t *str1 = (const len_str_t *)vstr1;
    const len_str_t *str2 = (const len_str_t *)vstr2;

    if (str1->len != str2->len) {
        return false;
    }

    return strncmp(str1->str, str2->str, str1->len) == 0;
}

/**
 * Same as vstrcmp, except the keys are pointers to a len_str_t
 *
 * @param vstrp1 Pointer to first string
 * @param vstrp2 Pointer to second string
 */
inline bool ind_vstrcmp(const void *vstrp1, const void *vstrp2) {
    const len_str_t **strp1 = (const len_str_t **)vstrp1;
    const len_str_t **strp2 = (const len_str_t **)vstrp2;
    return vstrcmp(*strp1, *strp2);
}

#define ASCII_LOWER \
'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': \
case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':\
case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':\
case 'y': case 'z'

#define ASCII_UPPER \
'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': \
case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':\
case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':\
case 'Y': case 'Z'

#define ASCII_DIGIT \
'0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': \
case '8': case '9'

#define HEX_DIGIT                                                       \
    '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': \
case '8': case '9':                                                     \
case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':             \
case 'A': case 'B': case 'C': case 'D': case 'E': case 'F'

#define OCT_DIGIT                                                       \
    '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7'

#endif /* _UTIL_H_ */
