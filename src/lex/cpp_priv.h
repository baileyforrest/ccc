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
 * C private interface
 */

#ifndef _CPP_PRIV_
#define _CPP_PRIV_

#include "cpp.h"

#include "util/htable.h"

typedef struct cpp_state_t {
    char *filename;
    token_man_t *token_man;
    lexer_t *lexer;
    htable_t macros; /**< char * -> cpp_macro_t */
} cpp_state_t;

typedef struct cpp_macro_t {
    sl_link_t *link;
    char *name;
    vec_t stream;
    slist_t params; /**< str_node_t, NULL if varargs */
    int num_params;
} cpp_macro_t;

typedef struct cpp_macro_param_t {
    sl_link_t *link;
    char *name;
    vec_t stream;
} cpp_macro_param_t;

typedef struct cpp_macro_inst_t {
    cpp_macro_t *macro;
    slist_t args;       /**< cpp_macro_param_t */
} cpp_macro_inst_t;

status_t cpp_process_file(cpp_state_t *cs, char *filename, vec_t *output);

void cpp_state_init(cpp_state_t *cs, token_man_t *token_man, lexer_t *lexer);
void cpp_state_destroy(cpp_state_t *cs);

status_t cpp_expand(cpp_state_t *cs, vec_iter_t *ts, vec_t *output);

status_t cpp_substitute(cpp_state_t *cs, cpp_macro_inst_t *macro_inst,
                        str_set_t *hideset, vec_t *output);

status_t cpp_handle_directive(cpp_state_t *cs, vec_iter_t *ts);

status_t cpp_fetch_macro_params(cpp_state_t *cs, vec_iter_t *ts,
                                cpp_macro_inst_t *macro_inst);

token_t *cpp_stringify(cpp_state_t *cs, vec_t *ts);

vec_t *cpp_macro_inst_lookup(cpp_macro_inst_t *inst, char *arg_name);

status_t cpp_glue(cpp_state_t *cs, vec_t *left, vec_iter_t *right,
                  size_t nelems);


#endif /* _CPP_PRIV_ */
