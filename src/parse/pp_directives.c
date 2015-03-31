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
 * Preprocessor directive implementation
 */

#include "pp_directives.h"
#include "preprocessor_priv.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "manager.h"
#include "optman.h"

#include "util/htable.h"
#include "util/util.h"

#include "typecheck/typechecker.h"

#define MAX_PATH_LEN 2048 /**< Max include path len */
#define MAX_LINE 512      /**< Max length for line pragma */

#define DIRECTIVE_LIT(name) \
    { SL_LINK_LIT, LEN_STR_LIT(#name), pp_directive_ ## name  }

static pp_directive_t s_directives[] = {
    DIRECTIVE_LIT(include     ),
    DIRECTIVE_LIT(include_next),
    DIRECTIVE_LIT(define      ),
    DIRECTIVE_LIT(undef       ),
    DIRECTIVE_LIT(ifdef       ),
    DIRECTIVE_LIT(ifndef      ),
    DIRECTIVE_LIT(if          ),
    DIRECTIVE_LIT(elif        ),
    DIRECTIVE_LIT(else        ),
    DIRECTIVE_LIT(endif       ),
    DIRECTIVE_LIT(error       ),
    DIRECTIVE_LIT(warning     ),
    DIRECTIVE_LIT(pragma      ),
    DIRECTIVE_LIT(line        )
};

/**
 * Default search path for #include files. Ordering is important
 * Must have trailing backslash
 */
static len_str_node_t s_default_search_path[] = {
    { SL_LINK_LIT, LEN_STR_LIT(".") }, // Current directory
    { SL_LINK_LIT, LEN_STR_LIT("/usr/local/include") },
    { SL_LINK_LIT, LEN_STR_LIT("/usr/include") },

    // TODO: conditionally compile these
    { SL_LINK_LIT, LEN_STR_LIT("/usr/lib/gcc/x86_64-unknown-linux-gnu/4.9.2/include") },
    //{ SL_LINK_LIT, LEN_STR_LIT("/usr/lib/clang/3.6.0/include") },
};

status_t pp_directives_init(preprocessor_t *pp) {
    status_t status = CCC_OK;

    // Add directive handlers
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_directives); ++i) {
        if (CCC_OK !=
            (status = ht_insert(&pp->directives, &s_directives[i].link))) {
            goto fail;
        }
    }

    // Add to search path with -I option
    sl_link_t *cur;
    SL_FOREACH(cur, &optman.include_paths) {
        len_str_node_node_t *node = GET_ELEM(&optman.include_paths, cur);
        sl_append(&pp->search_path, &node->node.link);
    }

    // Add default search path
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_default_search_path); ++i) {
        sl_append(&pp->search_path, &s_default_search_path[i].link);
    }

fail:
    return status;
}

void pp_directives_destroy(preprocessor_t *pp) {

    // Remove search path -I options
    // This should be fast because the -I options are at the front
    sl_link_t *cur;
    SL_FOREACH(cur, &optman.include_paths) {
        len_str_node_node_t *node = GET_ELEM(&optman.include_paths, cur);
        sl_remove(&pp->search_path, &node->node.link);
    }

}

status_t pp_directive_include(preprocessor_t *pp) {
    return pp_directive_include_helper(pp, false);
}

status_t pp_directive_include_next(preprocessor_t *pp) {
    return pp_directive_include_helper(pp, true);
}

