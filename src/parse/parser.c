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
 * Recursive descent style parser.
 *
 * Based on BNF Grammar from K & R book with left factoring and left recursion
 * elimination applied.
 *
 * In order to avoid duplicate code, there is a small amount of backtracking
 * used, but only ever 1 or 2 function calls deep. Tokens are never returned to
 * the lexer, so this should not be a performance problem.
 */
// TODO1: Use TYPE_TYPEDEF only if print ast mode is on

#include "parser.h"
#include "parser_priv.h"

#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include "optman.h"

#include "lex/symtab.h"

#include "util/htable.h"
#include "util/logger.h"

#include "typecheck/typechecker.h"

status_t parser_parse(vec_t *tokens, trans_unit_t **result) {
    assert(tokens != NULL);
    assert(vec_size(tokens) > 0);
    assert(result != NULL);

    lex_wrap_t lex;
    vec_iter_init(&lex.tokens, tokens);

    return par_translation_unit(&lex, result);
}

status_t parser_parse_expr(vec_t *tokens, trans_unit_t *tunit,
                           expr_t **result) {
    assert(tokens != NULL);
    assert(vec_size(tokens) > 0);
    assert(result != NULL);

    lex_wrap_t lex;
    lex.tunit = tunit;
    vec_iter_init(&lex.tokens, tokens);

    return par_expression(&lex, result);
}

/**
 * Returns the precidence of binary operators.
 */
int par_get_binary_prec(oper_t op) {
    switch (op) {
    case OP_TIMES:
    case OP_DIV:
    case OP_MOD:      return 10;

    case OP_PLUS:
    case OP_MINUS:    return 9;

    case OP_LSHIFT:
    case OP_RSHIFT:   return 8;

    case OP_LT:
    case OP_GT:
    case OP_LE:
    case OP_GE:       return 7;

    case OP_EQ:
    case OP_NE:       return 6;

    case OP_BITAND:   return 5;
    case OP_BITXOR:   return 4;
    case OP_BITOR:    return 3;
    case OP_LOGICAND: return 2;
    case OP_LOGICOR:  return 1;
    case OP_NOP:      return 0;
    default:
        assert(false);
    }
    return -1;
}

status_t par_translation_unit(lex_wrap_t *lex, trans_unit_t **result) {
    status_t status = CCC_OK;
    trans_unit_t *tunit = ast_trans_unit_create(false);
    lex->typetab = &tunit->typetab; // Set top type table to translation units
    lex->tunit = tunit;

    while (vec_iter_has_next(&lex->tokens)) {
        gdecl_t *gdecl;
        if (CCC_OK != (status = par_external_declaration(lex, &gdecl))) {
            goto fail;
        }
        sl_append(&tunit->gdecls, &gdecl->link);
    }

fail:
    *result = tunit;
    return status;
}

status_t par_external_declaration(lex_wrap_t *lex, gdecl_t **result) {
    status_t status = CCC_OK;
    gdecl_t *gdecl = ast_gdecl_create(lex->tunit, &LEX_CUR(lex)->mark,
                                      GDECL_DECL);
    gdecl->decl = ast_decl_create(lex->tunit, &LEX_CUR(lex)->mark);

    if (CCC_OK !=
        (status = par_declaration_specifiers(lex, &gdecl->decl->type))) {
        if (status != CCC_BACKTRACK) {
            goto fail;
        }
    }
    if (gdecl->decl->type == NULL) {
        logger_log(&LEX_CUR(lex)->mark, LOG_WARN,
                   "Data definition has no type or storage class");

        // When no type is specified, assume it is int
        gdecl->decl->type = tt_int;
    }

    if (CCC_OK != (status = par_declarator_base(lex, gdecl->decl))) {
        // If the next character isn't a declarator, then its a type declaration
        if (status == CCC_BACKTRACK && gdecl->decl->type != NULL) {
            status = CCC_OK;
            gdecl->type = GDECL_DECL;
            LEX_MATCH(lex, SEMI);
            goto done;
        } else {
            goto fail;
        }
    }

    // Skip function pointers/declarations
    if (LEX_CUR(lex)->type == SEMI) {
        LEX_ADVANCE(lex);
        goto done;
    }

    decl_node_t *decl_node = sl_tail(&gdecl->decl->decls);
    if (decl_node->type != NULL && decl_node->type->type == TYPE_FUNC) {
        if (CCC_OK != (status = par_function_definition(lex, gdecl))) {
            goto fail;
        }
    } else {
        if (CCC_OK != (status = par_declaration(lex, &gdecl->decl, true))) {
            goto fail;
        }
        LEX_MATCH(lex, SEMI); // Must end with semi colon
    }

done:
    *result = gdecl;

fail:
    return status;
}

/**
 * Continues parsing after type and declarator
 *
 * gdecl_t.decl holds type and declarator as a declaration stmt
 */
status_t par_function_definition(lex_wrap_t *lex, gdecl_t *gdecl) {
    status_t status = CCC_OK;
    gdecl->type = GDECL_FDEFN;
    gdecl->fdefn.stmt = NULL;
    sl_init(&gdecl->fdefn.gotos, offsetof(stmt_t, goto_params.link));

    static const ht_params_t s_gdecl_ht_params = {
        0,                              // Size estimate
        offsetof(stmt_t, label.label),  // Offset of key
        offsetof(stmt_t, label.link),   // Offset of ht link
        ind_str_hash,                   // Hash function
        ind_str_eq,                     // void string compare
    };

    ht_init(&gdecl->fdefn.labels, &s_gdecl_ht_params);

    /* TODO2: Handle K & R style function signature
    while (LEX_CUR(lex)->type != RPAREN) {
        stmt_t *param = NULL;
        if (CCC_OK != (status = par_declaration(lex, &param))) {
            goto fail;
        }
        sl_append(&gdecl->fdefn.params, param->link);
    }
    */

    decl_node_t *node = sl_head(&gdecl->decl->decls);
    assert(node != NULL && node->id != NULL);

    // Set the current function we're in
    log_function = node->id;
    lex->function = node->id;

    if (CCC_OK != (status = par_compound_statement(lex, &gdecl->fdefn.stmt))) {
        goto fail;
    }
    log_function = NULL;
    lex->function = NULL;

fail:
    return status;
}

status_t par_declaration_specifiers(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;;
    *type = NULL; // Set to NULL so sub functions will allocate

    while (true) {
        switch (LEX_CUR(lex)->type) {
            // Storage class specifiers
        case DECL_SPEC_STORAGE_CLASS:
            if(CCC_OK != (status = par_storage_class_specifier(lex, type))) {
                goto fail;
            }
            break;

            // Type specifiers:
        case ID: {
            // Type specifier only if its a typedef name
            typetab_entry_t *entry =
                tt_lookup(lex->typetab, LEX_CUR(lex)->id_name);
            if (entry == NULL) {
                return CCC_BACKTRACK;
            }

            // If these are the next character, the id must be the variable name
            // if there's already a type specifer
            switch (LEX_NEXT(lex)->type) {
            case ASSIGN:
            case SEMI:
            case COMMA:
                if (*type != NULL && ((*type)->type != TYPE_MOD ||
                                      (*type)->mod.base != NULL)) {
                    return CCC_BACKTRACK;
                }
            default:
                break;
            }

            // Allow repeat typedefs
            // Repeat typedef if this is a typedef, an entry exists, and the
            // next character is a semicolon or comma
            if (*type != NULL && (*type)->type == TYPE_MOD &&
                ((*type)->mod.type_mod & TMOD_TYPEDEF) &&
                (LEX_NEXT(lex)->type == SEMI || LEX_NEXT(lex)->type == COMMA)) {
                return CCC_BACKTRACK;
            }
            // FALL THROUGH
        }
        case DECL_SPEC_TYPE_SPEC_NO_ID:
            if (CCC_OK != (status = par_type_specifier(lex, type))) {
                goto fail;
            }
            break;

            // Type qualitifiers
        case DECL_SPEC_TYPE_QUALIFIER:
            if (CCC_OK != (status = par_type_qualifier(lex, type))) {
                goto fail;
            }
            break;
        default:
            return CCC_BACKTRACK;
        }
    }

fail:
    return status;
}

