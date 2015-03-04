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

#include <fnctl.h>

#include "util/slist.h"
#include "util/htable.h"

status_t pp_init(preprocessor_t *pp) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = sl_init(&pp->file_insts))) {
        goto fail1;
    }

    if (CCC_OK != (status = sl_init(&pp->macro_param_stack))) {
        goto fail2;
    }

    if (CCC_OK != (status = ht_init(&pp->macros))) {
        goto fail3;
    }

    pp->comment = false;
    pp->char_line = false;

    return status;

fail3:
    sl_destroy(&pp->macro_param_stack, SL_FREE);
fail2:
    sl_destroy(&pp->file_insts, SL_FREE);
fail1:
    return status;
}

static void clean_pp(void *pp_file) {
    pp_destroy((pp_file_t *)pp_file);
}

static void clean_ht(void *ht) {
    ht_destroy((htable_t *)ht);
}

void pp_destroy(preprocessor_t *pp) {
    // Unmap all of the files on the file stack, then free the handles
    sl_foreach(&pp->file_insts, clean_pp);
    sl_destroy(&pp->file_insts, SL_FREE);

    // Clear hashtables on macro param stack then free the them
    sl_foreach(&pp->macro_param_stack, clean_ht);
    sl_destroy(&pp->macro_param_stack, SL_FREE);

    ht_destroy(&pp->macros);
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
    if (CCC_OK != (status = pp_map_file(filename, &pp_file))) {
        goto done;
    }

    sl_prepend(&pp->file_insts, &pp_file->link);

done:
    return status;
}

int pp_nextchar(preprocessor_t *pp) {
    // We are in a macro paramater, just copy the string
    if (NULL != pp->cur_param) {
        int result = pp->cur_param++;

        // Record if we reached end of the paramater
        if (pp->cur_param == pp->param_end) {
            pp->cur_param = NULL;
            pp->param_end = NULL;
        }

        return result;
    }

    pp_macro_inst *macro_inst = sl_head(&&pp->macro_insts);
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

        pp_macro_inst *last = sl_pop_front(&pp->macro_insts);
        pp_macro_inst_destroy(last);
        macro_inst = sl_head(&pp->macro_insts);
    }

    // if we're done with macros, try to find an incomplete file
    if (NULL == macro_inst) {
        while (NULL != file) {
            &cur = file->cur;
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
    int next_char = *cur + 1 != end ? *(*cur + 1) : -1;

    switch (cur_char) {
        // Cases which cannot be before a macro
        // TODO: verify/add more of these
    case #include "util/lower.inc":
    case #include "util/upper.inc":
    case #include "util/number.inc":
        // Return the current character
        // Also advance the current cur pointer
        return *((*cur_char)++);
    default:
        // Fall through, need to look for macros
    }

    // if next character is not alphanumeric, we know this shouldn't be a macro
    if (!isalnum(next_char)) {
        return *((*cur_char)++);
    }

    // Need to look ahead current identifier and see if its a macro
    char *lookahead = next_char;

    while(lookahead != end) {
        // Charaters allowed to be in macro
    case #include "util/lower.inc":
    case #include "util/upper.inc":
    case #include "util/number.inc":
        lookahead++
    default: // Found end
        break;
    }

    len_str_t lookup = {
        next_char,
        (size_t)(lookahead - next_char);
    };

    // Macro paramaters take precidence, look them up first
    if (NULL != macro_inst) {
        pp_param_map_elemnt_t *param = ht_lookup(&macro->param_map, lookup);

        // Found a parameter
        // Advance lookhead to end of param, and set param state in pp
        if (NULL != param) {
            int retval = **cur;
            *cur = lookahead;
            pp->cur_param = param->val.str;
            pp->param_end = param->val.str + param->val.len;
            return retval;
        }
    }

    // Look up in the macro table
    pp_macro_t *macro = ht_lookup(&pp->macros, lookup);

    if (NULL == macro) { // No macro found
        return *((*cur_char)++);
    }

    pp_macro_inst_t *macro_inst;
    status_t status = pp_macro_inst_create(macro, &macro_inst);

    if (CCC_OK != status) {
        // TODO Handle this correctly, propgate the error
        assert(false);
    }

    // No paramaters on macro
    if (lookahead != '(') {

        // If macro requires params, but none are provided, this is just
        // treated as identifier
        if (macro->num_params != 0) {
            return *((*cur_char)++);
        }
    } else {
        // Need to create param map

        int num_params = 0;

        bool done = false;
        char *cur_param = ++lookahead;
        SL_FOREACH(cur, &macro->params, link) {
            num_params++;

            while (lookahead != end) {
                // Skip single line whitespace
                if (*lookahead == ' ' || *lookahead == '\t') {
                    lookahead++;
                    continue;
                }

                if (*lookahead == ',') {
                    break;
                }

                if (*lookahead == ')') {
                    done = true;
                    break;
                }
            }

            // TODO handle/report error
            if (lookahead == end && *lookahead != ')') {
                assert(false && "Unfilled macro");
            }

            size_t cur_len = lookhead - cur_param;

            pp_param_map_elem_t *param_elem =
                malloc(sizeof(pp_param_map_elem_t));

            param_elem->key.str = cur_param;
            param_elem->key.len = cur_len;

            ht_insert(&macro_inst->param_map, &param_elem->link);
        }

        // TOOD: handle/report error
        if (done && num_params != macro->num_params) {
            assert(false && "Incorrect number of macro paramaters");
        }
    }

    macro_inst->cur = macro->start;
    macro_inst->end = macro->end;

    // Add new macro instance to the stack
    sl_push_front(&pp->macro_insts, &macro_inst->link);

    int retval = **cur;   // Return current character
    *cur = lookahead; // Set current to the end of the macro and params

    return retval;
}

status_t pp_map_file(const char *filename, pp_file_t **result) {
    status_t status = CCC_OK;
    pp_file_t *pp_file = malloc(sizeof(pp_file_t));
    if (NULL == pp_file) {
        status = CCC_NOMEM;
        goto fail1;
    }

    if (-1 == (pp_file->fd = open(filename, O_RD, 0))) {
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
    pp_file->max = pp_file->buf + size;

    *result = pp_file;

    return status;
fail2:
    free(pp_file);
fail1:
    *result = NULL;
    return status;
}