status_t pp_directive_include_helper(preprocessor_t *pp, bool next) {
    char path_buf[MAX_PATH_LEN];
    char suffix_buf[MAX_PATH_LEN];

    assert(sl_head(&pp->macro_insts) == NULL && "include inside macro!");
    status_t status = CCC_OK;

    if (pp->ignore) { // If we're ignoring, just skip
        return status;
    }

    // Initialize to safe state
    len_str_t suffix = { suffix_buf, 0 };


    pp_file_t *file = sl_head(&pp->file_insts);
    tstream_t *stream = &file->stream;

    ts_skip_ws_and_comment(stream);
    if (ts_end(stream)) {
        logger_log(&stream->mark, LOG_ERR, "Unexpected EOF in #include");
        status = CCC_ESYNTAX;
        goto fail;
    }

    char *cur = NULL;

    int endsym = 0;
    switch (ts_cur(stream)) {
        // quote or ange bracket
    case '"':
        endsym = '"';
    case '<':
        endsym = endsym ? endsym : '>';

        ts_advance(stream);
        cur = ts_location(stream);

        bool done = false;
        while (!done && !ts_end(stream)) {
            /* Charaters allowed to be in path name */
            switch (ts_cur(stream)) {
            case ASCII_LOWER:
            case ASCII_UPPER:
            case ASCII_DIGIT:
            case '_':
            case '-':
            case '.':
            case '/':
                ts_advance(stream);
                break;
            default: /* Found end */
                done = true;
            }
        }

        // Reached end
        if (ts_end(stream)) {
            logger_log(&stream->mark, LOG_ERR, "Unexpected EOF in #include");
            status = CCC_ESYNTAX;
            goto fail;
        }

        // 0 length
        if (ts_location(stream) == cur) {
            logger_log(&stream->mark, LOG_ERR, "0 length include path");
            status = CCC_ESYNTAX;
            goto fail;
        }

        // Incorrect end symbol
        if (ts_cur(stream) != endsym) {
            logger_log(&stream->mark, LOG_ERR, "Unexpected symbol in #include");
            status = CCC_ESYNTAX;
            goto fail;
        }

        suffix.str = cur;
        suffix.len = ts_location(stream) - cur;
        break;

        // Identifier, expand macros
    case ASCII_LOWER:
    case ASCII_UPPER:
    case ASCII_DIGIT:
    case '_':
        done = false;
        // Find the starting character
        while (!done) {
            int next = pp_nextchar(pp);
            if (next == PP_EOF) {
                logger_log(&stream->mark, LOG_ERR,
                           "Unexpected EOF in #include");
                status = CCC_ESYNTAX;
                goto fail;
            }

            switch (next) {
            case '"':
                endsym = '"';
            case '<':
                endsym = endsym ? endsym : '>';
                done = true;
                break;
            case ' ':
            case '\t':
                continue;
            default:
                logger_log(&stream->mark, LOG_ERR,
                           "Unexpected character %c in #include", next);
                status = CCC_ESYNTAX;
                goto fail;
            }
        }

        int offset = 0;
        done = false;

        // Find the end of the include string
        while (true) {
            int next = pp_nextchar(pp);
            if (offset == MAX_PATH_LEN) {
                logger_log(&stream->mark, LOG_ERR,
                           "Include path name too long");
                status = CCC_ESYNTAX;
                goto fail;
            }
            if (next == PP_EOF) {
                logger_log(&stream->mark, LOG_ERR,
                           "Unexpected EOF in #include");
                status = CCC_ESYNTAX;
                goto fail;
            }

            if (next == endsym) {
                break;
            }
            suffix_buf[offset++] = next;
        }

        suffix_buf[offset] = '\0';

        suffix.str = suffix_buf;
        suffix.len = offset;

        // Skip until next line
        done = false;
        int last = -1;
        while (!done) {
            int next = pp_nextchar(pp);
            if (PP_EOF == next) {
                done = true;
            }

            if (next == '\n' && last != '\\') {
                done = true;
            }
            last = next;
        }
        break;
    default:
        logger_log(&stream->mark, LOG_ERR,
                   "Unexpected character %c in #include", *cur);
        status = CCC_ESYNTAX;
        goto fail;
    }

    len_str_t cur_path = { file->stream.mark.file->str,
                           file->stream.mark.file->len };
    if (next) {
        while (cur_path.len > 0 && cur_path.str[cur_path.len - 1] != '/') {
            cur_path.len--;
        }

        // If no current path, set it to current directory
        if (cur_path.len == 0) {
            cur_path.str = "./";
            cur_path.len = 2;
        }
    }

    bool found_cur_path = false;

    // Search for the string in all of the search paths
    sl_link_t *link;
    SL_FOREACH(link, &pp->search_path) {
        len_str_node_t *cur = GET_ELEM(&pp->search_path, link);

        // If we're processing include next, look for the current path and
        // mark it when we find it.
        if (next && !found_cur_path && cur->str.len == cur_path.len) {
            if (strncmp(cur->str.str, cur_path.str, cur_path.len) == 0) {
                found_cur_path = true;
                continue;
            }
        }

        // 1 for /, one for \0
        if (cur->str.len + suffix.len + 2 > MAX_PATH_LEN) {
            logger_log(&stream->mark, LOG_ERR, "Include path name too long");
            status = CCC_ESYNTAX;
            goto fail;
        }

        strncpy(path_buf, cur->str.str, cur->str.len);
        path_buf[cur->str.len] = '/';
        strncpy(path_buf + cur->str.len + 1, suffix.str, suffix.len);
        size_t len = cur->str.len + suffix.len + 1;
        path_buf[len] = '\0';

        // File isn't accessible
        if(-1 == access(path_buf, R_OK)) {
            continue;
        }

        // File accessible
        pp_file_t *pp_file;
        status_t status = pp_map_file(path_buf, len, file, &pp_file);
        if (CCC_OK != status) {
            goto fail;
        }

        // Add the file to the top of the stack
        sl_prepend(&pp->file_insts, &pp_file->link);
        return CCC_OK;
    }

fail:
    logger_log(&stream->mark, LOG_ERR, "Failed to include file: %.*s",
               (int)suffix.len, suffix.str);
    status = CCC_ESYNTAX;

    return status;
}

