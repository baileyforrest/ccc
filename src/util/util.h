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
 * Misc Utilities
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/slist.h"
#include "util/string_builder.h"

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

// TODO0: Remove this in favor of string vectors to avoid many small allocations
/**
 * String slist node
 */
typedef struct str_node_t {
    sl_link_t link; /**< Singly linked list node */
    char *str;      /**< Stored string object */
} str_node_t;

void exit_err(char *msg);

char *ccc_basename(char *path);
char *ccc_dirname(char *path);

/**
 * Malloc that reports error and exits if memory cannot be obtained
 *
 * @param size Size of memory to allocate
 * @return Heap memory returned from malloc
 */
void *emalloc(size_t size);

/**
 * calloc that reports error and exits if memory cannot be obtained
 *
 * @param nmemb Number of elems
 * @param size size of each elem
 * @return Heap memory returned from calloc
 */
void *ecalloc(size_t nmemb, size_t size);

/**
 * realloc that reports error and exits if memory cannot be obtained
 *
 * @param ptr previously allocated
 * @param size of memory to reallocate
 * @return Heap memory returned from realloc
 */
void *erealloc(void *ptr, size_t size);

void directed_print(string_builder_t *sb, FILE *file, char *fmt, ...);

/**
 * djb2 String hash function
 * Must be void * for hashtable interface
 *
 * Source: http://www.cse.yorku.ca/~oz/hash.html
 *
 * @param vstr The char * to hash
 */
inline uint32_t ind_str_hash(const void *vstr) {
    const char *str = *(const char **)vstr;
    uint32_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

/**
 * String compare with void pointers to be compatible with the hash table
 * interface
 *
 * @param vstr1 First string
 * @param vstr2 Second string
 */
inline bool ind_str_eq(const void *vstr1, const void *vstr2) {
    const char *str1 = *(const char **)vstr1;
    const char *str2 = *(const char **)vstr2;

    return strcmp(str1, str2) == 0;
}

char *unescape_str(char *str);

char *format_basename_ext(char *path, char *ext);

#endif /* _UTIL_H_ */
