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

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "util/logger.h"
#include "util/text_stream.h"

#define DEFAULT_STD STD_C11

typedef enum long_opt_idx_t {
    LOPT_STD = 0,
    LOPT_DUMP_TOKENS,
    LOPT_DUMP_AST,
    LOPT_DUMP_IR,
    LOPT_EMIT_LLVM,
    LOPT_NUM_ITEMS,
} long_opt_idx_t;

optman_t optman;

static status_t optman_parse(int argc, char **argv);

status_t optman_init(int argc, char **argv) {
    static char ccc_path[PATH_MAX];
    if (-1 == readlink("/proc/self/exe", ccc_path, sizeof(ccc_path))) {
        exit_err(strerror(errno));
    }

    ccc_dirname(ccc_path); // Remove excutable name
    assert(strcmp(ccc_basename(ccc_path), "bin") == 0);
    ccc_dirname(ccc_path); // Remove bin path

    optman.ccc_path = ccc_path;
    optman.ccc_path_len = strlen(ccc_path);
    optman.exec_name = NULL;
    optman.output = NULL;
    vec_init(&optman.include_paths, 0);
    vec_init(&optman.link_opts, 0);
    vec_init(&optman.src_files, 0);
    vec_init(&optman.obj_files, 0);
    vec_init(&optman.macros, 0);
    optman.dump_opts = 0;
    optman.warn_opts = 0;
    optman.olevel = 0;
    optman.std = DEFAULT_STD;
    optman.misc = 0;
    optman.pp_deps = 0;
    optman.output_opts = 0;

    return optman_parse(argc, argv);
}

void optman_destroy(void) {
    vec_destroy(&optman.include_paths);
    vec_destroy(&optman.link_opts);
    vec_destroy(&optman.src_files);
    vec_destroy(&optman.asm_files);
    vec_destroy(&optman.obj_files);
    vec_destroy(&optman.macros);
}

static status_t optman_parse(int argc, char **argv) {
    status_t status = CCC_OK;
    optman.exec_name = ccc_basename(argv[0]);

    while (true) {
        int opt_idx = 0;

        // Important: Each index must correspond to the order in lopt_idx_t
        static struct option long_options[LOPT_NUM_ITEMS + 1] = {
            { "std"        , required_argument, 0, 0 },

            // Custom options
            { "dump_tokens", no_argument      , 0, 0 },
            { "dump_ast"   , no_argument      , 0, 0 },
            { "dump_ir"    , no_argument      , 0, 0 },
            { "emit-llvm"  , no_argument      , 0, 0 },

            { 0            , 0                , 0, 0 } // Terminator
        };

        int c = getopt_long_only(argc, argv,
                                 "W:O:l:I:o:M::D:Sscg",
                                 long_options, &opt_idx);

        if (c == -1) {
            break;
        }

        bool opt_err = false;

        // Option argument
        switch (c) {
        case 0:
            switch (opt_idx) {
            case LOPT_STD:
                if (strcmp("C11", optarg) == 0) {
                    optman.std = STD_C11;
                } else {
                    opt_err = true;
                }
                break;
            case LOPT_DUMP_TOKENS:
                optman.dump_opts |= DUMP_TOKENS;
                break;
            case LOPT_DUMP_AST:
                optman.dump_opts |= DUMP_AST;
                break;
            case LOPT_DUMP_IR:
                optman.dump_opts |= DUMP_IR;
                break;
            case LOPT_EMIT_LLVM:
                optman.output_opts |= OUTPUT_EMIT_LLVM;
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

        case 'l': { // Link options
            vec_push_back(&optman.link_opts, optarg);
            break;
        }

        case 'I': { // Search path additions
            vec_push_back(&optman.include_paths, optarg);
            break;
        }

        case 'o': // Output location
            optman.output = optarg;
            break;

        case 'M': // Dependency options
            if (strcmp("P", optarg) == 0) {
                optman.pp_deps |= PP_DEP_MP;
            } else if (strcmp("MD", optarg) == 0) {
                optman.pp_deps |= PP_DEP_MMD;
            } else {
                opt_err = true;
            }
            break;

        case 'D': { // Define macros
            vec_push_back(&optman.macros, optarg);
            break;
        }

        case 's': // Stop after ASM
        case 'S': // Stop after ASM
            optman.output_opts |= OUTPUT_ASM;
            break;

        case 'c': // Don't link
            optman.output_opts |= OUTPUT_OBJ;
            break;

        case 'g': // Create debug info
            optman.output_opts |= OUTPUT_DBG_SYM;
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

    }
    for (int i = optind; i < argc; ++i) {
        char *param = argv[i];
        size_t len = strlen(param);
        switch (param[len - 1]) {
        case 'c':
        case 'C':
            vec_push_back(&optman.src_files, param);
            break;
        case 's':
        case 'S':
            vec_push_back(&optman.asm_files, param);
            break;
        default:
            vec_push_back(&optman.obj_files, param);
        }
    }

    return status;
}
