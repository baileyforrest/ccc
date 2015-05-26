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
 * C preprocessor implementation
 */

#include "cpp.h"
#include "cpp_priv.h"
#include "cpp_directives.h"

#include <assert.h>
#include <time.h>

#include "optman.h"
#include "util/logger.h"
#include "util/string_store.h"
#include "util/file_directory.h"

#define VARARG_NAME "__VA_ARGS__"
#define TIME_DATE_BUF_SZ 128

static char *s_search_path[] = {
    "", // Denotes current directory
    "/usr/local/include",

    // TODO1: conditionally compile these
    "/usr/lib/gcc/x86_64-unknown-linux-gnu/4.9.2/include",

    "/usr/include",

};

static char *s_predef_macros[] = {
    // Standard C required macros
    "__STDC__ 1", // ISO C
    "__STDC_VERSION__ 201112L", // C11
    "__STDC_HOSTED__ 1", // stdlib available

    "__STDC_UTF_16__ 1", // UTF16 supported
    "__STDC_UTF_32__ 1", // UTF32 supported

    // We don't support these C features
    "__STDC_NO_ATOMICS__ 1",
    "__STDC_NO_COMPLEX__ 1",
    "__STDC_NO_THREADS__ 1",
    "__STDC_NO_VLA__ 1",

    // Required for compatability
    "__alignof__ _Alignof",
    "__FUNCTION__ __func__",

#ifdef __x86_64__
    "__amd64 1",
    "__amd64__ 1",
    "__x86_64 1",
    "__x86_64__ 1",
#endif

#ifdef __linux
    "__linux 1",
    "__linux__ 1",
    "__gnu_linux__ 1",
    "__unix 1",
    "__unix__ 1",
    "_LP64 1",
    "__LP64__ 1",
    "__ELF__ 1",
#endif

    // TODO1: Conditionally compile or handle these better
    "char16_t short",
    "char32_t int"
};

static struct {
    char *name;
    cpp_macro_type_t type;
} s_special_macros[] = {
    { "__FILE__", CPP_MACRO_FILE },
    { "__LINE__", CPP_MACRO_LINE },
    { "__DATE__", CPP_MACRO_DATE },
    { "__TIME__", CPP_MACRO_TIME }
};

extern void cpp_iter_skip_space(vec_iter_t *iter);

status_t cpp_state_init(cpp_state_t *cs, token_man_t *token_man,
                        lexer_t *lexer) {
    status_t status = CCC_OK;

    static const ht_params_t params = {
        STATIC_ARRAY_LEN(s_predef_macros), // Size estimate
        offsetof(cpp_macro_t, name),         // Offset of key
        offsetof(cpp_macro_t, link),         // Offset of ht link
        ind_str_hash,                        // Hash function
        ind_str_eq,                          // void string compare
    };

    ht_init(&cs->macros, &params);
    vec_init(&cs->search_path, STATIC_ARRAY_LEN(s_search_path));

    cs->filename = NULL;
    cs->token_man = token_man;
    cs->lexer = lexer;
    cs->cur_filename = NULL;
    cs->line_mod = 0;
    cs->line_orig = 0;
    cs->if_count = 0;
    cs->if_taken = false;
    cs->ignore = false;
    cs->last_dir = CPP_DIR_NONE;

    // Add search path from command line options
    VEC_FOREACH(cur, &optman.include_paths) {
        vec_push_back(&cs->search_path, vec_get(&optman.include_paths, cur));
    }

    // Add default search path
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_search_path); ++i) {
        vec_push_back(&cs->search_path, s_search_path[i]);
    }

    // Add special macros
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_special_macros); ++i) {
        if (CCC_OK !=
            (status = cpp_macro_define(cs, s_special_macros[i].name,
                                       s_special_macros[i].type, false))) {
            return status;
        }
    }

    // Add default macros
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_predef_macros); ++i) {
        if (CCC_OK !=
            (status = cpp_macro_define(cs, s_predef_macros[i], CPP_MACRO_BASIC,
                                       false))) {
            return status;
        }
    }

    VEC_FOREACH(cur, &optman.macros) {
        char *macro = vec_get(&optman.macros, cur);
        if (CCC_OK != (status = cpp_macro_define(cs, macro, CPP_MACRO_BASIC,
                                                 false))) {
            return status;
        }
    }

    return CCC_OK;
}

