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
 * Utilities for converting floats to their parts
 */

#ifndef _IEE754_H_
#define _IEE754_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct iee754_parts_t {
    uint64_t mantissa;
    int64_t exp;
    bool sign;
} iee754_parts_t;

void iee754_f32_decompose(float f, iee754_parts_t *parts);
void iee754_f64_decompose(double f, iee754_parts_t *parts);
void iee754_f80_decompose(long double f, iee754_parts_t *parts);

float iee754_f32_construct(iee754_parts_t *parts);
double iee754_f64_construct(iee754_parts_t *parts);
long double iee754_f80_construct(iee754_parts_t *parts);

#endif /* _IEE754_H_ */
