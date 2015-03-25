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

#include "util/htable.h"
#include "util/util.h"

#include "typecheck/typechecker.h"

#define MAX_PATH_LEN 4096 /**< Max include path len */

#define DIRECTIVE_LIT(name) \
    { SL_LINK_LIT, LEN_STR_LIT(#name), pp_directive_ ## name  }

static pp_directive_t s_directives[] = {
    DIRECTIVE_LIT(include),
    DIRECTIVE_LIT(define ),
    DIRECTIVE_LIT(undef  ),
    DIRECTIVE_LIT(ifdef  ),
    DIRECTIVE_LIT(ifndef ),
    DIRECTIVE_LIT(if     ),
    DIRECTIVE_LIT(elif   ),
    DIRECTIVE_LIT(else   ),
    DIRECTIVE_LIT(endif  )
    //DIRECTIVE_LIT(error   )   // TODO: This
    //DIRECTIVE_LIT(warning   )   // TODO: This
    //DIRECTIVE_LIT(line   )   // TODO: This
    //DIRECTIVE_LIT(pragma   )   // TODO: This
};

// Default search path for #include files. Ordering is important
static len_str_node_t s_default_search_path[] = {
    { { NULL }, LEN_STR_LIT("./") }, // Current directory
    { { NULL }, LEN_STR_LIT("/usr/local/include/") },
    { { NULL }, LEN_STR_LIT("/usr/include/") }
};

status_t pp_directives_init(preprocessor_t *pp) {
    status_t status = CCC_OK;

    static ht_params_t params = {
        STATIC_ARRAY_LEN(s_directives),   // Size estimate
        offsetof(pp_directive_t, key),    // Offset of key
        offsetof(pp_directive_t, link),   // Offset of ht link
        strhash,                          // Hash function
        vstrcmp,                          // void string compare
    };

    if (CCC_OK != (status = ht_init(&pp->directives, &params))) {
        goto done;
    }

    // Add directive handlers
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_directives); ++i) {
        ht_insert(&pp->directives, &s_directives[i].link);
    }

    // Add default search path
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_default_search_path); ++i) {
        sl_append(&pp->search_path, &s_default_search_path[i].link);
    }

done:
    return status;
}

void pp_directives_destroy(preprocessor_t *pp) {
    ht_destroy(&pp->directives);
    sl_destroy(&pp->macro_insts);
}

status_t pp_skip_cond(preprocessor_t *pp, tstream_t *stream,
                      const char *directive) {
    status_t status = CCC_OK;

    // Need to skip over contents
    pp->ignore = true;
    while (pp->ignore) {
        // Fetch characters until another directive is run to tell us to stop
        // ignoring
        pp_nextchar(pp);
        if (ts_end(stream)) { // Only go to end of current file
            snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                     "Unexpected EOF in #%s directive", directive);
            logger_log(&stream->mark, logger_fmt_buf, LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }
    }

fail:
    return status;
}

/**
 * Warning: This is not reentrant!
 */