void cpp_macro_destroy(cpp_macro_t *macro) {
    if (macro == NULL) {
        return;
    }

    vec_destroy(&macro->stream);
    vec_destroy(&macro->params);
    free(macro);
}

void cpp_macro_inst_destroy(cpp_macro_inst_t *macro_inst) {
    SL_FOREACH(cur, &macro_inst->args) {
        cpp_macro_param_t *param = GET_ELEM(&macro_inst->args, cur);
        vec_destroy(&param->stream);
    }

    SL_DESTROY_FUNC(&macro_inst->args, free);
}

void cpp_state_destroy(cpp_state_t *cs) {
    HT_DESTROY_FUNC(&cs->macros, cpp_macro_destroy);
    vec_destroy(&cs->search_path);
}

token_t *cpp_iter_advance(vec_iter_t *iter) {
    if (!vec_iter_has_next(iter)) {
        return NULL;
    }
    token_t *cur = vec_iter_advance(iter);
    cpp_iter_skip_space(iter);

    return cur;
}

token_t *cpp_iter_lookahead(vec_iter_t *iter, size_t lookahead) {
    vec_iter_t temp;
    memcpy(&temp, iter, sizeof(temp));

    while (vec_iter_has_next(iter) && lookahead--) {
        cpp_iter_advance(&temp);
    }

    return vec_iter_has_next(&temp) ? vec_iter_get(&temp) : NULL;
}

size_t cpp_skip_line(vec_iter_t *ts, bool skip_newline) {
    size_t skipped = 0;

    for (; vec_iter_has_next(ts); cpp_iter_advance(ts)) {
        token_t *token = vec_iter_get(ts);
        if (token->type == NEWLINE) {
            if (skip_newline) {
                cpp_iter_advance(ts);
            }
            break;
        }
        ++skipped;
    }

    return skipped;
}

bool cpp_macro_equal(cpp_macro_t *m1, cpp_macro_t *m2) {
    if (m1 == m2) {
        return true;
    }

    if (m1->num_params != m2->num_params) {
        return false;
    }

    if (strcmp(m1->name, m2->name) != 0) {
        return false;
    }
    if (vec_size(&m1->params) != vec_size(&m2->params)) {
        return false;
    }
    for (size_t i = 0; i < vec_size(&m1->params); ++i) {
        if (strcmp(vec_get(&m1->params, i), vec_get(&m2->params, i)) != 0) {
            return false;
        }
    }

    vec_iter_t stream1 = { &m1->stream, 0 }, stream2 = { &m2->stream, 0 };
    cpp_iter_skip_space(&stream1);
    cpp_iter_skip_space(&stream2);
    while (vec_iter_has_next(&stream1) && vec_iter_has_next(&stream2)) {
        token_t *t1 = vec_iter_get(&stream1);
        token_t *t2 = vec_iter_get(&stream2);
        if (!token_equal(t1, t2)) {
            return false;
        }

        cpp_iter_advance(&stream1);
        cpp_iter_advance(&stream2);
    }

    if (vec_iter_has_next(&stream1) || vec_iter_has_next(&stream2)) {
        return false;
    }

    return true;
}

vec_t *cpp_macro_inst_lookup(cpp_macro_inst_t *inst, char *arg_name) {
    SL_FOREACH(cur, &inst->args) {
        cpp_macro_param_t *param = GET_ELEM(&inst->args, cur);
        if (strcmp(param->name, arg_name) == 0) {
            return &param->stream;
        }
    }

    return NULL;
}

status_t cpp_macro_define(cpp_state_t *cs, char *string,
                          cpp_macro_type_t type, bool has_eq) {
    status_t status = CCC_OK;
    tstream_t stream;
    ts_init(&stream, string, string + strlen(string), COMMAND_LINE_FILENAME,
            NULL);
    vec_t tokens;
    vec_init(&tokens, 0);

    if (CCC_OK != (status = lexer_lex_stream(cs->lexer, &stream, &tokens))) {
        goto fail;
    }

    vec_iter_t tstream = { &tokens, 0 };
    if (CCC_OK != (status = cpp_define_helper(cs, &tstream, type, has_eq))) {
        goto fail;
    }

fail:
    vec_destroy(&tokens);
    return status;
}

