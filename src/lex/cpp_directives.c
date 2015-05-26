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

#include "optman.h"
#include "parse/ast.h"
#include "parse/parser.h"
#include "typecheck/typechecker.h"
#include "util/logger.h"

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

status_t cpp_expand_line(cpp_state_t *cs, vec_iter_t *ts, vec_t *output,
                         bool pp_if) {
    status_t status = CCC_OK;

    vec_t input;
    vec_init(&input, 0);

    for (; vec_iter_has_next(ts); cpp_iter_advance(ts)) {
        token_t *token = vec_iter_get(ts);
        if (token->type == NEWLINE) {
            break;
        }

        if (pp_if && token->type == ID) {
            // Handle defined operator
            if (strcmp(token->id_name, "defined") == 0) {
                cpp_iter_advance(ts);
                token = vec_iter_get(ts);
                bool has_paren = false;
                if (token->type == LPAREN) {
                    has_paren = true;
                    cpp_iter_advance(ts);
                    token = vec_iter_get(ts);
                }

                if (token->type != ID) {
                    logger_log(&token->mark, LOG_ERR,
                               "operator \"defined\" requires an identifier");
                    status = CCC_ESYNTAX;
                    goto fail;
                }

                cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
                token = macro == NULL ? &token_int_zero : &token_int_one;

                if (has_paren) {
                    cpp_iter_advance(ts); // Get paren
                    token_t *next_token = vec_iter_get(ts);
                    if (next_token->type != RPAREN) {
                        logger_log(&token->mark, LOG_ERR,
                                   "missing ')' after \"defined\"");
                        status = CCC_ESYNTAX;
                        goto fail;
                    }
                }
            } else {
                // Non 'defined' idenitifer if undefined macro, just output zero
                cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
                token = macro == NULL ? &token_int_zero : token;

            }
        }

        vec_push_back(&input, token);
    }

    vec_iter_t input_iter = { &input, 0 };

    if (CCC_OK != (status = cpp_expand(cs, &input_iter, output))) {
        goto fail;
    }

fail:
    vec_destroy(&input);
    return status;
}