status_t pp_directive_define(preprocessor_t *pp) {
    assert(sl_head(&pp->macro_insts) == NULL && "Define inside macro!");
    status_t status = CCC_OK;

    if (pp->ignore) { // If we're ignoring, just skip
        return status;
    }

    pp_file_t *file = sl_head(&pp->file_insts);
    tstream_t *stream = &file->stream;

    pp_macro_t *new_macro = NULL;
    if (CCC_OK !=
        (status = pp_directive_define_helper(stream, &new_macro, false))) {
        goto fail;
    }

    pp_macro_t *cur_macro = ht_lookup(&pp->macros, &new_macro->name);

    // Check for macro redefinition
    if (cur_macro != NULL) {
        bool redefined = false;

        if (cur_macro->stream.cur == new_macro->stream.cur) {
            // If they point to same file location, we know they are the same
            redefined = false;
        } else if (cur_macro->num_params != new_macro->num_params) {
            // If they have different # params, they are different
            redefined = true;
        } else {
            // Slow path, need to check if the two are "effectively the same"
            // https://gcc.gnu.org/onlinedocs/cpp/Undefining-and-Redefining-Macros.html#Undefining-and-Redefining-Macros

            // Check if params are the same:
            sl_link_t *cur = cur_macro->params.head;
            sl_link_t *new = new_macro->params.head;
            for (;cur != NULL; cur = cur->next, new = new->next) {
                len_str_node_t *cur_param = GET_ELEM(&cur_macro->params, cur);
                len_str_node_t *new_param = GET_ELEM(&new_macro->params, new);

                if (!vstrcmp(&cur_param->str, &new_param->str)) {
                    redefined = true;
                    break;
                }
            }

            // Now check macro bodies
            if (!redefined) {
                tstream_t cur_stream;
                tstream_t new_stream;
                ts_copy(&cur_stream, &cur_macro->stream, TS_COPY_SHALLOW);
                ts_copy(&new_stream, &new_macro->stream, TS_COPY_SHALLOW);

                while (ts_cur(&cur_stream) != EOF) {
                    bool is_space_cur = (ts_cur(&cur_stream) == '\\' &&
                                         ts_next(&cur_stream) == '\n') ||
                        isspace(ts_cur(&cur_stream));
                    bool is_space_new = (ts_cur(&new_stream) == '\\' &&
                                         ts_next(&new_stream) == '\n') ||
                        isspace(ts_cur(&new_stream));
                    if (is_space_cur) {
                        if (!is_space_new) {
                            redefined = true;
                            break;
                        }
                        ts_skip_ws_and_comment(&cur_stream);
                        ts_skip_ws_and_comment(&new_stream);
                    }

                    if (ts_advance(&cur_stream) != ts_advance(&new_stream)) {
                        redefined = true;
                        break;
                    }
                }
                if (ts_cur(&new_stream) != EOF) {
                    redefined = true;
                }
            }
        }
        if (redefined) {
            logger_log(&stream->mark, LOG_WARN, "\"%.*s\" redefined",
                       (int)new_macro->name.len, new_macro->name.str);
        }

        // Remove and cleanup existing macro
        ht_remove(&pp->macros, &cur_macro->name);
        pp_macro_destroy(cur_macro);
    }

    // Add it to the hashtable
    if (CCC_OK != (status = ht_insert(&pp->macros, &new_macro->link))) {
        goto fail;
    }

    return status;

fail:
    pp_macro_destroy(new_macro);
    return status;
}


