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
 * Preprocessor directives implementation
 */

#include "cpp_directives.h"

#define DIR_DECL(directive) \
    status_t cpp_dir_ ## directive(cpp_state_t *cs, vec_iter_t *ts)

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

#define DIR_ENTRY(directive) { #directive, cpp_dir_ ## directive }

cpp_directive_t directives[] = {
    DIR_ENTRY(include),

    DIR_ENTRY(define),
    DIR_ENTRY(undef),

    DIR_ENTRY(ifdef),
    DIR_ENTRY(ifndef),
    DIR_ENTRY(if),
    DIR_ENTRY(elif),
    DIR_ENTRY(else),
    DIR_ENTRY(endif),

    DIR_ENTRY(error),
    DIR_ENTRY(warning),

    DIR_ENTRY(pragma),
    DIR_ENTRY(line),
    { NULL, NULL }
};

status_t cpp_dir_include(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_define(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_undef(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_ifdef(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_ifndef(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_if(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_elif(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_else(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_endif(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_error(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_warning(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_pragma(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}

status_t cpp_dir_line(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return CCC_ESYNTAX;
}