status_t par_storage_class_specifier(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;
    // Allocate new type mod node if one isn't assigned
    if (*type == NULL || (*type)->type != TYPE_MOD) {
        type_t *new_type = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark,
                                           TYPE_MOD);
        new_type->mod.base = *type;
        new_type->mod.type_mod = TMOD_NONE;
        *type = new_type;
    }
    type_mod_t tmod;
    switch (LEX_CUR(lex)->type) {
    case AUTO:     tmod = TMOD_AUTO;     break;
    case REGISTER: tmod = TMOD_REGISTER; break;
    case STATIC:   tmod = TMOD_STATIC;   break;
    case EXTERN:   tmod = TMOD_EXTERN;   break;
    case TYPEDEF:  tmod = TMOD_TYPEDEF;  break;
    case INLINE:   tmod = TMOD_INLINE;   break;
    default:
        assert(false);
    }
    if ((*type)->mod.type_mod & tmod) {
        logger_log(&LEX_CUR(lex)->mark, LOG_WARN,
                   "Duplicate storage class specifer: %s",
                   ast_type_mod_str(tmod));
    }

    (*type)->mod.type_mod |= tmod;
    LEX_ADVANCE(lex);

    return status;
}

status_t par_type_specifier(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;

    if (LEX_CUR(lex)->type == STATIC_ASSERT) {
        if (*type != NULL) {
            logger_log(&LEX_CUR(lex)->mark, LOG_ERR,
                       "Unexpected token %s",
                       token_type_str(LEX_CUR(lex)->type));
            return CCC_ESYNTAX;
        }
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        type_t *sa_type = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark,
                                          TYPE_STATIC_ASSERT);
        if (CCC_OK !=
            (status = par_oper_expression(lex, OP_NOP, NULL,
                                          &sa_type->sa_params.expr))) {
            return CCC_ESYNTAX;
        }
        LEX_MATCH(lex, COMMA);
        LEX_CHECK(lex, STRING);
        sa_type->sa_params.msg = LEX_CUR(lex)->str_val;
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, RPAREN);
        LEX_CHECK(lex, SEMI);

        *type = sa_type;

        return CCC_OK;
    }

    type_t *new_node = NULL;

    // Check first node for mod node
    type_t *mod_node = NULL;
    if (*type != NULL && (*type)->type == TYPE_MOD) {
        mod_node = *type;
    }

    type_t **end_node;
    if (mod_node != NULL) {
        end_node = &mod_node->mod.base;
    } else {
        end_node = type;
    }

    bool allowed = false;
    switch (LEX_CUR(lex)->type) {
    case ALIGNAS:
    case NORETURN:
        allowed = true;
    default:
        break;
    }

    // TODO1: This is nasty, find better solution for this
    // Handle repeat end nodes
    if (!allowed && *end_node != NULL) {
        bool okay = false;
        switch ((*end_node)->type) {
        case TYPE_INT:
            switch (LEX_CUR(lex)->type) {
                // Just have int overwritten with the correct type
            case SHORT:
                *end_node = tt_short;
                okay = true;
                break;
            case LONG:
                *end_node = tt_long;
                okay = true;
                break;
            case UNSIGNED:
            case SIGNED:
                okay = true;
                break;
            default:
                break;
            }
            break;
        case TYPE_SHORT:
            switch (LEX_CUR(lex)->type) {
            case INT:
            case UNSIGNED:
            case SIGNED:
                okay = true;
                break;
            default:
                break;
            }
            break;
        case TYPE_LONG:
            switch (LEX_CUR(lex)->type) {
            case INT:
            case UNSIGNED:
            case SIGNED:
                okay = true;
                break;
            case LONG: // Create long long int
                *end_node = tt_long_long;
                okay = true;
                break;
            case DOUBLE: // Create long double
                *end_node = tt_long_double;
                okay = true;
                break;
            default:
                break;
            }
            break;
        case TYPE_LONG_LONG:
            switch (LEX_CUR(lex)->type) {
            case INT:
            case UNSIGNED:
            case SIGNED:
                okay = true;
                break;
            default:
                break;
            }
            break;
        case TYPE_DOUBLE:
            if (LEX_CUR(lex)->type == LONG) {
                okay = true;
                *end_node = tt_long_double;
            }
            break;
        default:
            break;
        }

        if (okay) {
            LEX_ADVANCE(lex);
            return CCC_OK;
        }

        // We found a node that is type, so we cannot have two
        logger_log(&LEX_CUR(lex)->mark, LOG_ERR, "Multiple type specifers");
        status = CCC_ESYNTAX;
        goto fail;
    }

    switch (LEX_CUR(lex)->type) {
    case ID: { // typedef name
        // Type specifier only if its a typedef name
        typetab_entry_t *entry =
            tt_lookup(lex->typetab, LEX_CUR(lex)->id_name);
        assert(entry != NULL); // Must be checked before calling
        new_node = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark,
                                   TYPE_TYPEDEF);
        new_node->typedef_params.name = LEX_CUR(lex)->id_name;
        new_node->typedef_params.base = entry->type;
        new_node->typedef_params.type = TYPE_VOID;
        *end_node = new_node;
        break;
    }
        // Primitive types
    case VOID:    *end_node = tt_void;    break;
    case BOOL:    *end_node = tt_bool;    break;
    case CHAR:    *end_node = tt_char;    break;
    case SHORT:   *end_node = tt_short;   break;
    case INT:     *end_node = tt_int;     break;
    case LONG:    *end_node = tt_long;    break;
    case FLOAT:   *end_node = tt_float;   break;
    case DOUBLE:  *end_node = tt_double;  break;
    case VA_LIST: *end_node = tt_va_list; break;

        // Don't give a base type for signed/unsigned. No base type defaults to
        // int
    case ALIGNAS:
    case NORETURN:
    case SIGNED:
    case UNSIGNED: {
        type_mod_t mod;
        switch (LEX_CUR(lex)->type) {
        case ALIGNAS:  mod = TMOD_ALIGNAS;  break;
        case NORETURN: mod = TMOD_NORETURN; break;
        case SIGNED:   mod = TMOD_SIGNED;   break;
        case UNSIGNED: mod = TMOD_UNSIGNED; break;
        default:
            assert(false);
        }
        if (mod_node == NULL) {
            mod_node = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark,
                                       TYPE_MOD);
            new_node = mod_node;
            mod_node->mod.base = *type;
            mod_node->mod.type_mod = TMOD_NONE;
            *type = mod_node;
        }

        if (mod_node->mod.type_mod & mod) {
            logger_log(&LEX_CUR(lex)->mark, LOG_ERR,
                       "Duplicate type specifer: %s",
                       ast_type_mod_str(mod));
            status = CCC_ESYNTAX;
        }
        mod_node->mod.type_mod |= mod;
        if (mod == TMOD_ALIGNAS) {
            LEX_ADVANCE(lex);
            LEX_MATCH(lex, LPAREN);
            if (CCC_OK !=
                (status = par_type_name(lex, false,
                                        &mod_node->mod.alignas_type))) {
                if (status != CCC_BACKTRACK) {
                    goto fail;
                }
            }

            // Failed to parse a typename, parse an expression instead
            if (status == CCC_BACKTRACK) {
                if (CCC_OK !=
                    (status =
                     par_oper_expression(lex, OP_NOP, NULL,
                                         &(*type)->mod.alignas_expr))) {
                    goto fail;
                }
            }
            LEX_CHECK(lex, RPAREN);
        }

        break;
    }
    case STRUCT:
    case UNION:
    case ENUM:
        return par_struct_or_union_or_enum_specifier(lex, end_node);
    default:
        assert(false);
    }

    LEX_ADVANCE(lex);

fail:
    return status;
}

