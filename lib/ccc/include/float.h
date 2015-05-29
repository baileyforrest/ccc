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

#ifndef _FLOAT_H___
#define _FLOAT_H___

/* Characteristics of floating point types, C99 5.2.4.2.2 */

#define FLT_EVAL_METHOD 0
#define FLT_ROUNDS 1
#define FLT_RADIX 2

#define FLT_MANT_DIG 24
#define DBL_MANT_DIG 53
#define LDBL_MANT_DIG 64

#define DECIMAL_DIG 21

#define FLT_DIG 6
#define DBL_DIG 15
#define LDBL_DIG 8

#define FLT_MIN_EXP -125
#define DBL_MIN_EXP -1021
#define LDBL_MIN_EXP -16381

#define FLT_MIN_10_EXP -37
#define DBL_MIN_10_EXP -307
#define LDBL_MIN_10_EXP -4931

#define FLT_MAX_EXP 128
#define DBL_MAX_EXP 1024
#define LDBL_MAX_EXP 16384

#define FLT_MAX_10_EXP 38
#define DBL_MAX_10_EXP 308
#define LDBL_MAX_10_EXP 4932

#define FLT_MAX 0x1.fffffep+127
#define DBL_MAX 0x1.fffffffffffffp+1023
#define LDBL_MAX 0xf.fffffffffffffffp+16380

#define FLT_EPSILON 0x1p-23
#define DBL_EPSILON 0x1p-52
#define LDBL_EPSILON 0x8p-66

#define FLT_MIN 0x1p-126
#define DBL_MIN 0x1p-1022
#define LDBL_MIN 0x8p-16385

#if __STDC_VERSION__ >= 201112L || !defined(__STRICT_ANSI__)
#  define FLT_TRUE_MIN 0x1p-149
#  define DBL_TRUE_MIN 0x0.0000000000001p-1022
#  define LDBL_TRUE_MIN 0x0.000000000000001p-16385
#endif

#endif /* _FLOAT_H___ */
