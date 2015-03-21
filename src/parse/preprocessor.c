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
 *
 * TODO: On errors, skip input to prevent infinite loops in lexer
 */

#include "preprocessor.h"
#include "preprocessor_priv.h"

#include <stddef.h>
#include <string.h>

#include <assert.h>
#include <ctype.h>

#include "parse/pp_directives.h"

#include "util/htable.h"
#include "util/slist.h"
#include "util/text_stream.h"
#include "util/util.h"

status_t pp_init(preprocessor_t *pp) {
    static ht_params_t params = {
        0,                                // No Size estimate
        offsetof(pp_macro_t, name),       // Offset of key
        offsetof(pp_macro_t, link),       // Offset of ht link
        strhash,                          // Hash function
        vstrcmp,                          // void string compare
    };

    status_t status = CCC_OK;
    sl_init(&pp->file_insts, offsetof(pp_file_t, link));
    sl_init(&pp->macro_insts, offsetof(pp_macro_inst_t, link));
    sl_init(&pp->search_path, offsetof(len_str_node_t, link));

    if (CCC_OK != (status = ht_init(&pp->macros, &params))) {
        goto fail1;
    }

    // Register directive handlers
    if (CCC_OK != (status = pp_directives_init(pp))) {
        goto fail2;
    }

    return status;

fail2:
    ht_destroy(&pp->macros);
fail1:
    sl_destroy(&pp->search_path);
    sl_destroy(&pp->macro_insts);
    sl_destroy(&pp->file_insts);
    return status;
}

/**
 * Release all resources in pp
 */
void pp_destroy(preprocessor_t *pp) {
    SL_DESTROY_FUNC(&pp->file_insts, pp_file_destroy);
    SL_DESTROY_FUNC(&pp->macro_insts, pp_macro_inst_destroy);
    HT_DESTROY_FUNC(&pp->macros, pp_macro_destroy);
    pp_directives_destroy(pp);
}

void pp_close(preprocessor_t *pp) {
    pp_destroy(pp);
}

/**
 * Map the file and push it onto the file_insts stack
 */
status_t pp_open(preprocessor_t *pp, const char *filename) {
    status_t status = CCC_OK;
    size_t len = strlen(filename);
    pp_file_t *pp_file;
    if (CCC_OK !=
        (status = pp_file_map(filename, len, NULL, &pp_file))) {
        goto fail;
    }

    sl_prepend(&pp->file_insts, &pp_file->link);

    fdir_entry_t *file = fdir_lookup(filename, len);
    assert(file != NULL); // pp_file_map should add the file to the directory

    pp->cur_param.cur = NULL;
    pp->param_stringify = false;

    pp->block_comment = false;
    pp->line_comment = false;
    pp->string = false;
    pp->char_line = false;
    pp->ignore = false;

fail:
    return status;
}

void pp_last_mark(preprocessor_t *pp, fmark_t *result) {
    assert(result != NULL);
    memcpy(result, &pp->last_mark, sizeof(fmark_t));
}

int pp_search_string_concat(preprocessor_t *pp, tstream_t *stream) {
    tstream_t lookahead;
    ts_copy(&lookahead, stream, TS_COPY_SHALLOW);

    ts_skip_ws_and_comment(&lookahead);

    // Concatenate strings
    if (ts_cur(&lookahead) == '"') {
        ts_advance(&lookahead);

        // Copy lookahead back into stream
        ts_copy(stream, &lookahead, TS_COPY_SHALLOW);
        return pp_nextchar(pp);
    }

    pp->string = false;
    return '"';
}

int pp_nextchar(preprocessor_t *pp) {
    return pp_nextchar_helper(pp, false);
}

status_t pp_file_map(const char *filename, size_t len, pp_file_t *last_file,
                     pp_file_t **result) {
    status_t status = CCC_OK;
    pp_file_t *pp_file = malloc(sizeof(pp_file_t));
    if (NULL == pp_file) {
        status = CCC_NOMEM;
        goto fail;
    }

    fdir_entry_t *entry;
    if (CCC_OK != (status = fdir_insert(filename, len, &entry))) {
        goto fail;
    }
    fmark_t *last_mark = last_file == NULL ? NULL : &last_file->stream.mark;
    ts_init(&pp_file->stream, entry->buf, entry->end, &entry->filename,
            entry->buf, last_mark, 1, 1);
    pp_file->if_count = 0;

    *result = pp_file;
    return status;

fail:
    pp_file_destroy(pp_file);
    return status;
}

