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
 * Interface for holding information about source files
 */

#ifndef _FILE_DIRECTORY_H_
#define _FILE_DIRECTORY_H_

#include "util/util.h"

/**
 * Structure for representing a location in a file
 */
typedef struct fmark_t {
    len_str_t *filename; /**< Note that this should not be freed with fmark_t */
    int line_num;        /**< Line number */
    int col_num;         /**< Column number */
} fmark_t;

/**
 * Initializes the file directiory
 *
 * @return CCC_OK on success, error code on failure
 */
status_t fdir_init();

/**
 * Destroys the file directory, freeing its memory
 */
void fdir_destroy();

/**
 * Add a file to the file directory. Returns the current entry if it exists.
 *
 * @param filename Name of the file to add. Note that this function will
 * reallocate its own copy of the same string.
 *
 * @param len of the filename
 * @param result Location to store file name. Result points to a null terminated
 * string
 *
 * @return CCC_OK on success, error code on failure
 */
status_t fdir_insert(const char *filename, size_t len, len_str_t **result);

/**
 * Retrieves the file entry with given filename

 * @param filename Name of the file to lookup
 * @param len of the filename
 * @return Returns the len_str of the file if successful, NULL on failure
 */
len_str_t *fdir_lookup(const char *filename, size_t len);

#endif /* _FILE_DIRECTORY_H_ */
