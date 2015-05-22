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

#include "parse/ast.h"
#include "parse/parser.h"
#include "typecheck/typechecker.h"
#include "util/logger.h"

#define VERIFY_TOK_ID(token)                            \
    do {                                                    \
        if (token->type != ID) {                            \
            logger_log(&token->mark, LOG_ERR,               \
                       "macro names must be identifiers");  \
            return CCC_ESYNTAX;                             \
        }                                                   \
    } while (0)

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

status_t cpp_dir_error_helper(vec_iter_t *ts, bool is_err);

status_t cpp_if_helper(cpp_state_t *cs, vec_iter_t *ts, vec_t *output,
                       bool if_taken);

status_t cpp_evaluate_line(cpp_state_t *cs, vec_iter_t *ts, long long *val);

#define DIR_ENTRY(directive, if_ignored) \
    { #directive, cpp_dir_ ## directive, CPP_DIR_ ## directive, if_ignored }

cpp_directive_t directives[] = {
    DIR_ENTRY(include, true),

    DIR_ENTRY(define, true),
    DIR_ENTRY(undef, true),

    DIR_ENTRY(ifdef, false),
    DIR_ENTRY(ifndef, false),
    DIR_ENTRY(if, false),
    DIR_ENTRY(elif, false),
    DIR_ENTRY(else, false),
    DIR_ENTRY(endif, false),

    DIR_ENTRY(error, true),
    DIR_ENTRY(warning, true),

    DIR_ENTRY(pragma, true),
    DIR_ENTRY(line, true),
    { NULL, NULL, CPP_DIR_NONE, false }
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
    status_t status = CCC_OK;
    cpp_macro_t *macro;
    if (CCC_OK != (status = cpp_define_helper(ts, false, &macro))) {
        return status;
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
}

status_t cpp_define_helper(vec_iter_t *ts, bool has_eq, cpp_macro_t **result) {
    token_t *token = vec_iter_get(ts);
    VERIFY_TOK_ID(token);

    status_t status = CCC_OK;

    cpp_macro_t *macro = emalloc(sizeof(*macro));
    macro->name = token->id_name;
    macro->mark = &token->mark;
    vec_init(&macro->stream, 0);
    vec_init(&macro->params, 0);
    macro->num_params = -1;

    cpp_iter_advance(ts);

    if (has_eq && (token = vec_iter_get(ts))->type == EQ) {
        cpp_iter_advance(ts);
    }

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

    *result = macro;
    return status;

fail:
    cpp_macro_destroy(macro);
    return status;
}

status_t cpp_dir_undef(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)output;
    token_t *token = vec_iter_get(ts);
    VERIFY_TOK_ID(token);

    cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
    cpp_macro_destroy(macro);

    return CCC_OK;
}

status_t cpp_dir_ifdef(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    token_t *token = vec_iter_get(ts);
    VERIFY_TOK_ID(token);

    cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
    return cpp_if_helper(cs, ts, output, macro != NULL);
}

status_t cpp_dir_ifndef(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    token_t *token = vec_iter_get(ts);
    VERIFY_TOK_ID(token);

    cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
    return cpp_if_helper(cs, ts, output, macro == NULL);
}

status_t cpp_dir_if(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    status_t status = CCC_OK;
    long long val;
    if (CCC_OK != (status = cpp_evaluate_line(cs, ts, &val))) {
        return status;
    }

    return cpp_if_helper(cs, ts, output, val != 0);
}

status_t cpp_evaluate_line(cpp_state_t *cs, vec_iter_t *ts, long long *val) {
    status_t status = CCC_OK;
    trans_unit_t *ast = NULL;
    *val = 0;

    vec_t input;
    vec_t output;
    vec_init(&input, 0);
    vec_init(&output, 0);

    for (; vec_iter_has_next(ts); cpp_iter_advance(ts)) {
        token_t *token = vec_iter_get(ts);
        if (token->type == NEWLINE) {
            break;
        }

        if (token->type == ID) {
            // Handle defined operator
            if (strcmp(token->id_name, "defined") == 0) {
                cpp_iter_advance(ts);
                bool has_paren = false;
                if (token->type == LPAREN) {
                    has_paren = true;
                    cpp_iter_advance(ts);
                }

                if (token->type != ID) {
                    logger_log(&token->mark, LOG_ERR,
                               "operator \"defined\" requires an identifier");
                    status = CCC_ESYNTAX;
                    goto fail;
                }

                cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
                token = macro == NULL ? &token_int_zero : &token_int_one;

                cpp_iter_advance(ts);
                if (has_paren) {
                    if (token->type != RPAREN) {
                        logger_log(&token->mark, LOG_ERR,
                                   "missing ')' after \"defined\"");
                        status = CCC_ESYNTAX;
                        goto fail;
                    }
                    cpp_iter_advance(ts);
                }
            }
        }

        vec_push_back(&input, token);
    }
    vec_iter_t input_iter = { &input, 0 };

    if (CCC_OK != (status = cpp_expand(cs, &input_iter, &output))) {
        goto fail;
    }

    expr_t *expr = NULL;
    if (CCC_OK != (status = parser_parse_expr(&output, ast, &expr))) {
        goto fail;
    }

    if (!typecheck_const_expr(expr, val, true)) {
        goto fail;
    }

fail:
    ast_destroy(ast);
    cpp_skip_line(ts, false);
    vec_destroy(&input);
    vec_destroy(&output);
    return status;
}

status_t cpp_if_helper(cpp_state_t *cs, vec_iter_t *ts, vec_t *output,
                       bool if_taken) {
    status_t status = CCC_OK;
    token_t *start_token = vec_iter_get(ts);

    bool ignore_save = cs->ignore;
    cs->if_taken = if_taken; // Mark if_taken for last directive

    // We ignore first if, if we were already ignoring, or if wasn't taken
    cs->ignore = ignore_save || !if_taken;
    ++cs->if_count;

    do {
        // If last directive was a taken branch, then mark if_taken as true
        if (cs->if_taken) {
            if_taken = true;
        }
        if (CCC_BACKTRACK != (status = cpp_expand(cs, ts, output))) {
            if (status == CCC_OK) {
                logger_log(&start_token->mark, LOG_ERR, "Unterminted #if");
            }
            goto fail;
        }
        // If we were ignoring before, or already took an if branch then ignore
        if (ignore_save || if_taken) {
            cs->ignore = true;
        }
        cs->if_taken = false;
    } while (cs->last_dir != CPP_DIR_endif);

    // Restore state
    --cs->if_count;
    cs->ignore = ignore_save;

fail:
    return status;
}

status_t cpp_dir_elif(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)output;
    status_t status = CCC_OK;

    token_t *token = vec_iter_get(ts);
    if (cs->if_count == 0) {
        logger_log(&token->mark, LOG_ERR, "#else without #if");
        return CCC_ESYNTAX;
    }

    long long val;
    if (CCC_OK != (status = cpp_evaluate_line(cs, ts, &val))) {
        return status;
    }

    // This if is taken if it's value evaluated to non zero
    cs->if_taken = val != 0;

    return CCC_BACKTRACK; // Return to calling if
}