status_t pp_directive_define_helper(tstream_t *stream, pp_macro_t **result,
                                    bool is_cli_param) {
    status_t status = CCC_OK;
    pp_macro_t *new_macro = NULL;

    // Skip whitespace before name
    ts_skip_ws_and_comment(stream);
    if (ts_end(stream)) {
        logger_log(&stream->mark, LOG_ERR,
                   "Unexpected EOF in macro definition");
        status = CCC_ESYNTAX;
        goto fail;
    }

    // Read the name of the macro
    char *cur = ts_location(stream);
    size_t name_len = ts_advance_identifier(stream);

    // Allocate new macro object
    if (CCC_OK != (status = pp_macro_create(cur, name_len, &new_macro))) {
        logger_log(&stream->mark, LOG_ERR, "Failed to create macro");
        goto fail;
    }

    // Process paramaters
    new_macro->num_params = 0;
    if (ts_cur(stream) == '(') {
        ts_advance(stream);

        bool done = false;
        while (!done && !ts_end(stream)) {
            new_macro->num_params++;
            ts_skip_ws_and_comment(stream);
            cur = ts_location(stream);
            size_t param_len = ts_advance_identifier(stream);

            if (param_len == 0) {
                logger_log(&stream->mark, LOG_ERR,
                           "Macro missing paramater name");
                status = CCC_ESYNTAX;
                goto fail;
            }

            // Allocate paramater with string in one chunk
            len_str_node_t *string =
                malloc(sizeof(len_str_node_t) + param_len + 1);
            if (string == NULL) {
                logger_log(&stream->mark, LOG_ERR,
                           "Out of memory while defining macro");
                status = CCC_NOMEM;
                goto fail;
            }
            string->str.len = param_len;
            string->str.str = (char *)string + sizeof(*string);
            strncpy(string->str.str, cur, param_len);
            string->str.str[param_len] = '\0';

            sl_append(&new_macro->params, &string->link);

            ts_skip_ws_and_comment(stream);

            switch (ts_cur(stream)) {
            case ')':
                done = true;
                // FALL THROUGH
            case ',':
                ts_advance(stream);
                break;
            default:
                logger_log(&stream->mark, LOG_ERR,
                           "Unexpected garbage in macro parameters");
                status = CCC_ESYNTAX;
                goto fail;
            }

        }
        if (!done) {
            logger_log(&stream->mark, LOG_ERR,
                       "Unexpected EOF in macro parameters");
            status = CCC_ESYNTAX;
            goto fail;
        }
    }

    // CLI parameter requires that there is an equal sign between params and
    // body
    if (is_cli_param) {
        while (!ts_end(stream) && ts_advance(stream) != '=')
            continue;
    }

    // Skip whitespace after parameters
    ts_skip_ws_and_comment(stream);
    cur = ts_location(stream);

    // Set macro to start at this location on the stream
    if (CCC_OK != (status = ts_copy((tstream_t *)&new_macro->stream, stream,
                                    TS_COPY_DEEP))) {
        goto fail;
    }

    // Keep processing macro until we find a newline
    while (!ts_end(stream)) {
        int last = *(ts_location(stream) - 1);
        if (ts_cur(stream) == '\n' && last != '\\') {
            break;
        }
        ts_advance(stream);
    }

    // Set end
    ((tstream_t *)&new_macro->stream)->end = ts_location(stream);

    *result = new_macro;
    return status;

fail:
    pp_macro_destroy(new_macro);
    return status;
}

