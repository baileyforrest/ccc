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
    // Add directive handlers
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_directives); ++i) {
        ht_insert(&pp->directives, &s_directives[i].link);
    }

    // Add default search path
    for (size_t i = 0; i < STATIC_ARRAY_LEN(s_default_search_path); ++i) {
        sl_append(&pp->search_path, &s_default_search_path[i].link);
    }

    return CCC_OK;
}

/**
 * Note that this function needs to allocate memory for the paramaters, macro
 * name and body. This is because the mmaped file will be unmapped when we
 * are done with the file.
 */
status_t pp_directive_define(preprocessor_t *pp) {
    assert(NULL == sl_head(&pp->macro_insts) && "Define inside macro!");

    status_t status = CCC_OK;
    pp_file_t *file = sl_head(&pp->file_insts);
    char *cur = file->cur;
    char *end = file->end;
    char *lookahead = file->cur;

    // Skip whitespace before name
    SKIP_WS_AND_COMMENT(lookahead, end);
    if (lookahead == end) {
        LOG_ERROR(pp, "Macro definition at end of file", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail1;
    }
    cur = lookahead;

    // Read the name of the macro
    ADVANCE_IDENTIFIER(lookahead, end);
    if (lookahead == end) {
        LOG_ERROR(pp, "Macro definition at end of file", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail2;
    }

    size_t name_len = lookahead - cur;

    len_str_node_t lookup = { { NULL }, { cur, name_len } };

    pp_macro_t *cur_macro = ht_lookup(&pp->macros, &lookup);
    if (NULL != cur_macro) {
        LOG_ERROR(pp, "Macro redefinition", LOG_WARN);

        // Remove and free existing macro
        ht_remove(&pp->macros, cur_macro, DOFREE);
    }

    // Create the macro object
    pp_macro_t *new_macro = malloc(sizeof(pp_macro_t));
    if (NULL == new_macro) {
        LOG_ERROR(pp, "Out of memory while defining macro", LOG_ERR);
        status = CCC_NOMEM;
        goto fail1;
    }

    status = pp_macro_init(new_macro);
    if (CCC_OK != status) {
        LOG_ERROR(pp, "Failed to create macro", LOG_ERR);
        status = status;
        goto fail2;
    }

    // Allocate the name
    new_macro->name.len = name_len;
    new_macro->name.str = malloc(name_len + 1);
    if (NULL == new_macro->name.str) {
        LOG_ERROR(pp, "Out of memory while defining macro", LOG_ERR);
        status = CCC_NOMEM;
        goto fail3;
    }
    strncpy(new_macro->name.str, cur, new_macro->name.len);
    new_macro->name.str[new_macro->name.len] = '\0';

    cur = lookahead;

    // Process paramaters
    new_macro->num_params = 0;
    if ('(' == *lookahead) {
        while (lookahead != end) {
            new_macro->num_params++;

            ADVANCE_IDENTIFIER(lookahead, end);
            size_t param_len = lookahead - cur;

            if (param_len == 0) {
                LOG_ERROR(pp, "Macro missing paramater name", LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail3;
            }

            // Allocate paramaters
            len_str_node_t *string = malloc(sizeof(len_str_t));
            if (NULL == string) {
                LOG_ERROR(pp, "Out of memory while defining macro", LOG_ERR);
                status = CCC_NOMEM;
                goto fail3;
            }
            string->str.len = param_len;
            string->str.str = malloc(param_len + 1);
            strncpy(string->str.str, cur, param_len);
            string->str.str[param_len] = '\0';

            sl_append(&new_macro->params, &string->link);

            cur = lookahead + 1;

            if (*lookahead == ',') {
                lookahead++;
            }

            if (*lookahead == ')') { // End of param list
                lookahead++;
                break;
            }
        }
        if (lookahead == end) {
            LOG_ERROR(pp, "Unexpected EOF in macro definition", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail3;
        }
    }

    // Skip whitespace after parameters
    SKIP_WS_AND_COMMENT(lookahead, end);
    if (lookahead == end) {
        LOG_ERROR(pp, "Unexpected EOF in macro definition", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail3;
    }

    cur = lookahead;

    // Keep processing macro until we find a newline
    while (lookahead != end) {
        if ('\n' == *lookahead && '\\' != *(lookahead - 1)) {
            break;
        }
        lookahead++;
    }
    if (lookahead == end) {
        LOG_ERROR(pp, "Unexpected EOF in macro definition", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail3;
    }

    // Allocate the macro body
    size_t macro_len = lookahead - cur;
    new_macro->start = macro_len == 0 ? NULL : malloc(macro_len + 1);
    if (macro_len != 0 && NULL == new_macro->start) {
        LOG_ERROR(pp, "Out of memory while defining macro", LOG_ERR);
        status = CCC_NOMEM;
        goto fail3;
    }
    strncpy(new_macro->start, cur, macro_len);

    new_macro->end = new_macro->start + macro_len;
    *new_macro->end = '\0';

    // Add it to the hashtable
    if (CCC_OK != (status = ht_insert(&pp->macros, &new_macro->link))) {
        goto fail3;
    }
    return status;

fail3:
    pp_macro_destroy(new_macro);
fail2:
    free(new_macro);
fail1:
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
    char *cur = file->cur;
    char *end = file->end;
    char *lookahead = file->cur;

    SKIP_WS_AND_COMMENT(lookahead, end);
    if (lookahead == end) {
        LOG_ERROR(pp, "Unexpected EOF in #include", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail1;
    }

    len_str_t suffix;

    cur = lookahead;
    char endsym = 0;
    switch (*cur) {
        // quote or ange bracket
    case '"':
        endsym = '"';
    case '<':
        endsym = endsym ? endsym : '>';

        cur++;
        lookahead++;

        bool done = false;
        while (!done && lookahead != end) {
            /* Charaters allowed to be in path name */
            switch (*lookahead) {
            case ASCII_LOWER:
            case ASCII_UPPER:
            case ASCII_DIGIT:
            case '_':
            case '-':
            case '.':
                lookahead++;
                break;
            default: /* Found end */
                done = true;
            }
        }

        // Reach end
        if (lookahead == end) {
            LOG_ERROR(pp, "Unexpected EOF in #include", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail1;
        }

        // 0 length
        if (lookahead == cur) {
            LOG_ERROR(pp, "0 length include path", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail1;
        }

        // Incorrect end symbol
        if (*lookahead != endsym) {
            LOG_ERROR(pp, "Unexpected symbol in #include", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail1;
        }

        suffix.str = cur;
        suffix.len = lookahead - cur;

        // skip the rest of the line
        SKIP_LINE(lookahead, end);
        file->cur = lookahead;

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
            if (PP_EOF == next) {
                LOG_ERROR(pp, "Unexpected EOF in #include", LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail1;
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
                // TODO: report expected/found character
                LOG_ERROR(pp, "Unexpected character in #include", LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail1;
            }
        }

        int offset = 0;
        done = false;

        // Act like we're in a string
        pp->string = true;

        // Find the end of the include string
        while (true) {
            int next = pp_nextchar_helper(pp, true);
            if (offset == MAX_PATH_LEN) {
                LOG_ERROR(pp, "Include path name too long", LOG_ERR);
                return -(int)CCC_ESYNTAX;
            }
            if (PP_EOF == next) {
                LOG_ERROR(pp, "Unexpected EOF in #include", LOG_ERR);
                return -(int)CCC_ESYNTAX;
            }

            if (next != endsym) {
                s_suffix_buf[offset++] = next;
                continue;
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
        }
        break;
    default:
        // TODO: report expected/found character
        LOG_ERROR(pp, "Unexpected character in #include", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail1;
    }

    // Search for the string in all of the search paths
    sl_link_t *link;
    SL_FOREACH(link, &pp->search_path) {
        len_str_t *cur = GET_ELEM(&pp->search_path, link);

        if (cur->len + suffix.len + 1 > MAX_PATH_LEN) {
            LOG_ERROR(pp, "Include path name too long", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail1;
        }

        strncpy(s_path_buf, cur->str, cur->len);
        strncpy(s_path_buf + cur->len, suffix.str, suffix.len);
        size_t len = cur->len + suffix.len;
        s_path_buf[len] = '\0';

        // File isn't accessible
        if(-1 == access(s_path_buf, R_OK)) {
            continue;
        }

        // File accessible
        pp_file_t *pp_file;
        status_t status = pp_file_map(s_path_buf, len, &pp_file);
        if (CCC_OK != status) {
            LOG_ERROR(pp, "Failed to include file", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail1;
        }

        // Add the file to the top of the stack
        sl_prepend(&pp->file_insts, &pp_file->link);
        break;
    }

    return status;

fail1:
    return status;
}

status_t pp_directive_ifndef(preprocessor_t *pp) {
    assert(NULL == sl_head(&pp->macro_insts) && "include inside macro!");

    status_t status = CCC_OK;

    pp_file_t *file = sl_head(&pp->file_insts);
    file->if_count++;

    char *cur = file->cur;
    char *end = file->end;
    char *lookahead = file->cur;

    SKIP_WS_AND_COMMENT(lookahead, end);
    if (lookahead == end) {
        LOG_ERROR(pp, "Unexpected EOF in #ifndef", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail1;
    }

    cur = lookahead;
    ADVANCE_IDENTIFIER(lookahead, end);
    if (lookahead == end) {
        LOG_ERROR(pp, "Unexpected EOF in #ifndef", LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail1;
    }

    len_str_t lookup = { cur, (size_t)(lookahead - cur) };

    pp_macro_t *macro = ht_lookup(&pp->macros, &lookup);
    SKIP_LINE(lookahead, end);

    // No macro found, proceed after skipping line
    if (NULL == macro) {
        file->cur = lookahead;
        return CCC_OK;
    }

    cur = lookahead;
    // Skip ahead until we find a directive telling us to stop
    while (cur != end) {
        int cur_char = *lookahead;
        char *next_char = lookahead + 1;

        // TODO: this logic is duplicated in preprocessor.c
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
            pp->string = false;
        }

        // Reset line variables
        if ('\n' == cur_char) {
            pp->char_line = false;
            pp->line_comment = false;
        }

        // Cases to ignore directives
        if (pp->block_comment || pp->line_comment || pp->string ||
            pp->char_line) {
            continue;
        }

        if ('#' != cur_char) {
            continue;
        }

        // cur_char == '#' look for a preprocessor command
        lookahead = cur + 1;

        ADVANCE_IDENTIFIER(lookahead, end);
        size_t len = lookahead - cur;
        if (0 != strncmp(cur, "endif", len)) {
            continue;
        }
        pp_directive_endif(pp);
    }

    return status;

fail1:
    return status;
}

status_t pp_directive_endif(preprocessor_t *pp) {
    assert(NULL == sl_head(&pp->macro_insts) && "include inside macro!");

    pp_file_t *file = sl_head(&pp->file_insts);
    if (0 == file->if_count) {
        LOG_ERROR(pp, "Unexpected #endif", LOG_ERR);
        return CCC_ESYNTAX;
    }
    file->if_count--;

    return CCC_OK;
}
