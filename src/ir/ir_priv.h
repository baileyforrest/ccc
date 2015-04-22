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
 * IR private interface
 */

#ifndef _IR_PRIV_H_
#define _IR_PRIV_H_

void ir_trans_unit_print(FILE *stream, ir_trans_unit_t *irtree);

void ir_gdecl_print(FILE *stream, ir_gdecl_t *gdecl);

void ir_stmt_print(FILE *stream, ir_stmt_t *stmt, bool indent);

void ir_expr_print(FILE *stream, ir_expr_t *expr);

void ir_type_print(FILE *stream, ir_type_t *type, char *func_name);

const char *ir_oper_str(ir_oper_t op);

const char *ir_convert_str(ir_convert_t conv);

const char *ir_icmp_str(ir_icmp_type_t conv);

const char *ir_fcmp_str(ir_fcmp_type_t conv);

const char *ir_float_type_str(ir_float_type_t ftype);

void ir_type_destroy(ir_type_t *type);

void ir_expr_destroy(ir_expr_t *expr);

void ir_stmt_destroy(ir_stmt_t *stmt);

void ir_gdecl_destroy(ir_gdecl_t *gdecl);


#endif /* _IR_PRIV_H_ */