status_t par_struct_or_union_or_enum_specifier(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;

    type_t *new_type = NULL;
    char *name = NULL;

    type_type_t btype;
    switch (LEX_CUR(lex)->type) {
    case STRUCT: btype = TYPE_STRUCT; break;
    case UNION:  btype = TYPE_UNION;  break;
    case ENUM:   btype = TYPE_ENUM;   break;
    default:
        assert(false);
    }
    LEX_ADVANCE(lex);

    typetab_entry_t *entry = NULL;
    type_t *entry_type;
    if (LEX_CUR(lex)->type == ID) {
        name = LEX_CUR(lex)->id_name;
        entry = tt_lookup_compound(lex->typetab, name);

        LEX_ADVANCE(lex);

        // Not a definition
        if (LEX_CUR(lex)->type != LBRACE && entry != NULL) {
            if (entry->type->type != btype) {
                logger_log(&LEX_CUR(lex)->mark, LOG_ERR,
                           "Incorrect type specifer %s. Expected: %s.",
                           ast_basic_type_str(entry->type->type),
                           ast_basic_type_str(btype));
                status = CCC_ESYNTAX;
                goto fail;
            }
            type_t *typedef_type = ast_type_create(lex->tunit,
                                                   &LEX_CUR(lex)->mark,
                                                   TYPE_TYPEDEF);
            typedef_type->typedef_params.name = name;
            typedef_type->typedef_params.base = entry->type;
            typedef_type->typedef_params.type = btype;

            *type = typedef_type;
            return CCC_OK;
        }

        // If we're defining a new type, and the last entry is in a different
        // scope than this one, then we need to create a new entry
        if (LEX_CUR(lex)->type == LBRACE && entry != NULL &&
            entry->typetab != lex->typetab) {
            entry = NULL;
        }
    }

    if (entry == NULL) { // Allocate a new type if it doesn't exist
        new_type = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark, btype);
        if (btype == TYPE_ENUM) {
            new_type->enum_params.name = name;
            new_type->enum_params.type = tt_int;
        } else {
            new_type->struct_params.name = name;
        }
        entry_type = new_type;
    } else {
        entry_type = entry->type;
    }

    // Create a new declaration in the table
    if (entry == NULL && name != NULL) {
        if (CCC_OK != (status = tt_insert(lex->typetab, new_type,
                                          TT_COMPOUND, name, &entry))) {
            goto fail;
        }
    }

    if (LEX_CUR(lex)->type != LBRACE) {
        if (name != NULL) {
            type_t *typedef_type = ast_type_create(lex->tunit,
                                                   &LEX_CUR(lex)->mark,
                                                   TYPE_TYPEDEF);
            typedef_type->typedef_params.name = name;
            typedef_type->typedef_params.base = entry_type;
            typedef_type->typedef_params.type = btype;

            *type = typedef_type;
            return CCC_OK;
        } else { // Can't have a compound type without a name or definition
            logger_log(&LEX_CUR(lex)->mark, LOG_ERR,
                       "Compound type without name or definition");
            status = CCC_ESYNTAX;
            goto fail;
        }
    }

    if (entry != NULL) {
        if (entry->struct_defined) {
            logger_log(&LEX_CUR(lex)->mark, LOG_ERR, "redefinition of '%s'",
                       entry->key);
            logger_log(&entry->type->mark, LOG_NOTE, "originally defined here");
            status = CCC_ESYNTAX;
            goto fail;
        }
        entry->struct_defined = true;
    }

    LEX_MATCH(lex, LBRACE);

    if (btype == TYPE_ENUM) {
        if (CCC_OK != (status = par_enumerator_list(lex, entry_type))) {
            goto fail;
        }
    } else { // struct/union
        while (CCC_BACKTRACK !=
               (status = par_struct_declaration(lex, entry_type))) {
            if (status != CCC_OK) {
                goto fail;
            }
        }
    }
    status = CCC_OK;
    LEX_MATCH(lex, RBRACE);

    *type = entry_type;

fail:
    return status;
}

status_t par_struct_declaration(lex_wrap_t *lex, type_t *type) {
    status_t status = CCC_OK;
    type_t *decl_type = NULL;
    if (CCC_OK != (status = par_specifier_qualifiers(lex, true, &decl_type))) {
        if (decl_type == NULL || status != CCC_BACKTRACK) {
            goto fail;
        }
    }

    if (CCC_OK != (status = par_struct_declarator_list(lex, type, decl_type))) {
        goto fail;
    }
    LEX_MATCH(lex, SEMI);

fail:
    return status;
}

status_t par_specifier_qualifiers(lex_wrap_t *lex, bool compound,
                                  type_t **type) {
    status_t status = CCC_OK;;
    *type = NULL; // Set to NULL so sub functions will allocate

    while (true) {
        switch (LEX_CUR(lex)->type) {
            // Type specifiers:
        case ID:
            // Type specifier only if its a typedef name
            if (tt_lookup(lex->typetab, LEX_CUR(lex)->id_name) == NULL) {
                goto done;
            }

            // Compound types are in another namespace, so if we get an
            // the names are allowed to collide with a typedef name
            if (compound) {
                bool has_specifier = *type != NULL &&
                ((*type)->type != TYPE_MOD || (*type)->mod.base != NULL);
                if (has_specifier) {
                    goto done;
                }
            }

            // FALL THROUGH
        case DECL_SPEC_TYPE_SPEC_NO_ID:
            if (CCC_OK != (status = par_type_specifier(lex, type))) {
                goto fail;
            }
            break;

            // Type qualitifiers
        case DECL_SPEC_TYPE_QUALIFIER:
            if (CCC_OK != (status = par_type_qualifier(lex, type))) {
                goto fail;
            }
            break;

        default:
            goto done;
        }
    }

done:
    if (*type == NULL) {
        status = CCC_BACKTRACK;
    } else if ((*type)->type == TYPE_MOD && (*type)->mod.base == NULL) {
        // If we parsed a type with no type specifier, then just make it an int
        (*type)->mod.base = tt_int;
    }
fail:
    return status;
}

status_t par_struct_declarator_list(lex_wrap_t *lex, type_t *base,
                                    type_t *decl_type) {
    status_t status = CCC_OK;
    decl_t *decl = ast_decl_create(lex->tunit, &LEX_CUR(lex)->mark);
    decl->type = decl_type;

    if (LEX_CUR(lex)->type != SEMI) {
        if (CCC_OK != (status = par_struct_declarator(lex, decl))) {
            goto fail;
        }
        while (LEX_CUR(lex)->type == COMMA) {
            LEX_ADVANCE(lex);
            if (CCC_OK != (status = par_struct_declarator(lex, decl))) {
                goto fail;
            }
        }
    }

    sl_append(&base->struct_params.decls, &decl->link);

fail:
    return status;
}

status_t par_struct_declarator(lex_wrap_t *lex, decl_t *decl) {
    status_t status = CCC_OK;
    decl_node_t *dnode = NULL;

    if (LEX_CUR(lex)->type != COLON) {
        if (CCC_OK != (status = par_declarator_base(lex, decl))) {
            goto fail;
        }
        dnode = sl_tail(&decl->decls);
    } else {
        dnode = ast_decl_node_create(lex->tunit, &LEX_CUR(lex)->mark);
        dnode->type = decl->type;
        sl_append(&decl->decls, &dnode->link);
    }

    if (LEX_CUR(lex)->type == COLON) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_oper_expression(lex, OP_NOP, NULL,
                                                    &dnode->expr))) {
            goto fail;
        }
    }

fail:
    return status;
}

status_t par_declarator_base(lex_wrap_t *lex, decl_t *decl) {
    switch (LEX_CUR(lex)->type) {
    case STAR:
    case ID:
    case LPAREN:
    case LBRACK:
        break;
    default:
        return CCC_BACKTRACK;
    }
    status_t status = CCC_OK;
    decl_node_t *decl_node = ast_decl_node_create(lex->tunit,
                                                  &LEX_CUR(lex)->mark);
    decl_node->type = decl->type;
    if (CCC_OK != (status = par_declarator(lex, decl_node, NULL))) {
        goto fail;
    }
    sl_append(&decl->decls, &decl_node->link);

    bool is_typedef = decl->type != NULL && decl->type->type == TYPE_MOD &&
        (decl->type->mod.type_mod & TMOD_TYPEDEF);

    // Add typedefs to the typetable on the top of the stack
    if (is_typedef) {
        // Remove the typedef from the type inserted into type table
        type_t *typedef_base;
        if (decl_node->type->type != TYPE_MOD) {
            typedef_base = decl_node->type;
        } else if ((decl_node->type->mod.type_mod & ~TMOD_TYPEDEF) == 0) {
            typedef_base = decl_node->type->mod.base;
        } else {
            typedef_base = ast_type_create(lex->tunit, &decl_node->type->mark,
                                           TYPE_MOD);
            typedef_base->mod.base = decl_node->type->mod.base;
            typedef_base->mod.type_mod =
                decl_node->type->mod.type_mod & ~TMOD_TYPEDEF;
        }
        if (CCC_OK !=
            (status = tt_insert(lex->typetab, typedef_base, TT_TYPEDEF,
                                decl_node->id, NULL))) {
            if (status != CCC_DUPLICATE) {
                goto fail;
            } else {
                typetab_entry_t *entry = tt_lookup(lex->typetab, decl_node->id);
                if (typecheck_type_equal(entry->type, decl_node->type)) {
                    status = CCC_OK;
                } else {
                    logger_log(&decl_node->mark, LOG_ERR,
                               "conflicting types for '%s'", decl_node->id);
                }
            }
        }
    }

fail:
    return status;
}

