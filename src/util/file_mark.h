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
 * File mark interface
 */

#ifndef _FILE_MARK_H_
#define _FILE_MARK_H_

#include <stddef.h>

#include "util/slist.h"

/**
 * Structure for representing a location in a file
 */
typedef struct fmark_t {
    struct fmark_t *last;   /**< Last mark in stack */
    char *filename;         /**< Filename */
    const char *line_start; /**< Start of current line */
    int line;               /**< Line number */
    int col;                /**< Column number */
} fmark_t;

/**
 * Name of "file" for built in objects
 */
#define BUILT_IN_FILENAME "<built in>"

/**
 * Name of "file" for command line
 */
#define COMMAND_LINE_FILENAME "<command-line>"

/**
 * File mark literal
 *
 * @param file Filename
 * @param line_start Pointer to start of current line
 * @param last Last file mark on stack
 * @param line Current line number
 * @param col Current column number
 */
#define FMARK_LIT(last, file, line_start, line, col) \
    { last, file, line_start, line, col }

extern fmark_t fmark_built_in;

typedef struct fmark_man_t {
    slist_t list;
    size_t offset;
} fmark_man_t;

void fmark_man_init(fmark_man_t *man);
void fmark_man_destroy(fmark_man_t *man);

fmark_t *fmark_man_insert(fmark_man_t *man, fmark_t *copy_from);

#endif /* _FILE_MARK_H_ */
