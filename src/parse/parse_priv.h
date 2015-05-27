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
#include "ast/type_table.h"

#include <stdio.h>

/**
 * Number of tokens to store. Must be power of two.
 */
#define LEX_LOOKAHEAD 2 // Store current token and next token

/**
 * Container for lexer containing lex context
 */
typedef struct lex_wrap_t {
    trans_unit_t *tunit; /**< Current tranlation unit */
    typetab_t *typetab;  /**< Type table on top of stack */
    vec_iter_t tokens;   /**< Token stream */
    char *function;      /**< Current function. NULL if none */
} lex_wrap_t;

/**
 * Get the current token
 *
 * @param Lexer wrapper to get token from
 */
#define LEX_CUR(wrap) ((token_t *)vec_iter_get(&(wrap)->tokens))

/**
 * Get the next token
 *
 * @param Lexer wrapper to get token from
 */
#define LEX_NEXT(wrap) \
    ((token_t *)vec_get((wrap)->tokens.vec, (wrap)->tokens.off + 1))

/**
 * Advance lexer wrapper to next token
 *
 * @param wrap Wrapper to advance
 */
#define LEX_ADVANCE(wrap) vec_iter_advance(&(wrap)->tokens)

/**
 * Match lexer wrapper with specified token, then advance to next token
 *
 * @param wrap Wrapper to match with
 * @param token Token to match
 */
#define LEX_MATCH(wrap, token)                                          \
    do {                                                                \
        token_type_t cur = LEX_CUR(wrap)->type;                         \
        if (cur != (token)) {                                           \
            logger_log(LEX_CUR(wrap)->mark, LOG_ERR,                    \
                       "expected '%s' before '%s' token",               \
                       token_type_str(token), token_type_str(cur));     \
            status = CCC_ESYNTAX;                                       \
            goto fail;                                                  \
        }                                                               \
        LEX_ADVANCE(wrap);                                              \
    } while (0)

/**
 * Check if current token is what is expected. Like LEX_MATCH,
 * but without advancing
 *
 * @param wrap Wrapper to match with
 * @param token Token to match
 */
// TODO1: Replace some parser code with this
#define LEX_CHECK(wrap, token)                                          \
    do {                                                                \
        token_type_t cur = LEX_CUR(wrap)->type;                         \
        if (cur != (token)) {                                           \
            logger_log(LEX_CUR(wrap)->mark, LOG_ERR,                    \
                       "expected '%s' before '%s' token",               \
                       token_type_str(token), token_type_str(cur));     \
            status = CCC_ESYNTAX;                                       \
            goto fail;                                                  \
        }                                                               \
    } while (0)

#define DECL_SPEC_STORAGE_CLASS \
    AUTO: case REGISTER: case STATIC: case EXTERN: case TYPEDEF: case INLINE

#define DECL_SPEC_TYPE_SPEC_NO_ID \
    VOID: case BOOL: case CHAR: case SHORT: case INT: case LONG: case FLOAT: \
case DOUBLE: case SIGNED: case UNSIGNED: case STRUCT: case UNION: case ENUM: \
case VA_LIST: case ALIGNAS: case STATIC_ASSERT


#define DECL_SPEC_TYPE_QUALIFIER CONST: case VOLATILE

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
 * @param result Location to store resulting translation unit. This function
 *     will allocate one.
 * @return CCC_OK on success, error code on error
 */
status_t par_translation_unit(lex_wrap_t *lex, trans_unit_t **result);
/**
 * Parses an external declaration (declaration or function definition)
 *
 * @param lex Current lexer state
 * @param result Lecation to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_external_declaration(lex_wrap_t *lex, gdecl_t **result);

/**
 * Parses a function definition after the return type, function name, and
 * parameter list in parens.
 *
 * @param lex Current lexer state
 * @param gdecl Global declaration with the provided information
 * @return CCC_OK on success, error code on error
 */
status_t par_function_definition(lex_wrap_t *lex, gdecl_t *gdecl);

/**
 * Parses declaration specifers until there are none left. This function
 * allocates a new type object.
 *
 * The types may form a chain, storage class specifers and type qualifiers
 * are always in the front in a TYPE_MOD type node.
 *
 * @param lex Current lexer state
 * @param type Location to store resulting type
 * @return CCC_OK on success, CCC_BACKTRACK if the next token does now allow
 *     parsing a declaratios_specifier, error code on error
 */
status_t par_declaration_specifiers(lex_wrap_t *lex, type_t **type);

/**
 * Parses a storage class specifer.
 *
 * auto, register, static, extern, typedef
 *
 * This function may allocate a new TYPE_MOD node if one does not exist.
 * This function should only be called if there is known to be a storage class
 * specifer next.
 *
 * @param lex Current lexer state
 * @param type The location to store the result.
 * @return CCC_OK on success, error code on error
 */
status_t par_storage_class_specifier(lex_wrap_t *lex, type_t **type);

