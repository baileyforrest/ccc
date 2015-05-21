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

#include <limits.h>
#include <unistd.h>

#include "util/logger.h"

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

status_t cpp_include_helper(cpp_state_t *cs, fmark_t *mark, char *filename,
                            bool bracket, vec_t *output);

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

status_t cpp_dir_include(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    status_t status = CCC_OK;
    token_t *token = vec_iter_get(ts);
    fmark_t *mark = &token->mark;
    char *filename = NULL;
    vec_t macro_output;
    vec_init(&macro_output, 0);

    // If its an identifier, then try to expand it
    if (token->type == ID) {
        cpp_iter_advance(ts);

        vec_t input;
        vec_init(&input, 1);

        vec_push_back(&input, token);

        vec_iter_t input_iter = { &input, 0 };
        status = cpp_expand(cs, &input_iter, &macro_output);
        assert(status == CCC_OK);

        // Set token stream to expanded macro
        ts = &(vec_iter_t){ &macro_output, 0 };
        vec_destroy(&input);
        token = vec_iter_get(ts);
    }

    switch (token->type) {
    case STRING: // "filename
        cpp_iter_advance(ts);
        filename = token->str_val;
        status = cpp_include_helper(cs, mark, filename, false, output);
        break;
    case LT: { // <filename>
        string_builder_t sb;
        sb_init(&sb, 0);

        // Don't use cpp_iter_advance, we want to preserve whitespace
        bool done = false;
        vec_iter_advance(ts);
        for (; vec_iter_has_next(ts); vec_iter_advance(ts)) {
            token_t *token = vec_iter_get(ts);

            if (token->type == NEWLINE) {
                break;
            }
            if (token->type == GT) {
                done = true;
                vec_iter_advance(ts);
                break;
            }

            token_str_append_sb(&sb, token);
        }
        if (done) {
            status = cpp_include_helper(cs, mark, sb_buf(&sb), true, output);
        } else {
            logger_log(&token->mark, LOG_ERR,
                       "missing terminating > character");
            status = CCC_ESYNTAX;
        }
        sb_destroy(&sb);
        break;
    }
    default:
        logger_log(&token->mark, LOG_ERR,
                   "#include expects \"FILENAME\" or <FILENAME>");
        status = CCC_ESYNTAX;
    }

    vec_destroy(&macro_output);
    return status;
}

status_t cpp_include_helper(cpp_state_t *cs, fmark_t *mark, char *filename,
                            bool bracket, vec_t *output) {
    char *file_dir, file_dir_buf[PATH_MAX + 1];
    char include_path[PATH_MAX + 1];

    strncpy(file_dir_buf, cs->filename, PATH_MAX);
    file_dir_buf[PATH_MAX] = '\0';

    file_dir = ccc_dirname(file_dir_buf);

    VEC_FOREACH(cur, &cs->search_path) {
        char *cur_path = vec_get(&cs->search_path, cur);

        // Empty string denotes current directory
        if (*cur_path == '\0') {
            // Don't search current directory when brackets are used
            if (bracket) {
                continue;
            }

            // Set cur_path to current directory
            cur_path = file_dir;
        }

        size_t cur_path_len = strlen(cur_path);
        if (cur_path_len + strlen(filename) + 1 > PATH_MAX) {
            logger_log(mark, LOG_ERR, "Include path name too long");
            return CCC_ESYNTAX;
        }
        strcpy(include_path, cur_path);
        include_path[cur_path_len] = '/';
        strcpy(include_path + cur_path_len + 1, filename);

        // File isn't accessible
        if(-1 == access(include_path, R_OK)) {
            continue;
        }

        return cpp_process_file(cs, include_path, output);
    }

    logger_log(mark, LOG_ERR, "%s: No such file or directory", filename);
    return CCC_ESYNTAX;
}

status_t cpp_dir_define(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)output;

    token_t *token = vec_iter_get(ts);
    if (token->type != ID) {
        logger_log(&token->mark, LOG_ERR, "macro names must be identifiers");
        return CCC_ESYNTAX;
    }
    status_t status = CCC_OK;

    cpp_macro_t *macro = emalloc(sizeof(*macro));
    macro->name = token->id_name;
    macro->mark = &token->mark;
    vec_init(&macro->stream, 0);
    vec_init(&macro->params, 0);
    macro->num_params = -1;

    cpp_iter_advance(ts);

    if (vec_iter_has_next(ts) && (token = vec_iter_get(ts))->type == LPAREN) {
        cpp_iter_advance(ts);
        macro->num_params = 0;

        bool done = false;
        bool first = true;
        for (; vec_iter_has_next(ts); cpp_iter_advance(ts)) {
            token = vec_iter_get(ts);
            if (token->type == NEWLINE) {
                break;
            }
            if (token->type == RPAREN) {
                done = true;
                break;
            }
            if (!first && token->type != COMMA) {
                logger_log(&token->mark, LOG_ERR,
                           "macro parameters must be comma-separated");
                status = CCC_ESYNTAX;
                goto fail;
            }
            if (token->type != ID) {
                logger_log(&token->mark, LOG_ERR,
                           "\"%s\" may not appear in macro parameter list",
                           token_type_str(token->type));
                status = CCC_ESYNTAX;
                goto fail;
            }
            vec_push_back(&macro->params, token->id_name);
            ++macro->num_params;
            first = false;
        }

        if (!done) {
            logger_log(macro->mark, LOG_ERR,
                       "missing ')' in macro parameter list");
            status = CCC_ESYNTAX;
            goto fail;
        }
    }

    // Don't use cpp_iter_advance, we want to preserve whitespace
    for (; vec_iter_has_next(ts); vec_iter_advance(ts)) {
        token = vec_iter_get(ts);
        if (token->type == NEWLINE) {
            break;
        }

        vec_push_back(&macro->stream, token);
    }

    cpp_macro_t *old_macro = ht_remove(&cs->macros, &macro->name);
    if (old_macro != NULL) {
        if (!cpp_macro_equal(macro, old_macro)) {
            logger_log(macro->mark, LOG_WARN, "\"%s\" redefined", macro->name);
            logger_log(old_macro->mark, LOG_NOTE,
                       "this is the location of the previous definition");
        }
        cpp_macro_destroy(old_macro);
    }
    status = ht_insert(&cs->macros, &macro->link);
    assert(status == CCC_OK);

    return status;

fail:
    cpp_macro_destroy(macro);
    return status;
}

status_t cpp_dir_undef(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_ifdef(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_ifndef(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_if(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_elif(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_else(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_endif(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_error(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_warning(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_pragma(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}

status_t cpp_dir_line(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)ts;
    (void)output;
    return CCC_ESYNTAX;
}
