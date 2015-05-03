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

#include "optman.h"

void logger_log_line(fmark_t *mark);


typedef struct logger_t {
    bool has_error;
    bool has_warning;
} logger_t;

static logger_t logger;

void logger_init(void) {
    logger.has_error = false;
    logger.has_warning = false;
}

void logger_log_line(fmark_t *mark) {
    if (mark->line_start == NULL) {
        return;
    }
    // Print the line
    for (const char *c = mark->line_start; *c && *c != '\n'; ++c) {
        fputc(*c, stderr);
    }
    fputc('\n', stderr);

    // Print the error marker
    for (int i = 0; i < mark->col - 1; ++i) {
        fputc(' ', stderr);
    }
    fputc('^', stderr);
    fputc('\n', stderr);
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

    if (mark == NULL) {
        fprintf(stderr, "%s: %s ", optman.exec_name, header);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        return;
    }

    fprintf(stderr, "%s:%d:%d %s ", mark->filename, mark->line, mark->col,
            header);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    logger_log_line(mark);

    for (fmark_t *cur = mark->last; cur != NULL; cur = cur->last) {
        fprintf(stderr, "%s:%d:%d note: %s\n", cur->filename, cur->line,
                cur->col, "In expansion of macro");
        logger_log_line(cur);
    }
}

bool logger_has_error(void) {
    return logger.has_error;
}

bool logger_has_warn(void) {
    return logger.has_warning;
}
