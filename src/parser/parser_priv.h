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
 * Parser private interface
 */

#ifndef _PARSER_PRIV_H_
#define _PARSER_PRIV_H_

#include "util/status.h"
#include "parser/type_table.h"

#include <stdio.h>

#define ALLOC_NODE(loc, type)                                       \
    do {                                                            \
        loc = malloc(sizeof(type));                                 \
        if (loc == NULL) {                                          \
            logger_log(&lex->cur.mark, "Out of memory in parser",   \
                       LOG_ERR);                                    \
            status = CCC_NOMEM;                                     \
            goto fail;                                              \
        }                                                           \
    } while (0)

/**
 * Advance lexer wrapper to next token
 *
 * @param wrap Wrapper to advance
 */
#define LEX_ADVANCE(wrap)                                               \
    do {                                                                \
        if (CCC_OK !=                                                   \
            (status = lexer_next_token((wrap)->lexer, &(wrap)->cur))) { \
            goto fail;                                                  \
        }                                                               \
    } while (0)

/**
 * Match lexer wrapper with specified token, then advance to next token
 *
 * @param wrap Wrapper to match with
 * @param token Token to match
 */
#define LEX_MATCH(wrap, token)                                          \
    do {                                                                \
        token_t cur = (wrap)->cur.type;                                 \
        if (cur != (token)) {                                           \
            snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,                  \
                     "Parse Error: Expected %s, Found: %s.",            \
                     token_str(token), token_str(cur));                 \
            logger_log(&(wrap)->cur.mark, logger_fmt_buf, LOG_ERR);     \
            status = CCC_ESYNTAX;                                       \
            goto fail;                                                  \
        }                                                               \
        LEX_ADVANCE(wrap);                                              \
    } while (0)

/**
 * Container for lexer containing lex context
 */
typedef struct lex_wrap_t {
    lexer_t *lexer;     /* Lexer */
    typetab_t *typetab; /* Type table on top of stack */
    lexeme_t cur;       /* Current token */
} lex_wrap_t;

/**
 * Returns the relative precedence of a binary operator
 *
 * @param op Operator to get precedence for
 * @return The relative precedence
 */
int par_get_binary_prec(oper_t op);

/**
 * Parses a tranlation unit
 *
 * @param lex Current lexer state
 * @param file Filename of translation unit
 * @param result Location to store resulting translation unit. This function
 *     will allocate one.
 * @return CCC_OK on success, error code on error
 */
status_t par_translation_unit(lex_wrap_t *lex, len_str_t *file,
                              trans_unit_t **result);
/**
 * Parses an external declaration (declaration or function definition)
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_external_declaration(lex_wrap_t *lex, gdecl_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_function_definition(lex_wrap_t *lex, gdecl_t *gdecl);

/**
 * If *type == NULL, allocates statement, otherwise continues parsing existing
 * declaration
 */
/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_declaration_specifiers(lex_wrap_t *lex, type_t **type);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_storage_class_specifier(lex_wrap_t *lex, type_t **type);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_type_specifier(lex_wrap_t *lex, type_t **type);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_struct_or_union_or_enum_specifier(lex_wrap_t *lex, type_t **type);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_struct_declaration(lex_wrap_t *lex, type_t *type);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_specifier_qualifier(lex_wrap_t *lex, type_t **type);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_struct_declarator_list(lex_wrap_t *lex, type_t *base,
                                    type_t *decl_type);
/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */

status_t par_struct_declarator(lex_wrap_t *lex, type_t *base,
                               type_t *decl_type);

status_t par_declarator_base(lex_wrap_t *lex, decl_t *decl);
/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_declarator(lex_wrap_t *lex, type_t *base, decl_node_t *decl_node);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_pointer(lex_wrap_t *lex, type_t **mod);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_type_qualifier(lex_wrap_t *lex, type_t **type);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_direct_declarator(lex_wrap_t *lex, decl_node_t *node,
                               type_t *base);
/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */

status_t par_non_binary_expression(lex_wrap_t *lex, bool *is_unary,
                                   expr_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_expression(lex_wrap_t *lex, expr_t *left, expr_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_unary_expression(lex_wrap_t *lex, expr_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_cast_expression(lex_wrap_t *lex, bool skip_paren, expr_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_postfix_expression(lex_wrap_t *lex, expr_t *base,
                                expr_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_assignment_expression(lex_wrap_t *lex, expr_t *left,
                                   expr_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_primary_expression(lex_wrap_t *lex, expr_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_type_name(lex_wrap_t *lex, decl_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_parameter_type_list(lex_wrap_t *lex, type_t *func);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_parameter_list(lex_wrap_t *lex, type_t *func);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_parameter_declaration(lex_wrap_t *lex, type_t *func);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_enumerator_list(lex_wrap_t *lex, type_t *type);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_enumerator(lex_wrap_t *lex, type_t *type);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
/**
 * If *stmt == NULL, allocates statement, otherwise continues parsing existing
 * declaration
 */
status_t par_declaration(lex_wrap_t *lex, decl_t **decl);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_init_declarator(lex_wrap_t *lex, decl_t *decl);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_initializer(lex_wrap_t *lex, expr_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_initializer_list(lex_wrap_t *lex, expr_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_compound_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_labeled_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_expression_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_selection_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_iteration_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a
 *
 * @param lex Current lexer state
 * @return CCC_OK on success, error code on error
 */
status_t par_jump_statement(lex_wrap_t *lex, stmt_t **result);

#endif /* _PARSER_PRIV_H_ */
