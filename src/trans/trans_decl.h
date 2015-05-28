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
 * Declaration translator functions
 */

#ifndef _TRANS_DECL_H_
#define _TRANS_DECL_H_

#include "trans_priv.h"

void trans_gdecl_node(trans_state_t *ts, decl_node_t *node);

char *trans_decl_node_name(ir_symtab_t *symtab, char *name);

typedef enum ir_decl_node_type_t {
    IR_DECL_NODE_GLOBAL,
    IR_DECL_NODE_LOCAL,
    IR_DECL_NODE_FDEFN,
    IR_DECL_NODE_FUNC_PARAM,
} ir_decl_node_type_t;

ir_type_t *trans_decl_node(trans_state_t *ts, decl_node_t *node,
                           ir_decl_node_type_t type, void *context);

#endif /* _TRANS_DECL_H_ */
