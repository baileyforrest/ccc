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

#include <string.h>
#include <errno.h>

#include "manager.h"
#include "optman.h"

#include "ir/ir.h"
#include "ir/translator.h"

#include "parse/ast.h"

#include "util/file_directory.h"
#include "util/logger.h"

#include "typecheck/typechecker.h"

int main(int argc, char **argv) {
    status_t status = CCC_OK;

    if (CCC_OK != (status = optman_init(argc, argv))) {
        goto fail0;
    }

    fdir_init();

    SL_FOREACH(cur, &optman.src_files) {
        len_str_node_t *node = GET_ELEM(&optman.src_files, cur);
        manager_t manager;

        man_init(&manager, NULL);

        if (CCC_OK != (status = pp_open(&manager.pp, node->str.str))) {
            man_destroy(&manager);
            goto fail1;
        }

        if (optman.dump_opts & DUMP_TOKENS) {
            printf("//@ Tokens %s\n", node->str.str);
            man_dump_tokens(&manager);
            goto src_done0;
        }

        trans_unit_t *ast;
        if (CCC_OK != (status = man_parse(&manager, &ast))) {
            logger_log(NULL, LOG_ERR, "Failed to parse %s", node->str.str);
            goto src_done0;
        }

        if (optman.dump_opts & DUMP_AST) {
            printf("//@ AST %s\n", node->str.str);
            ast_print(ast);
        }

        if (!typecheck_ast(ast)) {
            logger_log(NULL, LOG_ERR, "Failed to typecheck %s", node->str.str);
            goto src_fail0;
        }

        // TODO1: Remove after later stages debugged
        if (optman.dump_opts & DUMP_AST) {
            goto src_fail0;
        }

        ir_trans_unit_t *ir = trans_translate(ast);
        ast_destroy(ast); // Don't need ast after translation

        if (optman.dump_opts & DUMP_IR) {
            ir_print(stdout, ir, node->str.str);
        }

        if (optman.output_opts & OUTPUT_ASM &&
            optman.output_opts & OUTPUT_EMIT_LLVM) {
            FILE *output = fopen(optman.output, "w");
            if (output == NULL) {
                logger_log(NULL, LOG_ERR, "%s: %s", optman.output,
                           strerror(errno));
                goto src_fail1;
            }

            ir_print(output, ir, node->str.str);
            if (EOF == fclose(output)) {
                logger_log(NULL, LOG_ERR, "%s: %s", optman.output,
                           strerror(errno));
                goto src_fail1;
            }

        }

        ir_trans_unit_destroy(ir);

    src_done0:
        man_destroy(&manager);

        continue;

    src_fail1:
        ir_trans_unit_destroy(ir);

    src_fail0:
        ast_destroy(ast);

        man_destroy(&manager);
    }

fail1:
    fdir_destroy();
fail0:
    optman_destroy();
    return status;
}
