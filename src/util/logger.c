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
 * Implementation for logger for handling exceptional events
 *
 * This is implemented as a singleton
 */

#include "logger.h"

#include <stdio.h>

char logger_fmt_buf[LOG_FMT_BUF_SIZE];

typedef struct logger_t {
    bool has_error;
    bool has_warning;
} logger_t;

static logger_t logger;

status_t logger_init() {
    logger.has_error = false;
    logger.has_warning = false;
    return CCC_OK;
}

void logger_destroy() {
    // No op
}

void logger_log(fmark_t *mark, const char *message, log_type_t type) {
    char *header;

    switch (type) {
    case LOG_ERR:
        header = "error: ";
        logger.has_error = true;
        break;

    case LOG_WARN:
        header = "warning: ";
        logger.has_warning = true;
        break;

    default:
        header = "info: ";
        break;
    }

    printf("%s:%d:%d %s %s\n", mark->filename->str, mark->line_num,
           mark->col_num, header, message);
}
