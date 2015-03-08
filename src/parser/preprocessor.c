/*
  Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>

  This file is part of CCC.

  CCC is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  CCC is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with CCC.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * Implementation for preprocessor/file reader
 */

#include "preprocessor.h"
#include "preprocessor_priv.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include "parser/pp_directives.h"
#include "util/slist.h"
#include "util/htable.h"
#include "util/util.h"

status_t pp_init(preprocessor_t *pp) {
    static ht_params params = {
        0,                                // No Size estimate
        offsetof(pp_macro_t, name),       // Offset of key
        offsetof(pp_macro_t, link),       // Offset of ht link
        strhash,                          // Hash function
        vstrcmp,                          // void string compare
    };

    status_t status = CCC_OK;
    if (CCC_OK !=
        (status = sl_init(&pp->file_insts, offsetof(pp_file_t, link)))) {
        goto fail1;
    }

    if (CCC_OK !=
        (status = sl_init(&pp->macro_insts, offsetof(pp_macro_inst_t, link)))) {
        goto fail2;
    }

    if (CCC_OK !=
        (status = sl_init(&pp->search_path, offsetof(len_str_node_t, link)))) {
        goto fail3;
    }

    if (CCC_OK != (status = ht_init(&pp->macros, &params))) {
        goto fail4;
    }

    // Register directive handlers
    if (CCC_OK != (status = pp_directives_init(pp))) {
        goto fail5;
    }

    return status;

fail5:
    ht_destroy(&pp->macros, DOFREE);
fail4:
    sl_destroy(&pp->search_path, DOFREE);
fail3:
    sl_destroy(&pp->macro_insts, DOFREE);
fail2:
    sl_destroy(&pp->file_insts, DOFREE);
fail1:
    return status;
}

/**
 * Release all resources in pp
 */
void pp_destroy(preprocessor_t *pp) {
    sl_link_t *cur;
    SL_FOREACH(cur, &pp->file_insts) {
        pp_file_destroy(GET_ELEM(&pp->file_insts, &cur));
    }
    sl_destroy(&pp->file_insts, NOFREE);

    SL_FOREACH(cur, &pp->macro_insts) {
        pp_macro_inst_destroy(GET_ELEM(&pp->macro_insts, cur));
    }
    sl_destroy(&pp->macro_insts, DOFREE);

    HT_FOREACH(cur, &pp->macros) {
        pp_macro_t *macro = GET_HT_ELEM(&pp->macros, cur);
        pp_macro_destroy(macro);
    }
    ht_destroy(&pp->macros, NOFREE);
}

void pp_close(preprocessor_t *pp) {
    pp_destroy(pp);
}

/**
 * Map the file and push it onto the file_insts stack
 */
status_t pp_open(preprocessor_t *pp, const char *filename) {
    status_t status = CCC_OK;
    pp_file_t *pp_file;
    if (CCC_OK !=
        (status = pp_file_map(filename, strlen(filename), &pp_file))) {
        goto done;
    }

    sl_prepend(&pp->file_insts, &pp_file->link);

    size_t len = strlen(filename);
    len_str_t *file = fdir_lookup(filename, len);
    if (file == NULL &&
        CCC_OK != (status = fdir_insert(filename, len, &file))) {
        goto done;
    }
    pp->last_mark.filename = file;
    pp->last_mark.line_num = 0;
    pp->last_mark.col_num = 0;

    pp->cur_param = NULL;
    pp->param_end = NULL;
    pp->param_stringify = false;

    pp->block_comment = false;
    pp->line_comment = false;
    pp->string = false;
    pp->char_line = false;

done:
    return status;
}

void pp_lastmark(preprocessor_t *pp, fmark_t *mark) {
    memcpy(mark, &pp->last_mark, sizeof(fmark_t));
}

