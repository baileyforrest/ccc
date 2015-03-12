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
 * Parser implementation
 *
 * Recursive descent style parser
 */

#include "parser.h"
#include "parser_priv.h"

#include <assert.h>
#include <stdlib.h>

#include "util/logger.h"

bool par_greater_or_equal_prec(token_t t1, token_t t2) {
    (void)t1;
    (void)t2;
    // TODO: Implement this
    return false;
}

status_t par_translation_unit(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    while (lex->cur.type != TOKEN_EOF) {
        if (CCC_OK != (status = par_external_declaration(lex))) {
            goto fail;
        }
    }
fail:
    return status;
}

status_t par_external_declaration(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    // Must match at least one declaration specifier
    if (CCC_OK != (status = par_declaration_specifier(lex))) {
        goto fail;
    }
    // Match declaration specifers until they don't match anymore
    while (CCC_BACKTRACK != (status = par_declaration_specifier(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    if (CCC_BACKTRACK == (status = par_declarator(lex))) {
        // If the next character isn't a declarator, then its a type declaration
        return CCC_OK;
    }
    status = CCC_OK;

    switch (lex->cur.type) {
    case LPAREN:
        return par_function_definition(lex);
    case EQ:
    default:
        return par_declaration(lex);
    }
fail:
    return status;
}

/**
 * Continues parsing after type and declarator
 */
status_t par_function_definition(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    LEX_MATCH(lex, LPAREN);

    while (lex->cur.type != RPAREN) {
        if (CCC_OK != (status = par_declaration(lex))) {
            goto fail;
        }
    }

    return par_compound_statement(lex);
fail:
    return status;
}

status_t par_declaration_specifier(lex_wrap_t *lex) {
    switch (lex->cur.type) {
        // Storage class specifiers
    case AUTO:
    case REGISTER:
    case STATIC:
    case EXTERN:
    case TYPEDEF:
        return par_storage_class_specifier(lex);

        // Type specifiers:
    case VOID:
    case CHAR:
    case SHORT:
    case INT:
    case LONG:
    case FLOAT:
    case DOUBLE:
    case SIGNED:
    case UNSIGNED:
    case STRUCT:
    case UNION:
    case ENUM:
    case ID: // TODO: Need to check if this is a type
        return par_type_specifier(lex);

        // Type qualitifiers
    case CONST:
    case VOLATILE:
        return par_type_qualifier(lex);
    default:
        return CCC_BACKTRACK;
    }
}

status_t par_storage_class_specifier(lex_wrap_t *lex) {
    (void)lex;
    return CCC_OK;
}

status_t par_type_specifier(lex_wrap_t *lex) {
    switch (lex->cur.type) {
    case VOID:
    case CHAR:
    case SHORT:
    case INT:
    case LONG:
    case FLOAT:
    case DOUBLE:
    case SIGNED:
    case UNSIGNED:
        return CCC_OK;
    case STRUCT:
    case UNION:
        return par_struct_or_union_specifier(lex);
    case ENUM:
        return par_enum_specifier(lex);
    case ID:
        // TODO: Make sure this is a typedef name
        return CCC_OK;
    default:
        return CCC_ESYNTAX;
    }
}

status_t par_struct_or_union_specifier(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    switch (lex->cur.type) {
    case STRUCT:
    case UNION:
        break;
    default:
        return CCC_ESYNTAX;
    }
    LEX_ADVANCE(lex);

    if (lex->cur.type == ID) {
        LEX_ADVANCE(lex);

        if (lex->cur.type != LBRACE) {
            return CCC_OK;
        }
    }

    if (lex->cur.type != LBRACE) {
        return CCC_ESYNTAX;
    }

    // Must match at least one struct declaration
    if (CCC_OK != (status = par_struct_declaration(lex))) {
        goto fail;
    }
    while (CCC_BACKTRACK != (status = par_struct_declaration(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;
    LEX_MATCH(lex, RBRACE);

fail:
    return status;
}

status_t par_struct_declaration(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_specifier_qualifier(lex))) {
        goto fail;
    }
    while (CCC_BACKTRACK != (status = par_specifier_qualifier(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }

    if (CCC_OK != par_struct_declarator_list(lex)) {
        goto fail;
    }
    LEX_MATCH(lex, SEMI);
fail:
    return status;
}

status_t par_specifier_qualifier(lex_wrap_t *lex) {
    switch (lex->cur.type) {
        // Type specifiers:
    case VOID:
    case CHAR:
    case SHORT:
    case INT:
    case LONG:
    case FLOAT:
    case DOUBLE:
    case SIGNED:
    case UNSIGNED:
    case STRUCT:
    case UNION:
    case ENUM:
    case ID: // TODO: Make sure this is a type
        return par_type_specifier(lex);

        // Type qualitifiers
    case CONST:
    case VOLATILE:
        return par_type_qualifier(lex);
    default:
        return CCC_BACKTRACK;
    }

    return CCC_OK;
}

status_t par_struct_declarator_list(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_struct_declarator(lex))) {
        goto fail;
    }
    while (lex->cur.type == COMMA) {
        if (CCC_OK != (status = par_struct_declarator(lex))) {
            goto fail;
        }
    }

fail:
    return status;
}

status_t par_struct_declarator(lex_wrap_t *lex) {
    status_t status = CCC_OK;

    if (lex->cur.type == COLON) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_expression(lex, false))) {
            goto fail;
        }
        goto done;
    }

    if (CCC_OK != (status = par_declarator(lex))) {
        goto fail;
    }

    if (lex->cur.type == COLON) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_expression(lex, false))) {
            goto fail;
        }
        goto done;
    }