status_t cpp_dir_include(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    status_t status = CCC_OK;
    token_t *token = vec_iter_get(ts);
    fmark_t *mark = &token->mark;
    char *filename = NULL;

    vec_t line;
    vec_init(&line, 0);

    if (CCC_OK != (status = cpp_expand_line(cs, ts, &line, false))) {
        goto fail;
    }

    vec_iter_t line_iter = { &line, 0 };
    token = vec_iter_get(&line_iter);

    switch (token->type) {
    case STRING: // "filename"
        cpp_iter_advance(&line_iter);
        filename = token->str_val;
        status = cpp_include_helper(cs, mark, filename, false, output);
        break;
    case LT: { // <filename>
        string_builder_t sb;
        sb_init(&sb, 0);

        // Don't use cpp_iter_advance, we want to preserve whitespace
        bool done = false;
        vec_iter_advance(&line_iter);
        for (; vec_iter_has_next(&line_iter); vec_iter_advance(&line_iter)) {
            token_t *token = vec_iter_get(&line_iter);

            if (token->type == NEWLINE) {
                break;
            }
            if (token->type == GT) {
                done = true;
                vec_iter_advance(&line_iter);
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

fail:
    vec_destroy(&line);
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
        bool relative;
        if (*cur_path != '/' && *cur_path != '.') {
            relative = true;
            cur_path_len = optman.ccc_path_len + strlen(cur_path) + 1;
        } else {
            relative = false;
            cur_path_len = strlen(cur_path);
        }

        if (cur_path_len + strlen(filename) + 1 > PATH_MAX) {
            logger_log(mark, LOG_ERR, "Include path name too long");
            return CCC_ESYNTAX;
        }

        if (relative) {
            strcpy(include_path, optman.ccc_path);
            include_path[optman.ccc_path_len] = '/';
            strcpy(include_path + optman.ccc_path_len + 1, cur_path);
        } else {
            strcpy(include_path, cur_path);
        }
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
    return cpp_define_helper(cs, ts, CPP_MACRO_BASIC, false);
}

status_t cpp_define_helper(cpp_state_t *cs, vec_iter_t *ts,
                           cpp_macro_type_t type, bool has_eq) {
    token_t *token = vec_iter_get(ts);
    VERIFY_TOK_ID(token);

    status_t status = CCC_OK;

    cpp_macro_t *macro = emalloc(sizeof(*macro));
    macro->name = token->id_name;
    macro->mark = &token->mark;
    vec_init(&macro->stream, 0);
    vec_init(&macro->params, 0);
    macro->num_params = -1;
    macro->type = type;

    if (has_eq) {
        cpp_iter_advance(ts);
        if ((token = vec_iter_get(ts))->type == EQ) {
            vec_iter_advance(ts);
        }
    }

    // Don't want to skip spaces, because lparen must be right after macro name
    vec_iter_advance(ts);

    if (vec_iter_has_next(ts) && (token = vec_iter_get(ts))->type == LPAREN) {
        cpp_iter_advance(ts);
        macro->num_params = 0;

        bool done = false;
        bool first = true;
        bool vararg = false;

        for (; vec_iter_has_next(ts); cpp_iter_advance(ts)) {
            token = vec_iter_get(ts);
            if (token->type == NEWLINE) {
                break;
            }
            if (token->type == RPAREN) {
                cpp_iter_advance(ts);
                done = true;
                break;
            }
            if (vararg) {
                break;
            }
            if (!first) {
                if (token->type != COMMA) {
                    logger_log(&token->mark, LOG_ERR,
                               "macro parameters must be comma-separated");
                    status = CCC_ESYNTAX;
                    goto fail;
                }
                cpp_iter_advance(ts);
                token = vec_iter_get(ts);
            }
            if (token->type == ELIPSE) {
                vec_push_back(&macro->params, NULL);
                vararg = true;
            } else if (token->type == ID) {
                vec_push_back(&macro->params, token->id_name);
            } else {
                logger_log(&token->mark, LOG_ERR,
                           "\"%s\" may not appear in macro parameter list",
                           token_type_str(token->type));
                status = CCC_ESYNTAX;
                goto fail;
            }
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

    cpp_iter_skip_space(ts); // Skip space between header and body

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
        if (old_macro->type != CPP_MACRO_BASIC ||
            !cpp_macro_equal(macro, old_macro)) {
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
    (void)output;
    token_t *token = vec_iter_get(ts);
    VERIFY_TOK_ID(token);

    cpp_macro_t *macro = ht_remove(&cs->macros, &token->id_name);
    cpp_macro_destroy(macro);

    return CCC_OK;
}

status_t cpp_dir_ifdef(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    bool taken;
    if (cs->ignore) {
        cpp_skip_line(ts, true);
        taken = false;
    } else {
        token_t *token = vec_iter_get(ts);
        VERIFY_TOK_ID(token);
        cpp_iter_advance(ts);

        cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
        taken = macro != NULL;
    }
    return cpp_if_helper(cs, ts, output, taken);
}

status_t cpp_dir_ifndef(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    bool taken;
    if (cs->ignore) {
        cpp_skip_line(ts, true);
        taken = false;
    } else {
        token_t *token = vec_iter_get(ts);
        VERIFY_TOK_ID(token);
        cpp_iter_advance(ts);

        cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
        taken = macro == NULL;
    }
    return cpp_if_helper(cs, ts, output, taken);
}

status_t cpp_dir_if(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    status_t status = CCC_OK;
    bool taken;
    if (cs->ignore) {
        cpp_skip_line(ts, true);
        taken = false;
    } else {
        long long val;
        if (CCC_OK != (status = cpp_evaluate_line(cs, ts, &val))) {
            return status;
        }
        taken = val != 0;
    }

    return cpp_if_helper(cs, ts, output, taken);
}

status_t cpp_evaluate_line(cpp_state_t *cs, vec_iter_t *ts, long long *val) {
    bool ignore_save = cs->ignore;
    cs->ignore = false;

    status_t status = CCC_OK;
    trans_unit_t *ast = ast_trans_unit_create(true);
    *val = 0;

    vec_t line;
    vec_init(&line, 0);

    if (CCC_OK != (status = cpp_expand_line(cs, ts, &line, true))) {
        goto fail;
    }

    expr_t *expr = NULL;
    if (CCC_OK != (status = parser_parse_expr(&line, ast, &expr))) {
        goto fail;
    }

    if (!typecheck_const_expr(expr, val, true)) {
        status = CCC_ESYNTAX;
        goto fail;
    }

fail:
    ast_destroy(ast);
    cpp_skip_line(ts, false);
    vec_destroy(&line);

    cs->ignore = ignore_save;
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

    // Mark the level of the current unignored if
    if (!ignore_save) {
        cs->if_level = cs->if_count;
    }

    do {
        if (CCC_BACKTRACK != (status = cpp_expand(cs, ts, output)) &&
            cs->last_dir != CPP_DIR_endif) {
            if (status == CCC_OK) {
                logger_log(&start_token->mark, LOG_ERR, "Unterminted #if");
            }
            goto fail;
        }
        // If last directive was a taken branch, then mark if_taken as true
        if (cs->if_taken && !ignore_save && !if_taken) {
            if_taken = true;
            cs->ignore = false; // Stop ignoring if last branch taken
        } else {
            cs->ignore = true;
        }
        cs->if_taken = false;
    } while (cs->last_dir != CPP_DIR_endif);
    status = CCC_OK;

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

    if (cs->if_level == cs->if_count) {
        long long val;
        if (CCC_OK != (status = cpp_evaluate_line(cs, ts, &val))) {
            return status;
        }

        // This if is taken if it's value evaluated to non zero
        cs->if_taken = val != 0;
    }

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

status_t cpp_dir_line(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    (void)output;
    status_t status = CCC_OK;
    token_t *head = vec_iter_get(ts);

    vec_t line;
    vec_init(&line, 0);

    if (CCC_OK != (status = cpp_expand_line(cs, ts, &line, false))) {
        goto fail;
    }

    for (size_t i = 0; i < vec_size(&line); ++i) {
        token_t *token = vec_get(&line, i);
        switch (i) {
        case 0:
            if (token->type != INTLIT) {
                logger_log(&token->mark, LOG_ERR,
                           "\"%s\" after #line is not a positive integer",
                           token_type_str(token->type));
                status = CCC_ESYNTAX;
                goto fail;
            }
            // -1 because this line value applies to next line
            cs->line_mod = token->int_params->int_val - 1;
            cs->line_orig = head->mark.line;
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
    vec_destroy(&line);
    return status;
}