status_t pp_directive_include(preprocessor_t *pp) {
    static char s_path_buf[MAX_PATH_LEN];
    static char s_suffix_buf[MAX_PATH_LEN];

    assert(NULL == sl_head(&pp->macro_insts) && "include inside macro!");

    if (pp->ignore) { // If we're ignoring, just skip
        return CCC_OK;
    }

    status_t status = CCC_OK;

    pp_file_t *file = sl_head(&pp->file_insts);
    tstream_t *stream = &file->stream;

    ts_skip_ws_and_comment(stream);
    if (ts_end(stream)) {
        logger_log(&stream->mark, "Unexpected EOF in #include", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    len_str_t suffix;

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
                ts_advance(stream);
                break;
            default: /* Found end */
                done = true;
            }
        }

        // Reached end
        if (ts_end(stream)) {
            logger_log(&stream->mark, "Unexpected EOF in #include", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }

        // 0 length
        if (ts_location(stream) == cur) {
            logger_log(&stream->mark, "0 length include path", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }

        // Incorrect end symbol
        if (ts_cur(stream) != endsym) {
            logger_log(&stream->mark, "Unexpected symbol in #include", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }

        suffix.str = cur;
        suffix.len = ts_location(stream) - cur;

        // skip the rest of the line
        ts_skip_line(stream);
        break;

        // Identifier, expand macros
    case ASCII_LOWER:
    case ASCII_UPPER:
    case ASCII_DIGIT:
    case '_':
        done = false;
        // Find the starting character
        while (!done) {
            int next = pp_nextchar_helper(pp, true);
            if (next == PP_EOF) {
                logger_log(&stream->mark, "Unexpected EOF in #include",
                           LOG_ERR);
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
                snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                        "Unexpected character %c in #include", next);
                logger_log(&stream->mark, logger_fmt_buf, LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail;
            }
        }

        int offset = 0;
        done = false;

        // Find the end of the include string
        while (true) {
            int next = pp_nextchar_helper(pp, true);
            if (offset == MAX_PATH_LEN) {
                logger_log(&stream->mark, "Include path name too long",
                           LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail;
            }
            if (next == PP_EOF) {
                logger_log(&stream->mark, "Unexpected EOF in #include",
                           LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail;
            }

            if (next == endsym) {
                break;
            }
            s_suffix_buf[offset++] = next;
        }

        s_suffix_buf[offset] = '\0';

        suffix.str = s_suffix_buf;
        suffix.len = offset;

        // Skip until next line
        done = false;
        int last = -1;
        while (!done) {
            int next = pp_nextchar_helper(pp, true);
            if (PP_EOF == next) {
                done = true;
            }

            if ('\n' == next && '\\' != last) {
                done = true;
            }
            last = next;
        }
        break;
    default:
        snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                 "Unexpected character %c in #include", *cur);
        logger_log(&stream->mark, logger_fmt_buf, LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    // Search for the string in all of the search paths
    sl_link_t *link;
    SL_FOREACH(link, &pp->search_path) {
        len_str_node_t *cur = GET_ELEM(&pp->search_path, link);

        if (cur->str.len + suffix.len + 1 > MAX_PATH_LEN) {
            logger_log(&stream->mark, "Include path name too long", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }

        strncpy(s_path_buf, cur->str.str, cur->str.len);
        strncpy(s_path_buf + cur->str.len, suffix.str, suffix.len);
        size_t len = cur->str.len + suffix.len;
        s_path_buf[len] = '\0';

        // File isn't accessible
        if(-1 == access(s_path_buf, R_OK)) {
            continue;
        }

        // File accessible
        pp_file_t *pp_file;
        status_t status = pp_map_file(s_path_buf, len, file, &pp_file);
        if (CCC_OK != status) {
            goto fail;
        }

        // Add the file to the top of the stack
        sl_prepend(&pp->file_insts, &pp_file->link);
        return CCC_OK;
    }

fail:
    snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
             "Failed to include file: %.*s", (int)suffix.len, suffix.str);
    logger_log(&stream->mark, logger_fmt_buf, LOG_ERR);
    status = CCC_ESYNTAX;

    return status;
}

status_t pp_directive_define(preprocessor_t *pp) {
    assert(sl_head(&pp->macro_insts) == NULL && "Define inside macro!");

    if (pp->ignore) { // If we're ignoring, just skip
        return CCC_OK;
    }

    status_t status = CCC_OK;
    pp_file_t *file = sl_head(&pp->file_insts);
    tstream_t *stream = &file->stream;

    pp_macro_t *new_macro = NULL;

    // Skip whitespace before name
    ts_skip_ws_and_comment(stream);
    if (ts_end(stream)) {
        logger_log(&stream->mark, "Unexpected EOF in macro definition",
                   LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    // Read the name of the macro
    char *cur = ts_location(stream);
    size_t name_len = ts_advance_identifier(stream);
    len_str_t lookup = { cur, name_len };
    pp_macro_t *cur_macro = ht_lookup(&pp->macros, &lookup);
    if (cur_macro != NULL) {
        logger_log(&stream->mark, "Macro redefinition", LOG_WARN);

        // Remove and cleanup existing macro
        ht_remove(&pp->macros, cur_macro);
        pp_macro_destroy(cur_macro);
    }

    // Allocate new macro object
    if (CCC_OK != (status = pp_macro_create(cur, name_len, &new_macro))) {
        logger_log(&stream->mark, "Failed to create macro", LOG_ERR);
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
                logger_log(&stream->mark, "Macro missing paramater name",
                           LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail;
            }

            // Allocate paramater with string in one chunk
            len_str_node_t *string =
                malloc(sizeof(len_str_node_t) + param_len + 1);
            if (string == NULL) {
                logger_log(&stream->mark, "Out of memory while defining macro",
                           LOG_ERR);
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
                logger_log(&stream->mark,
                           "Unexpected garbage in macro parameters", LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail;
            }

        }
        if (!done) {
            logger_log(&stream->mark, "Unexpected EOF in macro parameters",
                       LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }
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

    // Add it to the hashtable
    if (CCC_OK != (status = ht_insert(&pp->macros, &new_macro->link))) {
        goto fail;
    }
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
        logger_log(&stream->mark, "Unexpected EOF inside undef", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    // Read the name of the macro
    char *cur = ts_location(stream);
    size_t name_len = ts_advance_identifier(stream);
    len_str_t lookup = { cur, name_len };
    ht_remove(&pp->macros, &lookup);

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
    assert(NULL == sl_head(&pp->macro_insts) && "Directive inside macro!");
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
        snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE, "Unexpected EOF in %s",
                 directive);
        logger_log(&stream->mark, logger_fmt_buf, LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    if (NULL == (cond_inst = malloc(sizeof(*cond_inst)))) {
        status = CCC_NOMEM;
        goto fail;
    }

    char *cur = ts_location(stream);
    size_t len = ts_advance_identifier(stream);
    if (ts_end(stream)) {
        snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE, "Unexpected EOF in %s",
                 directive);
        logger_log(&stream->mark, logger_fmt_buf, LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    len_str_t lookup = { cur, len };
    pp_macro_t *macro = ht_lookup(&pp->macros, &lookup);
    ts_skip_line(stream);

    // Put the conditional instance on the stack, and mark if we used it or not
    sl_prepend(&file->cond_insts, &cond_inst->link);

    if ((ifdef && macro != NULL) ||  // ifdef and macro found
        (!ifdef && macro == NULL)) { // ifndef and macro not found
        file->start_if_count = file->if_count;
        cond_inst->if_taken = true;
        return CCC_OK;
    }
    cond_inst->if_taken = false;

    status = pp_skip_cond(pp, stream, directive);

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
    if (pp->ignore &&
        (file->if_count > file->start_if_count || head->if_taken)) {
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
    ts_skip_line(&lookahead);
    char *end = lookahead.cur;

    ts_copy(&lookahead, stream, TS_COPY_SHALLOW);
    lookahead.end = end;

    manager_t manager;
    if (CCC_OK != (status = man_init(&manager, &pp->macros))) {
        goto fail0;
    }
    if (CCC_OK != (status = pp_map_stream(&manager.pp, &lookahead))) {
        goto fail1;
    }

    expr_t *expr = NULL;
    if (CCC_OK != (status = man_parse_expr(&manager, &expr))) {
        goto fail1;
    }

    long long value;
    if (!typecheck_const_expr(expr, &value)) {
        goto fail2;
    }

    pp_cond_inst_t *head;
    if (is_if) {
        file->start_if_count = file->if_count;
        if (NULL == (head = malloc(sizeof(*head)))) {
            status = CCC_NOMEM;
            goto fail2;
        }
    } else {
        head = sl_head(&file->cond_insts);
    }

    if (value) {
        head->if_taken = true;
        pp->ignore = false; // stop skipping
    } else {
        head->if_taken = false;
        if (!pp->ignore) { // Only skip if we're not already skipping
            status = pp_skip_cond(pp, stream, directive);
        }
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

    // Skip if this is a nested else, or if on the current branch, if was taken
    if (pp->ignore &&
        (file->if_count > file->start_if_count || head->if_taken)) {
        return CCC_OK;
    }

    pp->ignore = false; // stop skipping
    return CCC_OK;
}

status_t pp_directive_endif(preprocessor_t *pp) {
    assert(NULL == sl_head(&pp->macro_insts) && "#endif inside macro!");

    pp_file_t *file = sl_head(&pp->file_insts);
    if (file->if_count == 0) {
        logger_log(&file->stream.mark, "Unexpected #endif", LOG_ERR);
        return CCC_ESYNTAX;
    }

    // Stop ignoring input if we reached the if causing us to skip
    if (pp->ignore && file->if_count == file->start_if_count) {
        pp->ignore = false;
    }

    file->if_count--;
    pp_cond_inst_t *cond_inst = sl_head(&file->cond_insts);
    assert(cond_inst != NULL);
    free(cond_inst);

    return CCC_OK;
}
