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

#include <assert.h>

#include "cpp.h"
#include "cpp_priv.h"

#include "util/logger.h"

// TODO0: Need to copy in some places for expand/substitute


status_t cpp_process(token_man_t *token_man, char *filename, vec_t *tokens,
                     vec_t *output) {
    cpp_state_t cs;
    cpp_state_init(&cs, token_man, filename);
    vec_iter_t iter = { tokens, 0 };
    status_t status = cpp_expand(&cs, &iter, output);

    cpp_state_destroy(&cs);

    return status;
}

void cpp_state_init(cpp_state_t *cs, token_man_t *token_man, char *filename) {
    static const ht_params_t params = {
        0,                           // Size estimate
        offsetof(cpp_macro_t, name), // Offset of key
        offsetof(cpp_macro_t, link), // Offset of ht link
        ind_str_hash,                // Hash function
        ind_str_eq,                  // void string compare
    };

    ht_init(&cs->macros, &params);
    cs->filename = filename;
    cs->token_man = token_man;
}

void cpp_macro_destroy(cpp_macro_t *macro) {
    vec_destroy(&macro->stream);
    free(macro);
}

void cpp_state_destroy(cpp_state_t *cs) {
    HT_DESTROY_FUNC(&cs->macros, cpp_macro_destroy);
}

status_t cpp_expand(cpp_state_t *cs, vec_iter_t *ts, vec_t *output) {
    status_t status = CCC_OK;

    lexeme_t *new_last = NULL, *last = NULL;
    for (; vec_iter_has_next(ts); vec_iter_advance(ts), last = new_last) {
        lexeme_t *token = vec_iter_get(ts);
        new_last = token;

        switch (token->type) {
        case HASH:
            if (last->type != NEWLINE) {
                logger_log(&token->mark, LOG_ERR, "stray '#' in program");
                break;
            }
            cpp_handle_directive(cs, ts);
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
        if (str_set_mem(&token->hideset, token->id_name)) {
            vec_push_back(output, token);
            continue;
        }

        cpp_macro_t *macro = ht_lookup(&cs->macros, &token->id_name);
        if (macro == NULL) {
            vec_push_back(output, token);
            continue;
        }

        cpp_macro_inst_t macro_inst =
            { macro, SLIST_LIT(offsetof(cpp_macro_param_t, link)) };

        str_set_t hideset;

        vec_t subbed;
        vec_init(&subbed, 0);

        // Object like macro
        if (macro->num_params == 0) {
            str_set_copy(&hideset, &token->hideset);
            str_set_add(&hideset, token->id_name);
            if (CCC_OK !=
                (status = cpp_substitute(cs, &macro_inst, &hideset, &subbed))) {
                goto fail;
            }
        } else {
            if (CCC_OK !=
                (status = cpp_fetch_macro_params(cs, ts, &macro_inst))) {
                goto fail;
            }
            lexeme_t *rparen = vec_iter_get(ts);
            assert(rparen->type == RPAREN);
            str_set_intersect(&token->hideset, &rparen->hideset, &hideset);
            str_set_add(&hideset, token->id_name);
            if (CCC_OK !=
                (status = cpp_substitute(cs, &macro_inst, &hideset, &subbed))) {
                goto fail;
            }
        }

        VEC_FOREACH(cur, &subbed) {
            lexeme_t *token = vec_get(&subbed, cur);
            vec_push_back(output, token);
        }

        str_set_destroy(&hideset);
        vec_destroy(&subbed);
        continue;

    fail:
        str_set_destroy(&hideset);
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

    for (; vec_iter_has_next(&iter); vec_iter_advance(&iter)) {
        lexeme_t *token = vec_iter_get(&iter);
        vec_iter_t *param_iter;

        if (token->type == HASH) {
            vec_iter_advance(&iter);
            lexeme_t *param = vec_iter_get(&iter);
            if (param->type != ID ||
                NULL == (param_iter =
                         cpp_macro_inst_lookup(macro_inst, param->id_name))) {
                logger_log(&param->mark, LOG_ERR,
                           "'#' is not followed by a macro parameter");
                goto fail;
            }

            lexeme_t *stringified = cpp_stringify(cs, param_iter);
            vec_push_back(output, stringified);
        } else if (token->type == HASHHASH) {
            vec_iter_advance(&iter);
            lexeme_t *next = vec_iter_get(&iter);
            if (next->type == ID &&
                NULL != (param_iter =
                         cpp_macro_inst_lookup(macro_inst, next->id_name))) {
                if (CCC_OK != (status = cpp_glue(cs, output, param_iter, 0))) {
                    goto fail;
                }
            } else {
                if (CCC_OK != (status = cpp_glue(cs, output, &iter, 1))) {
                    goto fail;
                }
            }
        } else if (token->type == ID &&
                   NULL !=
                   (param_iter = cpp_macro_inst_lookup(macro_inst,
                                                       token->id_name))) {
            vec_iter_advance(&iter);
            lexeme_t *next = vec_iter_get(&iter);
            if (next->type == HASHHASH) {
                if (!vec_iter_has_next(param_iter)) {
                    vec_iter_advance(&iter);
                    lexeme_t *next = vec_iter_get(&iter);
                    if (next->type == ID &&
                        NULL != (param_iter =
                                 cpp_macro_inst_lookup(macro_inst,
                                                       next->id_name))) {
                        vec_append(output, param_iter->vec);
                    }
                } else {
                    vec_iter_reverse(&iter);
                    vec_append(output, param_iter->vec);
                }
            } else {
                cpp_expand(cs, param_iter, output);
            }
        } else {
            vec_push_back(output, token);
        }
    }

    VEC_FOREACH(cur, output) {
        lexeme_t *token = vec_get(output, cur);
        str_set_union_inplace(&token->hideset, hideset);
    }

fail:
    return status;
}

status_t cpp_handle_directive(cpp_state_t *cs, vec_iter_t *ts) {
    (void)ts;
    (void)cs;
    return CCC_OK;
}

status_t cpp_fetch_macro_params(cpp_state_t *cs, vec_iter_t *ts,
                                cpp_macro_inst_t *macro_inst) {
    (void)cs;
    (void)ts;
    (void)macro_inst;
    return CCC_OK;
}

lexeme_t *cpp_stringify(cpp_state_t *cs, vec_iter_t *ts) {
    (void)cs;
    (void)ts;
    return NULL;
}

vec_iter_t *cpp_macro_inst_lookup(cpp_macro_inst_t *inst, char *arg_name) {
    (void)inst;
    (void)arg_name;
    return NULL;
}

status_t cpp_glue(cpp_state_t *cs, vec_t *left, vec_iter_t *right,
                  size_t nelems) {
    (void)cs;
    (void)left;
    (void)right;
    (void)nelems;
    return CCC_OK;
}
