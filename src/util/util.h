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

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "util/slist.h"

/**
 * Typesafe/single evalutation max macro
 *
 * Source: https://gcc.gnu.org/onlinedocs/gcc-4.9.2/gcc/Typeof.html#Typeof
 */
#ifndef MAX
#define MAX(a,b) \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })
#endif /* MAX */

/**
 * Typesafe/single evalutation max macro
 *
 * Source: https://gcc.gnu.org/onlinedocs/gcc-4.9.2/gcc/Typeof.html#Typeof
 */
#ifndef MIN
#define MIN(a,b) \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })
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
#define LEN_STR_LITERAL(str) { str, sizeof(str) - 1 }


/**
 * String slist node
 */
typedef struct len_str_node_t {
    sl_link_t link; /**< Singly linked list node */
    len_str_t str;  /**< Stored string object */
} len_str_node_t;

/**
 * djb2 String hash function for len_str_t.
 * Must be void * for hashtable interface
 *
 * Source: http://www.cse.yorku.ca/~oz/hash.html
 *
 * @param len_str The len_str to hash
 */
uint32_t strhash(const void *len_str);

/**
 * String compare with void pointers to be compatible with the hash table
 * interface
 *
 * @param len_str1 First string
 * @param len_str2 Second string
 */
bool vstrcmp(const void *len_str1, const void *len_str2);

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

#endif /* _UTIL_H_ */
