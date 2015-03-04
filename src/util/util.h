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
    const char *str; /**< String. Possibly non null terminated */
    size_t len;      /**< Length of the string */
} len_str_t;

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

#endif /* _UTIL_H_ */
