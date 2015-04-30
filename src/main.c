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

#define _DEFAULT_SOURCE 1

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "manager.h"
#include "optman.h"

#include "ir/ir.h"

#include "parse/ast.h"

#include "util/file_directory.h"
#include "util/logger.h"
#include "util/tempfile.h"

#include "typecheck/typechecker.h"

#define LLVM_EXT "ll"
#define ASM_EXT "s"
#define OBJ_EXT "o"

#define AS "as"
#define LLC "llc"
#define LD "cc" // TODO1: Should be ld

#define DEF_LD_OPTS 8

slist_t temp_files;

int main(int argc, char **argv) {
    status_t status = CCC_OK;

    logger_init();
    fdir_init();
    sl_init(&temp_files, offsetof(tempfile_t, link));

    bool link = false;

    if (CCC_OK != (status = optman_init(argc, argv))) {
        goto fail0;
    }

    SL_FOREACH(cur, &optman.src_files) {
        str_node_t *node = GET_ELEM(&optman.src_files, cur);
        char *filename = node->str;
        size_t name_len = strlen(filename);
        while (name_len > 0 && filename[name_len-- - 1] != '.')
            continue;
        assert(name_len != 0);

        manager_t manager;

        man_init(&manager, NULL);

        if (CCC_OK != (status = pp_open(&manager.pp, filename))) {
            man_destroy(&manager);
            goto fail1;
        }

        if (optman.dump_opts & DUMP_TOKENS) {
            printf("//@ Tokens %s\n", filename);
            man_dump_tokens(&manager);
            goto src_done0;
        }

        trans_unit_t *ast;
        if (CCC_OK != (status = man_parse(&manager, &ast))) {
            logger_log(NULL, LOG_ERR, "Failed to parse %s", filename);
            goto src_done0;
        }

        if (optman.dump_opts & DUMP_AST) {
            printf("//@ AST %s\n", filename);
            ast_print(ast);
        }

        if (!typecheck_ast(ast)) {
            logger_log(NULL, LOG_ERR, "Failed to typecheck %s", filename);
            goto src_done0;
        }

        // TODO1: Remove after later stages debugged
        if (optman.dump_opts & DUMP_AST) {
            goto src_done0;
        }

        ir_trans_unit_t *ir = man_translate(&manager);
        man_destroy_parse(&manager); // Don't parse structures after translation

        if (optman.dump_opts & DUMP_IR) {
            ir_print(stdout, ir, filename);
        }

        if (optman.output_opts & OUTPUT_ASM &&
            optman.output_opts & OUTPUT_EMIT_LLVM) {
            FILE *output = fopen(optman.output, "w");
            if (output == NULL) {
                logger_log(NULL, LOG_ERR, "%s: %s", optman.output,
                           strerror(errno));
                goto src_done0;
            }

            ir_print(output, ir, filename);
            if (EOF == fclose(output)) {
                logger_log(NULL, LOG_ERR, "%s: %s", optman.output,
                           strerror(errno));
                goto src_done0;
            }

            goto src_done0;
        }

        // TODO1 Replace this with codegen when implemented
        tempfile_t *llvm_tempfile = tempfile_create(filename, LLVM_EXT);
        sl_append(&temp_files, &llvm_tempfile->link);
        ir_print(tempfile_file(llvm_tempfile), ir, filename);
        tempfile_close(llvm_tempfile);

        tempfile_t *asm_tempfile = tempfile_create(filename, ASM_EXT);
        sl_append(&temp_files, &asm_tempfile->link);
        tempfile_close(asm_tempfile);

        pid_t pid = fork();
        if (pid == -1) {
            // TODO1: handle/report error
            goto src_done0;
        } else if (pid == 0) {
            execlp(LLC, LLC, tempfile_path(llvm_tempfile), "-o",
                   tempfile_path(asm_tempfile), (char *)NULL);
            logger_log(NULL, LOG_ERR, "Failed to exec %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        int child_status;
        waitpid(pid, &child_status, 0);
        if (child_status != 0) {
            logger_log(NULL, LOG_ERR, "%s: llc Failed!", filename);
            goto src_done0;
        }

        tempfile_t *obj_tempfile = tempfile_create(filename, OBJ_EXT);
        sl_append(&temp_files, &obj_tempfile->link);
        tempfile_close(obj_tempfile);

        // Shell out to as
        // TODO1: Pass all options to  as
        pid = fork();
        if (pid == -1) {
            goto src_done0;
        } else if (pid == 0) {
            execlp(AS, AS, tempfile_path(asm_tempfile), "-o",
                   tempfile_path(obj_tempfile), NULL);
            logger_log(NULL, LOG_ERR, "Failed to exec %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        waitpid(pid, &child_status, 0);
        if (child_status != 0) {
            logger_log(NULL, LOG_ERR, "%s: as Failed!", filename);
            goto src_done0;
        }

        str_node_t *obj_node = emalloc(sizeof(str_node_t));
        obj_node->str = tempfile_path(obj_tempfile);
        sl_append(&optman.obj_files, &obj_node->link);

        link = true;

    src_done0:
        man_destroy(&manager);
        if (status != CCC_OK || logger_has_error() ||
            (optman.warn_opts & WARN_ERROR && logger_has_warn())) {
            link = false;
            break;
        }
    }

    if (link) {
        // Shell out to linker
        pid_t pid = fork();
        if (pid == -1) {
            // TODO1: handle/report error
            goto fail1;
        } else if (pid == 0) {
            vec_t argv;
            vec_init(&argv, 0);

            // Add exec name as argv[0]
            vec_push_back(&argv, LD);

            // output file option
            vec_push_back(&argv, "-o");
            vec_push_back(&argv, optman.output);


            // Add the object files
            SL_FOREACH(cur, &optman.obj_files) {
                str_node_t *str = GET_ELEM(&optman.obj_files, cur);
                vec_push_back(&argv, str->str);
            }

            // Add NULL terminator
            vec_push_back(&argv, (char *)NULL);

            execvp(LD, (char **)vec_elems(&argv));
            logger_log(NULL, LOG_ERR, "Failed to exec %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        int ld_status;
        waitpid(pid, &ld_status, 0);
        if (ld_status != 0) {
            logger_log(NULL, LOG_ERR, "ld returned %d exit status",
                       WEXITSTATUS(ld_status));
            goto fail1;
        }
    }

fail1:
    optman_destroy();
fail0:
    fdir_destroy();

    SL_DESTROY_FUNC(&temp_files, tempfile_destroy);

    if (status != CCC_OK || logger_has_error() ||
        (optman.warn_opts & WARN_ERROR && logger_has_warn())) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