status_t par_declarator(lex_wrap_t *lex, decl_node_t *decl_node,
                        type_t ***patch) {
    status_t status = CCC_OK;

    type_t **lpatch = &decl_node->type;
    while (LEX_CUR(lex)->type == STAR) {
        if (CCC_OK != (status = par_pointer(lex, lpatch))) {
            goto fail;
        }
        lpatch = &(*lpatch)->ptr.base;
    }

    if (CCC_OK != (status = par_direct_declarator(lex, decl_node, patch))) {
        goto fail;
    }

    if (patch != NULL) {
        *patch = lpatch;
    }

fail:
    return status;
}

status_t par_pointer(lex_wrap_t *lex, type_t **base_ptr) {
    status_t status = CCC_OK;
    LEX_MATCH(lex, STAR);

    type_t *new_type = NULL;
    while (CCC_BACKTRACK != (status = par_type_qualifier(lex, &new_type))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;

    type_mod_t mods;
    if (new_type != NULL) {
        mods = new_type->mod.type_mod;
    } else {
        mods = TMOD_NONE;
        new_type = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark, TYPE_PTR);
    }
    new_type->type = TYPE_PTR;
    new_type->ptr.type_mod = mods;

    new_type->ptr.base = *base_ptr;
    *base_ptr = new_type;

    return status;

fail:
    // Nothing to clean up here
    return status;
}

status_t par_type_qualifier(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;
    type_mod_t mod;
    switch (LEX_CUR(lex)->type) {
    case CONST:    mod = TMOD_CONST;    break;
    case VOLATILE: mod = TMOD_VOLATILE; break;
    default:
        return CCC_BACKTRACK;
    }
    LEX_ADVANCE(lex);

    // Check first node for mod node
    type_t *mod_node;
    if (*type != NULL && (*type)->type == TYPE_MOD) {
        mod_node = *type;
    } else {
        mod_node = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark, TYPE_MOD);
        mod_node->mod.type_mod = TMOD_NONE;
        mod_node->mod.base = *type;
        *type = mod_node;
    }
    mod_node->mod.type_mod |= mod;

    return status;
}

status_t par_direct_declarator(lex_wrap_t *lex, decl_node_t *node,
                               type_t ***patch) {
    status_t status = CCC_OK;
    type_t *base = node->type;
    type_t **lpatch = NULL;
    type_t **last_node = NULL;

    switch (LEX_CUR(lex)->type) {
    case LPAREN: {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_declarator(lex, node, &lpatch))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);

        if (optman.dump_opts & DUMP_AST) {
            // Only create paren node if we're printing AST
            type_t *paren_type = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark,
                                                 TYPE_PAREN);
            paren_type->paren_base = *lpatch;
            *lpatch = paren_type;
            lpatch = &paren_type->paren_base;
        }
        break;
    }

    case ID:
        node->id = LEX_CUR(lex)->id_name;
        LEX_ADVANCE(lex);
        break;

    default:
        // Default is not an error, because may be abstract without an
        // identifier or parens
        break;
    }

    last_node = &base;
    bool done = false;
    while (!done) {
        switch (LEX_CUR(lex)->type) {
        case LBRACK: // Array dimension
            LEX_ADVANCE(lex);
            type_t *arr_type = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark,
                                               TYPE_ARR);
            arr_type->arr.base = *last_node;
            *last_node = arr_type;
            last_node = &arr_type->arr.base;

            if (LEX_CUR(lex)->type == RBRACK) {
                LEX_ADVANCE(lex);
            } else {
                if (CCC_OK !=
                    (status = par_oper_expression(lex, OP_NOP, NULL,
                                                  &arr_type->arr.len))) {
                    goto fail;
                }
                LEX_MATCH(lex, RBRACK);
            }
            break;
        case LPAREN:
            LEX_ADVANCE(lex);
            type_t *func_type = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark,
                                                TYPE_FUNC);
            func_type->func.varargs = false;

            func_type->func.type = *last_node;
            *last_node = func_type;
            last_node = &func_type->func.type;

            /* TODO2: Support K & R decl syntax
            if (LEX_CUR(lex)->type == ID) {
                while (LEX_CUR(lex)->type == ID) {
                    LEX_ADVANCE(lex);
                }
                LEX_MATCH(lex, RPAREN);
            } else {
            */
            if (CCC_OK != (status = par_parameter_type_list(lex, func_type))) {
                goto fail;
            }
            //}
            LEX_MATCH(lex, RPAREN);
            break;
        default:
            done = true;
        }
    }

fail:
    if (lpatch == NULL) {
        node->type = base;
    } else if (last_node != &base) {
        *lpatch = base;
    }

    if (patch != NULL) {
        if (last_node != &base) {
            *patch = last_node;
        } else {
            *patch = NULL;
        }
    }

    return status;
}

status_t par_oper_expression(lex_wrap_t *lex, oper_t prev_op, expr_t *left,
                             expr_t **result) {
    status_t status = CCC_OK;
    expr_t *new_node = NULL; // Newly allocated expression

    if (left == NULL) { // Only search for first operand if not provided
        if (CCC_OK !=
            (status = par_cast_expression(lex, &left))) {
            goto fail;
        }
    }

    // This loop runs to combine binary expressions
    bool new_left = true;
    while (new_left) {
        new_left = false;
        oper_t op1 = OP_NOP;

        switch (LEX_CUR(lex)->type) {
            // Binary operators
        case STAR:     op1 = OP_TIMES;    break;
        case DIV:      op1 = OP_DIV;      break;
        case MOD:      op1 = OP_MOD;      break;
        case PLUS:     op1 = OP_PLUS;     break;
        case MINUS:    op1 = OP_MINUS;    break;
        case LSHIFT:   op1 = OP_LSHIFT;   break;
        case RSHIFT:   op1 = OP_RSHIFT;   break;
        case LT:       op1 = OP_LT;       break;
        case GT:       op1 = OP_GT;       break;
        case LE:       op1 = OP_LE;       break;
        case GE:       op1 = OP_GE;       break;
        case EQ:       op1 = OP_EQ;       break;
        case NE:       op1 = OP_NE;       break;
        case BITAND:   op1 = OP_BITAND;   break;
        case BITXOR:   op1 = OP_BITXOR;   break;
        case BITOR:    op1 = OP_BITOR;    break;
        case LOGICAND: op1 = OP_LOGICAND; break;
        case LOGICOR:  op1 = OP_LOGICOR;  break;

        case COND: { // Conditional operator
            // Cond takes lowest precedence, if there was a previous operator,
            // just return left
            if (prev_op != OP_NOP) {
                *result = left;
                return CCC_OK;
            }
            LEX_ADVANCE(lex);
            new_node = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                       EXPR_COND);
            new_node->cond.expr1 = left;

            if (CCC_OK !=
                (status = par_expression(lex, &new_node->cond.expr2))) {
                goto fail;
            }
            LEX_MATCH(lex, COLON);
            if (CCC_OK !=
                (status =
                 par_oper_expression(lex, OP_NOP, NULL,
                                     &new_node->cond.expr3))) {
                goto fail;
            }

            // Used the parsed conditional expression new left
            left = new_node;
            new_left = true;
            break;
        }
        default: // No binary operator, just complete with the left side
            *result = left;
            return CCC_OK;
        }

        if (new_left) {
            continue;
        }

        // Consume the binary operation
        assert(op1 != OP_NOP);
        LEX_ADVANCE(lex);

        expr_t *right; // Right side of binary expression
        if (CCC_OK != (status = par_cast_expression(lex, &right))) {
            goto fail;
        }

        oper_t op2 = OP_NOP;
        switch (LEX_CUR(lex)->type) {
            // Binary operators
        case STAR:     op2 = OP_TIMES;    break;
        case DIV:      op2 = OP_DIV;      break;
        case MOD:      op2 = OP_MOD;      break;
        case PLUS:     op2 = OP_PLUS;     break;
        case MINUS:    op2 = OP_MINUS;    break;
        case LSHIFT:   op2 = OP_LSHIFT;   break;
        case RSHIFT:   op2 = OP_RSHIFT;   break;
        case LT:       op2 = OP_LT;       break;
        case GT:       op2 = OP_GT;       break;
        case LE:       op2 = OP_LE;       break;
        case GE:       op2 = OP_GE;       break;
        case EQ:       op2 = OP_EQ;       break;
        case NE:       op2 = OP_NE;       break;
        case BITAND:   op2 = OP_BITAND;   break;
        case BITXOR:   op2 = OP_BITXOR;   break;
        case BITOR:    op2 = OP_BITOR;    break;
        case LOGICAND: op2 = OP_LOGICAND; break;
        case LOGICOR:  op2 = OP_LOGICOR;  break;

        case COND: { // Cond has lowest precedence, combine left and right
            // Cond takes lowest precedence, if there was a previous operator,
            // combine left and right and return
            if (prev_op != OP_NOP) {
                new_node = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                           EXPR_BIN);
                new_node->type = EXPR_BIN;
                new_node->bin.op = op1;
                new_node->bin.expr1 = left;
                new_node->bin.expr2 = right;
                *result = new_node;
                return CCC_OK;
            }
            LEX_ADVANCE(lex);
            new_node = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                       EXPR_BIN);
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;
            new_node->bin.expr2 = right;

            expr_t *cond_node = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                                EXPR_COND);
            cond_node->cond.expr1 = new_node;

            // Set new_node to cond_node so it is properly freed on error
            new_node = cond_node;
            if (CCC_OK !=
                (status = par_expression(lex, &cond_node->cond.expr2))) {
                goto fail;
            }
            LEX_MATCH(lex, COLON);
            if (CCC_OK !=
                (status =
                 par_oper_expression(lex, OP_NOP, NULL,
                                     &cond_node->cond.expr3))) {
                goto fail;
            }

            // We're done because cond has lowest precedence, so any preceeding
            // binary operators are in cond.expr3
            *result = cond_node;
            return CCC_OK;
        }
        default: { // Not binary 2, just combine the left and light
            new_node = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                       EXPR_BIN);
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;
            new_node->bin.expr2 = right;
            *result = new_node;
            return CCC_OK;
        }
        }

        if (par_get_binary_prec(op1) >= par_get_binary_prec(op2)) {
            // op1 has greater or equal precedence, so combine left and right
            // and restart loop with new left
            new_node = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                       EXPR_BIN);
            new_node->type = EXPR_BIN;
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;
            new_node->bin.expr2 = right;

            // If previous operation has greater or equal precedence to op2,
            // then just return here so left associativity is preserved
            if (par_get_binary_prec(prev_op) >= par_get_binary_prec(op2)) {
                *result = new_node;
                return CCC_OK;
            }
        } else {
            // op2 has greater precedence, parse expression with right as the
            // left side, then combine with the current left
            new_node = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                       EXPR_BIN);
            new_node->type = EXPR_BIN;
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;

            if (CCC_OK !=
                (status =
                 par_oper_expression(lex, op1, right, &new_node->bin.expr2))) {
                goto fail;
            }

        }

        new_left = true;
        left = new_node;
    }