int pp_nextchar_helper(preprocessor_t *pp, bool ignore_directive) {
    // We are in a macro paramater, just copy the string
    if (NULL != pp->cur_param) {
        int result = *(pp->cur_param++);

        // Record if we reached end of the paramater
        if (pp->cur_param == pp->param_end) {
            pp->cur_param = NULL;
            pp->param_end = NULL;

            pp_macro_inst_t *macro_inst = sl_head(&pp->macro_insts);

            // Check for concatenate operator
            assert(NULL != macro_inst);
            char *cur = macro_inst->cur;
            char *end = macro_inst->end;

            SKIP_WS_AND_COMMENT(cur, end);
            if (cur == end || '#' != *cur) {
                return result;
            }

            if (++cur == end) {
                LOG_ERROR(pp, "Unexpected #", LOG_ERR);
                return -(int)CCC_ESYNTAX;
            }

            // Not ##
            if ('#' != *(cur++)) {
                return result;
            }

            SKIP_WS_AND_COMMENT(cur, end);
            char *lookahead = cur;

            ADVANCE_IDENTIFIER(lookahead, end);
            if (cur == end) {
                LOG_ERROR(pp, "Expected macro paramater after ##", LOG_ERR);
                return -(int)CCC_ESYNTAX;
            }

            len_str_t lookup = {
                cur,
                (size_t)(lookahead - cur)
            };

            pp_param_map_elem_t *param =
                ht_lookup(&macro_inst->param_map, &lookup);

            // Found a parameter
            // Advance lookahead to end of param, and set param state in pp
            if (NULL == param) {
                assert(false &&
                       "Expected macro paramater after ##");
            }

            macro_inst->cur = lookahead;
            // Substitute next parameter
            pp->cur_param = param->val.str;
            pp->param_end = param->val.str + param->val.len;

            return result;
        }

        return result;
    }

    pp_macro_inst_t *macro_inst = sl_head(&pp->macro_insts);
    pp_file_t *file = sl_head(&pp->file_insts);

    char **cur;
    char *end;

    // Try to find an incomplete macro on the stack
    while (NULL != macro_inst) {
        cur = &macro_inst->cur;
        end = macro_inst->end;

        if (*cur != end) { // Found an unfinished macro
            break;
        }

        // Handle closing quote of stringification
        if (pp->param_stringify) {
            pp->param_stringify = false;
            return '"';
        }

        pp_macro_inst_t *last = sl_pop_front(&pp->macro_insts);
        pp_macro_inst_destroy(last);
        macro_inst = sl_head(&pp->macro_insts);
    }

    // if we're done with macros, try to find an incomplete file
    if (NULL == macro_inst) {
        while (NULL != file) {
            cur = &file->cur;
            end = file->end;

            if (*cur != end) { // Found an unfinished file
                break;
            }

            pp_file_t *last = sl_pop_front(&pp->file_insts);
            pp_file_destroy(last);
            file = sl_head(&pp->file_insts);
        }
    }

    // Finished processing all files
    if (NULL == file) {
        return PP_EOF;
    }

    int cur_char = **cur;
    char *next_char = *cur + 1;

    // Found a character on line, don't process new directives
    if (!pp->char_line && !isspace(cur_char)) {
        pp->char_line = true;
    }

    // Record comments
    if (cur_char == '/' && next_char != end &&
        !pp->line_comment && !pp->block_comment && !pp->string) {
        if (*next_char == '/') {
            pp->line_comment = true;
        } else if (*next_char == '*') {
            pp->block_comment = true;
        }
    }

    if (!pp->string && cur_char == '"') {
        pp->string = true;
    }

    if (pp->string && cur_char == '"') {
        char *lookahead = next_char;
        SKIP_WS_AND_COMMENT(lookahead, end);

        // Concatenate strings
        if (lookahead != end && '"' == *lookahead) {
            *cur = lookahead;
            return pp_nextchar(pp);
        }

        pp->string = false;
    }

    bool check_directive = false;

    // Reset line variables
    if ('\n' == cur_char) {
        pp->char_line = false;
        pp->line_comment = false;
    }

    // If we're in a comment, return whitespace
    if (pp->block_comment || pp->line_comment) {
        (*cur)++; // advance the cur pointer
        return ' ';
    }

    switch (cur_char) {
        // Cases which cannot be before a macro. Macros are identifiers
    case ASCII_LOWER:
    case ASCII_UPPER:
    case ASCII_DIGIT:
    case '_':
        // Return the current character
        // Also advance the current cur pointer
        return *((*cur)++);

    case '#':
        // Skip cases which ignore preprocessor
        if (pp->block_comment || pp->line_comment || pp->string ||
            pp->char_line) {
            return *((*cur)++);
        }

        if (NULL != macro_inst) {
            if (next_char == end) {
                (*cur)++;
                LOG_ERROR(pp, "Unexpected EOF in macro", LOG_ERR);
                return -(int)CCC_ESYNTAX;
            }

            if ('#' != *next_char) {
                // Stringification, should be a macro paramater
                char *lookahead = next_char;
                ADVANCE_IDENTIFIER(lookahead, end);

                len_str_t lookup = {
                    next_char,
                    (size_t)(lookahead - next_char)
                };

                pp_param_map_elem_t *param =
                    ht_lookup(&macro_inst->param_map, &lookup);

                // Found a parameter
                // Advance lookahead to end of param, and set param state in pp
                if (NULL == param) {
                    LOG_ERROR(pp,
                              "Expected macro paramater for stringification",
                              LOG_ERR);
                    return -(int)CCC_ESYNTAX;
                }

                *cur = lookahead;

                // Substitute current paramater
                pp->cur_param = param->val.str;
                pp->param_end = param->val.str + param->val.len;
                pp->param_stringify = true;

                // We are stringifying, so return a double quote
                return '"';
            }

            return *((*cur)++);
        }

        // Not currently processing a macro
        check_directive = true;
    default:
        break;
        // Fall through, need to look for macros
    }

    // if next character is not alphanumeric, we know this shouldn't be a macro
    if (next_char == end || !isalnum(*next_char)) {
        return *((*cur)++);
    }

    // Need to look ahead current identifier and see if its a macro
    char *lookahead = next_char;

    ADVANCE_IDENTIFIER(lookahead, end);

    len_str_t lookup = {
        next_char,
        (size_t)(lookahead - next_char)
    };

    // Check for preprocessor directive matching the string
    if (check_directive && !ignore_directive) {
        pp_directive_t *directive = ht_lookup(&pp->directives, &lookup);

        if (NULL == directive) {
            LOG_ERROR(pp, "Invalid preprocessor command", LOG_ERR);
            return -(int)CCC_ESYNTAX;
        }

        // Skip over directive name
        *cur = lookahead;

        // Perform directive action and return next character
        status_t status = directive->action(pp);
        if (CCC_OK != status) {
            // If there was an error, just skip directive
            lookahead = *cur;
            SKIP_LINE(lookahead, end);
            *cur = lookahead;

            return status;
        }
        return pp_nextchar(pp);
    }

    // Macro paramaters take precidence, look them up first
    if (NULL != macro_inst) {
        pp_param_map_elem_t *param = ht_lookup(&macro_inst->param_map, &lookup);

        // Found a parameter
        // Advance lookahead to end of param, and set param state in pp
        if (NULL != param) {
            int retval = **cur;
            *cur = lookahead;
            pp->cur_param = param->val.str;
            pp->param_end = param->val.str + param->val.len;
            return retval;
        }
    }

    // Look up in the macro table
    pp_macro_t *macro = ht_lookup(&pp->macros, &lookup);

    if (NULL == macro) { // No macro found
        return *((*cur)++);
    }

    pp_macro_inst_t *new_macro_inst;
    status_t status = pp_macro_inst_create(macro, &new_macro_inst);
    int error;

    if (CCC_OK != status) {
        LOG_ERROR(pp, "Failed to create new macro.", LOG_ERR);
        error = -(int)status;
        goto fail1;
    }

    // No paramaters on macro
    if (lookahead == end || *lookahead != '(') {

        // If macro requires params, but none are provided, this is just
        // treated as identifier
        if (macro->num_params != 0) {
            return *((*cur)++);
        }
    } else {
        // Need to create param map

        int num_params = 0;

        bool done = false;
        char *cur_param = ++lookahead;

        sl_link_t *cur_link;
        SL_FOREACH(cur_link, &macro->params) {
            num_params++;

            while (lookahead != end) {
                // Skip single line whitespace
                if (*lookahead == ' ' || *lookahead == '\t') {
                    lookahead++;
                    continue;
                }

                if (*lookahead == ',') { // end of current param
                    break;
                }

                if (*lookahead == ')') { // end of all params
                    done = true;
                    break;
                }
            }

            if (lookahead == end && *lookahead != ')') {
                LOG_ERROR(pp, "Unexpected EOF while scanning macro paramaters",
                          LOG_ERR);
                error = -(int)CCC_ESYNTAX;
                goto fail2;
            }

            size_t cur_len = lookahead - cur_param;

            pp_param_map_elem_t *param_elem =
                malloc(sizeof(pp_param_map_elem_t));

            if (NULL == param_elem) {
                LOG_ERROR(pp, "Out of memory while scanning macro", LOG_ERR);
                error = -(int)CCC_NOMEM;
                goto fail2;
            }

            // Get current paramater in macro
            len_str_t *param_str = GET_ELEM(&macro->params, cur_link);

            // Insert the paramater mapping into the instance's hash table
            param_elem->key.str = param_str->str;
            param_elem->key.len = param_str->len;
            param_elem->val.str = cur_param;
            param_elem->val.len = cur_len;

            ht_insert(&new_macro_inst->param_map, &param_elem->link);
        }

        if (done && num_params != macro->num_params) {
            LOG_ERROR(pp, "Incorrect number of macro paramaters", LOG_ERR);
            error = -(int)CCC_ESYNTAX;
            goto fail2;
        }
    }

    new_macro_inst->cur = macro->start;
    new_macro_inst->end = macro->end;

    // Add new macro instance to the stack
    sl_prepend(&pp->macro_insts, &new_macro_inst->link);

    int retval = **cur;   // Return current character
    *cur = lookahead; // Set current to the end of the macro and params

    return retval;

fail2:
    pp_macro_inst_destroy(new_macro_inst);
fail1:
    return error;
}

