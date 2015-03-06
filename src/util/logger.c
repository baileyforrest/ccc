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

status_t logger_init() {
    // No op
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
        break;

    case LOG_WARN:
        header = "warning: ";

    default:
        header = "info: ";
        break;
    }

    printf("%s:%d:%d %s %s", mark->filename->str, mark->line_num, mark->col_num,
            header, message);
}