/**
 * Parses a type specifier
 *
 * void, char, short, int, long, float, double, signed, unsigned,
 * <struct-or-union-specifier>, <enum-specifier>, <typedef-name>
 *
 * This function may allocate a new node of the appropriate type if one does not
 * exist.
 * This function should only be called if there is known to be a type specifer
 * next.
 *
 * @param lex Current lexer state
 * @param type The location to store the result.
 * @return CCC_OK on success, error code on error
 */
status_t par_type_specifier(lex_wrap_t *lex, type_t **type);

/**
 * Parses a struct, union, or enum specifer.
 *
 * This function may allocate a new node of the appropriate type if one does not
 * exist.
 * This function should only be called if there is known to be an enum
 * specifer next.
 *
 * @param lex Current lexer state
 * @param type The location to store the result.
 * @return CCC_OK on success, error code on error
 */
status_t par_struct_or_union_or_enum_specifier(lex_wrap_t *lex, type_t **type);

/**
 * Parses a single declaration in a struct specifier.
 *
 * @param lex Current lexer state
 * @param type The struct specifier type to add declarations on to
 * @return CCC_OK on success, error code on error
 */
status_t par_struct_declaration(lex_wrap_t *lex, type_t *type);

/**
 * Parses specifers qualifiers until there are none left. This function
 * allocates a new type object.
 *
 * @param lex Current lexer state
 * @param compound If true, parsing specifier qualifiers for compound type
 * @param type The location to store the result.
 * @return CCC_OK on success, CCC_BACKTRACK if the next token does now allow
 *     parsing a declaratios_specifier, error code on error
 */
status_t par_specifier_qualifiers(lex_wrap_t *lex, bool compound,
                                  type_t **type);

/**
 * Parses a struct declarator list.
 *
 * @param lex Current lexer state
 * @param base The struct type being constructed
 * @param decl_type The current declaration's type
 * @return CCC_OK on success, error code on error
 */
status_t par_struct_declarator_list(lex_wrap_t *lex, type_t *base,
                                    type_t *decl_type);
/**
 * Parses a struct declarator
 *
 * @param lex Current lexer state
 * @param decl The declaration being added to
 * @return CCC_OK on success, error code on error
 */
status_t par_struct_declarator(lex_wrap_t *lex, decl_t *decl);

/**
 * Parses a posibly abstract declarator given a preexisting declaration.
 *
 * @param lex Current lexer state
 * @param decl The declaration to parse a declarator for
 * @return CCC_OK on success, error code on error
 */
status_t par_declarator_base(lex_wrap_t *lex, decl_t *decl);

/**
 * Declarator parsing helper function. Parses a possibly abstract declarator.
 *
 * Delarators are identifers with (possibly const or volatile) pointers
 * possibly in parens.
 *
 * @param lex Current lexer state
 * @param decl_node The declaration's node
 * @param patch Pointer to the location pointing to original type of node.
 *     This is necessary for left to right parsing
 * @return CCC_OK on success, CCC_BACKTRACK if a declarator cannot be parsed,
 *     error code on error
 */
status_t par_declarator(lex_wrap_t *lex, decl_node_t *decl_node,
                        type_t ***patch);

/**
 * Parses a pointer with an optionally const or valitle type qualifiers.
 *
 * The pointers are always at the front of the type chain.
 *
 * @param lex Current lexer state
 * @param base_ptr Pointer to base element, where new pointer nodes should be
 *     added
 * @return CCC_OK on success, error code on error
 */
status_t par_pointer(lex_wrap_t *lex, type_t **base_ptr);

/**
 * Parses a type qualifier. (const, volatile)
 *
 * @param lex Current lexer state
 * @param mod The type to modify. May add a new node on the front of the chain.
 * @return CCC_OK on success, CCC_BACKTRACK if a type_qualifier cannot be
 *     parsed, error code on error
 */
status_t par_type_qualifier(lex_wrap_t *lex, type_t **type);

/**
 * Parses a possibly abstract direct declarator.
 *
 * May make no change to decl_node if abstract
 *
 * @param lex Current lexer state
 * @param node The declaration node being processed
 * @param patch Pointer to the location pointing to original type of node.
 *     This is necessary for left to right parsing
 * @return CCC_OK on success, error code on error
 */
status_t par_direct_declarator(lex_wrap_t *lex, decl_node_t *node,
                               type_t ***patch);
/**
 * Parses a non binary expression (cast, unary, postfix, primary, constant)
 *
 * @param lex Current lexer state
 * @param is_unary true if the parsed expression is unary, false otherwise.
 *     If NULL, it is not affected
 * @param result The parsed expression
 * @return CCC_OK on success, error code on error
 */
status_t par_non_binary_expression(lex_wrap_t *lex, bool *is_unary,
                                   expr_t **result);

/**
 * Parses a binary expression
 *
 * @param lex Current lexer state
 * @param left Expression to use for the left side of a binary expression. NULL
 *     if none should be used
 * @param result Location where the parsed expression shourd be stored
 * @return CCC_OK on success, error code on error
 */
status_t par_oper_expression(lex_wrap_t *lex, oper_t prev_op, expr_t *left,
                             expr_t **result);

status_t par_mem_acc_list(lex_wrap_t *lex, mem_acc_list_t *list, bool nodot);

