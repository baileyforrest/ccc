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

#include "iee754.h"

#define IEE754_EXP_BIAS(bits) ((1 << (bits - 1)) - 1)

#define IEE754_F32_BITS 32
#define IEE754_F32_EXP_BITS 8
#define IEE754_F32_EXP_BIAS IEE754_EXP_BIAS(IEE754_F32_EXP_BITS)
#define IEE754_F32_MANT_BITS 23

#define IEE754_F64_BITS 64
#define IEE754_F64_EXP_BITS 11
#define IEE754_F64_EXP_BIAS IEE754_EXP_BIAS(IEE754_F64_EXP_BITS)
#define IEE754_F64_MANT_BITS 52

#define IEE754_F80_BITS 80
#define IEE754_F80_EXP_BITS 15
#define IEE754_F80_EXP_BIAS IEE754_EXP_BIAS(IEE754_F80_EXP_BITS)
#define IEE754_F80_MANT_BITS 64

void iee754_f32_decompose(float f, iee754_parts_t *parts) {
    union {
        float f;
        uint32_t i;
    } converter = { f };

    parts->sign = converter.i >> (IEE754_F32_BITS - 1);

    parts->exp = (int64_t)((converter.i >> IEE754_F32_MANT_BITS) &
                  ((1 << IEE754_F32_EXP_BITS) - 1)) - IEE754_F32_EXP_BIAS;

    parts->mantissa =
        (uint64_t)(converter.i & ((1 << IEE754_F32_MANT_BITS) - 1)) <<
        (64 - IEE754_F32_MANT_BITS);
}

void iee754_f64_decompose(double f, iee754_parts_t *parts) {
    union {
        double f;
        uint64_t i;
    } converter = { f };

    parts->sign = converter.i >> (IEE754_F64_BITS - 1);

    parts->exp = (int64_t)((converter.i >> IEE754_F64_MANT_BITS) &
                  ((1 << IEE754_F64_EXP_BITS) - 1)) - IEE754_F64_EXP_BIAS;

    parts->mantissa =
        (uint64_t)(converter.i & ((1ULL << IEE754_F64_MANT_BITS) - 1)) <<
        (64 - IEE754_F64_MANT_BITS);
}

void iee754_f80_decompose(long double f, iee754_parts_t *parts) {
    union {
        uint64_t i[2];
        double f;
    } converter = { { 0, 0 } };
    converter.f = f;

    parts->sign = converter.i[1] >> (IEE754_F80_BITS - 64 - 1);

    parts->exp = (int64_t)(converter.i[1] & ((1 << IEE754_F64_EXP_BITS) - 1))
        - IEE754_F64_EXP_BIAS;

    parts->mantissa = converter.i[0];
}

float iee754_f32_construct(iee754_parts_t *parts) {
    union {
        uint32_t i;
        float f;
    } converter = { 0 };

    if (parts->sign) {
        converter.i |= 1ULL << (IEE754_F32_BITS - 1);
    }

    converter.i |= (parts->exp + IEE754_F32_EXP_BIAS) << IEE754_F32_MANT_BITS;
    converter.i |= parts->mantissa >> (32 - IEE754_F32_MANT_BITS);

    return converter.f;
}

double iee754_f64_construct(iee754_parts_t *parts) {
    union {
        uint64_t i;
        double f;
    } converter = { 0 };

    if (parts->sign) {
        converter.i |= 1ULL << (IEE754_F64_BITS - 1);
    }

    converter.i |= (parts->exp + IEE754_F64_EXP_BIAS) << IEE754_F64_MANT_BITS;
    converter.i |= parts->mantissa >> (64 - IEE754_F64_MANT_BITS);

    return converter.f;
}

long double iee754_f80_construct(iee754_parts_t *parts) {
    union {
        uint64_t i[2];
        long double f;
    } converter = { { 0, 0 } };

    if (parts->sign) {
        converter.i[1] |= 1ULL << (IEE754_F80_BITS - 64 - 1);
    }

    converter.i[0] = parts->mantissa;
    converter.i[1] |= (parts->exp + IEE754_F80_EXP_BIAS);

    return converter.f;
}
