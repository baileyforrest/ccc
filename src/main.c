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
 * Program entry point
 */

#include "parse/ast.h"

#include "util/file_directory.h"
#include "util/logger.h"

#include "manager.h"
#include "optman.h"

int main(int argc, char **argv) {
    status_t status = CCC_OK;

    if (CCC_OK != (status = optman_init(argc, argv))) {
        goto fail0;
    }

    if (CCC_OK != (status = fdir_init())) {
        goto fail0;
    }

    sl_link_t *cur;
    SL_FOREACH(cur, &optman.src_files) {
        len_str_node_t *node = GET_ELEM(&optman.src_files, cur);
        manager_t manager;

        if (CCC_OK != (status = man_init(&manager, NULL))) {
            goto fail1;
        }
        if (CCC_OK != (status = pp_open(&manager.pp, node->str.str))) {
            man_destroy(&manager);
            goto fail1;
        }

        if (optman.dump_opts & DUMP_TOKENS) {
            printf("//@ Tokens %s\n", node->str.str);
            man_dump_tokens(&manager);
        } else {
            trans_unit_t *ast;
            if (optman.dump_opts & DUMP_AST) {
                printf("//@ AST %s\n", node->str.str);
            }

            if (CCC_OK != (status = man_parse(&manager, &ast))) {
                logger_log(NULL, LOG_ERR, "Failed to parse %s", node->str.str);
                goto src_fail0;
            }
            if (optman.dump_opts & DUMP_AST) {
                ast_print(ast);
            }

            ast_destroy(ast);
        }

    src_fail0:
        man_destroy(&manager);
    }

fail1:
    fdir_destroy();
fail0:
    optman_destroy();
    return status;
}