/**
 * Parses a unary expression.
 *
 * This is a postfix expression, prefix expression, unary operator, or sizeof
 * expression.
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, CCC_BACKTRACK if the current input cannot match
 *     a unary expression, error code on error
 */
status_t par_unary_expression(lex_wrap_t *lex, expr_t **result);

/**
 * Parses a cast expression. This is a unary expression or a cast of a cast
 * expression.
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_cast_expression(lex_wrap_t *lex, expr_t **result);

/**
 * Parses a postfix expression after the primary expression part.
 *
 * If no postfix expressions can be parsed, base is unchanged is returned
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_postfix_expression(lex_wrap_t *lex, expr_t **result);

/**
 * Parses a primary expression.
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_primary_expression(lex_wrap_t *lex, expr_t **result);

/**
 * Parses an expression
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_expression(lex_wrap_t *lex, expr_t **result);

/**
 * Parses an assignment expression
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_assignment_expression(lex_wrap_t *lex, expr_t **result);

/**
 * Parses a type name, optionally match parens too.
 *
 * If parens are to be matched, but the token in the parens is not a type, then
 * the paren is not consumed.
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @param If true, try to match parens
 * @return CCC_OK on success, CCC_BACKTRACK if a type name cannot be parsed,
 *     error code on error
 */
status_t par_type_name(lex_wrap_t *lex, bool match_parens, decl_t **result);

/**
 * Parses a funtion parameter type list, which may include vaargs
 *
 * @param lex Current lexer state
 * @param func Function to parse paramaters for
 * @return CCC_OK on success, error code on error
 */
status_t par_parameter_type_list(lex_wrap_t *lex, type_t *func);

/**
 * Parses a parameter list, a list of types and parameters, as well as trailing
 * comma before vaargs
 *
 * @param lex Current lexer state
 * @param func Function to parse paramaters for
 * @return CCC_OK on success, error code on error
 */
status_t par_parameter_list(lex_wrap_t *lex, type_t *func);

/**
 * Parses a parameter declaration
 *
 * @param lex Current lexer state
 * @param func Function to parse paramaters for
 * @return CCC_OK on success, error code on error
 */
status_t par_parameter_declaration(lex_wrap_t *lex, type_t *func);

/**
 * Parses a list of enum declarations
 *
 * @param lex Current lexer state
 * @param enum type to parse declarations for
 * @return CCC_OK on success, error code on error
 */
status_t par_enumerator_list(lex_wrap_t *lex, type_t *type);

/**
 * Parses a single enum declaration, which is a name and possibly a value
 *
 * @param lex Current lexer state
 * @param enum type to parse declarations for
 * @return CCC_OK on success, error code on error
 */
status_t par_enumerator(lex_wrap_t *lex, type_t *type);

/**
 * Parses a declaration.
 *
 * @param lex Current lexer state
 * @param decl Location to store declaration. If *decl != NULL, it will add onto
 *     an existing declaration. Otherwise a new declaration will be allocated
 * @param partial, if true, the first decl_node is parsed. This is necessary
 *     to differentiate between functions and declarations
 * @return CCC_OK on success, error code on error
 */
status_t par_declaration(lex_wrap_t *lex, decl_t **decl, bool partial);

/**
 * Parses an init declarator, which is a declarator with an optional
 * initialization.
 *
 * @param lex Current lexer state
 * @param decl Declaration to parse init declarators for
 * @param partial, if true, the first decl_node is parsed. This is necessary
 *     to differentiate between functions and declarations
 * @return CCC_OK on success, error code on error
 */
status_t par_init_declarator(lex_wrap_t *lex, decl_t *decl, bool partial);

/**
 * Parses an initalizer, which is either an expression or a initizer list
 *
 * @param lex Current lexer state
 * @param Lecation to store the resulting initializer
 * @return CCC_OK on success, error code on error
 */
status_t par_initializer(lex_wrap_t *lex, expr_t **result);

/**
 * Parses an initializer list.
 *
 * @param lex Current lexer state
 * @param result Location to store the resulting initializer list
 * @return CCC_OK on success, error code on error
 */
status_t par_initializer_list(lex_wrap_t *lex, expr_t **result);

/**
 * Parses a statement
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a labeled statement.(goto label, case, default)

 * This function should only be called if there is known to be an labeled
 * statement next.
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_labeled_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a selection statement. (if, switch)
 *
 * This function should only be called if there is known to be an selection
 * statement next.
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_selection_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses an iteration statement (do, while, for)
 *
 * This function should only be called if there is known to be an iteration
 * statement next.
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_iteration_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a jump statement.(goto, continue, break, return)
 *
 * This function should only be called if there is known to be an jump
 * statement next.
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_jump_statement(lex_wrap_t *lex, stmt_t **result);

/**
 * Parses a compound statement, multiple statements in a block
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_compound_statement(lex_wrap_t *lex, stmt_t **result);


/**
 * Parses an expression statement. (Possibly empty statement)
 *
 * @param lex Current lexer state
 * @param result Location to store the result
 * @return CCC_OK on success, error code on error
 */
status_t par_expression_statement(lex_wrap_t *lex, stmt_t **result);


#endif /* _PARSER_PRIV_H_ */