status_t cpp_process(token_man_t *token_man, lexer_t *lexer, char *filepath,
                     vec_t *output) {
    status_t status = CCC_OK;

    cpp_state_t cs;
    if (CCC_OK != (status = cpp_state_init(&cs, token_man, lexer))) {
        goto fail;
    }
    cs.cur_filename = filepath;

    if (CCC_OK != (status = cpp_process_file(&cs, filepath, output))) {
        goto fail;
    }

fail:
    cpp_state_destroy(&cs);
    return status;
}

status_t cpp_process_file(cpp_state_t *cs, char *filename, vec_t *output) {
    status_t status = CCC_OK;
    char *filename_save = cs->filename;
    cs->filename = filename;

    vec_t file_tokens;
    vec_init(&file_tokens, 0);

    fdir_entry_t *entry;
    if (CCC_OK != (status = fdir_insert(filename, &entry))) {
        goto fail;
    }

    tstream_t stream;
    ts_init(&stream, entry->buf, entry->end, entry->filename, NULL);

    if (CCC_OK != (status = lexer_lex_stream(cs->lexer, &stream,
                                             &file_tokens))) {
        goto fail;
    }

    vec_iter_t iter = { &file_tokens, 0 };
    cpp_iter_skip_space(&iter);
    if (CCC_OK != (status = cpp_expand(cs, &iter, output))) {
        goto fail;
    }

fail:
    vec_destroy(&file_tokens);
    cs->filename = filename_save;
    return status;
}

status_t cpp_expand(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    status_t status = CCC_OK;

    token_t *token = NULL, *last = NULL;
    for (; vec_iter_has_next(ts); last = token, cpp_iter_advance(ts)) {
        token = vec_iter_get(ts);

        // If we're ignoring and, we only want to check # directives
        if (cs->ignore && token->type != HASH) {
            continue;
        }

        switch (token->type) {
        case HASH:
            if (last != NULL && last->type != NEWLINE) {
                logger_log(&token->mark, LOG_ERR, "stray '#' in program");
                break;
            }
            cpp_iter_advance(ts);
            if (CCC_OK != (status = cpp_handle_directive(cs, ts, output))) {
                return status;
            }

            // Set last as NULL to allow another directive following this one
            token = NULL;
            continue;
        case ID:
            break;

        case HASHHASH:
            logger_log(&token->mark, LOG_ERR, "stray '##' in program");
        case NEWLINE:
            // ignore these
            continue;

        default:
            // If the token isn't one of the above, just transfer it
            vec_push_back(output, token);
            continue;
        }

        assert(token->type == ID);

        // If the current token is a member of its hideset, just pass it
        if (str_set_mem(token->hideset, token->id_name)) {
            vec_push_back(output, token);
            continue;
        }

        token_t *next = cpp_iter_lookahead(ts, 1);

        cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
        if (macro == NULL ||
            (macro->num_params != -1 && next->type != LPAREN)) {
            // Just continue if this isn't a macro, or a function like macro
            // without a paren next
            vec_push_back(output, token);
            continue;
        }
        if (macro->type != CPP_MACRO_BASIC) {
            cpp_handle_special_macro(cs, &token->mark, macro->type, output);
        }

        cpp_macro_inst_t macro_inst =
            { macro, SLIST_LIT(offsetof(cpp_macro_param_t, link)) };

        str_set_t *hideset; // Macro's hideset
        vec_t subbed;
        vec_init(&subbed, 0);

        // Object like macro
        if (macro->num_params == -1) {
            hideset = str_set_copy(token->hideset);
            hideset = str_set_add(hideset, token->id_name);
            if (CCC_OK !=
                (status = cpp_substitute(cs, &macro_inst, hideset, &subbed))) {
                goto fail;
            }
        } else {
            cpp_iter_advance(ts); // Skip lparen
            if (CCC_OK !=
                (status = cpp_fetch_macro_params(cs, ts, &macro_inst))) {
                goto fail;
            }
            token_t *rparen = vec_iter_get(ts);

            assert(rparen->type == RPAREN);
            hideset = str_set_intersect(token->hideset, rparen->hideset);
            hideset = str_set_add(hideset, token->id_name);
            if (CCC_OK !=
                (status = cpp_substitute(cs, &macro_inst, hideset, &subbed))) {
                goto fail;
            }
        }

        // Expand the result of the substitution
        vec_iter_t sub_iter = { &subbed, 0 };
        cpp_expand(cs, &sub_iter, output);

        cpp_macro_inst_destroy(&macro_inst);
        str_set_destroy(hideset);
        vec_destroy(&subbed);
        continue;

    fail:
        cpp_macro_inst_destroy(&macro_inst);
        str_set_destroy(hideset);
        vec_destroy(&subbed);
        goto done;
    }

done:
    return status;
}

