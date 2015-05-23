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

typedef status_t (*cpp_directive_func_t)(cpp_state_t *cs, vec_iter_t *ts,
                                         vec_t *output);

typedef struct cpp_directive_t {
    const char *name;
    cpp_directive_func_t func;
    cpp_dir_type_t type;
    bool if_ignore;
} cpp_directive_t;

extern cpp_directive_t directives[];

#define DIR_DECL(directive) \
    status_t cpp_dir_ ## directive(cpp_state_t *cs, vec_iter_t *ts, \
                                   vec_t *output)

DIR_DECL(include);

DIR_DECL(define);
DIR_DECL(undef);

DIR_DECL(ifdef);
DIR_DECL(ifndef);
DIR_DECL(if);
DIR_DECL(elif);
DIR_DECL(else);
DIR_DECL(endif);

DIR_DECL(error);
DIR_DECL(warning);

DIR_DECL(pragma);
DIR_DECL(line);

status_t cpp_expand_line(cpp_state_t *cs, vec_iter_t *ts, vec_t *output,
                         bool pp_if);

status_t cpp_include_helper(cpp_state_t *cs, fmark_t *mark, char *filename,
                            bool bracket, vec_t *output);

status_t cpp_dir_error_helper(vec_iter_t *ts, bool is_err);

status_t cpp_if_helper(cpp_state_t *cs, vec_iter_t *ts, vec_t *output,
                       bool if_taken);

status_t cpp_evaluate_line(cpp_state_t *cs, vec_iter_t *ts, long long *val);

status_t cpp_define_helper(cpp_state_t *cs, vec_iter_t *ts, bool has_eq);

#endif /* _CPP_DIRECTIVES_H_ */