done:
fail:
    return status;
}

// TODO: Handle abstract-declarator
status_t par_declarator(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    switch (lex->cur.type) {
    case STAR:
        if (CCC_OK != (status = par_pointer(lex))) {
            goto fail;
        }
        break;
    case ID:
    case LPAREN:
        if (CCC_OK != (status = par_direct_declarator(lex))) {
            goto fail;
        }
        break;
    default:
        return CCC_BACKTRACK;
    }

fail:
    return status;
}

status_t par_pointer(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    LEX_MATCH(lex, STAR);

    while (CCC_BACKTRACK != (status = par_type_qualifier(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }

    if (lex->cur.type == STAR) {
        if (CCC_OK != par_pointer(lex)) {
            goto fail;
        }
    }

fail:
    return status;
}

status_t par_type_qualifier(lex_wrap_t *lex) {
    switch (lex->cur.type) {
    case CONST:
    case VOLATILE:
        return CCC_OK;
    default:
        return CCC_BACKTRACK;
    }
}

status_t par_direct_declarator(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (lex->cur.type == LPAREN) {
        LEX_ADVANCE(lex);
        par_declarator(lex);
        LEX_MATCH(lex, RPAREN);
    } else if (lex->cur.type == ID) {
        LEX_ADVANCE(lex);
    }

    bool done = false;
    while (!done) {
        switch (lex->cur.type) {
        case LBRACK:
            LEX_ADVANCE(lex);
            if (lex->cur.type == RBRACK) {
                LEX_ADVANCE(lex);
            } else {
                if (CCC_OK != (status = par_expression(lex, false))) {
                    goto fail;
                }
                LEX_MATCH(lex, RBRACK);
            }
            break;
        case LPAREN:
            LEX_ADVANCE(lex);

            if (lex->cur.type == ID) {
                while (lex->cur.type == ID) {
                    LEX_ADVANCE(lex);
                }
                LEX_MATCH(lex, RPAREN);
            } else {
                if (CCC_OK != (status = par_parameter_type_list(lex))) {
                    goto fail;
                }
            }
            break;
        default:
            done = true;
        }
    }

fail:
    return status;
}

status_t par_non_binary_expression(lex_wrap_t *lex, bool *is_unary) {
    status_t status;
    bool primary = false;
    bool unary = false;

    switch (lex->cur.type) {
        // Unary expressions
    case INC:
    case DEC:
    case SIZEOF:

        // Unary operators
    case BITAND:
    case STAR:
    case PLUS:
    case MINUS:
    case BITNOT:
    case LOGICNOT:
        if (CCC_OK != (status = par_unary_expression(lex))) {
            goto fail;
        }
        unary = true;
        break;

        // Primary expressions
    case ID:
    case STRING:
    case INTLIT:
    case FLOATLIT:
        if (CCC_OK != (status = par_primary_expression(lex))) {
            goto fail;
        }
        unary = true;
        primary = true;
        break;

        // Casts and parens around expressions
    case LPAREN:
        LEX_ADVANCE(lex);
        switch (lex->cur.type) {
            // Cases for casts
        case VOID:
        case CHAR:
        case SHORT:
        case INT:
        case LONG:
        case FLOAT:
        case DOUBLE:
        case SIGNED:
        case UNSIGNED:
        case STRUCT:
        case UNION:
        case ENUM:
            // case ID: TODO: ID needs to be handled separately

            // Type qualitifiers
        case CONST:
        case VOLATILE:
            if (CCC_OK != (status = par_cast_expression(lex, true))) {
                goto fail;
            }
            break;

            // Parens
        default:
            if (CCC_OK != (status = par_expression(lex, false))) {
                goto fail;
            }
            primary = true;
            unary = true;
            LEX_MATCH(lex, RPAREN);
            break;
        }
    default:
        return CCC_ESYNTAX;
    }

    if (primary) {
        // If we found a primary expression, need to search for postfix
        switch (lex->cur.type) {
        case DEREF:
        case INC:
        case DEC:
        case DOT:
        case LBRACK:
        case LPAREN:
            if (CCC_OK != (status = par_postfix_expression(lex))) {
                goto fail;
            }
            break;
        default:
            break;
        }
    }

    *is_unary = unary;
fail:
    return status;
}

status_t par_expression(lex_wrap_t *lex, bool has_left) {
    status_t status = CCC_OK;
    bool unary1;

    if (!has_left) { // Only search for first operand if not provided
        if (CCC_OK != (status = par_non_binary_expression(lex, &unary1))) {
            goto fail;
        }

        if (unary1) {
            // Search for assignment operators
            switch (lex->cur.type) {
            case EQ:
            case STAREQ:
            case DIVEQ:
            case MODEQ:
            case PLUSEQ:
            case MINUSEQ:
            case LSHIFTEQ:
            case RSHIFTEQ:
            case BITANDEQ:
            case BITXOREQ:
            case BITOREQ:
                if (CCC_OK != (status = par_assignment_expression(lex))) {
                    goto fail;
                }

                return par_expression(lex, false);
            default:
                break;
            }
        }
    }

    bool done = false;
    while (!done) {
        bool new_left = false;
        token_t op1;

        switch (lex->cur.type) {
            // Binary operators
        case STAR:
        case DIV:
        case MOD:
        case PLUS:
        case MINUS:
        case LSHIFT:
        case RSHIFT:
        case LT:
        case GT:
        case LE:
        case GE:
        case EQ:
        case NE:
        case BITAND:
        case BITXOR:
        case BITOR:
        case LOGICAND:
        case LOGICOR:
            op1 = lex->cur.type;
            LEX_ADVANCE(lex);
            break;

        case COND:
            // Conditional operator
            LEX_ADVANCE(lex);
            if (CCC_OK != par_expression(lex, false)) {
                goto fail;
            }
            LEX_MATCH(lex, COLON);
            if (CCC_OK != par_expression(lex, false)) {
                goto fail;
            }
            // Restart the loop with a new expresson on the left
            break;
        default:
            // We're done with the expression
            return CCC_OK;
        }

        if (new_left) {
            continue;
        }

        bool unary2;
        if (CCC_OK != (status = par_non_binary_expression(lex, &unary2))) {
            goto fail;
        }

        bool binary2 = false;
        bool cond2 = false;
        token_t op2;
        switch (lex->cur.type) {
            // Binary operators
        case STAR:
        case DIV:
        case MOD:
        case PLUS:
        case MINUS:
        case LSHIFT:
        case RSHIFT:
        case LT:
        case GT:
        case LE:
        case GE:
        case EQ:
        case NE:
        case BITAND:
        case BITXOR:
        case BITOR:
        case LOGICAND:
        case LOGICOR:
            binary2 = true;
            op2 = lex->cur.type;
            break;

        case COND: // Cond has lowest precedence
            cond2 = true;
            // TODO: Handle this,
            break;
        default:
            // TODO Return expression combinaned with last op
            return CCC_OK;
        }

        if (cond2) {
            // Conditional operator
            LEX_ADVANCE(lex);
            if (CCC_OK != par_expression(lex, false)) {
                goto fail;
            }
            LEX_MATCH(lex, COLON);
            if (CCC_OK != par_expression(lex, false)) {
                goto fail;
            }
            // Restart loop with new second expression
        } else if (!binary2) {
            // Combine op1
            done = true;
        } else if (par_greater_or_equal_prec(op1, op2)) {
            // Combine op1
            new_left = true;
            if (CCC_OK != (status = par_expression(lex, true))) {
                goto fail;
            }
            done = true;
        } else {
            // Evaluate op2 and next operand, then combine with op1
            if (CCC_OK != (status = par_expression(lex, true))) {
                goto fail;
            }
            done = true;
        }

        if (new_left) {
            continue;
        }
    }

fail:
    return status;
}

status_t par_unary_expression(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    switch (lex->cur.type) {
        // Primary expressions
    case ID:
    case STRING:
    case INTLIT:
    case FLOATLIT:
        return par_postfix_expression(lex);
    case INC:
    case DEC:
        LEX_ADVANCE(lex);
        return par_unary_expression(lex);
    case SIZEOF:
        LEX_ADVANCE(lex);
        if (CCC_BACKTRACK != (status = par_unary_expression(lex))) {
            if (status != CCC_OK) {
                goto fail;
            }
            return CCC_OK;
        }
        return par_type_name(lex);
    case BITAND:
    case STAR:
    case PLUS:
    case MINUS:
    case BITNOT:
    case LOGICNOT:
        return par_cast_expression(lex, false);
    default:
        return CCC_BACKTRACK;
    }

fail:
    return status;
}

status_t par_cast_expression(lex_wrap_t *lex, bool skip_paren) {
    status_t status;
    if (!skip_paren && lex->cur.type != LPAREN) {
        return par_unary_expression(lex);
    }
    if (!skip_paren) {
        LEX_ADVANCE(lex);
    }
    if (CCC_OK != (status =par_type_name(lex))) {
        goto fail;
    }
    LEX_MATCH(lex, RPAREN);
    if (CCC_OK != (status = par_cast_expression(lex, false))) {
        goto fail;
    }
fail:
    return status;
}

/**
 * Parses postfix expression after the primary expression part
 */
status_t par_postfix_expression(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    bool done = false;
    while (!done) {
        switch (lex->cur.type) {
        case LBRACK:
            LEX_ADVANCE(lex);
            if (CCC_OK != (status = par_expression(lex, false))) {
                goto fail;
            }
            LEX_MATCH(lex, RBRACK);
            break;

        case LPAREN:
            LEX_ADVANCE(lex);
            while (lex->cur.type != RPAREN) {
                if (CCC_OK != (status =par_expression(lex, false))) {
                    goto fail;
                }
                LEX_MATCH(lex, COMMA);
            }
            LEX_ADVANCE(lex);
            break;

        case DOT:
            LEX_MATCH(lex, ID);
            break;

        case DEREF:
            LEX_MATCH(lex, ID);
            break;

        case INC:
        case DEC:
            LEX_ADVANCE(lex);
            break;
        default: // Found a terminating character
            done = true;
        }
    }

fail:
    return status;
}

/**
 * Parses assignment after the assignment operator
 */
status_t par_assignment_expression(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    switch (lex->cur.type) {
    case EQ:
    case STAREQ:
    case DIVEQ:
    case MODEQ:
    case PLUSEQ:
    case MINUSEQ:
    case LSHIFTEQ:
    case RSHIFTEQ:
    case BITANDEQ:
    case BITXOREQ:
    case BITOREQ:
        break;
    default:
        return CCC_ESYNTAX;
    }
    LEX_ADVANCE(lex);

    if (CCC_OK != (status = par_expression(lex, false))) {
        goto fail;
    }

fail:
    return status;
}

// Excludes parens because they need to be diferrentiated from casts
status_t par_primary_expression(lex_wrap_t *lex) {
    switch (lex->cur.type) {
    case ID:
    case STRING:
    case INTLIT:
    case FLOATLIT:
        return CCC_OK;
    default:
        return CCC_ESYNTAX;
    }
}

status_t par_type_name(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_specifier_qualifier(lex))) {
        goto fail;
    }
    while (CCC_BACKTRACK != (status = par_specifier_qualifier(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    if (CCC_BACKTRACK != (status = par_declarator(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;

fail:
    return status;
}

status_t par_parameter_type_list(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    par_parameter_list(lex);
    if (lex->cur.type != ELIPSE) {
        return CCC_OK;
    }
    LEX_ADVANCE(lex);

fail:
    return status;
}

status_t par_parameter_list(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_parameter_declaration(lex))) {
        goto fail;
    }
    while (CCC_BACKTRACK != (status = par_parameter_declaration(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;
fail:
    return status;
}

status_t par_parameter_declaration(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_declaration_specifier(lex))) {
        goto fail;
    }
    while (CCC_BACKTRACK != (status = par_declaration_specifier(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;

    if (CCC_BACKTRACK != (status = par_declarator(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
fail:
    return status;
}

status_t par_enum_specifier(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    LEX_MATCH(lex, ENUM);
    if (lex->cur.type == ID) {
        LEX_ADVANCE(lex);
    }

    if (lex->cur.type == LBRACE) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_enumerator_list(lex))) {
            goto fail;
        }
        LEX_MATCH(lex, RBRACE);
    }

fail:
    return status;
}

status_t par_enumerator_list(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_enumerator(lex))) {
        goto fail;
    }
    while (lex->cur.type == COMMA) {
        if (CCC_OK != (status = par_enumerator(lex))) {
            goto fail;
        }
    }

fail:
    return status;
}

status_t par_enumerator(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    LEX_MATCH(lex, ID);
    if (lex->cur.type == EQ) {
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, INTLIT);
    }
fail:
    return status;
}

status_t par_declaration(lex_wrap_t *lex) {
    status_t status = CCC_OK;

    // Must match at least one declaration specifier
    if (CCC_OK != (status = par_declaration_specifier(lex))) {
        goto fail;
    }
    // Match declaration specifers until they don't match anymore
    while (CCC_BACKTRACK != (status = par_declaration_specifier(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    while (CCC_BACKTRACK != (status = par_init_declarator(lex))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;

fail:
    return status;
}

status_t par_init_declarator(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_declarator(lex))) {
        goto fail;
    }

    if (lex->cur.type == ASSIGN) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_initializer(lex))) {
            goto fail;
        }
    }

fail:
    return status;
}

status_t par_initializer(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (lex->cur.type != LBRACK) {
        return par_assignment_expression(lex);
    }
    LEX_ADVANCE(lex);
    if (CCC_OK != (status = par_initializer_list(lex))) {
        goto fail;
    }
    LEX_MATCH(lex, RBRACK);
fail:
    return status;
}

status_t par_initializer_list(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_initializer(lex))) {
        goto fail;
    }
    while (lex->cur.type == COMMA) {
        LEX_ADVANCE(lex);
        if (lex->cur.type == RBRACK) {
            break;
        }
        if (CCC_OK != (status = par_initializer_list(lex))) {
            goto fail;
        }
    }
fail:
    return status;
}


status_t par_compound_statement(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    LEX_MATCH(lex, LBRACK);
    while (lex->cur.type != RBRACK) {
        if (CCC_OK != (status =par_statement(lex))) {
            goto fail;
        }
    }
    LEX_ADVANCE(lex);
fail:
    return status;
}

status_t par_statement(lex_wrap_t *lex) {
    switch (lex->cur.type) {
        // Cases for declaration specifier
        // Storage class specifiers
    case AUTO:
    case REGISTER:
    case STATIC:
    case EXTERN:
    case TYPEDEF:

        // Type specifiers:
    case VOID:
    case CHAR:
    case SHORT:
    case INT:
    case LONG:
    case FLOAT:
    case DOUBLE:
    case SIGNED:
    case UNSIGNED:
    case STRUCT:
    case UNION:
    case ENUM:
        // case ID: ID needs to be handled separately

        // Type qualitifiers
    case CONST:
    case VOLATILE:
        return par_declaration(lex);

    case ID: // TODO: Id can be in expression. need to have backtracking
    case CASE:
    case DEFAULT:
        return par_labeled_statement(lex);

    case IF:
    case SWITCH:
        return par_selection_statement(lex);

    case DO:
    case WHILE:
    case FOR:
        return par_iteration_statement(lex);

    case GOTO:
    case CONTINUE:
    case BREAK:
    case RETURN:
        return par_iteration_statement(lex);

    case SEMI: // noop
        return CCC_OK;
    default:
        return par_expression_statement(lex);
    }
}

status_t par_labeled_statement(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    switch (lex->cur.type) {
    case ID:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, COLON);
        if (CCC_OK != (status = par_statement(lex))) {
            goto fail;
        }
        break;
    case CASE:
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_expression(lex, false))) {
            goto fail;
        }
        LEX_MATCH(lex, COLON);
        if (CCC_OK != (status = par_statement(lex))) {
            goto fail;
        }
        break;
    case DEFAULT:
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_statement(lex))) {
            goto fail;
        }
        break;
    default:
        return CCC_ESYNTAX;
    }

