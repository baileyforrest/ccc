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
 * Directory for holding information about source files
 *
 * This is implemented as a singleton
 */

#include "file_directory.h"

#include <stddef.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util/htable.h"
#include "util/util.h"


/**
 * Destroys a file_directory entry
 *
 * Note that there is no associated constructor because fdir_insert serves as
 * the constructor.
 *
 * @param entry Entry to destroy
 * @return CCC_OK on success, error code on error.
 */
static status_t fdir_entry_destroy(fdir_entry_t *entry);

status_t fmark_copy_chain(fmark_t *mark, fmark_t **result) {
    status_t status = CCC_OK;
    fmark_t *res = NULL;

    for (fmark_t *cur = mark, *res_cur = NULL; cur != NULL; cur = cur->last) {
        fmark_t *new_mark;
        if (res == NULL) {
            fmark_refcnt_t *mark_refcnt = malloc(sizeof(fmark_refcnt_t));
            if (mark_refcnt == NULL) {
                status = CCC_NOMEM;
                goto fail;
            }
            mark_refcnt->refcnt = 1;
            new_mark = &mark_refcnt->mark;
            res = new_mark;
        } else {
            new_mark = malloc(sizeof(fmark_t));
            res_cur->last = new_mark;
        }
        if (new_mark == NULL) {
            status = CCC_NOMEM;
            goto fail;
        }
        memcpy(new_mark, cur, sizeof(fmark_t));
        res_cur = new_mark;
    }

    *result = res;
    return status;

fail:
    fmark_chain_free(res);
    return status;
}

void fmark_chain_free(fmark_t *mark) {
    if (mark == NULL) {
        return;
    }
    fmark_refcnt_t *mark_refcnt = (fmark_refcnt_t *)mark;
    if (--mark_refcnt->refcnt > 0) {
        return;
    }
    for (fmark_t *cur = mark, *next = NULL; cur != NULL; cur = next) {
        next = cur->last;
        free(cur);
    }
}

typedef struct fdir_t {
    htable_t table;
} fdir_t;

static fdir_t s_fdir;

status_t fdir_init(void) {
    static const ht_params_t s_params = {
        0,                                // No Size estimate
        offsetof(fdir_entry_t, filename), // Offset of key
        offsetof(fdir_entry_t, link),     // Offset of ht link
        strhash,                          // Hash function
        vstrcmp,                          // void string compare
    };

    return ht_init(&s_fdir.table, &s_params);
}

static status_t fdir_entry_destroy(fdir_entry_t *entry) {
    status_t status = CCC_OK;
    if (entry == NULL) {
        return status;
    }

    if (entry->buf != MAP_FAILED &&
        (-1 == munmap(entry->buf, (size_t)(entry->end - entry->buf)))) {
        status = CCC_FILEERR;
    }

    if (entry->fd != -1 && (-1 == close(entry->fd))) {
        status = CCC_FILEERR;
    }

    free(entry);
    return status;
}

void fdir_destroy(void) {
    HT_DESTROY_FUNC(&s_fdir.table, fdir_entry_destroy);
}

status_t fdir_insert(const char *filename, size_t len, fdir_entry_t **result) {
    status_t status = CCC_OK;

    len_str_t lookup = { (char *)filename, len };
    fdir_entry_t *entry = ht_lookup(&s_fdir.table, &lookup);
    if (entry != NULL) {
        goto done;
    }

    // Allocate the entry and name string in one region
    entry = malloc(sizeof(fdir_entry_t) + len + 1);
    if (entry == NULL) {
        status = CCC_NOMEM;
        goto fail;
    }
    // Initialize to safe values for destructor
    entry->buf = MAP_FAILED;
    entry->fd = -1;

    entry->filename.str = (char *)entry + sizeof(*entry);
    entry->filename.len = len;

    strncpy(entry->filename.str, filename, len);
    entry->filename.str[len] = '\0';

    if (-1 == (entry->fd = open(filename, O_RDONLY, 0))) {
        status = CCC_FILEERR;
        goto fail;
    }

    struct stat st;
    if (-1 == (fstat(entry->fd, &st))) {
        status = CCC_FILEERR;
        goto fail;
    }
    size_t size = st.st_size;

    if (MAP_FAILED ==
        (entry->buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, entry->fd, 0))) {
        status = CCC_FILEERR;
        goto fail;
    }
    entry->end = entry->buf + size;

    if (CCC_OK != (status = ht_insert(&s_fdir.table, &entry->link))) {
        goto fail;
    }

done:
    *result = entry;
    return status;

fail:
    fdir_entry_destroy(entry);
    return status;
}

fdir_entry_t *fdir_lookup(const char *filename, size_t len) {
    len_str_t lookup = { (char *)filename, len };
    return ht_lookup(&s_fdir.table, &lookup);
}
