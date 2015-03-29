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
 */
// TODO: Finalize and doc this

#ifndef _OPTMAN_H_
#define _OPTMAN_H_

#include "util/slist.h"
#include "util/util.h"
#include "util/status.h"

typedef enum dump_opts_t {
    DUMP_TOKENS = 1 << 0,
    DUMP_AST    = 1 << 1,
} dump_opts_t;

typedef enum warn_opts_t {
    WARN_ALL   = 1 << 0,
    WARN_EXTRA = 1 << 1,
    WARN_ERROR = 1 << 2,
} warn_opts_t;

typedef enum olevel_t {
    O0 = 0,
    O1 = 1,
    O2 = 2,
    O3 = 3,
} olevel_t;

typedef struct optman_t {
    char *exec_name;
    char *output;
    slist_t include_paths;
    slist_t link_opts;
    slist_t src_files;
    slist_t obj_files;
    dump_opts_t dump_opts;
    warn_opts_t warn_opts;
    olevel_t olevel;
} optman_t;

extern optman_t optman;

void optman_init(void);
void optman_destroy(void);

status_t optman_parse(int argc, char **argv);

#endif /* _OPTMAN_H_ */
