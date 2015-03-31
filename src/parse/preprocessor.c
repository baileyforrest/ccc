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

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "optman.h"

#include "parse/pp_directives.h"

#include "util/htable.h"
#include "util/slist.h"
#include "util/text_stream.h"
#include "util/util.h"

static len_str_t s_built_in_file = LEN_STR_LIT(BUILT_IN_FILENAME);

#define PREDEF_MACRO_LIT(name, word, type)                              \
    { SL_LINK_LIT, LEN_STR_LIT(name),                                   \
      TSTREAM_LIT(word, NULL, &s_built_in_file, BUILT_IN_FILENAME, 0, 0), \
      SLIST_LIT(offsetof(len_str_node_t, link)), 0, type }

static pp_macro_t s_predef_macros[] = {
    // Standard C required macros
    PREDEF_MACRO_LIT("__FILE__", "", MACRO_FILE),
    PREDEF_MACRO_LIT("__LINE__", "", MACRO_LINE),
    PREDEF_MACRO_LIT("__DATE__", "", MACRO_DATE),
    PREDEF_MACRO_LIT("__TIME__", "", MACRO_TIME),
    PREDEF_MACRO_LIT("defined", "",  MACRO_DEFINED),
    PREDEF_MACRO_LIT("_Pragma", "",  MACRO_PRAGMA),
    PREDEF_MACRO_LIT("__STDC__", "1", MACRO_BASIC), // ISO C
    PREDEF_MACRO_LIT("__STDC_VERSION__", "201112L", MACRO_BASIC), // C11
    PREDEF_MACRO_LIT("__STDC_HOSTED__", "1", MACRO_BASIC), // stdlib available

#ifdef __x86_64__
    PREDEF_MACRO_LIT("__x86_64__", "1", MACRO_BASIC),
#endif

    PREDEF_MACRO_LIT("__builtin_va_list", "char *", MACRO_BASIC)
};

status_t pp_init(preprocessor_t *pp, htable_t *macros) {
    static const ht_params_t macro_params = {
        0,                                // No Size estimate
        offsetof(pp_macro_t, name),       // Offset of key
        offsetof(pp_macro_t, link),       // Offset of ht link
        strhash,                          // Hash function
        vstrcmp,                          // void string compare
    };

    static const ht_params_t directive_params = {
        0,                                // No Size estimate
        offsetof(pp_directive_t, key),    // Offset of key
        offsetof(pp_directive_t, link),   // Offset of ht link
        strhash,                          // Hash function
        vstrcmp,                          // void string compare
    };

    status_t status = CCC_OK;
    sl_init(&pp->file_insts, offsetof(pp_file_t, link));
    sl_init(&pp->macro_insts, offsetof(pp_macro_inst_t, link));
    sl_init(&pp->search_path, offsetof(len_str_node_t, link));

    if (CCC_OK != (status = ht_init(&pp->directives, &directive_params))) {
        goto fail1;
    }


    if (macros == NULL) {
        if (CCC_OK != (status = ht_init(&pp->macros, &macro_params))) {
            goto fail2;
        }

        // Register directive handlers
        if (CCC_OK != (status = pp_directives_init(pp))) {
            goto fail3;
        }

        // Load predefined macros
        for (size_t i = 0; i < STATIC_ARRAY_LEN(s_predef_macros); ++i) {
            if (CCC_OK != (status = ht_insert(&pp->macros,
                                              &s_predef_macros[i].link))) {
                goto fail3;
            }
        }

        // Load command line parameter macros
        sl_link_t *cur;
        SL_FOREACH(cur, &optman.macros) {
            macro_node_t *macro_node = GET_ELEM(&optman.macros, cur);
            if (CCC_OK != (status = ht_insert(&pp->macros,
                                              &macro_node->macro->link))) {
                goto fail3;
            }
        }

        pp->pp_if = false;
    } else {
        ht_create_handle(&pp->macros, macros);
        pp->pp_if = true;
    }

    // Initialize state
    pp->block_comment = false;
    pp->line_comment = false;
    pp->string = false;
    pp->char_line = false;
    pp->ignore = false;
    pp->in_directive = false;

    return status;

fail3:
    if (macros != NULL) {
        ht_destroy(&pp->macros);
    }
fail2:
    ht_destroy(&pp->directives);
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

    if (!pp->pp_if) {
        HT_DESTROY_FUNC(&pp->macros, pp_macro_destroy);
    }
    pp_directives_destroy(pp);
    ht_destroy(&pp->directives);
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
        (status = pp_map_file(filename, len, NULL, &pp_file))) {
        goto fail;
    }

    sl_prepend(&pp->file_insts, &pp_file->link);

    fdir_entry_t *file = fdir_lookup(filename, len);
    assert(file != NULL); // pp_map_file should add the file to the directory