fail:
    return status;
}

status_t par_mem_acc_list(lex_wrap_t *lex, mem_acc_list_t *list, bool nodot) {
    status_t status = CCC_OK;
    while (nodot || LEX_CUR(lex)->type == DOT ||
           LEX_CUR(lex)->type == LBRACK) {
        expr_t *access;
        if (nodot || LEX_CUR(lex)->type == DOT) {
            if (nodot) {
                nodot = false;
            } else {
                LEX_ADVANCE(lex);
            }

            if (LEX_CUR(lex)->type != ID) { // Not a name
                logger_log(&LEX_CUR(lex)->mark, LOG_ERR,
                           "Parse Error: Expected <identifer>, Found: %s.",
                           token_type_str(LEX_CUR(lex)->type));
                status = CCC_ESYNTAX;
                goto fail;
            }

            access = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                     EXPR_MEM_ACC);
            access->mem_acc.base = NULL;
            access->mem_acc.name = LEX_CUR(lex)->id_name;
            access->mem_acc.op = OP_DOT;
            LEX_ADVANCE(lex);
        } else if (LEX_CUR(lex)->type == LBRACK) {
            LEX_ADVANCE(lex);
            access = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                     EXPR_ARR_IDX);
            access->arr_idx.array = NULL;

            if (CCC_OK !=
                (status = par_expression(lex, &access->arr_idx.index))) {
                goto fail;
            }
            LEX_MATCH(lex, RBRACK);
        } else {
            break;
        }
        sl_append(&list->list, &access->link);
    }

fail:
    return status;
}

status_t par_unary_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *base = NULL;

    switch (LEX_CUR(lex)->type) {
    case INC:
    case DEC: {
        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_UNARY);
        oper_t op = LEX_CUR(lex)->type == INC ? OP_PREINC : OP_PREDEC;
        LEX_ADVANCE(lex);
        base->unary.op = op;
        if (CCC_OK != (status = par_unary_expression(lex, &base->unary.expr))) {
            goto fail;
        }
        break;
    }

    case SIZEOF:
    case ALIGNOF: {
        expr_type_t btype =
            LEX_CUR(lex)->type == SIZEOF ? EXPR_SIZEOF : EXPR_ALIGNOF;
        LEX_ADVANCE(lex);
        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, btype);
        if (LEX_CUR(lex)->type == LPAREN) {
            if (CCC_OK !=
                (status = par_type_name(lex, true,
                                        &base->sizeof_params.type))) {
                if (status != CCC_BACKTRACK) {
                    goto fail;
                }
            }

            // Failed to parse a typename, parse an expression instead
            if (base->sizeof_params.type == NULL) {
                if (CCC_OK !=
                    (status =
                     par_unary_expression(lex, &base->sizeof_params.expr))) {
                    goto fail;
                }
            }
        } else {
            if (CCC_OK !=
                (status =
                 par_unary_expression(lex, &base->sizeof_params.expr))) {
                goto fail;
            }
        }
        break;
    }
    case OFFSETOF:
        LEX_ADVANCE(lex);
        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_OFFSETOF);
        LEX_MATCH(lex, LPAREN);
        if (CCC_OK != (status = par_type_name(lex, false,
                                              &base->offsetof_params.type))) {
            goto fail;
        }
        LEX_MATCH(lex, COMMA);
        par_mem_acc_list(lex, &base->offsetof_params.path, true);
        LEX_MATCH(lex, RPAREN);
        break;

    case VA_START:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);

        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_VA_START);
        if (CCC_OK != par_assignment_expression(lex, &base->vastart.ap)) {
            goto fail;
        }
        LEX_MATCH(lex, COMMA);
        if (CCC_OK != par_assignment_expression(lex, &base->vastart.last)) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        break;
    case VA_ARG:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);

        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_VA_ARG);
        if (CCC_OK != par_assignment_expression(lex, &base->vaarg.ap)) {
            goto fail;
        }
        LEX_MATCH(lex, COMMA);
        if (CCC_OK != par_type_name(lex, false, &base->vaarg.type)) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        break;
    case VA_END:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);

        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_VA_END);
        if (CCC_OK != par_assignment_expression(lex, &base->vaend.ap)) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        break;
    case VA_COPY:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);

        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_VA_COPY);
        if (CCC_OK != par_assignment_expression(lex, &base->vacopy.dest)) {
            goto fail;
        }
        LEX_MATCH(lex, COMMA);
        if (CCC_OK != par_assignment_expression(lex, &base->vacopy.src)) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        break;

    case BITAND:
    case STAR:
    case PLUS:
    case MINUS:
    case BITNOT:
    case LOGICNOT: {
        oper_t op;
        switch (LEX_CUR(lex)->type) {
        case BITAND:   op = OP_ADDR;     break;
        case STAR:     op = OP_DEREF;    break;
        case PLUS:     op = OP_UPLUS;    break;
        case MINUS:    op = OP_UMINUS;   break;
        case BITNOT:   op = OP_BITNOT;   break;
        case LOGICNOT: op = OP_LOGICNOT; break;
        default:
            assert(false);
        }
        LEX_ADVANCE(lex);

        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_UNARY);
        base->unary.op = op;
        if (CCC_OK !=
            (status = par_cast_expression(lex, &base->unary.expr))) {
            goto fail;
        }
        break;
    }
    default:
        return par_postfix_expression(lex, result);
    }

    *result = base;

fail:
    return status;
}

status_t par_cast_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    if (LEX_CUR(lex)->type != LPAREN) {
        // No paren, just parse a unary expression
        return par_unary_expression(lex, result);
    }
    decl_t *type = NULL;
    expr_t *expr = NULL;
    if (CCC_OK != (status = par_type_name(lex, true, &type))) {
        if (status != CCC_BACKTRACK) {
            goto fail;
        }
        return par_unary_expression(lex, result);
    }

    expr = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_CAST);
    expr->cast.cast = type;

    if (LEX_CUR(lex)->type == LBRACE) {
        // Try to parse compound literal
        if (CCC_OK != (status = par_initializer(lex, &expr->cast.base))) {
            goto fail;
        }
    } else if (CCC_OK !=
               (status = par_cast_expression(lex, &expr->cast.base))) {
        goto fail;
    }

    *result = expr;

