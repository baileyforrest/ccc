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
 * Type checker interface
 */

#ifndef _TYPECHECK_H_
#define _TYPECHECK_H_

#include <stdbool.h>

#include "parse/ast.h"

/**
 * Typecheck an ast. Error and warnings will be sent to the logger.
 *
 * @param tunit The translation unit to typecheck
 * @return true if the tranlation unit typechecks, false otherwise
 */
bool typecheck_ast(trans_unit_t *tunit);

#endif /* _TYPECHECK_H_ */