fail:
    return status;
}

void pp_last_mark(preprocessor_t *pp, fmark_t *result) {
    assert(result != NULL);
    memcpy(result, &pp->last_mark, sizeof(fmark_t));
}

int pp_nextchar(preprocessor_t *pp) {
    int result = PP_EOF;
    while (!pp->ignore) {
        if (-(int)CCC_RETRY != (result = pp_nextchar_helper(pp))) {
            return result;
        }
    }

    while (pp->ignore || result == -(int)CCC_RETRY) {
        // Fetch characters until another directive is run to tell us to stop
        // ignoring
        result = pp_nextchar_helper(pp);
        if (pp->ignore && result == PP_EOF) { // Only go to end of current file
            logger_log(&pp->last_mark, LOG_ERR, "Unexpected EOF");
            return PP_EOF;
        }
    }
    return result;
}

status_t pp_file_create(pp_file_t **result) {
    status_t status = CCC_OK;
    pp_file_t *pp_file = malloc(sizeof(pp_file_t));
    if (pp_file == NULL) {
        status = CCC_NOMEM;
        goto fail;
    }

    sl_init(&pp_file->cond_insts, offsetof(pp_cond_inst_t, link));
    pp_file->if_count = 0;
    pp_file->owns_name = false;

    *result = pp_file;
fail:
    return status;
}

void pp_file_destroy(pp_file_t *pp_file) {
    if (pp_file->owns_name) {
        free(pp_file->stream.mark.file);
    }
    SL_DESTROY_FUNC(&pp_file->cond_insts, free);
    free(pp_file);
}

status_t pp_map_file(const char *filename, size_t len, pp_file_t *last_file,
                     pp_file_t **result) {
    status_t status = CCC_OK;
    pp_file_t *pp_file;
    if (CCC_OK != (status = pp_file_create(&pp_file))) {
        goto fail;
    }

    fdir_entry_t *entry;
    if (CCC_OK != (status = fdir_insert(filename, len, &entry))) {
        goto fail;
    }

    fmark_t *last_mark = last_file == NULL ? NULL : &last_file->stream.mark;
    ts_init(&pp_file->stream, entry->buf, entry->end, &entry->filename,
            entry->buf, last_mark, 1, 1);

    *result = pp_file;
    return status;

fail:
    pp_file_destroy(pp_file);
    return status;
}

status_t pp_map_stream(preprocessor_t *pp, tstream_t *stream) {
    status_t status = CCC_OK;
    pp_file_t *pp_file;
    if (CCC_OK != (status = pp_file_create(&pp_file))) {
        goto fail;
    }

    ts_copy(&pp_file->stream, stream, TS_COPY_SHALLOW);
    sl_prepend(&pp->file_insts, &pp_file->link);
    return status;

fail:
    pp_file_destroy(pp_file);
    return status;
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

    macro->type = MACRO_BASIC;

    *result = macro;
    return status;

fail:
    pp_macro_destroy(macro);
    return status;
}

