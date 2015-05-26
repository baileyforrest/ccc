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

typedef enum cpp_dir_type_t {
    CPP_DIR_NONE,
    CPP_DIR_include,
    CPP_DIR_define,
    CPP_DIR_undef,
    CPP_DIR_ifdef,
    CPP_DIR_ifndef,
    CPP_DIR_if,
    CPP_DIR_elif,
    CPP_DIR_else,
    CPP_DIR_endif,
    CPP_DIR_error,
    CPP_DIR_warning,
    CPP_DIR_pragma,
    CPP_DIR_line,
} cpp_dir_type_t;


typedef struct cpp_state_t {
    char *filename;
    token_man_t *token_man;
    lexer_t *lexer;
    htable_t macros; /**< char * -> cpp_macro_t */
    vec_t search_path; /**< (char *) */

    char *cur_filename; /**< filename for __FILE__ */
    int line_mod; /**< line number that was changed to 0, if unmodified */
    int line_orig; /**< original line number, used to calulate __LINE__ */

    cpp_dir_type_t last_dir;

    int if_count;
    int if_level;
    bool if_taken;
    bool ignore;
    bool in_param;

    token_t *last_top_token;
    int expand_level;
} cpp_state_t;

typedef enum cpp_macro_type_t {
    CPP_MACRO_BASIC, /**< Regular macro */
    CPP_MACRO_FILE,  /**< __FILE__ */
    CPP_MACRO_LINE,  /**< __LINE__ */
    CPP_MACRO_DATE,  /**< __DATE__ */
    CPP_MACRO_TIME,  /**< __TIME__ */
} cpp_macro_type_t;

typedef struct cpp_macro_t {
    sl_link_t link;
    char *name;
    fmark_t *mark;
    vec_t stream; /**< (token_t) */
    vec_t params; /**< (char *) name NULL if varargs */
    int num_params; /**< -1 if object like macro */
    cpp_macro_type_t type;
} cpp_macro_t;

typedef struct cpp_macro_param_t {
    sl_link_t link;
    char *name;
    vec_t stream;
} cpp_macro_param_t;

typedef struct cpp_macro_inst_t {
    cpp_macro_t *macro;
    slist_t args;       /**< cpp_macro_param_t */
} cpp_macro_inst_t;

#define VERIFY_TOK_ID(token)                                \
    do {                                                    \
        if (token->type != ID) {                            \
            logger_log(token->mark, LOG_ERR,                \
                       "macro names must be identifiers");  \
            return CCC_ESYNTAX;                             \
        }                                                   \
    } while (0)

inline void cpp_iter_skip_space(vec_iter_t *iter) {
    for (; vec_iter_has_next(iter); vec_iter_advance(iter)) {
        token_t *token = vec_iter_get(iter);
        if (token->type != SPACE) {
            break;
        }
    }
}

status_t cpp_state_init(cpp_state_t *cs, token_man_t *token_man,
                        lexer_t *lexer);

void cpp_macro_destroy(cpp_macro_t *macro);

void cpp_macro_inst_destroy(cpp_macro_inst_t *macro_inst);

void cpp_state_destroy(cpp_state_t *cs);

token_t *cpp_iter_advance(vec_iter_t *iter);

token_t *cpp_iter_lookahead(vec_iter_t *iter, size_t lookahead);

token_t *cpp_next_nonspace(vec_iter_t *iter, bool inplace);

void cpp_stream_append(cpp_state_t *cs, vec_t *output, token_t *token);

status_t cpp_process_file(cpp_state_t *cs, char *filename, vec_t *output);

size_t cpp_skip_line(vec_iter_t *ts, bool skip_newline);

bool cpp_macro_equal(cpp_macro_t *m1, cpp_macro_t *m2);

vec_t *cpp_macro_inst_lookup(cpp_macro_inst_t *inst, char *arg_name);

status_t cpp_macro_define(cpp_state_t *cs, char *string,
                          cpp_macro_type_t type, bool has_eq);

status_t cpp_expand(cpp_state_t *cs, vec_iter_t *ts, vec_t *output);

status_t cpp_substitute(cpp_state_t *cs, cpp_macro_inst_t *macro_inst,
                        str_set_t *hideset, vec_t *output);

status_t cpp_handle_directive(cpp_state_t *cs, vec_iter_t *ts, vec_t *output);

status_t cpp_fetch_macro_params(cpp_state_t *cs, vec_iter_t *ts,
                                cpp_macro_inst_t *macro_inst);

token_t *cpp_stringify(cpp_state_t *cs, vec_t *ts);

status_t cpp_glue(cpp_state_t *cs, vec_t *left, vec_iter_t *right,
                  size_t nelems);

void cpp_handle_special_macro(cpp_state_t *cs, fmark_t *mark,
                              cpp_macro_type_t type, vec_t *output);


#endif /* _CPP_PRIV_ */
