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
 * Option manager interface
 *
 * Not all options may actually do anything, but enough are supported so that
 * the project's Makefile can use ccc.
 */

#ifndef _OPTMAN_H_
#define _OPTMAN_H_

#include "util/slist.h"
#include "util/util.h"
#include "util/status.h"

#include "lex/cpp.h"

/**
 * Options for dumping at various stages of the compilation
 */
typedef enum dump_opts_t {
    DUMP_TOKENS = 1 << 0, // Dump tokens from lexer
    DUMP_AST    = 1 << 1, // Dump the AST after parsing
    DUMP_IR     = 1 << 2, // Dump IR after translation
} dump_opts_t;

/**
 * Warning options
 */
typedef enum warn_opts_t {
    WARN_ALL   = 1 << 0, // -Wall
    WARN_EXTRA = 1 << 1, // -Wextra
    WARN_ERROR = 1 << 2, // -Werror
} warn_opts_t;

/**
 * Optimization level
 */
typedef enum olevel_t {
    O0 = 0,
    O1 = 1,
    O2 = 2,
    O3 = 3,
} olevel_t;

/**
 * Standard types
 */
typedef enum std_t {
    STD_C11,
} std_t;

/**
 * Misc flags
 */
typedef enum misc_flags_t {
    MISC_MISC = 1 << 0, // TODO2: Remove if unused
} misc_flags_t;

/**
 * Preprocessor dependency options
 */
typedef enum pp_dep_opts_t {
    PP_DEP_MP  = 1 << 0, // -MP Generate phony targets for header files
    PP_DEP_MMD = 1 << 1, // -MMD Generate dependencies for non system headers
} pp_dep_opts_t;

typedef enum output_opts_t {
    OUTPUT_EMIT_LLVM = 1 << 0, // -emit-llvm Emit llvm ir
    OUTPUT_ASM       = 1 << 1, // -S Stop after asm generated
    OUTPUT_OBJ       = 1 << 2, // -c Stop after object files generated
    OUTPUT_DBG_SYM   = 1 << 3, // -g Generate debug symbols
} output_opts_t;

typedef struct macro_node_t {
    sl_link_t link;
    // TODO0: This
} macro_node_t;

/**
 * The Option manager. Contains flags/lists from the command line parameters.
 */
typedef struct optman_t {
    char *exec_name;           /**< Name of the executable */
    char *output;              /**< Name of the output file */
    slist_t include_paths;     /**< Search path additions with -I flag */
    slist_t link_opts;         /**< Linker libraries */
    slist_t src_files;         /**< C files */
    slist_t asm_files;         /**< Assember files */
    slist_t obj_files;         /**< All other files assumed for linker */
    slist_t macros;            /**< Parameter defined macros */
    dump_opts_t dump_opts;     /**< Dump options */
    warn_opts_t warn_opts;     /**< Warn options */
    olevel_t olevel;           /**< Optimization level */
    std_t std;                 /**< Standard used */
    misc_flags_t misc;         /**< Misc flags */
    pp_dep_opts_t pp_deps;     /**< Preprocessor dependency ops */
    output_opts_t output_opts; /**< Output options */
} optman_t;

/**
 * The single option manager. Its members are are assumed to be read only unless
 * otherwise noted.
 */
extern optman_t optman;

/**
 * Initialize the option manager
 *
 * @param argc Argument count passed into main
 * @param argv Argument vector passed into main
 * @return CCC_OK on success, error code on error
 */
status_t optman_init(int argc, char **argv);

/**
 * Destroy the option manager. Destroying after a failed initialization is safe.
 */
void optman_destroy(void);

#endif /* _OPTMAN_H_ */