void pp_macro_destroy(pp_macro_t *macro) {
    if (macro == NULL ||
        macro->type == MACRO_CLI_OPT || // Don't destroy CLI option macros
        (macro >= s_predef_macros &&
         macro < s_predef_macros + STATIC_ARRAY_LEN(s_predef_macros))) {
        // Don't do anything when destroying predefined macros
        return;
    }
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
    if (macro != NULL) {
        ts_copy(&macro_inst->stream, &macro->stream, TS_COPY_SHALLOW);
    }

    macro_inst->cur_param.cur = NULL;
    macro_inst->param_stringify = false;

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

tstream_t *pp_get_stream(preprocessor_t *pp, bool *stringify) {
    tstream_t *stream = NULL; // Stream to work on
    pp_macro_inst_t *macro_inst = sl_head(&pp->macro_insts);
    if (stringify != NULL) {
        *stringify = false;
    }

    // Try to find an incomplete macro on the stack
    while (macro_inst != NULL) {
        stream = &macro_inst->stream;

        // If this macro is processing a parameter, use the parameter's stream
        if (macro_inst->cur_param.cur != NULL) {
            if (!ts_end(&macro_inst->cur_param)) {
                return &macro_inst->cur_param;
            }

            macro_inst->cur_param.cur = NULL;

            // Mark stringification of paramaters
            if (stringify != NULL && macro_inst->param_stringify) {
                *stringify = true;
            }
            macro_inst->param_stringify = false;
        }

        if (!ts_end(stream)) { // Found an unfinished macro
            return stream;
        }

        pp_macro_inst_t *last = sl_pop_front(&pp->macro_insts);
        pp_macro_inst_destroy(last);
        macro_inst = sl_head(&pp->macro_insts);
    }

    pp_file_t *file = sl_head(&pp->file_insts);

    // if we're done with macros, try to find an incomplete file
    while (file != NULL) {
        stream = &file->stream;

        if (!ts_end(stream)) { // Found an unfinished file
            return stream;
        }

        pp_file_t *last = sl_pop_front(&pp->file_insts);
        pp_file_destroy(last);
        file = sl_head(&pp->file_insts);
    }

    return NULL;
}

int pp_nextchar_helper(preprocessor_t *pp) {
    status_t status = CCC_OK;

    bool stringify;
    tstream_t *stream = pp_get_stream(pp, &stringify);

    // Handle closing quote of stringification
    if (stringify) {
        return '"';
    }
    pp_macro_inst_t *macro_inst = sl_head(&pp->macro_insts);

    // Finished processing all files
    if (stream == NULL) {
        return PP_EOF;
    }

    // Get copy of current location, the last mark
    memcpy(&pp->last_mark, &stream->mark, sizeof(fmark_t));

    int cur_char = ts_cur(stream);
    int next_char = ts_next(stream);
    int last_char = ts_last(stream);

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
        pp->string = false;
    }

    if (cur_char == '\n') {
        pp->char_line = false;
    }

    tstream_t lookahead;
    ts_copy(&lookahead, stream, TS_COPY_SHALLOW);

    bool concat = false;
    // Handle concatenation
    if (macro_inst != NULL && !isspace(last_char) &&
        (cur_char == ' ' || cur_char == '\t' || cur_char == '\\' ||
         cur_char == '#')) {
        ts_skip_ws_and_comment(&lookahead);

        // Skip multiple ## with only white space around them
        while (!ts_end(&lookahead)) {
            if (ts_cur(&lookahead) == '#' && ts_next(&lookahead) == '#') {
                concat = true;
                ts_advance(&lookahead);
                ts_advance(&lookahead);
            } else {
                break;
            }
            ts_skip_ws_and_comment(&lookahead);
        }
        if (concat) {
            // Set stream to new location
            ts_copy(stream, &lookahead, TS_COPY_SHALLOW);
            cur_char = ts_cur(stream);
            last_char = ts_last(stream);
            next_char = ts_next(stream);
        } else {
            // Need to reset lookahad
            ts_copy(&lookahead, stream, TS_COPY_SHALLOW);
        }
    }

    if (cur_char == '#') {
        // Check for preprocessor directive if we're not in a macro
        if (macro_inst == NULL) {
            // If we found a character before the #, just ignore it
            if (pp->char_line) {
                logger_log(&stream->mark, LOG_ERR, "Stray '#' in program");
                ts_advance(stream);
                return -(int)CCC_ESYNTAX;
            }

            if (pp->in_directive) {
                logger_log(&stream->mark, LOG_ERR,
                           "Unexpected '#' in directive");
            }
            ts_advance(stream);
            ts_skip_ws_and_comment(stream);
            char *start = ts_location(stream);
            size_t len = ts_advance_identifier(stream);
            len_str_t lookup = { start, len };
            pp_directive_t *directive = ht_lookup(&pp->directives, &lookup);

            if (directive == NULL) {
                logger_log(&stream->mark, LOG_ERR,
                           "Invalid preprocessing directive %.*s", (int)len,
                           start);
                ts_skip_line(stream, &pp->block_comment); // Skip rest of line
                return -(int)CCC_ESYNTAX;
            }

            // Perform directive action
            pp->in_directive = true;
            status_t status = directive->action(pp);
            pp->in_directive = false;

            if (directive->skip_line) {
                ts_skip_line(stream, &pp->block_comment); // Skip rest of line
            }

            if (status != CCC_OK) {
                return -(int)status;
            }
            // Tell caller to fetch another character
            return -(int)CCC_RETRY;
        } else {
            // In macro, must be stringification, concatenation handled abave
            ts_advance(stream);
            char *start = ts_location(stream);
            size_t len = ts_advance_identifier(stream);
            len_str_t lookup = { start, len };
            pp_param_map_elem_t *param =
                ht_lookup(&macro_inst->param_map, &lookup);

            if (param == NULL) {
                logger_log(&stream->mark, LOG_ERR,
                           "'#' Is not followed by a macro paramater");
                ts_advance(stream);
                return -(int)CCC_ESYNTAX;
            }

            // Found a parameter, set param state in pp
            ts_copy(&macro_inst->cur_param, stream, TS_COPY_SHALLOW);
            macro_inst->cur_param.cur = param->val.str;
            macro_inst->cur_param.end = param->val.str + param->val.len;
            macro_inst->param_stringify = true;

            // We are stringifying, so return a double quote
            return '"';
        }
    }

    switch (last_char) {
        // Cases which cannot be before a macro. Macros are identifiers, so if
        // we are already in an identifier, cur char cannot be a macro
    case ASCII_LOWER:
    case ASCII_UPPER:
    case ASCII_DIGIT:
    case '_':
        return ts_advance(stream);
    default:
        break;
        // Fall through, need to look for macros parameters
    }

    // if cur character cannot start an identifier, we know not to check for
    // a macro
    switch (cur_char) {
    case ASCII_LOWER:
    case ASCII_UPPER:
    case '_':
        break;
    default:
        return ts_advance(stream);
    }

    char *start = ts_location(&lookahead);
    size_t len = ts_advance_identifier(&lookahead);
    len_str_t lookup = { start, len };

    // Macro paramaters take precidence, look them up first
    if (macro_inst != NULL && stream == &macro_inst->stream) {
        pp_param_map_elem_t *param = ht_lookup(&macro_inst->param_map, &lookup);

        // Found a parameter
        // Advance lookahead to end of param, and set param state in pp
        if (param != NULL) {
            // Skip over parameter name
            ts_copy(stream, &lookahead, TS_COPY_SHALLOW);
            ts_copy(&macro_inst->cur_param, &lookahead, TS_COPY_SHALLOW);
            macro_inst->cur_param.cur = param->val.str;
            macro_inst->cur_param.end = param->val.str + param->val.len;
            macro_inst->cur_param.last = ' ';
            return -(int)CCC_RETRY;
        }
    }

    // Don't expand macros if we're concatenating
    if (concat) {
        return ts_advance(stream);
    }

    // Look up in the macro table
    pp_macro_t *macro = ht_lookup(&pp->macros, &lookup);

    if (macro == NULL) { // No macro found
        if (pp->pp_if) { // In preprecessor conditional
            // Skip past end of parameter
            ts_copy(stream, &lookahead, TS_COPY_SHALLOW);

            // Replace undefined macros with 0
            return '0';
        } else {
            return ts_advance(stream);
        }
    }

    switch (macro->type) {
        // For basic macros, just keep going
    case MACRO_BASIC:
    case MACRO_CLI_OPT:
        break;
    case MACRO_FILE:
    case MACRO_LINE:
    case MACRO_DATE:
    case MACRO_TIME:
        ts_copy(stream, &lookahead, TS_COPY_SHALLOW);
        return pp_handle_special_macro(pp, stream, macro->type);
    case MACRO_DEFINED:
        return pp_handle_defined(pp, &lookahead, stream);
    case MACRO_PRAGMA:
        return pp_directive_pragma_helper(pp, PRAGMA_UNDER);
    default:
        assert(false);
    }

    pp_macro_inst_t *new_macro_inst;
    int error;

    if (CCC_OK != (status = pp_macro_inst_create(macro, &new_macro_inst))) {
        logger_log(&stream->mark, LOG_ERR, "Failed to create new macro.");
        error = -(int)status;
        goto fail;
    }

    if (macro->num_params != 0) {
        ts_skip_ws_and_comment(&lookahead);

        if (ts_cur(&lookahead) != '(') {
            // If macro requires params, but none are provided, this is just
            // treated as identifier
            return ts_advance(stream);
        }

        ts_advance(&lookahead); // Skip the paren
        // Need to create param map

        int num_params = 0;

        bool done = false;

        sl_link_t *cur_link;
        SL_FOREACH(cur_link, &macro->params) {
            ts_skip_ws_and_comment(&lookahead);
            num_params++;
            char *cur_param = ts_location(&lookahead);
            int num_parens = 0;
            while (!ts_end(&lookahead)) {
                if (ts_cur(&lookahead) == '(') {
                    num_parens++;
                } else if (num_parens > 0 && ts_cur(&lookahead) == ')') {
                    num_parens--;
                } else if (num_parens == 0) {
                    if (ts_cur(&lookahead) == ',') { // end of current param
                        break;
                    }
                    if (ts_cur(&lookahead) == ')') { // end of all params
                        done = true;
                        break;
                    }
                }
                ts_advance(&lookahead); // Blindly copy param data
            }

            if (ts_end(&lookahead)
                && (num_params != macro->num_params || !done)) {
                logger_log(&stream->mark, LOG_ERR,
                           "Unexpected EOF while scanning macro paramaters");
                error = -(int)CCC_ESYNTAX;
                goto fail;
            }

            size_t cur_len = ts_location(&lookahead) - cur_param;

            pp_param_map_elem_t *param_elem =
                malloc(sizeof(pp_param_map_elem_t));
            if (param_elem == NULL) {
                logger_log(&stream->mark, LOG_ERR,
                           "Out of memory while scanning macro");
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

            if (done) {
                break;
            }
        }

        if ((done && num_params != macro->num_params) || !done) {
            logger_log(&stream->mark, LOG_ERR,
                       "Incorrect number of macro paramaters");
            error = -(int)CCC_ESYNTAX;
            goto fail;
        }
    }

    // Add new macro instance to the stack
    sl_prepend(&pp->macro_insts, &new_macro_inst->link);

    // Set current to the end of the macro and params
    ts_copy(stream, &lookahead, TS_COPY_SHALLOW);
    return -(int)CCC_RETRY;

fail:
    ts_advance(stream); // Skip character to prevent infinite loop
    pp_macro_inst_destroy(new_macro_inst);
    return error;
}

int pp_handle_special_macro(preprocessor_t *pp, tstream_t *stream,
                            pp_macro_type_t type) {
    status_t status = CCC_OK;

    static bool date_err = false;
    static bool time_err = false;

    pp_macro_inst_t *macro_inst = NULL;
    if (CCC_OK != (status = pp_macro_inst_create(NULL, &macro_inst))) {
        goto fail;
    }

    time_t t;
    struct tm *tm;

    pp->macro_buf[0] = '"';
    char *buf = pp->macro_buf + 1;
    size_t buf_size = sizeof(pp->macro_buf) - 2;
    bool quotes = true;

    size_t len = 0;
    switch (type) {
    case MACRO_FILE:
        strncpy(buf, stream->mark.file->str, buf_size);
        len = stream->mark.file->len;
        break;
    case MACRO_LINE:
        quotes = false; // Line number is an integer
        len = snprintf(buf, buf_size, "%d", stream->mark.line);
        break;
    case MACRO_DATE: {
        if (-1 == (t = time(NULL)) || NULL == (tm = localtime(&t))) {
            if (!date_err) {
                date_err = true;
                logger_log(&stream->mark, LOG_WARN, "Failed to get Date!");
            }
            len = snprintf(buf, buf_size, "??? ?? ????");
        } else {
            len = strftime(buf, buf_size, "%b %d %Y", tm);
        }
        break;
    }
    case MACRO_TIME: {
        if (-1 == (t = time(NULL)) || NULL == (tm = localtime(&t))) {
            if (!time_err) {
                time_err = true;
                logger_log(&stream->mark, LOG_WARN, "Failed to get Time!");
            }
            len = snprintf(buf, buf_size, "??:??:??");
        } else {
            len = strftime(buf, buf_size, "%T", tm);
        }
        break;
    }
    default:
        assert(false);
    }
    len = MIN(len, buf_size - 2);
    if (quotes) {
        buf[len++] = '"';
    }
    buf[len] = '\0';

    ts_copy(&macro_inst->stream, stream, TS_COPY_SHALLOW);
    if (quotes) {
        macro_inst->stream.cur = pp->macro_buf;
    } else {
        macro_inst->stream.cur = pp->macro_buf + 1; // Skip the quote
    }
    macro_inst->stream.end = pp->macro_buf + len + 1;

    // Add new macro instance to the stack
    sl_prepend(&pp->macro_insts, &macro_inst->link);

    return -(int)CCC_RETRY;

fail:
    return -(int)status;
}

int pp_handle_defined(preprocessor_t *pp, tstream_t *lookahead,
                      tstream_t *stream) {
    ts_skip_ws_and_comment(lookahead);
    bool paren = false;
    if (ts_cur(lookahead) == '(') {
        ts_advance(lookahead);
        paren = true;
    }

    char *start = ts_location(lookahead);
    size_t len = ts_advance_identifier(lookahead);
    if (len == 0) {
        logger_log(&stream->mark, LOG_ERR,
                   "operator \"defined\" requires an identifier");
        ts_copy(stream, lookahead, TS_COPY_SHALLOW);
        return -(int)CCC_ESYNTAX;
    }
    len_str_t lookup = { start, len };
    pp_macro_t *macro = ht_lookup(&pp->macros, &lookup);
    if (paren) {
        if (ts_cur(lookahead) != ')') {
            logger_log(&stream->mark, LOG_ERR,
                       "missing ')' after \"defined\"");
            ts_copy(stream, lookahead, TS_COPY_SHALLOW);
            return -(int)CCC_ESYNTAX;
        }
        ts_advance(lookahead);
    }

    ts_copy(stream, lookahead, TS_COPY_SHALLOW);

    return macro == NULL ? '0' : '1';
}
