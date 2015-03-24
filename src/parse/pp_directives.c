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

#include "util/htable.h"
#include "util/util.h"

#define MAX_PATH_LEN 4096 /**< Max include path len */

static pp_directive_t s_directives[] = {
    { { NULL }, LEN_STR_LITERAL("define" ), pp_directive_define  },
    { { NULL }, LEN_STR_LITERAL("include"), pp_directive_include },
    { { NULL }, LEN_STR_LITERAL("ifndef" ), pp_directive_ifndef  },
    { { NULL }, LEN_STR_LITERAL("endif"  ), pp_directive_endif   }
};

// Default search path for #include files. Ordering is important
static len_str_node_t s_default_search_path[] = {
    { { NULL }, LEN_STR_LITERAL(".") }, // Current directory
    { { NULL }, LEN_STR_LITERAL("/usr/local/include") },
    { { NULL }, LEN_STR_LITERAL("/usr/include") }
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

/**
 * Note that this function needs to allocate memory for the paramaters, macro
 * name and body. This is because the mmaped file will be unmapped when we
 * are done with the file.
 */
status_t pp_directive_define(preprocessor_t *pp) {
    assert(sl_head(&pp->macro_insts) == NULL && "Define inside macro!");

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

/**
 * Warning: This is not reentrant!
 */
status_t pp_directive_include(preprocessor_t *pp) {
    static char s_path_buf[MAX_PATH_LEN];
    static char s_suffix_buf[MAX_PATH_LEN];

    assert(NULL == sl_head(&pp->macro_insts) && "include inside macro!");

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
        status_t status = pp_file_map(s_path_buf, len, file, &pp_file);
        if (CCC_OK != status) {
            logger_log(&stream->mark, "Failed to include file", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }

        // Add the file to the top of the stack
        sl_prepend(&pp->file_insts, &pp_file->link);
        break;
    }

fail:
    return status;
}

status_t pp_directive_ifndef(preprocessor_t *pp) {
    assert(NULL == sl_head(&pp->macro_insts) && "Directive inside macro!");

    status_t status = CCC_OK;

    pp_file_t *file = sl_head(&pp->file_insts);
    file->if_count++;

    tstream_t *stream = &file->stream;

    ts_skip_ws_and_comment(stream);
    if (ts_end(stream)) {
        logger_log(&stream->mark, "Unexpected EOF in #ifndef", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    char *cur = ts_location(stream);
    size_t len = ts_advance_identifier(stream);
    if (ts_end(stream)) {
        logger_log(&stream->mark, "Unexpected EOF in #ifndef", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    len_str_t lookup = { cur, len };
    pp_macro_t *macro = ht_lookup(&pp->macros, &lookup);
    ts_skip_line(stream);

    // No macro found, just do nothing
    if (macro == NULL) {
        return CCC_OK;
    }

    pp->ignore = true; // Set flag to ignore characters
    while (pp->ignore) {
        // Fetch characters until another directive is run to tell us to stop
        // ignoring
        pp_nextchar(pp);
        if (ts_end(stream)) { // Only go to end of current file
            logger_log(&stream->mark, "Unexpected EOF in #ifndef directive",
                       LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }
    }

    return status;

fail:
    return status;
}

status_t pp_directive_endif(preprocessor_t *pp) {
    assert(NULL == sl_head(&pp->macro_insts) && "include inside macro!");

    pp_file_t *file = sl_head(&pp->file_insts);
    if (0 == file->if_count) {
        logger_log(&file->stream.mark, "Unexpected #endif", LOG_ERR);
        return CCC_ESYNTAX;
    }
    file->if_count--;
    if (pp->ignore) {
        pp->ignore = false;
    }

    return CCC_OK;
}