status_t cpp_substitute(cpp_state_t *cs, cpp_macro_inst_t *macro_inst,
                        str_set_t *hideset, vec_t *output) {
    status_t status = CCC_OK;
    vec_iter_t iter = { &macro_inst->macro->stream, 0 };
    vec_t temp;
    vec_init(&temp, 0);

    for (; vec_iter_has_next(&iter); cpp_iter_advance(&iter)) {
        token_t *token = vec_iter_get(&iter);
        vec_t *param_vec;

        if (token->type == HASH) { // Handle stringification
            cpp_iter_advance(&iter);
            token_t *param = vec_iter_get(&iter);
            if (param->type != ID ||
                NULL == (param_vec =
                         cpp_macro_inst_lookup(macro_inst, param->id_name))) {
                logger_log(&param->mark, LOG_ERR,
                           "'#' is not followed by a macro parameter");
                goto fail;
            }

            token_t *stringified = cpp_stringify(cs, param_vec);
            vec_push_back(&temp, stringified);
        } else if (token->type == HASHHASH) { // Concatenation
            cpp_iter_advance(&iter);
            token_t *next = vec_iter_get(&iter);

            if (next->type == ID &&
                NULL != (param_vec =
                         cpp_macro_inst_lookup(macro_inst, next->id_name))) {
                // Macro parameter: glue tho whole parameter
                vec_iter_t param_iter = { param_vec, 0 };
                if (CCC_OK != (status = cpp_glue(cs, &temp, &param_iter, 0))) {
                    goto fail;
                }
            } else { // Not macro param, just glue the next token
                if (CCC_OK != (status = cpp_glue(cs, &temp, &iter, 1))) {
                    goto fail;
                }
            }
        } else if (token->type == ID && NULL !=
                   (param_vec = cpp_macro_inst_lookup(macro_inst,
                                                       token->id_name))) {
            // Found a macro paramater
            token_t *next = cpp_iter_lookahead(&iter, 1);

            if (next != NULL && next->type == HASHHASH) {
                // Next token is a paste
                if (vec_size(param_vec) == 0) {
                    // Empty parameter vector
                    token_t *after_paste = cpp_iter_lookahead(&iter, 2);

                    // If next token is a macro parameter, then skip the
                    // next two tokens and output that param's tokens
                    if (after_paste->type == ID &&
                        NULL != (param_vec =
                                 cpp_macro_inst_lookup(macro_inst,
                                                       after_paste->id_name))) {
                        cpp_iter_advance(&iter); // skip empty param
                        cpp_iter_advance(&iter); // skip ##
                        vec_append_vec(&temp, param_vec);
                    }
                } else {
                    // Just append all the tokens onto the output if pasting
                    vec_append_vec(&temp, param_vec);
                }
            } else {
                // Macro param not followed by ##, expand it onto the output
                vec_iter_t iter = { param_vec, 0 };
                cpp_expand(cs, &iter, &temp);
            }
        } else {
            // Regular token, just put it on the output
            vec_push_back(&temp, token);
        }
    }

    // Add the hideset passed into this function to all output tokens
    VEC_FOREACH(cur, &temp) {
        token_t *token = vec_get(&temp, cur);

        // Make a copy and add it to output
        token_t *copy = token_copy(cs->token_man, token);
        copy->hideset = str_set_union_inplace(copy->hideset, hideset);
        vec_push_back(output, copy);
    }

fail:
    vec_destroy(&temp);
    return status;
}