status_t cpp_dir_else(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)output;
    token_t *token = vec_iter_get(ts);
    if (cs->if_count == 0) {
        logger_log(&token->mark, LOG_ERR, "#else without #if");
        return CCC_ESYNTAX;
    }

    // Mark this branch as always taken
    cs->if_taken = true;
    return CCC_BACKTRACK; // Return to calling if
}

status_t cpp_dir_endif(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)output;
    token_t *token = vec_iter_get(ts);
    if (cs->if_count == 0) {
        logger_log(&token->mark, LOG_ERR, "#else without #if");
        return CCC_ESYNTAX;
    }

    return CCC_BACKTRACK; // Return to calling if
}

status_t cpp_dir_error(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)output;
    return cpp_dir_error_helper(ts, true);
}

status_t cpp_dir_error_helper(vec_iter_t *ts, bool is_err) {
    string_builder_t sb;
    sb_init(&sb, 0);
    token_t *token = vec_iter_get(ts);
    const char *line_start = token->mark.line_start;
    while (*line_start && *line_start != '\n') {
        sb_append_char(&sb, *(line_start++));
    }
    logger_log(&token->mark, is_err ? LOG_ERR : LOG_WARN, "%s", sb_buf(&sb));
    sb_destroy(&sb);

    return is_err ? CCC_ESYNTAX : CCC_OK;
}

status_t cpp_dir_warning(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)output;
    return cpp_dir_error_helper(ts, false);
}

status_t cpp_dir_pragma(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)cs;
    (void)output;
    cpp_skip_line(ts, false);
    return CCC_OK;
}

status_t cpp_dir_line(cpp_state_t *cs, vec_iter_t *ts, vec_t *out) {
    (void)out;
    status_t status = CCC_OK;

    vec_t input;
    vec_t output;
    vec_init(&input, 0);
    vec_init(&output, 0);

    for (; vec_iter_has_next(ts); cpp_iter_advance(ts)) {
        token_t *token = vec_iter_get(ts);
        if (token->type == NEWLINE) {
            break;
        }

        vec_push_back(&input, token);
    }
    vec_iter_t input_iter = { &input, 0 };

    if (CCC_OK != (status = cpp_expand(cs, &input_iter, &output))) {
        goto fail;
    }

    for (size_t i = 0; i < vec_size(&output); ++i) {
        token_t *token = vec_get(&output, i);
        switch (i) {
        case 0:
            if (token->type != INTLIT) {
                logger_log(&token->mark, LOG_ERR,
                           "\"%s\" after #line is not a positive integer",
                           token_type_str(token->type));
                status = CCC_ESYNTAX;
                goto fail;
            }
            cs->line_mod = token->int_params.int_val;
            cs->line_orig = token->mark.line;
            break;
        case 1:
            if (token->type != STRING) {
                logger_log(&token->mark, LOG_ERR,
                           "\"%s\" is not a valid filename",
                           token_type_str(token->type));
                status = CCC_ESYNTAX;
                goto fail;
            }
            cs->cur_filename = token->str_val;
            break;
        default:
            logger_log(&token->mark, LOG_WARN,
                       "extra tokens at end of #line directive");
            goto fail;
        }
    }


fail:
    cpp_skip_line(ts, false);
    vec_destroy(&input);
    vec_destroy(&output);
    return status;
}
