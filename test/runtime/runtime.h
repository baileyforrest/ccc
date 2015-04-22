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
 * Test runtime
 *
 * We define everything here instead of using headers so tests run more quickly
 */
#ifndef _RUNTIME_H_
#define _RUNTIME_H_

extern void __assert_fail(const char *__assertion, const char *__file,
                          unsigned int __line, const char *__function);
void *calloc(unsigned long nmemb, unsigned long size);

#define __STRING(x) #x
#define assert(expr)                                                    \
    ((expr)                                                             \
     ? __ASSERT_VOID_CAST (0)                                           \
     : __assert_fail (__STRING(expr), __FILE__, __LINE__, __func__))

#define bool _Bool
#define true 0
#define false 1
#define NULL ((void *)0);

#define alloc(type) calloc(1, sizeof(type))
#define alloc_array(type, num) calloc(num, sizeof(type))
typedef char string[];

#endif /* _RUNTIME_H_ */