int pp_nextchar(preprocessor_t *pp) {
    return pp_nextchar_helper(pp, false);
}

status_t pp_file_map(const char *filename, size_t len, pp_file_t **result) {
    status_t status = CCC_OK;
    pp_file_t *pp_file = malloc(sizeof(pp_file_t));
    if (NULL == pp_file) {
        status = CCC_NOMEM;
        goto fail1;
    }

    if (-1 == (pp_file->fd = open(filename, O_RDONLY, 0))) {
        status = CCC_FILEERR;
        goto fail2;
    }

    struct stat st;
    if (-1 == (fstat(pp_file->fd, &st))) {
        status = CCC_FILEERR;
        goto fail2;
    }
    size_t size = st.st_size;

    if (MAP_FAILED == (pp_file->buf =
         mmap(NULL, size, PROT_READ, MAP_PRIVATE, pp_file->fd, 0))) {
        status = CCC_FILEERR;
        goto fail2;
    }
    pp_file->cur = pp_file->buf;
    pp_file->end = pp_file->buf + size;
    pp_file->if_count = 0;

    if (CCC_OK != (status = fdir_insert(filename, len, &pp_file->filename))) {
        goto fail3;
    }

    pp_file->line_num = 0;
    pp_file->col_num = 0;

    *result = pp_file;
    return status;

fail3:
    munmap(pp_file->buf, (size_t)(pp_file->end - pp_file->buf));
fail2:
    free(pp_file);
fail1:
    *result = NULL;
    return status;
}