status_t pp_directive_undef(preprocessor_t *pp) {
    assert(sl_head(&pp->macro_insts) == NULL && "undef inside macro!");

    if (pp->ignore) { // If we're ignoring, just skip
        return CCC_OK;
    }

    status_t status = CCC_OK;
    pp_file_t *file = sl_head(&pp->file_insts);
    tstream_t *stream = &file->stream;

    // Skip whitespace before name
    ts_skip_ws_and_comment(stream);
    if (ts_end(stream)) {
        logger_log(&stream->mark, LOG_ERR, "Unexpected EOF inside undef");
        status = CCC_ESYNTAX;
        goto fail;
    }

    // Read the name of the macro
    char *cur = ts_location(stream);
    size_t name_len = ts_advance_identifier(stream);
    len_str_t lookup = { cur, name_len };
    pp_macro_t *removed = ht_remove(&pp->macros, &lookup);
    pp_macro_destroy(removed);

fail:
    return status;
}


status_t pp_directive_ifdef(preprocessor_t *pp) {
    return pp_directive_ifdef_helper(pp, "ifdef", true);
}

status_t pp_directive_ifndef(preprocessor_t *pp) {
    return pp_directive_ifdef_helper(pp, "ifndef", false);
}

status_t pp_directive_ifdef_helper(preprocessor_t *pp, const char *directive,
                                   bool ifdef) {
    assert(sl_head(&pp->macro_insts) == NULL && "Directive inside macro!");
    status_t status = CCC_OK;

    pp_file_t *file = sl_head(&pp->file_insts);
    tstream_t *stream = &file->stream;
    pp_cond_inst_t *cond_inst = NULL;

    file->if_count++;

    // We skip after incrementing so we keep track of endifs correctly
    if (pp->ignore) {
        return status;
    }

    ts_skip_ws_and_comment(stream);
    if (ts_end(stream)) {
        logger_log(&stream->mark, LOG_ERR, "Unexpected EOF in %s", directive);
        status = CCC_ESYNTAX;
        goto fail;
    }

    if (NULL == (cond_inst = malloc(sizeof(*cond_inst)))) {
        logger_log(&stream->mark, LOG_ERR, "Out of memory in %s", directive);
        status = CCC_NOMEM;
        goto fail;
    }

    char *cur = ts_location(stream);
    size_t len = ts_advance_identifier(stream);
    if (ts_end(stream)) {
        logger_log(&stream->mark, LOG_ERR, "Unexpected EOF in %s", directive);
        status = CCC_ESYNTAX;
        goto fail;
    }

    len_str_t lookup = { cur, len };
    pp_macro_t *macro = ht_lookup(&pp->macros, &lookup);

    // Put the conditional instance on the stack, and mark if we used it or not
    sl_prepend(&file->cond_insts, &cond_inst->link);
    cond_inst->start_if_count = file->if_count;

    if ((ifdef && macro != NULL) ||  // ifdef and macro found
        (!ifdef && macro == NULL)) { // ifndef and macro not found
        cond_inst->if_taken = true;
        return CCC_OK;
    }
    cond_inst->if_taken = false;
    pp->ignore = true;

    return status;

fail:
    free(cond_inst);
    return status;
}

status_t pp_directive_if(preprocessor_t *pp) {
    pp_file_t *file = sl_head(&pp->file_insts);

    file->if_count++;

    // We skip after incrementing so we keep track of endifs correctly
    if (pp->ignore) {
        return CCC_OK;
    }

    return pp_directive_if_helper(pp, "if", true);
}

status_t pp_directive_elif(preprocessor_t *pp) {
    pp_file_t *file = sl_head(&pp->file_insts);
    assert(sl_head(&file->cond_insts) != NULL);
    pp_cond_inst_t *head = sl_head(&file->cond_insts);

    // Skip if this is a nested elif, or if on the current branch, if was taken
    if ((pp->ignore && file->if_count > head->start_if_count) ||
        (head->if_taken && file->if_count == head->start_if_count)) {
        pp->ignore = true;
        return CCC_OK;
    }

    return pp_directive_if_helper(pp, "elif", false);
}