fail:
    return status;
}

status_t par_postfix_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *base = NULL;
    expr_t *expr = NULL;
    if (CCC_OK != (status = par_primary_expression(lex, &base))) {
        goto fail;
    }
    expr = base;

    while (base != NULL) {
        switch (LEX_CUR(lex)->type) {
        case LBRACK: // Array index
            LEX_ADVANCE(lex);
            expr = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                   EXPR_ARR_IDX);
            expr->arr_idx.array = base;

            if (CCC_OK !=
                (status = par_expression(lex, &expr->arr_idx.index))) {
                goto fail;
            }
            LEX_MATCH(lex, RBRACK);
            base = expr;
            break;

        case LPAREN: // Function call
            LEX_ADVANCE(lex);
            expr = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_CALL);
            expr->call.func = base;

            while (LEX_CUR(lex)->type != RPAREN) {
                expr_t *param;
                if (CCC_OK !=
                    (status = par_assignment_expression(lex, &param))) {
                    goto fail;
                }
                sl_append(&expr->call.params, &param->link);
                if (LEX_CUR(lex)->type == RPAREN) {
                    break;
                }
                LEX_MATCH(lex, COMMA);
            }
            LEX_ADVANCE(lex);
            base = expr;
            break;

        case DOT: // Structure access direct or indirect
        case DEREF: {
            oper_t op = LEX_CUR(lex)->type == DOT ? OP_DOT : OP_ARROW;
            LEX_ADVANCE(lex);

            if (LEX_CUR(lex)->type != ID) { // Not a name
                logger_log(&LEX_CUR(lex)->mark, LOG_ERR,
                           "Parse Error: Expected <identifer>, Found: %s.",
                           token_type_str(LEX_CUR(lex)->type));
                status = CCC_ESYNTAX;
                goto fail;
            }
            expr = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                   EXPR_MEM_ACC);
            expr->mem_acc.base = base;
            expr->mem_acc.name = LEX_CUR(lex)->id_name;
            expr->mem_acc.op = op;
            LEX_ADVANCE(lex);
            base = expr;
            break;
        }

        case INC:
        case DEC: {
            oper_t op = LEX_CUR(lex)->type == INC ? OP_POSTINC : OP_POSTDEC;
            LEX_ADVANCE(lex);
            expr = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_UNARY);
            expr->unary.op = op;
            expr->unary.expr = base;
            base = expr;
            break;
        }
        default: // Found a terminating character
            base = NULL;
        }
    }

    *result = expr;

fail:
    return status;
}

status_t par_primary_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *base = NULL;

    switch (LEX_CUR(lex)->type) {
    case LPAREN:
        LEX_ADVANCE(lex);
        // Try to parse as paren expression
        if (optman.dump_opts & DUMP_AST) {
            // Only create the paren node if we're printing ast
            base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_PAREN);
            if (CCC_OK !=
                (status = par_expression(lex, &base->paren_base))) {
                goto fail;
            }
        } else {
            if (CCC_OK !=
                (status = par_expression(lex, &base))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, RPAREN);
        break;
    case ID:
        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_VAR);
        base->var_id = LEX_CUR(lex)->id_name;
        LEX_ADVANCE(lex);
        break;

    case FUNC:
    case STRING: {
        bool is_func = LEX_CUR(lex)->type == FUNC;

        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_CONST_STR);
        if (is_func) {
            if (lex->function == NULL) {
                logger_log(&LEX_CUR(lex)->mark, LOG_WARN,
                           "'%s' is not defined outside of function scope",
                           token_type_str(LEX_CUR(lex)->type));
                base->const_val.str_val = "";
            } else {
                base->const_val.str_val = lex->function;
            }
        } else {
            base->const_val.str_val = LEX_CUR(lex)->str_val;
        }
        type_t *type = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark,
                                       TYPE_ARR);
        type->arr.base = tt_char;
        base->const_val.type = type;
        type->arr.len = NULL;
        type->arr.nelems = strlen(base->const_val.str_val) + 1;
        LEX_ADVANCE(lex);
        break;
    }
    case INTLIT: {
        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_CONST_INT);
        unsigned long long intval = LEX_CUR(lex)->int_params->int_val;
        base->const_val.int_val = intval;

        type_t *type;
        bool need_u = false;
        bool need_ll = false, need_l = false;
        if (intval > LLONG_MAX) {
            need_ll = true;
            need_u = true;
        } else if (intval > ULONG_MAX) {
            need_ll = true;
        } else if (intval > LONG_MAX) {
            need_l = true;
            need_u = true;
        } else if (intval > UINT_MAX) {
            need_l = true;
        } else if (intval > INT_MAX) {
            need_u = true;
        }

        type_t *explicit;
        if (LEX_CUR(lex)->int_params->hasLL) {
            explicit = tt_long_long;
        } else if (LEX_CUR(lex)->int_params->hasL) {
            explicit = tt_long;
        } else {
            explicit = tt_int;
        }
        size_t explicit_size = ast_type_size(explicit);
        size_t ll_size = ast_type_size(tt_long_long);

        if (need_ll && need_u) {
            type = tt_long_long;
            need_u = true;

            if (!LEX_CUR(lex)->int_params->hasU && explicit_size < ll_size) {
                logger_log(&LEX_CUR(lex)->mark, LOG_WARN,
                           "integer constant is so large that it is unsigned");
            }
        } else if (need_ll && explicit_size < ll_size) {
            type = tt_long_long;
        } else if (need_l && explicit_size < ast_type_size(tt_long_long)) {
            type = tt_long;
        } else {
            type = explicit;
        }

        if (LEX_CUR(lex)->int_params->hasU) {
            need_u = true;
        }

        if (need_u) {
            type_t *type_mod = ast_type_create(lex->tunit, &LEX_CUR(lex)->mark,
                                               TYPE_MOD);
            type_mod->mod.type_mod = TMOD_UNSIGNED;
            type_mod->mod.base = type;
            type = type_mod;
        }
        base->const_val.type = type;
        LEX_ADVANCE(lex);
        break;
    }
    case FLOATLIT: {
        base = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                               EXPR_CONST_FLOAT);
        base->const_val.float_val = LEX_CUR(lex)->float_params->float_val;
        if (LEX_CUR(lex)->float_params->hasF) {
            base->const_val.type = tt_float;
        } else if (LEX_CUR(lex)->float_params->hasL) {
            base->const_val.type = tt_long_double;
        } else {
            base->const_val.type = tt_double;
        }
        LEX_ADVANCE(lex);
        break;
    }
    default:
        logger_log(&LEX_CUR(lex)->mark, LOG_ERR,
                   "Unexpected token %s. Expected primary expression.",
                   token_type_str(LEX_CUR(lex)->type));
        status = CCC_ESYNTAX;
        goto fail;
    }

    *result = base;

fail:
    return status;
}

status_t par_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *expr = NULL;
    if (CCC_OK != (status = par_assignment_expression(lex, &expr))) {
        goto fail;
    }

    if (LEX_CUR(lex)->type == COMMA) {
        expr_t *cmpd = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                       EXPR_CMPD);
        sl_append(&cmpd->cmpd.exprs, &expr->link);
        expr = cmpd;

        while (LEX_CUR(lex)->type == COMMA) {
            LEX_ADVANCE(lex);
            expr_t *cur = NULL;
            if (CCC_OK != (status = par_assignment_expression(lex, &cur))) {
                goto fail;
            }
            sl_append(&cmpd->cmpd.exprs, &cur->link);
        }
    }

    *result = expr;

fail:
    return status;
}

status_t par_assignment_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *left = NULL;
    expr_t *expr = NULL;

    if (CCC_OK != (status = par_cast_expression(lex, &left))) {
        goto fail;
    }

    bool is_assign = true;
    oper_t op;
    switch (LEX_CUR(lex)->type) {
    case ASSIGN:   op = OP_NOP;    break;
    case STAREQ:   op = OP_TIMES;  break;
    case DIVEQ:    op = OP_DIV;    break;
    case MODEQ:    op = OP_MOD;    break;
    case PLUSEQ:   op = OP_PLUS;   break;
    case MINUSEQ:  op = OP_MINUS;  break;
    case LSHIFTEQ: op = OP_LSHIFT; break;
    case RSHIFTEQ: op = OP_RSHIFT; break;
    case BITANDEQ: op = OP_BITAND; break;
    case BITXOREQ: op = OP_BITXOR; break;
    case BITOREQ:  op = OP_BITOR;  break;
    default:
        is_assign = false;
    }

    if (is_assign) {
        LEX_ADVANCE(lex);

        expr = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark, EXPR_ASSIGN);
        expr->assign.dest = left;
        expr->assign.op = op;

        if (CCC_OK !=
            (status = par_assignment_expression(lex, &expr->assign.expr))) {
            goto fail;
        }

    } else {
        if (CCC_OK !=
            (status = par_oper_expression(lex, OP_NOP, left, &expr))) {
            goto fail;
        }
    }
    *result = expr;