void pp_file_destroy(pp_file_t *pp_file) {
    free(pp_file);
}

status_t pp_macro_create(char *name, size_t len, pp_macro_t **result) {
    status_t status = CCC_OK;
    // Allocate macro and name in one chunk
    pp_macro_t *macro = malloc(sizeof(pp_macro_t) + len + 1);
    if (macro == NULL) {
        status = CCC_NOMEM;
        goto fail;
    }
    sl_init(&macro->params, offsetof(len_str_node_t, link));

    // Set to safe value for destruction
    ((tstream_t *)&macro->stream)->mark.last = NULL;

    macro->name.str = (char *)macro + sizeof(pp_macro_t);
    macro->name.len = len;
    strncpy(macro->name.str, name, len);
    macro->name.str[len] = '\0';

    *result = macro;
    return status;

fail:
    pp_macro_destroy(macro);
    return status;
}

void pp_macro_destroy(pp_macro_t *macro) {
    SL_DESTROY_FUNC(&macro->params, free);
    ts_destroy((tstream_t *)&macro->stream);
    free(macro);
}

status_t pp_macro_inst_create(pp_macro_t *macro, pp_macro_inst_t **result) {
    status_t status = CCC_OK;

    pp_macro_inst_t *macro_inst = malloc(sizeof(pp_macro_inst_t));
    if (macro_inst == NULL) {
        status = CCC_NOMEM;
        goto fail;
    }

    static const ht_params_t pp_param_map_params = {
        0,                                   // No size hint
        offsetof(pp_param_map_elem_t, key),  // Key offset
        offsetof(pp_param_map_elem_t, link), // HT link offset
        strhash,                             // String Hash function
        vstrcmp                              // void string compare
    };

    if (CCC_OK !=
        (status = ht_init(&macro_inst->param_map, &pp_param_map_params))) {
        goto fail;
    }

    // Shallow copy because macro already has a copy of its fmarks
    ts_copy(&macro_inst->stream, &macro->stream, TS_COPY_SHALLOW);

    *result = macro_inst;
    return status;

fail:
    pp_macro_inst_destroy(macro_inst);
    return status;
}

void pp_macro_inst_destroy(pp_macro_inst_t *macro_inst) {
    if (macro_inst == NULL) {
        return;
    }
    HT_DESTROY_FUNC(&macro_inst->param_map, free);
    free(macro_inst);
}