status_t pp_directive_if_helper(preprocessor_t *pp, const char *directive,
                                bool is_if) {
    assert(sl_head(&pp->macro_insts) == NULL && "if inside macro!");
    status_t status = CCC_OK;

    pp_file_t *file = sl_head(&pp->file_insts);
    tstream_t *stream = &file->stream;
    tstream_t lookahead;
    pp_cond_inst_t *cond_inst = NULL;

    // Find the end of the line
    ts_copy(&lookahead, stream, TS_COPY_SHALLOW);
    ts_skip_line(&lookahead, NULL);
    char *end = lookahead.cur;

    ts_copy(&lookahead, stream, TS_COPY_SHALLOW);
    lookahead.end = end;
    lookahead.last = 0;

    manager_t manager;
    if (CCC_OK != (status = man_init(&manager, &pp->macros))) {
        logger_log(&stream->mark, LOG_ERR,
                   "Failed to initialize parser in #%s", directive);
        goto fail0;
    }
    if (CCC_OK != (status = pp_map_stream(&manager.pp, &lookahead))) {
        logger_log(&stream->mark, LOG_ERR,
                   "Failed to map stream in #%s", directive);
        goto fail1;
    }

    expr_t *expr = NULL;
    if (CCC_OK != (status = man_parse_expr(&manager, &expr))) {
        logger_log(&stream->mark, LOG_ERR,
                   "Failed to parse expression in #%s", directive);
        goto fail1;
    }

    long long value;
    if (!typecheck_const_expr(expr, &value)) {
        logger_log(&stream->mark, LOG_ERR,
                   "Failed to typecheck and parse conditional in #%s",
                   directive);
        goto fail2;
    }

    pp_cond_inst_t *head;
    if (is_if) {
        if (NULL == (head = malloc(sizeof(*head)))) {
            logger_log(&stream->mark, LOG_ERR, "Out of memory in #%s",
                       directive);
            status = CCC_NOMEM;
            goto fail2;
        }
        head->start_if_count = file->if_count;
        sl_prepend(&file->cond_insts, &head->link);
    } else {
        head = sl_head(&file->cond_insts);
    }

    if (value) {
        head->if_taken = true;
        pp->ignore = false; // stop skipping
    } else {
        head->if_taken = false;
        pp->ignore = true;
    }

    ast_expr_destroy(expr);
    man_destroy(&manager);
    return status;

fail2:
    ast_expr_destroy(expr);
fail1:
    man_destroy(&manager);
fail0:
    if (is_if) {
        free(cond_inst);
    }
    return status;
}

status_t pp_directive_else(preprocessor_t *pp) {
    pp_file_t *file = sl_head(&pp->file_insts);
    assert(sl_head(&file->cond_insts) != NULL);
    pp_cond_inst_t *head = sl_head(&file->cond_insts);

    // Skip if this is a nested elif, or if on the current branch, if was taken
    if ((pp->ignore && file->if_count > head->start_if_count) ||
        (head->if_taken && file->if_count == head->start_if_count)) {
        pp->ignore = true;
        return CCC_OK;
    }

    pp->ignore = false; // stop skipping
    return CCC_OK;
}

status_t pp_directive_endif(preprocessor_t *pp) {
    assert(sl_head(&pp->macro_insts) == NULL && "#endif inside macro!");

    pp_file_t *file = sl_head(&pp->file_insts);
    pp_cond_inst_t *head = sl_head(&file->cond_insts);
    if (file->if_count == 0) {
        logger_log(&file->stream.mark, LOG_ERR, "Unexpected #endif");
        return CCC_ESYNTAX;
    }

    if (file->if_count == head->start_if_count) {
        // Stop ignoring input if we reached the if causing us to skip
        if (pp->ignore) {
            pp->ignore = false;
        }
        // Remove the conditional instance
        pp_cond_inst_t *cond_inst = sl_pop_front(&file->cond_insts);
        assert(cond_inst != NULL);
        free(cond_inst);
    }

    file->if_count--;

    return CCC_OK;
}

