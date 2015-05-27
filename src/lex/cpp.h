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
 * C preprocessor interface
 */

#ifndef _CPP_H_
#define _CPP_H_

#include "lex/lex.h"
#include "lex/token.h"
#include "util/vector.h"

typedef struct cpp_macro_t cpp_macro_t;

status_t cpp_process(token_man_t *token_man, lexer_t *lexer, char *filepath,
                     vec_t *output);

#endif /* _CPP_H_ */