status_t cpp_handle_directive(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    status_t status = CCC_OK;
    token_t *token = vec_iter_get(ts);
    fmark_t *mark = &token->mark;
    char *tok_str = token_str(token);

    // Single # on a line allowed
    if (token->type == NEWLINE || !vec_iter_has_next(ts)) {
        return CCC_OK;
    }

    cpp_directive_t *dir = NULL;

    for (dir = directives; ; ++dir) {
        if (dir->name == NULL) {
            dir = NULL;
            break;
        }

        if (strcmp(dir->name, tok_str) == 0) {
            break;
        }
    }

    if (dir == NULL) {
        logger_log(&token->mark, LOG_ERR, "invalid preprocessing directive #%s",
                   tok_str);
        status = CCC_ESYNTAX;
    } else {
        cpp_iter_advance(ts); // Skip the directive name
        if (!cs->ignore || !dir->if_ignore) {
            status = dir->func(cs, ts, output);
            cs->last_dir = dir->type;
        }
    }

    if (cpp_skip_line(ts, false) > 1) {
        if (!cs->ignore && dir != NULL && status == CCC_OK) {
            logger_log(mark, LOG_WARN, "extra tokens at end of #%s directive",
                       dir->name);
        }
    }

    free(tok_str);
    return status;
}

status_t cpp_fetch_macro_params(cpp_state_t *cs, vec_iter_t *ts,
                                cpp_macro_inst_t *macro_inst) {
    (void)cs;
    token_t *lparen = vec_iter_get(ts);
    assert(lparen->type == LPAREN);
    cpp_iter_advance(ts);

    cpp_macro_t *macro = macro_inst->macro;
    assert(macro->num_params >= 0);

    int num_params = 0;

    bool done = false;
    int cur = 0;

    while (!done) {
        bool vararg = false;
        char *arg = NULL;
        cpp_macro_param_t *param = NULL;

        if (cur < macro->num_params) {
            arg = vec_get(&macro->params, cur);
            param = emalloc(sizeof(cpp_macro_param_t));
            vec_init(&param->stream, 0);

            if (arg == NULL) { // NULL arg denotes vararg
                assert(cur == macro->num_params - 1); // Must be last argument
                vararg = true;
                param->name = VARARG_NAME;
            } else {
                param->name = arg;
            }
        }
        token_t *token = vec_iter_get(ts);
        if (token->type != RPAREN) {
            ++num_params;
        }

        int parens = 0;
        for (; vec_iter_has_next(ts); cpp_iter_advance(ts)) {
            token_t *token = vec_iter_get(ts);
            if (token->type == LPAREN) {
                ++parens;
            } else if (parens > 0 && token->type == RPAREN) {
                --parens;
            } else if (parens == 0) {
                if (token->type == COMMA && !vararg) {
                    cpp_iter_advance(ts);
                    break;
                }
                if (token->type == RPAREN) {
                    done = true;
                    break;
                }
            }

            if (cur < macro->num_params) {
                vec_push_back(&param->stream, token);
            }
        }

        if (cur < macro->num_params) {
            sl_append(&macro_inst->args, &param->link);
            ++cur;
        }
    }

    if (num_params != macro->num_params) {
        logger_log(&lparen->mark, LOG_ERR,
                   "macro \"%s\" passed %d arguments, but takes %d",
                   macro->name, num_params, macro->num_params);
        return CCC_ESYNTAX;
    }

    return CCC_OK;
}

