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

// TODO0: Need to copy in some places for expand/substitute

#include <assert.h>

#include "cpp.h"
#include "cpp_priv.h"
#include "cpp_directives.h"

#include "optman.h"

#include "util/logger.h"
#include "util/string_store.h"

#define VARARG_NAME "__VA_ARGS__"

static char *search_path[] = {
    "", // Denotes current directory
    "/usr/local/include",

    // TODO1: conditionally compile these
    "/usr/lib/gcc/x86_64-unknown-linux-gnu/4.9.2/include"

    "/usr/include",

};

static char *predef_macros[] = {
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

status_t cpp_state_init(cpp_state_t *cs, token_man_t *token_man,
                        lexer_t *lexer) {
    status_t status = CCC_OK;

    static const ht_params_t params = {
        0,                           // Size estimate
        offsetof(cpp_macro_t, name), // Offset of key
        offsetof(cpp_macro_t, link), // Offset of ht link
        ind_str_hash,                // Hash function
        ind_str_eq,                  // void string compare
    };

    cs->filename = NULL;
    cs->token_man = token_man;
    cs->lexer = lexer;
    ht_init(&cs->macros, &params);
    vec_init(&cs->search_path, STATIC_ARRAY_LEN(search_path));
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
    for (size_t i = 0; i < STATIC_ARRAY_LEN(search_path); ++i) {
        vec_push_back(&cs->search_path, search_path[i]);
    }

    // Add default macros
    for (size_t i = 0; i < STATIC_ARRAY_LEN(predef_macros); ++i) {
        if (CCC_OK !=
            (status = cpp_macro_define(cs, predef_macros[i], false))) {
            return status;
        }
    }

    VEC_FOREACH(cur, &optman.macros) {
        char *macro = vec_get(&optman.macros, cur);
        if (CCC_OK != (status = cpp_macro_define(cs, macro, false))) {
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

void cpp_state_destroy(cpp_state_t *cs) {
    HT_DESTROY_FUNC(&cs->macros, cpp_macro_destroy);
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

token_t *cpp_iter_advance(vec_iter_t *iter) {
    cpp_iter_skip_space(iter);
    return vec_iter_advance(iter);
}

void cpp_iter_skip_space(vec_iter_t *iter) {
    for (; vec_iter_has_next(iter); vec_iter_advance(iter)) {
        token_t *token = vec_iter_get(iter);
        if (token->type != SPACE) {
            break;
        }
    }
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

status_t cpp_macro_define(cpp_state_t *cs, char *string, bool has_eq) {
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
    if (CCC_OK != (status = cpp_define_helper(cs, &tstream, has_eq))) {
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
        return status;
    }
    cs.cur_filename = ccc_basename(filepath);

    status = cpp_process_file(&cs, filepath, output);

    cpp_state_destroy(&cs);

    return status;
}

status_t cpp_process_file(cpp_state_t *cs, char *filename, vec_t *output) {
    status_t status = CCC_OK;
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
    status = cpp_expand(cs, &iter, output);

fail:
    vec_destroy(&file_tokens);
    return status;
}

status_t cpp_expand(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    status_t status = CCC_OK;

    token_t *new_last = NULL, *last = NULL;
    for (; vec_iter_has_next(ts); cpp_iter_advance(ts), last = new_last) {
        token_t *token = vec_iter_get(ts);
        new_last = token;

        if (cs->ignore && token->type != HASHHASH) {
            continue;
        }

        switch (token->type) {
        case HASH:
            if (last->type != NEWLINE) {
                logger_log(&token->mark, LOG_ERR, "stray '#' in program");
                break;
            }
            if (CCC_OK != (status = cpp_handle_directive(cs, ts, output))) {
                return status;
            }
            continue;
        case ID:
            break;

        case HASHHASH:
            logger_log(&token->mark, LOG_ERR, "stray '##' in program");
        case NEWLINE:
            // ignore spaces these
            continue;

        default:
            // If the token isn't one of the above, just transfer it
            vec_push_back(output, token);
            continue;
        }

        assert(token->type == ID);

        // If the current token is a member of the hideset, just pass it
        if (str_set_mem(token->hideset, token->id_name)) {
            vec_push_back(output, token);
            continue;
        }

        cpp_iter_advance(ts);
        token_t *next = vec_iter_get(ts);
        vec_iter_reverse(ts);

        cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
        if (macro == NULL ||
            (macro->num_params != -1 && next->type != LPAREN)) {
            vec_push_back(output, token);
            continue;
        }

        cpp_macro_inst_t macro_inst =
            { macro, SLIST_LIT(offsetof(cpp_macro_param_t, link)) };

        str_set_t *hideset;

        vec_t subbed;
        vec_init(&subbed, 0);

        // Object like macro
        if (macro->num_params == 0) {
            hideset = str_set_copy(token->hideset);
            str_set_add(hideset, token->id_name);
            if (CCC_OK !=
                (status = cpp_substitute(cs, &macro_inst, hideset, &subbed))) {
                goto fail;
            }
        } else {
            if (CCC_OK !=
                (status = cpp_fetch_macro_params(cs, ts, &macro_inst))) {
                goto fail;
            }
            token_t *rparen = vec_iter_get(ts);
            assert(rparen->type == RPAREN);
            hideset = str_set_intersect(token->hideset, rparen->hideset);
            str_set_add(hideset, token->id_name);
            if (CCC_OK !=
                (status = cpp_substitute(cs, &macro_inst, hideset, &subbed))) {
                goto fail;
            }
        }

        VEC_FOREACH(cur, &subbed) {
            token_t *token = vec_get(&subbed, cur);
            vec_push_back(output, token);
        }

        str_set_destroy(hideset);
        vec_destroy(&subbed);
        continue;

    fail:
        str_set_destroy(hideset);
        vec_destroy(&subbed);
        goto done;
    }

done:
    return status;
}

status_t cpp_substitute(cpp_state_t *cs, cpp_macro_inst_t *macro_inst,
                        str_set_t *hideset, vec_t *output) {
    vec_iter_t iter = { &macro_inst->macro->stream, 0 };
    status_t status = CCC_OK;

    for (; vec_iter_has_next(&iter); cpp_iter_advance(&iter)) {
        token_t *token = vec_iter_get(&iter);
        vec_t *param_vec;

        if (token->type == HASH) {
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
            vec_push_back(output, stringified);
        } else if (token->type == HASHHASH) {
            cpp_iter_advance(&iter);
            token_t *next = vec_iter_get(&iter);
            if (next->type == ID &&
                NULL != (param_vec =
                         cpp_macro_inst_lookup(macro_inst, next->id_name))) {
                vec_iter_t param_iter = { param_vec, 0 };
                if (CCC_OK != (status = cpp_glue(cs, output, &param_iter, 0))) {
                    goto fail;
                }
            } else {
                if (CCC_OK != (status = cpp_glue(cs, output, &iter, 1))) {
                    goto fail;
                }
            }
        } else if (token->type == ID &&
                   NULL !=
                   (param_vec = cpp_macro_inst_lookup(macro_inst,
                                                       token->id_name))) {
            cpp_iter_advance(&iter);
            token_t *next = vec_iter_get(&iter);
            if (next->type == HASHHASH) {
                if (vec_size(param_vec) == 0) {
                    cpp_iter_advance(&iter);
                    token_t *next = vec_iter_get(&iter);
                    if (next->type == ID &&
                        NULL != (param_vec =
                                 cpp_macro_inst_lookup(macro_inst,
                                                       next->id_name))) {
                        vec_append(output, param_vec);
                    }
                } else {
                    vec_iter_reverse(&iter);
                    vec_append(output, param_vec);
                }
            } else {
                vec_iter_t iter = { param_vec, 0 };
                cpp_expand(cs, &iter, output);
            }
        } else {
            vec_push_back(output, token);
        }
    }

    VEC_FOREACH(cur, output) {
        token_t *token = vec_get(output, cur);
        str_set_union_inplace(token->hideset, hideset);
    }

fail:
    return status;
}

status_t cpp_handle_directive(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    status_t status = CCC_OK;

    token_t *token = vec_iter_get(ts);

    // Single # on a line allowed
    if (token->type == NEWLINE || token->type == TOKEN_EOF ||
        !vec_iter_has_next(ts)) {
        return CCC_OK;
    }

    token_t *name_token = token;

    cpp_directive_t *dir = NULL;

    if (token->type == ID) {
        for (dir = directives; ; ++dir) {
            if (dir->name == NULL) {
                dir = NULL;
                break;
            }

            if (strcmp(dir->name, token->id_name) == 0) {
                break;
            }
        }
    }

    if (dir == NULL) {
        char *tok_str = token_str(token);
        logger_log(&token->mark, LOG_ERR, "invalid preprocessing directive #%s",
                   token_str);
        free(tok_str);
        status = CCC_ESYNTAX;
    } else {
        cpp_iter_advance(ts); // Skip the directive name
        if (!cs->ignore || !dir->if_ignore) {
            status = dir->func(cs, ts, output);
            cs->last_dir = dir->type;
        }
    }

    if (cpp_skip_line(ts, true) > 1) {
        if (dir != NULL && status == CCC_OK) {
            logger_log(&name_token->mark, LOG_WARN,
                       "extra tokens at end of #%s directive",
                       dir->name);
        }
    }

    return status;
}

status_t cpp_fetch_macro_params(cpp_state_t *cs, vec_iter_t *ts,
                                cpp_macro_inst_t *macro_inst) {
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

            if (arg == NULL) {
                // Must be last argument
                assert(cur = macro->num_params - 1);
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
                    break;
                }
                if (token->type == RPAREN) {
                    done = true;
                    break;
                }
            }

            if (cur < macro->num_params) {
                token_t *copy = token_copy(cs->token_man, token);
                copy->mark.last = &lparen->mark;

                vec_push_back(&param->stream, copy);
            }
        }

        if (cur < macro->num_params) {
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
    token_t *first = vec_get(ts, 0);
    memcpy(&token->mark, &first->mark, sizeof(fmark_t));

    bool last_space = false;

    VEC_FOREACH(cur, ts) {
        token_t *token = vec_get(ts, cur);

        if (token->type == SPACE) {
            if (!last_space) {
                last_space = true;
            } else {
                continue;
            }
        } else {
            last_space = false;
        }

        token_str_append_sb(&sb, token);
    }

    token->str_val = sstore_lookup(sb_buf(&sb));
    sb_destroy(&sb);

    return token;
}

status_t cpp_glue(cpp_state_t *cs, vec_t *left, vec_iter_t *right,
                  size_t nelems) {
    status_t status = CCC_OK;

    size_t lsize = vec_size(left);

    token_t *rhead = cpp_iter_advance(right);

    if (lsize == 0) {
        vec_push_back(left, rhead);
    } else {
        token_t *ltail = vec_get(left, lsize - 1);

        // Remove the left token
        vec_pop_back(left);

        // Combine the two tokens
        string_builder_t sb;
        sb_init(&sb, 0);

        token_str_append_sb(&sb, ltail);
        token_str_append_sb(&sb, rhead);

        tstream_t stream;
        ts_init(&stream, sb_buf(&sb), sb_buf(&sb) + sb_len(&sb),
                "builtin", NULL);

        size_t init_size = vec_size(left);

        status = lexer_lex_stream(cs->lexer, &stream, left);
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

    while (vec_iter_has_next(right) && nelems-- > 0) {
        vec_push_back(left, cpp_iter_advance(right));
    }

    return CCC_OK;
}
