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
 * Temp file wrapper implementation
 */
// TODO1: exit program on error

#define _DEFAULT_SOURCE 1

#include "tempfile.h"

#include <errno.h>

#include "util/logger.h"

#define TMP_SUFFIX "-XXXXXX"
#define TMP_DIR "/tmp/"

extern FILE *tempfile_file(tempfile_t *tf);
extern char *tempfile_path(tempfile_t *tf);

tempfile_t *tempfile_create(char *path, char *ext) {
    tempfile_t *tf = NULL;
    char *filename = ccc_basename(path);
    size_t ext_len = strlen(ext);

    // +2 one for null, one for the dot before extension
    size_t tempfile_len = strlen(TMP_DIR) + strlen(filename) +
        strlen(TMP_SUFFIX) + ext_len + 2;

    tf = emalloc(sizeof(*tf) + tempfile_len);
    tf->tmp_path = (char *)tf + sizeof(*tf);
    sprintf(tf->tmp_path, "%s%s%s.%s", TMP_DIR, filename, TMP_SUFFIX, ext);
    if (-1 == (tf->fd = mkstemps(tf->tmp_path, ext_len + 1))) {
        goto fail;
    }

    if (NULL == (tf->stream = fdopen(tf->fd, "w"))) {
        goto fail;
    }

    return tf;

fail:
    logger_log(NULL, LOG_ERR, "Failed to create tempfile: %s", strerror(errno));
    tempfile_destroy(tf);
    return NULL;
}

void tempfile_close(tempfile_t *tf) {
    if (tf->stream != NULL) {
        fclose(tf->stream);
        tf->stream = NULL;
    }
}

void tempfile_destroy(tempfile_t *tf) {
    if (tf == NULL) {
        return;
    }
    tempfile_close(tf);
    remove(tf->tmp_path);
    free(tf);
}