status_t pp_directive_error(preprocessor_t *pp) {
    return pp_directive_error_helper(pp, true);
}

status_t pp_directive_warning(preprocessor_t *pp) {
    return pp_directive_error_helper(pp, false);
}

status_t pp_directive_error_helper(preprocessor_t *pp, bool is_err) {
    assert(sl_head(&pp->macro_insts) == NULL);
    status_t status = CCC_OK;
    if (pp->ignore) { // Don't report if we're ignoring
        return status;
    }

    pp_file_t *file = sl_head(&pp->file_insts);
    tstream_t *stream = &file->stream;
    tstream_t lookahead;

    ts_copy(&lookahead, stream, TS_COPY_SHALLOW);
    size_t len = ts_skip_line(&lookahead, NULL);

    log_type_t log_type = is_err ? LOG_ERR : LOG_WARN;

    logger_log(&stream->mark, log_type, "%.*s", (int)len, stream->cur);

    return status;
}

status_t pp_directive_pragma(preprocessor_t *pp) {
    return pp_directive_pragma_helper(pp, PRAGMA_POUND);
}

status_t pp_directive_pragma_helper(preprocessor_t *pp, int pragma_type) {
    status_t status = CCC_OK;
    // Right now no pragmas are implemented
    // For PRAGMA_UNDER, do string substitution with backslash
    (void)pp;
    (void)pragma_type;
    return status;
}

status_t pp_directive_line(preprocessor_t *pp) {
    status_t status = CCC_OK;
    pp_file_t *file = sl_tail(&pp->file_insts);

    char linebuf[MAX_LINE];
    char filename[MAX_LINE];
    size_t offset = 0;
    int last = -1;
    int cur = pp_nextchar(pp);

    bool done = false;

    // Read until end of line. Use pp_nextchar so macros are expanded
    while (offset < sizeof(linebuf)) {
        cur = pp_nextchar(pp);
        if ((done = (cur == '\n' && last != '\\'))) {
            break;
        }
        last = cur;
        linebuf[offset++] = cur;
    }
    linebuf[offset] = '\0';

    char after;
    int line_num = -1;
    int matched = sscanf(linebuf, " %d %s %c", &line_num, filename, &after);

    switch (matched) {
    case EOF:
    case 0:
        logger_log(&pp->last_mark, LOG_ERR,
                   "unexpected end of file after #line");
        status = CCC_ESYNTAX;
        goto fail;
    case 1:
        file->stream.mark.line = line_num;
        break;
    case 2: {
        size_t len = strlen(filename);
        if (filename[0] != '"' ||
            !(filename[len - 1] == '"' && filename[len - 2] != '\\')) {
            logger_log(&pp->last_mark, LOG_ERR,
                       "\"%s\" is not a valid filename", filename);
            status = CCC_ESYNTAX;
            goto fail;
        }
        len -= 2; // -2 for quotes
        len_str_t *new_filename = malloc(sizeof(len_str_t) + len + 1);
        if (new_filename == NULL) {
            logger_log(&pp->last_mark, LOG_ERR, "Out of memory");
            status = CCC_NOMEM;
            goto fail;
        }
        new_filename->str = (char *)new_filename + sizeof(*new_filename);
        new_filename->len = len;
        // Copy filename + 1 so we don't copy the quote
        strncpy(new_filename->str, filename + 1, len);
        new_filename->str[len] = '\0';

        if (file->owns_name) { // Free the old new_filename if it owns it
            free(file->stream.mark.file);
        } else {
            file->owns_name = true;
        }
        file->stream.mark.file = new_filename;
        file->stream.mark.line = line_num;
        break;
    }
    case 3:
        logger_log(&pp->last_mark, LOG_ERR,
                   "extra tokens at end of #line directive");
        status = CCC_ESYNTAX;
        goto fail;
    default:
        assert(false);
    }

fail:
    return status;
}