fail:
    return status;
}

status_t par_type_name(lex_wrap_t *lex, bool match_parens, decl_t **result) {
    status_t status = CCC_OK;
    type_t *base = NULL;
    decl_t *decl = NULL;

    if (match_parens) {
        switch (LEX_NEXT(lex)->type) {
        case ID: {
            if (tt_lookup(lex->typetab, LEX_NEXT(lex)->id_name)
                == NULL) {
                return CCC_BACKTRACK;
            }
        }
        case DECL_SPEC_TYPE_SPEC_NO_ID:
        case DECL_SPEC_TYPE_QUALIFIER:
            break;

        default:
            return CCC_BACKTRACK;
        }
        LEX_MATCH(lex, LPAREN);
    }

    if (CCC_OK != (status = par_specifier_qualifiers(lex, false, &base))) {
        if (base == NULL || status != CCC_BACKTRACK) {
            goto fail;
        }
    }
    decl = ast_decl_create(lex->tunit, &LEX_CUR(lex)->mark);
    decl->type = base;

    if (CCC_BACKTRACK != (status = par_declarator_base(lex, decl))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;
    if (match_parens) {
        LEX_MATCH(lex, RPAREN);
    }

    *result = decl;

fail:
    return status;
}

status_t par_parameter_type_list(lex_wrap_t *lex, type_t *func) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_parameter_list(lex, func))) {
        if (status != CCC_BACKTRACK) {
            goto fail;
        }
    }

    if (LEX_CUR(lex)->type != ELIPSE) {
        return CCC_OK;
    }
    LEX_ADVANCE(lex);
    func->func.varargs = true;

fail:
    return status;
}

status_t par_parameter_list(lex_wrap_t *lex, type_t *func) {
    status_t status = CCC_OK;
    while (CCC_BACKTRACK != (status = par_parameter_declaration(lex, func))) {
        if (status != CCC_OK) {
            goto fail;
        }
        if (LEX_CUR(lex)->type != RPAREN) {
            LEX_MATCH(lex, COMMA);
            if (LEX_CUR(lex)->type == RPAREN) {
                logger_log(&LEX_CUR(lex)->mark, LOG_ERR,
                           "Unexpected token %s", token_type_str(COMMA));
                status = CCC_ESYNTAX;
                goto fail;
            }
        }
    }
    status = CCC_OK;

fail:
    return status;
}

status_t par_parameter_declaration(lex_wrap_t *lex, type_t *func) {
    status_t status = CCC_OK;
    type_t *type = NULL;
    decl_t *decl = NULL;

    // Must have type specifers
    if (CCC_OK != (status = par_declaration_specifiers(lex, &type))) {
        if (type == NULL || status != CCC_BACKTRACK) {
            goto fail;
        }
    }

    decl = ast_decl_create(lex->tunit, &LEX_CUR(lex)->mark);
    decl->type = type;

    // Declarators are optional
    if (CCC_BACKTRACK != (status = par_declarator_base(lex, decl))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;

    // Add declaration to the function
    sl_append(&func->func.params, &decl->link);

fail:
    return status;
}

status_t par_enumerator_list(lex_wrap_t *lex, type_t *type) {
    assert(type->type == TYPE_ENUM);
    status_t status = CCC_OK;

    if (CCC_OK != (status = par_enumerator(lex, type))) {
        goto fail;
    }

    // Trailing comma on last entry is allowed
    while (LEX_CUR(lex)->type == COMMA) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_enumerator(lex, type))) {
            if (status == CCC_BACKTRACK) {
                break;
            } else {
                goto fail;
            }
        }
    }
    status = CCC_OK;

fail:
    return status;
}

status_t par_enumerator(lex_wrap_t *lex, type_t *type) {
    status_t status = CCC_OK;
    if (LEX_CUR(lex)->type != ID) {
        return CCC_BACKTRACK;
    }
    decl_node_t *node = ast_decl_node_create(lex->tunit, &LEX_CUR(lex)->mark);
    node->type = type->enum_params.type;
    node->id = LEX_CUR(lex)->id_name;
    LEX_ADVANCE(lex);

    // Parse enum value if there is one
    if (LEX_CUR(lex)->type == ASSIGN) {
        LEX_ADVANCE(lex);
        if (CCC_OK !=
            (status = par_oper_expression(lex, OP_NOP, NULL, &node->expr))) {
            goto fail;
        }
    }
    sl_append(&type->enum_params.ids, &node->link);

fail:
    return status;
}

status_t par_declaration(lex_wrap_t *lex, decl_t **decl, bool partial) {
    status_t status = CCC_OK;
    if (*decl == NULL) {
        (*decl) = ast_decl_create(lex->tunit, &LEX_CUR(lex)->mark);
        (*decl)->type = NULL;

        // Must match at least one declaration specifier
        if (CCC_OK !=
            (status = par_declaration_specifiers(lex, &(*decl)->type))) {
            if ((*decl)->type == NULL || status != CCC_BACKTRACK) {
                goto fail;
            }
        }
    }

    if (CCC_BACKTRACK == (status = par_init_declarator(lex, *decl, partial))) {
        // No init declarators
        return CCC_OK;
    }

    while (LEX_CUR(lex)->type == COMMA) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_init_declarator(lex, *decl, false))) {
            if (status == CCC_BACKTRACK) {
                status = CCC_ESYNTAX;
            }
            goto fail;
        }
    }
    status = CCC_OK;

fail:
    return status;
}

status_t par_init_declarator(lex_wrap_t *lex, decl_t *decl, bool partial) {
    status_t status = CCC_OK;

    if (!partial && CCC_OK != (status = par_declarator_base(lex, decl))) {
        goto fail;
    }
    decl_node_t *decl_node = sl_tail(&decl->decls);

    bool is_typedef = decl->type != NULL && decl->type->type == TYPE_MOD &&
        (decl->type->mod.type_mod & TMOD_TYPEDEF);
    if (LEX_CUR(lex)->type == ASSIGN) {
        if (is_typedef) {
            logger_log(&LEX_CUR(lex)->mark, LOG_WARN,
                       "Typedef '%s' is initialized", decl_node->id);
            status = CCC_ESYNTAX;
            goto fail;
        }
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_initializer(lex, &decl_node->expr))) {
            goto fail;
        }
    }

fail:
    return status;
}

status_t par_initializer(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;

    // Not left bracket, so its an expression
    if (LEX_CUR(lex)->type != LBRACE) {
        return par_assignment_expression(lex, result);
    }
    LEX_ADVANCE(lex);
    if (CCC_OK != (status = par_initializer_list(lex, result))) {
        goto fail;
    }
    LEX_MATCH(lex, RBRACE);

fail:
    return status;
}

status_t par_initializer_list(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *expr = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                   EXPR_INIT_LIST);
    while (true) {
        if (LEX_CUR(lex)->type == COMMA) {
            LEX_ADVANCE(lex);
        }
        if (LEX_CUR(lex)->type == RBRACE) { // Trailing commas allowed
            break;
        }
        expr_t *cur = NULL;
        if (LEX_CUR(lex)->type == DOT) { // Designated initalizer
            LEX_ADVANCE(lex);
            if (LEX_CUR(lex)->type != ID || LEX_NEXT(lex)->type != ASSIGN) {
                goto fail;
            }
            cur = ast_expr_create(lex->tunit, &LEX_CUR(lex)->mark,
                                  EXPR_DESIG_INIT);
            cur->desig_init.name = LEX_CUR(lex)->id_name;
            LEX_ADVANCE(lex); // Skip the ID
            LEX_ADVANCE(lex); // Skip the =
            if (CCC_OK != (status = par_initializer(lex,
                                                    &cur->desig_init.val))) {
                goto fail;
            }
        } else {
            if (CCC_OK != (status = par_initializer(lex, &cur))) {
                goto fail;
            }
        }
        sl_append(&expr->init_list.exprs, &cur->link);
        ++expr->init_list.nelems;
    }

    *result = expr;

fail:
    return status;
}