int pp_nextchar_helper(preprocessor_t *pp, bool ignore_directive) {
    if (pp->cur_param.cur != NULL) {
        // We are in a macro paramater, just copy the string
        int result = ts_advance(&pp->cur_param);
        if (result != EOF) {
            return result;
        }
        // Record if we reached end of the paramater
        pp->cur_param.cur = NULL;

    }

    pp_macro_inst_t *macro_inst = sl_head(&pp->macro_insts);
    pp_file_t *file = sl_head(&pp->file_insts);

    // Handle closing quote of stringification
    if (pp->param_stringify) {
        assert(macro_inst != NULL);
        pp->param_stringify = false;
        return pp_search_string_concat(pp, &macro_inst->stream);
    }

    tstream_t *stream; // Stream to work on

    // Try to find an incomplete macro on the stack
    while (macro_inst != NULL) {
        stream = &macro_inst->stream;

        if (!ts_end(stream)) { // Found an unfinished macro
            break;
        }

        pp_macro_inst_t *last = sl_pop_front(&pp->macro_insts);
        pp_macro_inst_destroy(last);
        macro_inst = sl_head(&pp->macro_insts);
    }

    // if we're done with macros, try to find an incomplete file
    if (macro_inst == NULL) {
        while (file != NULL) {
            stream = &file->stream;

            if (!ts_end(stream)) { // Found an unfinished file
                break;
            }

            pp_file_t *last = sl_pop_front(&pp->file_insts);
            pp_file_destroy(last);
            file = sl_head(&pp->file_insts);
        }
    }

    // Finished processing all files
    if (file == NULL) {
        return PP_EOF;
    }

    // Get copy of current location, the last mark
    memcpy(&pp->last_mark, &stream->mark, sizeof(fmark_t));

    int cur_char = ts_cur(stream);
    int next_char = ts_next(stream);

    // Handle comments
    if (cur_char == '/' && !pp->line_comment && !pp->block_comment &&
        !pp->string) {
        if (next_char == '/') {
            pp->line_comment = true;
        } else if (next_char == '*') {
            pp->block_comment = true;
        }
    }

    // If we're in a comment, return whitespace
    if (pp->line_comment) {
        if (cur_char == '\n') {
            pp->line_comment = false;
        }
        ts_advance(stream);
        return ' ';
    }

    if (pp->block_comment) {
        // If we're in a comment, this must be safe (must have been a last char)
        int last_char = *(ts_location(stream) - 1);
        if (last_char == '*' && cur_char == '/') {
            pp->block_comment = false;
        }
        ts_advance(stream);
        return ' ';
    }

    // Found a character on line, don't process new directives
    if (!pp->char_line && cur_char != '#' && !isspace(cur_char)) {
        pp->char_line = true;
    }

    // Handle strings
    if (!pp->string && cur_char == '"') {
        pp->string = true;
        return ts_advance(stream);
    }

    if (pp->string && cur_char == '"') {
        return pp_search_string_concat(pp, stream);
    }

    if (cur_char == '\n') {
        pp->char_line = false;
    }

    bool concat = false;
    tstream_t lookahead;
    ts_copy(&lookahead, stream, TS_COPY_SHALLOW);

    // Handle concatenation
    if (macro_inst != NULL && (cur_char != ' ' && cur_char != '\t') &&
        (next_char == ' ' || next_char == '\t' || next_char == '#')) {
        ts_advance(&lookahead);

        // Skip multiple ## with only white space around them
        while (!ts_end(&lookahead)) {
            ts_skip_ws_and_comment(&lookahead);

            if (ts_cur(&lookahead) == '#' && ts_next(&lookahead) == '#') {
                concat = true;
            }

            ts_skip_ws_and_comment(&lookahead);
            if (!(ts_cur(&lookahead) == '#' && ts_next(&lookahead) == '#')) {
                break;
            }
        }
        next_char = ts_cur(&lookahead);
    }

    switch (cur_char) {
        // Cases which cannot be before a macro. Macros are identifiers, so if
        // we are already in an identifier, next char cannot be a macro
    case ASCII_LOWER:
    case ASCII_UPPER:
    case ASCII_DIGIT:
    case '_':
        if (!concat) {
            return ts_advance(stream);
        }
        break;

    case '#':
        // If we found a character before the #, just ignore it
        if (pp->char_line) {
            logger_log(&stream->mark, "Stray '#' in program", LOG_ERR);
            ts_advance(stream);
            return -(int)CCC_ESYNTAX;
        }

        // Check for preprocessor directive if we're not in a macro
        if (macro_inst == NULL) {
            if (ignore_directive) {
                logger_log(&stream->mark, "Unexpected '#' in directive",
                           LOG_ERR);
            }
            ts_advance(stream);
            ts_skip_ws_and_comment(stream);
            char *start = ts_location(stream);
            size_t len = ts_advance_identifier(stream);
            len_str_t lookup = { start, len };
            pp_directive_t *directive = ht_lookup(&pp->directives, &lookup);

            if (directive == NULL) {
                snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                         "Invalid preprocessing directive %.*s", (int)len,
                         start);
                logger_log(&stream->mark, logger_fmt_buf, LOG_ERR);
                ts_skip_line(stream); // Skip rest of line
                return -(int)CCC_ESYNTAX;
            }

            // Perform directive action
            status_t status = directive->action(pp);
            ts_skip_line(stream); // Skip rest of line

            if (status != CCC_OK) {
                return status;
            }
            // Return next character after directive
            return pp_nextchar(pp);
        } else {
            // In macro, must be stringification, concatenation handled abave
            ts_advance(stream);
            char *start = ts_location(stream);
            size_t len = ts_advance_identifier(stream);
            len_str_t lookup = { start, len };
            pp_param_map_elem_t *param =
                ht_lookup(&macro_inst->param_map, &lookup);

            if (param == NULL) {
                logger_log(&stream->mark,
                           "'#' Is not followed by a macro paramater", LOG_ERR);
                return -(int)CCC_ESYNTAX;
            }

            // Found a parameter, set param state in pp
            ts_copy(&pp->cur_param, stream, TS_COPY_SHALLOW);
            pp->cur_param.cur = param->val.str;
            pp->cur_param.end = param->val.str + param->val.len;
            pp->param_stringify = true;

            // We are stringifying, so return a double quote
            return '"';
        }
        break;
    default:
        break;
        // Fall through, need to look for macros parameters
    }

    // if next character cannot start an identifier, we know not to check for
    // a macro
    switch (next_char) {
    case ASCII_LOWER:
    case ASCII_UPPER:
    case '_':
        ts_advance(&lookahead); // Skip cur char
        break;
    default:
        return ts_advance(stream);
    }

    char *start = ts_location(&lookahead);
    size_t len = ts_advance_identifier(&lookahead);
    len_str_t lookup = { start, len };

    // Macro paramaters take precidence, look them up first
    if (macro_inst != NULL) {
        pp_param_map_elem_t *param = ht_lookup(&macro_inst->param_map, &lookup);

        // Found a parameter
        // Advance lookahead to end of param, and set param state in pp
        if (param != NULL) {
            // Skip over parameter name
            ts_copy(stream, &lookahead, TS_COPY_SHALLOW);
            ts_copy(&pp->cur_param, &lookahead, TS_COPY_SHALLOW);
            pp->cur_param.cur = param->val.str;
            pp->cur_param.end = param->val.str + param->val.len;
            return cur_char;
        }
    }

    // Look up in the macro table
    pp_macro_t *macro = ht_lookup(&pp->macros, &lookup);

    if (macro == NULL) { // No macro found
        return ts_advance(stream);
    }

    pp_macro_inst_t *new_macro_inst;
    status_t status = pp_macro_inst_create(macro, &new_macro_inst);
    int error;

    if (CCC_OK != status) {
        logger_log(&stream->mark, "Failed to create new macro.", LOG_ERR);
        error = -(int)status;
        goto fail;
    }

    if (macro->num_params != 0 && ts_cur(&lookahead) != '(') {
        // If macro requires params, but none are provided, this is just
        // treated as identifier
        return ts_advance(stream);
    } else if (macro->num_params != 0) {
        ts_advance(&lookahead); // Skip the paren
        // Need to create param map

        int num_params = 0;

        bool done = false;

        sl_link_t *cur_link;
        SL_FOREACH(cur_link, &macro->params) {
            num_params++;
            char *cur_param = ts_location(&lookahead);
            while (!ts_end(&lookahead)) {
                if (ts_cur(&lookahead) == ',') { // end of current param
                    break;
                }
                if (ts_cur(&lookahead) == ')') { // end of all params
                    done = true;
                    break;
                }
                ts_advance(&lookahead); // Blindly copy param data
            }

            if (ts_end(&lookahead)
                && (num_params != macro->num_params || !done)) {
                logger_log(&stream->mark,
                           "Unexpected EOF while scanning macro paramaters",
                           LOG_ERR);
                error = -(int)CCC_ESYNTAX;
                goto fail;
            }

            size_t cur_len = ts_location(&lookahead) - cur_param;

            pp_param_map_elem_t *param_elem =
                malloc(sizeof(pp_param_map_elem_t));
            if (NULL == param_elem) {
                logger_log(&stream->mark, "Out of memory while scanning macro",
                           LOG_ERR);
                error = -(int)CCC_NOMEM;
                goto fail;
            }

            // Get current paramater in macro
            len_str_node_t *param_str = GET_ELEM(&macro->params, cur_link);

            // Insert the paramater mapping into the instance's hash table
            param_elem->key.str = param_str->str.str;
            param_elem->key.len = param_str->str.len;
            param_elem->val.str = cur_param;
            param_elem->val.len = cur_len;

            ht_insert(&new_macro_inst->param_map, &param_elem->link);

            ts_advance(&lookahead);

            if (done && num_params != macro->num_params) {
                logger_log(&stream->mark,
                           "Incorrect number of macro paramaters", LOG_ERR);
                error = -(int)CCC_ESYNTAX;
                goto fail;
            }
        }

    }

    // Add new macro instance to the stack
    sl_prepend(&pp->macro_insts, &new_macro_inst->link);

    // Set current to the end of the macro and params
    ts_copy(stream, &lookahead, TS_COPY_SHALLOW);
    return cur_char;

fail:
    pp_macro_inst_destroy(new_macro_inst);
    return error;
}