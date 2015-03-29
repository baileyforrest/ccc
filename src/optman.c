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
 * Option manager implementation
 */

#include "optman.h"

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <getopt.h>

#include "util/logger.h"

//#define DEBUG_PARAMS

#define DEFAULT_OUTPUT_NAME "a.out"

typedef enum long_opt_idx_t {
    LOPT_STD = 0,
    LOPT_DUMP_TOKENS,
    LOPT_DUMP_AST,
    LOPT_NUM_ITEMS,
} long_opt_idx_t;

optman_t optman;

void optman_init(void) {
    optman.exec_name = NULL;
    optman.output = DEFAULT_OUTPUT_NAME;
    sl_init(&optman.include_paths, offsetof(len_str_node_node_t, link));
    sl_init(&optman.link_opts, offsetof(len_str_node_t, link));
    sl_init(&optman.src_files, offsetof(len_str_node_t, link));
    sl_init(&optman.obj_files, offsetof(len_str_node_t, link));
    optman.warn_opts = 0;
    optman.dump_opts = 0;
    optman.olevel = 0;
}

void optman_destroy(void) {
    SL_DESTROY_FUNC(&optman.include_paths, free);
    SL_DESTROY_FUNC(&optman.link_opts, free);
    SL_DESTROY_FUNC(&optman.src_files, free);
    SL_DESTROY_FUNC(&optman.obj_files, free);
}

status_t optman_parse(int argc, char **argv) {
    status_t status = CCC_OK;
    optman.exec_name = argv[0];

    while (true) {
        int opt_idx = 0;

        // Important: Each index must correspond to the order in lopt_idx_t
        static struct option long_options[LOPT_NUM_ITEMS + 1] = {
            { "std"        , required_argument, 0, 0 },

            // Custom options
            { "dump_tokens", no_argument      , 0, 0 },
            { "dump_ast"   , no_argument      , 0, 0 },

            { 0            , 0                , 0, 0 } // Terminator
        };

        int c = getopt_long_only(argc, argv,
                                 "W:O:l:I:o:M::D:cg",
                                 long_options, &opt_idx);

        if (c == -1) {
            break;
        }

        bool opt_err = false;
        slist_t *append_list = NULL;

#ifdef DEBUG_PARAMS
        if (c != 0 && c != '?') {
            printf("option %c, arg: %s\n", c, optarg);
        }
#endif  /* DEBUG_PARAMS */

        // Option argument
        switch (c) {
        case 0:
#ifdef DEBUG_PARAMS
            printf("option %s, arg: %s\n", long_options[opt_idx].name,
                   optarg);
#endif /* DEBUG_PARAMS */
            switch (opt_idx) {
            case LOPT_STD:
                // TODO: Record this
                break;
            case LOPT_DUMP_TOKENS:
                optman.dump_opts |= DUMP_TOKENS;
                break;
            case LOPT_DUMP_AST:
                optman.dump_opts |= DUMP_AST;
                break;
            default:
                break;
            }
            break;

        case 'W': // Warning/error options
            if (strcmp("all", optarg) == 0) {
                optman.warn_opts |= WARN_ALL;
            } else if (strcmp("extra", optarg) == 0) {
                optman.warn_opts |= WARN_EXTRA;
            } else if (strcmp("error", optarg) == 0) {
                optman.warn_opts |= WARN_ERROR;
            } else {
                opt_err = true;
            }
            break;

        case 'O': // Optimization level
            if (strlen(optarg) != 1) {
                opt_err = true;
            }
            switch (optarg[0]) {
            case '0': optman.olevel = 0; break;
            case '1': optman.olevel = 1; break;
            case '2': optman.olevel = 2; break;
            case '3': optman.olevel = 3; break;
            default:
                opt_err = true;
            }
            break;

        case 'l': // Link options
            append_list = &optman.link_opts;
            break;

        case 'I': // Search path additions
            append_list = &optman.include_paths;
            break;

        case 'o': // Output location
            optman.output = optarg;
            break;

        case 'c': // Don't link
            // TODO: Handle this
            break;

        case 'M': // Dependency options
            // TODO: Handle this
            break;

        case 'D': // Define macros
            // TODO: Handle this
            break;

        case 'g': // Create debug info
            // TODO: Handle this
            break;

        case '?':
        default:
            status = CCC_ESYNTAX;
            break;
        }

        if (opt_err) {
            logger_log(NULL, LOG_ERR,
                       "unrecognized command line option '%s'",
                       argv[optind - 1]);
        }

        if (append_list != NULL) {
            len_str_node_t *node = malloc(sizeof(len_str_node_t));
            node->str.str = optarg;
            node->str.len = strlen(optarg);
            sl_append(append_list, &node->link);
        }

    }
    for (int i = optind; i < argc; ++i) {
        char *param = argv[i];
#ifdef DEBUG_PARAMS
        printf("No param: %s\n", param);
#endif /* DEBUG_PARAMS */

        size_t len = strlen(param);
        bool is_src = false;
        if (param[len - 1] == 'c') {
            is_src = true;
        }

        len_str_node_t *node = malloc(sizeof(len_str_node_t));
        node->str.str = param;
        node->str.len = len;
        if (is_src) {
            sl_append(&optman.src_files, &node->link);
        } else {
            sl_append(&optman.obj_files, &node->link);
        }

    }

    return status;
}