status_t pp_file_destroy(pp_file_t *pp_file) {
    status_t status = CCC_OK;

    // If we failed to close the file, just continue
    if (-1 == munmap(pp_file->buf, (size_t)(pp_file->end - pp_file->buf))) {
        status = CCC_FILEERR;
    }

    if (-1 == close(pp_file->fd)) {
        status = CCC_FILEERR;
    }

    free(pp_file);

    return status;
}

status_t pp_macro_init(pp_macro_t *macro) {
    return sl_init(&macro->params, offsetof(len_str_node_t, link));
}

void pp_macro_destroy(pp_macro_t *macro) {
    sl_link_t *cur;
    SL_FOREACH(cur, &macro->params) {
        len_str_node_t *str = GET_ELEM(&macro->params, cur);
        free(str->str.str);
    }
    sl_destroy(&macro->params, DOFREE);

    free(macro->start);
    free(macro->name.str);
}

status_t pp_macro_inst_create(pp_macro_t *macro, pp_macro_inst_t **result) {
    status_t status = CCC_OK;

    pp_macro_inst_t *macro_inst = malloc(sizeof(pp_macro_inst_t));
    if (NULL == macro_inst) {
        status = CCC_NOMEM;
        goto fail1;
    }

    static const ht_params pp_param_map_params = {
        0,                                   // No size hint
        offsetof(pp_param_map_elem_t, key),  // Key offset
        offsetof(pp_param_map_elem_t, link), // HT link offset
        strhash,                             // String Hash function
        vstrcmp                              // void string compare
    };

    if (CCC_OK !=
        (status = ht_init(&macro_inst->param_map, &pp_param_map_params))) {
        goto fail2;
    }

    macro_inst->cur = macro->start;
    macro_inst->end = macro->end;
    *result = macro_inst;

    return status;

fail2:
    free(macro_inst);
fail1:
    return status;
}

void pp_macro_inst_destroy(pp_macro_inst_t *macro_inst) {
    ht_destroy(&macro_inst->param_map, DOFREE);
    free(macro_inst);
}