fail:
    return status;
}

status_t par_expression_statement(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    if (lex->cur.type != SEMI) {
        if (CCC_OK != (status = par_expression(lex, false))) {
            goto fail;
        }
    }

    LEX_MATCH(lex, SEMI);
fail:
    return status;
}

status_t par_selection_statement(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    switch (lex->cur.type) {
    case IF:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        if (CCC_OK != (status = par_expression(lex, false))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        if (CCC_OK != (status = par_statement(lex))) {
            goto fail;
        }
        if (lex->cur.type == ELSE) {
            LEX_ADVANCE(lex);
            if (CCC_OK != (status = par_statement(lex))) {
                goto fail;
            }
        }
        break;
    case SWITCH:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        if (CCC_OK != (status = par_expression(lex, false))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        if (CCC_OK != (status = par_statement(lex))) {
            goto fail;
        }
        break;
    default:
        return CCC_ESYNTAX;
    }
fail:
    return status;
}

status_t par_iteration_statement(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    switch (lex->cur.type) {
    case DO:
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_statement(lex))) {
            goto fail;
        }
        LEX_MATCH(lex, WHILE);
        LEX_MATCH(lex, LPAREN);
        if (CCC_OK != (status = par_expression(lex, false))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        LEX_MATCH(lex, SEMI);
        break;
    case WHILE:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        if (CCC_OK != (status = par_expression(lex, false))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        if (CCC_OK != (status = par_statement(lex))) {
            goto fail;
        }
        break;
    case FOR:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        if (lex->cur.type != SEMI) {
            if (CCC_OK != (status = par_expression(lex, false))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);
        if (lex->cur.type != SEMI) {
            if (CCC_OK != (status = par_expression(lex, false))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);
        if (lex->cur.type != SEMI) {
            if (CCC_OK != (status = par_expression(lex, false))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);
        LEX_MATCH(lex, RPAREN);
        if (CCC_OK != (status = par_statement(lex))) {
            goto fail;
        }
        break;
    default:
        return CCC_ESYNTAX;
    }
fail:
    return status;
}

status_t par_jump_statement(lex_wrap_t *lex) {
    status_t status = CCC_OK;
    switch (lex->cur.type) {
    case GOTO:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, ID);
        LEX_MATCH(lex, SEMI);
        break;
    case CONTINUE:
    case BREAK:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, SEMI);
        break;
    case RETURN:
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_expression(lex, false))) {
            goto fail;
        }
        LEX_MATCH(lex, SEMI);
        break;
    default:
        return CCC_ESYNTAX;
    }
fail:
    return status;
}
