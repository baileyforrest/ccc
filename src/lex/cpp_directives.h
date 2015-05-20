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
 * Preprocessor directives interface
 */

#ifndef _CPP_DIRECTIVES_H_
#define _CPP_DIRECTIVES_H_

#include "cpp.h"
#include "cpp_priv.h"

typedef status_t (*cpp_directive_func_t)(cpp_state_t *cs, vec_iter_t *ts);

typedef struct cpp_directive_t {
    const char *name;
    cpp_directive_func_t func;
} cpp_directive_t;

extern cpp_directive_t directives[];

#endif /* _CPP_DIRECTIVES_H_ */
