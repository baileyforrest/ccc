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
 * CCC status codes
 */


#ifndef _STATUS_H_
#define _STATUS_H_

/**
 * Status codes for CCC
 */
typedef enum {
    CCC_OK = 0,    /**< Success */
    CCC_FILEERR,   /**< File Error */
    CCC_ESYNTAX,   /**< Syntax Error */
    CCC_BACKTRACK, /**< Backtrack required */
    CCC_DUPLICATE, /**< Already Exists */
    CCC_RETRY,     /**< Retry last operation */
} status_t;

#endif /* _STATUS_H_ */