token_t *cpp_stringify(cpp_state_t *cs, vec_t *ts) {
    string_builder_t sb;
    sb_init(&sb, 0);

    token_t *token = token_create(cs->token_man);
    token->type = STRING;
    token->start = NULL;
    token->len = 0;
    token_t *first = vec_get(ts, 0);
    memcpy(&token->mark, &first->mark, sizeof(fmark_t));

    token_t *last = NULL;
    VEC_FOREACH(cur, ts) {
        token_t *token = vec_get(ts, cur);

        // Combine multiple spaces into one
        if (token->type == SPACE && last != NULL && last->type == SPACE) {
            continue;
        }

        token_str_append_sb(&sb, token);
        last = token;
    }

    token->str_val = sstore_lookup(sb_buf(&sb));
    sb_destroy(&sb);

    return token;
}

status_t cpp_glue(cpp_state_t *cs, vec_t *left, vec_iter_t *right,
                  size_t nelems) {
    status_t status = CCC_OK;

    if (!vec_iter_has_next(right)) { // Do nothing if right is empty
        return status;
    }

    token_t *rhead = vec_iter_get(right);

    if (vec_size(left) == 0) {
        vec_push_back(left, rhead);
    } else {
        // Remove the last token of the left
        token_t *ltail = vec_pop_back(left);

        // Combine right most left token and left most right token
        // Just print them and lex it into new tokens
        string_builder_t sb;
        sb_init(&sb, 0);

        token_str_append_sb(&sb, ltail);
        token_str_append_sb(&sb, rhead);

        tstream_t stream;
        char *buf = sstore_lookup(sb_buf(&sb));
        ts_init(&stream, buf, buf + sb_len(&sb), ltail->mark.filename, NULL);

        size_t init_size = vec_size(left);

        status = lexer_lex_stream(cs->lexer, &stream, left);
        // Must be valid if this already broke into tokens
        assert(status == CCC_OK);

        size_t post_size = vec_size(left);
        if (post_size > init_size + 1) {
            logger_log(&ltail->mark, LOG_ERR,
                       "pasting \"%s\" and \"%s\" does not give a valid "
                       "preprocessing token", token_type_str(ltail->type),
                       token_type_str(rhead->type));
            status = CCC_ESYNTAX;
        }

        sb_destroy(&sb);
    }

    // 0 wraps around to SIZE_MAX
    --nelems;

    while (nelems-- > 0) {
        cpp_iter_advance(right);
        if (!vec_iter_has_next(right)) {
            break;
        }
        vec_push_back(left, vec_iter_get(right));
    }

    return CCC_OK;
}

void cpp_handle_special_macro(cpp_state_t *cs, fmark_t *mark,
                              cpp_macro_type_t type, vec_t *output) {
    token_t *token = token_create(cs->token_man);
    memcpy(&token->mark, mark, sizeof(fmark_t));

    char buf[TIME_DATE_BUF_SZ];

    time_t t;
    struct tm *tm;

    switch (type) {
    case CPP_MACRO_FILE:
        token->type = STRING;
        token->str_val = cs->cur_filename;
        break;
    case CPP_MACRO_LINE:
        token->type = INTLIT;
        token->int_params = emalloc(sizeof(token_int_params_t));
        token->int_params->hasU = false;
        token->int_params->hasL = false;
        token->int_params->hasLL = false;
        token->int_params->int_val = mark->line - cs->line_orig + cs->line_mod;
        break;
    case CPP_MACRO_DATE:
        token->type = STRING;
        if (-1 == (t = time(NULL)) || NULL == (tm = localtime(&t))) {
            logger_log(mark, LOG_WARN, "Failed to get Date!");
            snprintf(buf, sizeof(buf), "??? ?? ????");
        } else {
            strftime(buf, sizeof(buf), "%b %d %Y", tm);
        }
        token->str_val = sstore_lookup(buf);
        break;
    case CPP_MACRO_TIME:
        token->type = STRING;
        if (-1 == (t = time(NULL)) || NULL == (tm = localtime(&t))) {
            logger_log(mark, LOG_WARN, "Failed to get Time!");
            snprintf(buf, sizeof(buf), "??:??:??");
        } else {
            strftime(buf, sizeof(buf), "%T", tm);
        }
        token->str_val = sstore_lookup(buf);
        break;
    default:
        assert(false);
    }

    vec_push_back(output, token);
}
