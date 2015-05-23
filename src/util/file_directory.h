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
 * Interface for holding information about source files
 */

#ifndef _FILE_DIRECTORY_H_
#define _FILE_DIRECTORY_H_

#include "util/util.h"

/**
 * File directory entry.
 *
 * Contains a filename and buffer of file contents
 */
typedef struct fdir_entry_t {
    sl_link_t link; /**< List link */
    char *filename; /**< Filename */
    char *buf;      /**< Buffer of file */
    char *end;      /**< Max location */
    int fd;         /**< File descriptor of open file */
} fdir_entry_t;

/**
 * Initializes the file directiory
 */
void fdir_init(void);

/**
 * Destroys the file directory, freeing its memory
 */
void fdir_destroy(void);

/**
 * Add a file to the file directory. Returns the current entry if it exists.
 *
 * @param filename Name of the file to add. Note that this function will
 *     reallocate its own copy of the same string.
 *
 * @param result Location to store file name. Result points to a null terminated
 *     string
 *
 * @return CCC_OK on success, error code on failure
 */
status_t fdir_insert(const char *filename, fdir_entry_t **result);

/**
 * Retrieves the file entry with given filename
 *
 * @param filename Name of the file to lookup
 * @return Returns the file directory entry of the file if successful, NULL on
 *     failure
 */
fdir_entry_t *fdir_lookup(const char *filename);

#endif /* _FILE_DIRECTORY_H_ */
