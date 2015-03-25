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
#include <stdarg.h>

void logger_log_line(fmark_t *mark);


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

void logger_log_line(fmark_t *mark) {
    // Print the line
    for (const char *c = mark->line_start; *c != '\n'; ++c) {
        putchar(*c);
    }
    putchar('\n');

    // Print the error marker
    for (int i = 0; i < mark->col - 1; ++i) {
        putchar(' ');
    }
    putchar('^');
    putchar('\n');
}

void logger_log(fmark_t *mark, log_type_t type, const char *fmt, ...) {
    char *header;

    va_list ap;
    va_start(ap, fmt);

    switch (type) {
    case LOG_ERR:
        header = "error:";
        logger.has_error = true;
        break;

    case LOG_WARN:
        header = "warning:";
        logger.has_warning = true;
        break;

    default:
        header = "info:";
        break;
    }

    printf("%s:%d:%d %s ", mark->file->str, mark->line, mark->col, header);
    vprintf(fmt, ap);
    printf("\n");
    logger_log_line(mark);

    for (fmark_t *cur = mark->last; cur != NULL; cur = cur->last) {
        printf("%s:%d:%d note: %s\n", cur->file->str, cur->line, cur->col,
               "In expansion of macro");
        logger_log_line(cur);
    }
}