status_t par_statement(lex_wrap_t *lex, stmt_t **result) {
    switch (LEX_CUR(lex)->type) {
    case ID:
        // If next character isn't a colon, then its an expression statement
        if (LEX_NEXT(lex)->type != COLON) {
            return par_expression_statement(lex, result);
        }
        // FALL THROUGH
    case CASE:
    case DEFAULT:
        return par_labeled_statement(lex, result);

    case IF:
    case SWITCH:
        return par_selection_statement(lex, result);

    case DO:
    case WHILE:
    case FOR:
        return par_iteration_statement(lex, result);

    case GOTO:
    case CONTINUE:
    case BREAK:
    case RETURN:
        return par_jump_statement(lex, result);
    case LBRACE:
        return par_compound_statement(lex, result);
    default:
        return par_expression_statement(lex, result);
    }
}

status_t par_labeled_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;

    switch (LEX_CUR(lex)->type) {
    case ID:
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_LABEL);
        stmt->label.label = LEX_CUR(lex)->id_name;
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, COLON);
        if (CCC_OK != (status = par_statement(lex, &stmt->label.stmt))) {
            goto fail;
        }
        break;

    case CASE:
        LEX_ADVANCE(lex);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_CASE);

        if (CCC_OK !=
            (status =
             par_oper_expression(lex, OP_NOP, NULL, &stmt->case_params.val))) {
            goto fail;
        }
        LEX_MATCH(lex, COLON);
        if (CCC_OK !=
            (status = par_statement(lex, &stmt->case_params.stmt))) {
            goto fail;
        }
        break;

    case DEFAULT:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, COLON);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_DEFAULT);

        if (CCC_OK !=
            (status = par_statement(lex, &stmt->default_params.stmt))) {
            goto fail;
        }
        break;

    default:
        assert(false);
    }

    *result = stmt;

fail:
    return status;
}

status_t par_selection_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt = NULL;

    switch (LEX_CUR(lex)->type) {
    case IF:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_IF);

        if (CCC_OK !=
            (status = par_expression(lex, &stmt->if_params.expr))) {
            goto fail;
        }

        LEX_MATCH(lex, RPAREN);
        if (CCC_OK !=
            (status = par_statement(lex, &stmt->if_params.true_stmt))) {
            goto fail;
        }

        if (LEX_CUR(lex)->type == ELSE) {
            LEX_ADVANCE(lex);
            if (CCC_OK !=
                (status = par_statement(lex, &stmt->if_params.false_stmt))) {
                goto fail;
            }
        }
        break;

    case SWITCH:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_SWITCH);

        if (CCC_OK !=
            (status = par_expression(lex, &stmt->switch_params.expr))) {
            goto fail;
        }

        LEX_MATCH(lex, RPAREN);
        if (CCC_OK !=
            (status = par_statement(lex, &stmt->switch_params.stmt))) {
            goto fail;
        }
        break;

    default:
        assert(false);
    }

    *result = stmt;

fail:
    return status;
}

status_t par_iteration_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt = NULL;

    switch (LEX_CUR(lex)->type) {
    case DO:
        LEX_ADVANCE(lex);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_DO);

        if (CCC_OK != (status = par_statement(lex, &stmt->do_params.stmt))) {
            goto fail;
        }

        LEX_MATCH(lex, WHILE);
        LEX_MATCH(lex, LPAREN);
        if (CCC_OK !=
            (status = par_expression(lex, &stmt->do_params.expr))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        LEX_MATCH(lex, SEMI);
        break;

    case WHILE:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_WHILE);

        if (CCC_OK !=
            (status = par_expression(lex, &stmt->while_params.expr))) {
            goto fail;
        }

        LEX_MATCH(lex, RPAREN);
        if (CCC_OK != (status = par_statement(lex, &stmt->while_params.stmt))) {
            goto fail;
        }
        break;

    case FOR:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_FOR);

        if (LEX_CUR(lex)->type != SEMI) {
            bool expr = false;
            switch (LEX_CUR(lex)->type) {
            case ID:
                if (tt_lookup(lex->typetab, LEX_CUR(lex)->id_name) ==
                    NULL) {
                    expr = true;
                    break;
                }
            case DECL_SPEC_STORAGE_CLASS:
            case DECL_SPEC_TYPE_SPEC_NO_ID:
            case DECL_SPEC_TYPE_QUALIFIER: {
                if (CCC_OK !=
                    (status =
                     par_declaration(lex, &stmt->for_params.decl1, false))) {
                    goto fail;
                }

                stmt->for_params.typetab = emalloc(sizeof(typetab_t));
                tt_init(stmt->for_params.typetab, lex->typetab);
                break;
            }
            default:
                expr = true;
            }
            if (expr && CCC_OK !=
                (status = par_expression(lex, &stmt->for_params.expr1))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);

        if (LEX_CUR(lex)->type != SEMI) {
            if (CCC_OK !=
                (status = par_expression(lex, &stmt->for_params.expr2))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);

        if (LEX_CUR(lex)->type != RPAREN) {
            if (CCC_OK !=
                (status = par_expression(lex, &stmt->for_params.expr3))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, RPAREN);

        // Enter new scope if there is one
        if (stmt->for_params.typetab != NULL) {
            lex->typetab = stmt->for_params.typetab;
        }

        if (CCC_OK != (status = par_statement(lex, &stmt->for_params.stmt))) {
            goto fail;
        }

        if (stmt->for_params.typetab != NULL) {
            lex->typetab = stmt->for_params.typetab->last;
        }

        break;

    default:
        assert(false);
    }

    *result = stmt;

fail:
    return status;
}

status_t par_jump_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt = NULL;

    switch (LEX_CUR(lex)->type) {
    case GOTO:
        LEX_ADVANCE(lex);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_GOTO);
        if (LEX_CUR(lex)->type != ID) {
            status = CCC_ESYNTAX;
            goto fail;
        }
        stmt->goto_params.label = LEX_CUR(lex)->id_name;
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, SEMI);
        break;

    case CONTINUE:
        LEX_ADVANCE(lex);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_CONTINUE);
        LEX_MATCH(lex, SEMI);
        break;
    case BREAK:
        LEX_ADVANCE(lex);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_BREAK);
        LEX_MATCH(lex, SEMI);
        break;

    case RETURN:
        LEX_ADVANCE(lex);
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_RETURN);

        if (LEX_CUR(lex)->type != SEMI
            && CCC_OK !=
            (status = par_expression(lex, &stmt->return_params.expr))) {
            goto fail;
        }
        LEX_MATCH(lex, SEMI);
        break;

    default:
        assert(false);
    }
    *result = stmt;

fail:
    return status;
}


status_t par_compound_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;
    stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_COMPOUND);
    tt_init(&stmt->compound.typetab, lex->typetab);
    // Add new typetab table to top of stack
    lex->typetab = &stmt->compound.typetab;

    LEX_MATCH(lex, LBRACE);
    while (LEX_CUR(lex)->type != RBRACE) {
        stmt_t *cur = NULL;
        bool is_decl = false;

        // Inside a compound statement, there can either be decls or statements
        switch (LEX_CUR(lex)->type) {
            // Cases for declaration specifier
        case DECL_SPEC_STORAGE_CLASS:
        case DECL_SPEC_TYPE_SPEC_NO_ID:
        case DECL_SPEC_TYPE_QUALIFIER:
            is_decl = true;
            break;
        case ID:
            // Can't be a decl if next token is a colon (its a label)
            if (LEX_NEXT(lex)->type == COLON) {
                break;
            }
            // Type specifier only if its a typedef name
            if (tt_lookup(lex->typetab, LEX_CUR(lex)->id_name) != NULL) {
                is_decl = true;
            }
            break;
        default:
            break;
        }

        if (is_decl) {
            cur = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_DECL);
            if(CCC_OK != (status = par_declaration(lex, &cur->decl, false))) {
                goto fail;
            }
            LEX_MATCH(lex, SEMI);
        } else {
            if (CCC_OK != (status = par_statement(lex, &cur))) {
                goto fail;
            }
        }
        sl_append(&stmt->compound.stmts, &cur->link);
    }
    LEX_ADVANCE(lex); // Consume RBRACE

    // Restore old type table to top of stack
    lex->typetab = stmt->compound.typetab.last;

    *result = stmt;

fail:
    return status;
}


status_t par_expression_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;

    if (LEX_CUR(lex)->type == SEMI) {
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_NOP);
    } else {
        stmt = ast_stmt_create(lex->tunit, &LEX_CUR(lex)->mark, STMT_EXPR);
        if (CCC_OK != (status = par_expression(lex, &stmt->expr.expr))) {
            goto fail;
        }
    }

    LEX_MATCH(lex, SEMI);

    *result = stmt;

fail:
    return status;
}
