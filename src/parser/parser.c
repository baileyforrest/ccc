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
 * TODO: proper destruction and and memory freeing on errors
 * TODO: Error reporting/recovery
 * TODO: Make sure all relevent members initialized on creation
 */

#include "parser.h"
#include "parser_priv.h"

#include <assert.h>
#include <stdlib.h>

#include "util/logger.h"
#include "util/util.h"

status_t parser_parse(lexer_t *lexer, len_str_t *file, trans_unit_t **result) {
    assert(lexer != NULL);
    assert(file != NULL);
    assert(result != NULL);
    status_t status = CCC_OK;
    lex_wrap_t lex;
    lex.lexer = lexer;
    LEX_ADVANCE(&lex);
    status = par_translation_unit(&lex, file, result);
fail:
    return status;
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
    default:
        assert(false);
    }
    return -1;
}

status_t par_translation_unit(lex_wrap_t *lex, len_str_t *file,
                              trans_unit_t **result) {
    status_t status = CCC_OK;
    trans_unit_t *tunit;
    ALLOC_NODE(tunit, trans_unit_t);
    tunit->path = file;
    sl_init(&tunit->gdecls, offsetof(gdecl_t, link));
    tt_init(&tunit->typetab, NULL);

    lex->typetab = &tunit->typetab; // Set top type table to translation units

    while (lex->cur.type != TOKEN_EOF) {
        gdecl_t *gdecl;
        if (CCC_OK != (status = par_external_declaration(lex, &gdecl))) {
            goto fail;
        }
        sl_append(&tunit->gdecls, &gdecl->link);
    }
    *result = tunit;
fail:
    return status;
}

status_t par_external_declaration(lex_wrap_t *lex, gdecl_t **result) {
    status_t status = CCC_OK;
    gdecl_t *gdecl = NULL;
    ALLOC_NODE(gdecl, gdecl_t);
    ALLOC_NODE(gdecl->decl, decl_t);
    sl_init(&gdecl->decl->decls, offsetof(decl_node_t, link));

    gdecl->decl->type = NULL;
    if (CCC_OK !=
        (status = par_declaration_specifiers(lex, &gdecl->decl->type))) {
        if (status != CCC_BACKTRACK) {
            goto fail;
        }
    }
    if (gdecl->decl->type == NULL) {
        logger_log(&lex->cur.mark,
                   "Data definition has no type or storage class",
                   LOG_WARN);
    }

    if (CCC_OK != (status = par_declarator_base(lex, gdecl->decl))) {
        // If the next character isn't a declarator, then its a type declaration
        if (status == CCC_BACKTRACK) {
            gdecl->type = GDECL_DECL;
            goto done;
        } else {
            goto fail;
        }
    }

    // Skip function pointers/declarations
    if (lex->cur.type == SEMI) {
        goto done;
    }

    decl_node_t *decl_node = sl_tail(&gdecl->decl->decls);
    if (decl_node->type->type == TYPE_FUNC) {
        if (CCC_OK != (status = par_function_definition(lex, gdecl))) {
            goto fail;
        }
    } else {
        if (CCC_OK != (status = par_declaration(lex, &gdecl->decl))) {
            goto fail;
        }
        LEX_MATCH(lex, SEMI); // Must end with semi colon
    }

done:
    *result = gdecl;
    return status;

fail:
    ast_gdecl_destroy(gdecl);
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

    /* TODO: Handle old style function signature
    while (lex->cur.type != RPAREN) {
        stmt_t *param = NULL;
        if (CCC_OK != (status = par_declaration(lex, &param))) {
            goto fail;
        }
        sl_append(&gdecl->fdefn.params, param->link);
    }
    */

    if (CCC_OK != (status = par_compound_statement(lex, &gdecl->fdefn.stmt))) {
        goto fail;
    }
fail:
    return status;
}

status_t par_declaration_specifiers(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;;
    *type = NULL; // Set to NULL so sub functions will allocate

    while (true) {
        switch (lex->cur.type) {
            // Storage class specifiers
        case AUTO:
        case REGISTER:
        case STATIC:
        case EXTERN:
        case TYPEDEF:
            if(CCC_OK != (status = par_storage_class_specifier(lex, type))) {
                goto fail;
            }
            break;

            // Type specifiers:
        case ID: {
            // Type specifier only if its a typedef name
            tt_key_t key = { &lex->cur.tab_entry->key, TT_TYPEDEF };
            if (tt_lookup(lex->typetab, &key) == NULL) {
                return CCC_BACKTRACK;
            }
        }
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
            if (CCC_OK != (status = par_type_specifier(lex, type))) {
                goto fail;
            }
            break;

            // Type qualitifiers
        case CONST:
        case VOLATILE:
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
        type_t *new_type;
        ALLOC_NODE(new_type, type_t);
        new_type->type = TYPE_MOD;
        new_type->dealloc = true;
        new_type->mod.base = *type;
        if (*type != NULL) {
            new_type->size = new_type->mod.base->size;
            new_type->align = new_type->mod.base->align;
        }
        *type = new_type;
    }
    type_mod_t tmod;
    switch (lex->cur.type) {
    case AUTO:     tmod = TMOD_AUTO;     break;
    case REGISTER: tmod = TMOD_REGISTER; break;
    case STATIC:   tmod = TMOD_STATIC;   break;
    case EXTERN:   tmod = TMOD_EXTERN;   break;
    case TYPEDEF:  tmod = TMOD_TYPEDEF;  break;
    default:
        assert(false);
    }
    if ((*type)->mod.type_mod & tmod) {
        snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                 "Duplicate storage class specifer: %s",
                 ast_type_mod_str(tmod));
        logger_log(&lex->cur.mark, logger_fmt_buf, LOG_WARN);
    }

    (*type)->mod.type_mod |= tmod;
fail:
    return status;
}

status_t par_type_specifier(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;

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

    // We found a node that is type, so we cannot have two
    if (*end_node != NULL) {
        logger_log(&lex->cur.mark, "Multiple type specifers",
                   LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    switch (lex->cur.type) {
    case ID: { // typedef name
        // Type specifier only if its a typedef name
        tt_key_t key = { &lex->cur.tab_entry->key , TT_TYPEDEF };
        typetab_entry_t *entry = tt_lookup(lex->typetab, &key);
        assert(entry != NULL); // Must be checked before calling
        *end_node = entry->type;
        break;
    }
        // Primitive types
    case VOID:   *end_node = tt_void;   break;
    case CHAR:   *end_node = tt_char;   break;
    case SHORT:  *end_node = tt_short;  break;
    case INT:    *end_node = tt_int;    break;
    case LONG:   *end_node = tt_long;   break;
    case FLOAT:  *end_node = tt_float;  break;
    case DOUBLE: *end_node = tt_double; break;

        // Don't give a base type for signed/unsigned. No base type defaults to
        // int
    case SIGNED:
    case UNSIGNED: {
        type_mod_t mod = lex->cur.type == SIGNED ? TMOD_SIGNED : TMOD_UNSIGNED;
        if (mod_node == NULL) {
            ALLOC_NODE(mod_node, type_t);
            mod_node->type = TYPE_MOD;
            mod_node->dealloc = true;
            mod_node->mod.base = *type;
            mod_node->size = (*type)->size;
            mod_node->align = (*type)->align;
            *type = mod_node;
        }

        if (mod_node->mod.type_mod & mod) {
            snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                     "Duplicate type specifer: %s",
                     ast_type_mod_str(mod));
            logger_log(&lex->cur.mark, logger_fmt_buf, LOG_ERR);
            status = CCC_ESYNTAX;
        }
        mod_node->mod.type_mod |= mod;

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

    basic_type_t btype;

    switch (lex->cur.type) {
    case STRUCT: btype = TYPE_STRUCT; break;
    case UNION:  btype = TYPE_UNION;  break;
    case ENUM:   btype = TYPE_ENUM;   break;
    default:
        assert(false);
    }
    LEX_ADVANCE(lex);

    len_str_t *name = NULL;
    typetab_entry_t *entry = NULL;
    if (lex->cur.type == ID) {
        name = &lex->cur.tab_entry->key;
        tt_key_t key = { name, TT_COMPOUND };
        entry = tt_lookup(lex->typetab, &key);

        LEX_ADVANCE(lex);

        // Not a definition
        if (lex->cur.type != LBRACE && entry != NULL) {
            if (entry->type->type != btype) {
                snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                         "Incorrect type specifer %s. Expected: %s.",
                         ast_basic_type_str(entry->type->type),
                         ast_basic_type_str(btype));
                logger_log(&lex->cur.mark, logger_fmt_buf, LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail;
            }
            *type = entry->type;
            return CCC_OK;
        }
    }

    type_t *new_type = NULL;
    if (entry == NULL) { // Allocate a new type if it doesn't exist
        ALLOC_NODE(new_type, type_t);
        new_type->type = btype;
        new_type->size = 0; // Mark as unitialized
        new_type->align = 0;
        new_type->dealloc = false;
        if (btype == TYPE_ENUM) {
            sl_init(&new_type->enum_ids, offsetof(enum_id_t, link));
        } else {
            sl_init(&new_type->struct_decls, offsetof(struct_decl_t, link));
        }
    }

    if (lex->cur.type != LBRACE) {
        if (name != NULL) { // Create a new declaration in the table
            if (entry == NULL) {
                if (CCC_OK != (status = tt_insert(lex->typetab, new_type,
                                                  TT_COMPOUND, name, &entry))) {
                    goto fail;
                }
            }

            *type = new_type;
            return CCC_OK;
        } else { // Can't have a compound type without a name or definition
            logger_log(&lex->cur.mark,
                       "Compound type without name or definition", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }
    }

    if (btype == TYPE_ENUM) {
        if (CCC_OK != (status = par_enumerator_list(lex, new_type))) {
            goto fail;
        }
    } else { // struct/union
        // Must match at least one struct declaration
        if (CCC_OK != (status = par_struct_declaration(lex, new_type))) {
            goto fail;
        }
        while (CCC_BACKTRACK !=
               (status = par_struct_declaration(lex, new_type))) {
            if (status != CCC_OK) {
                goto fail;
            }
        }
    }
    status = CCC_OK;
    LEX_MATCH(lex, RBRACE);

    // Add a new named compound type into the type table
    if (name != NULL && entry == NULL) {
        if (CCC_OK !=
            (status =
             tt_insert(lex->typetab, new_type, TT_COMPOUND, name, &entry))) {
            goto fail;
        }
    }

    *type = new_type;
    return status;

fail:
    ast_type_destroy(new_type, NO_OVERRIDE);
    return status;
}

status_t par_struct_declaration(lex_wrap_t *lex, type_t *type) {
    status_t status = CCC_OK;
    type_t *decl_type = NULL;
    if (CCC_OK != (status = par_specifier_qualifiers(lex, &type))) {
        if (type == NULL || status != CCC_BACKTRACK) {
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

status_t par_specifier_qualifiers(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;;
    *type = NULL; // Set to NULL so sub functions will allocate

    while (true) {
        switch (lex->cur.type) {
            // Type specifiers:
        case ID: {
            // Type specifier only if its a typedef name
            tt_key_t key = { &lex->cur.tab_entry->key , TT_TYPEDEF };
            if (tt_lookup(lex->typetab, &key) == NULL) {
                return CCC_BACKTRACK;
            }
        }
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
            if(CCC_OK != (status = par_type_specifier(lex, type))) {
                goto fail;
            }
            break;

            // Type qualitifiers
        case CONST:
        case VOLATILE:
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

status_t par_struct_declarator_list(lex_wrap_t *lex, type_t *base,
                                    type_t *decl_type) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_struct_declarator(lex, base, decl_type))) {
        goto fail;
    }
    while (lex->cur.type == COMMA) {
        if (CCC_OK != (status = par_struct_declarator(lex, base, decl_type))) {
            goto fail;
        }
    }

fail:
    return status;
}

status_t par_struct_declarator(lex_wrap_t *lex, type_t *base,
                               type_t *decl_type) {
    status_t status = CCC_OK;
    struct_decl_t *node;
    ALLOC_NODE(node, struct_decl_t);
    node->bf_bits = NULL;
    ALLOC_NODE(node->decl, decl_t);
    node->decl->type = decl_type;
    sl_init(&node->decl->decls, offsetof(decl_node_t, link));

    if (CCC_OK != (status = par_declarator_base(lex, node->decl))) {
        goto fail;
    }

    if (lex->cur.type == COLON) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_expression(lex, NULL, &node->bf_bits))) {
            goto fail;
        }
    }

    decl_node_t *dnode = sl_tail(&node->decl->decls);
    base->align = MAX(base->align, dnode->type->align);

    // TODO: Handle bitfields
    if (base->type == TYPE_STRUCT) {
        base->size += dnode->type->size;
    } else { // base->type == TYPE_UNION
        base->size = MAX(base->size, dnode->type->size);
    }

    // Update base type
    sl_append(&base->struct_decls, &node->link);

    return status;

fail:
    ast_struct_decl_destroy(node);
    return status;
}

status_t par_declarator_base(lex_wrap_t *lex, decl_t *decl) {
    switch (lex->cur.type) {
    case STAR:
    case ID:
    case LPAREN:
        break;
    default:
        return CCC_BACKTRACK;
    }
    status_t status = CCC_OK;
    decl_node_t *decl_node;
    ALLOC_NODE(decl_node, decl_node_t);
    decl_node->type = decl->type;
    decl_node->id = NULL;
    decl_node->expr = NULL;
    if (CCC_OK != (status = par_declarator(lex, decl->type, decl_node))) {
        goto fail;
    }
    sl_append(&decl->decls, &decl_node->link);

    return status;

fail:
    ast_decl_node_destroy(decl_node);
    return status;
}

// TODO: Handle abstract-declarator
status_t par_declarator(lex_wrap_t *lex, type_t *base, decl_node_t *decl_node) {
    status_t status = CCC_OK;

    while (lex->cur.type == STAR) {
        if (CCC_OK != (status = par_pointer(lex, &decl_node->type))) {
            goto fail;
        }
    }
    if (CCC_OK != (status = par_direct_declarator(lex, decl_node, base))) {
        goto fail;
    }

    return status;

fail:
    ast_decl_node_destroy(decl_node);
    return status;
}

status_t par_pointer(lex_wrap_t *lex, type_t **mod) {
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
        ALLOC_NODE(new_type, type_t);
    }
    new_type->type = TYPE_PTR;
    new_type->size = PTR_SIZE;
    new_type->align = PTR_ALIGN;
    new_type->dealloc = true;
    new_type->ptr.base = *mod;
    new_type->ptr.type_mod = mods;

    *mod = new_type;
    return status;

fail:
    // Nothing to clean up here
    return status;
}

status_t par_type_qualifier(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;
    type_mod_t mod;
    switch (lex->cur.type) {
    case CONST:    mod = TMOD_CONST;    break;
    case VOLATILE: mod = TMOD_VOLATILE; break;
    default:
        return CCC_BACKTRACK;
    }

    // Check first node for mod node
    type_t *mod_node;
    if (*type != NULL && (*type)->type == TYPE_MOD) {
        mod_node = *type;
    } else {
        ALLOC_NODE(mod_node, type_t);
        mod_node->type = TYPE_MOD;
        mod_node->dealloc = true;
        mod_node->mod.base = *type;
        *type = mod_node;
        if (mod_node->mod.base != NULL) {
            mod_node->size = mod_node->mod.base->size;
            mod_node->align = mod_node->mod.base->align;
        } else {
            mod_node->size = 0;
            mod_node->align = 0;
        }
    }
    mod_node->mod.type_mod |= mod;

    return status;

fail:
    // Nothing to clean up here
    return status;
}

status_t par_direct_declarator(lex_wrap_t *lex, decl_node_t *node,
                               type_t *base) {
    status_t status = CCC_OK;

    if (lex->cur.type == LPAREN) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_declarator(lex, base, node))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);

        type_t *paren_type;
        ALLOC_NODE(paren_type, type_t);
        paren_type->type = TYPE_PAREN;
        paren_type->dealloc = true;
        paren_type->size = node->type->size;
        paren_type->align = node->type->align;

        node->type = paren_type;
    } else if (lex->cur.type == ID) {
        node->id = &lex->cur.tab_entry->key;
        LEX_ADVANCE(lex);
    }

    bool done = false;
    while (!done) {
        switch (lex->cur.type) {
        case LBRACK: // Array dimension
            LEX_ADVANCE(lex);
            type_t *arr_type;
            ALLOC_NODE(arr_type, type_t);
            arr_type->type = TYPE_ARR;
            arr_type->dealloc = true;
            arr_type->align = node->type->align;
            arr_type->size = 0;
            arr_type->arr.base = node->type;
            arr_type->arr.len = NULL;

            node->type = arr_type;

            if (lex->cur.type == RBRACK) {
                LEX_ADVANCE(lex);
            } else {
                if (CCC_OK !=
                    (status = par_expression(lex, NULL, &arr_type->arr.len))) {
                    goto fail;
                }
                LEX_MATCH(lex, RBRACK);
            }
            break;
        case LPAREN:
            LEX_ADVANCE(lex);
            type_t *func_type;
            ALLOC_NODE(func_type, type_t);
            func_type->type = TYPE_FUNC;
            func_type->dealloc = true;
            func_type->align = PTR_ALIGN;
            func_type->size = PTR_SIZE;
            func_type->func.type = node->type;
            func_type->func.varargs = false;
            sl_init(&func_type->func.params, offsetof(stmt_t, link));
            node->type = func_type;

            /* TODO: Support old decl syntax
            if (lex->cur.type == ID) {
                while (lex->cur.type == ID) {
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
    return status;
}

status_t par_non_binary_expression(lex_wrap_t *lex, bool *is_unary,
                                   expr_t **result) {
    status_t status = CCC_OK;

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
        if (CCC_OK != (status = par_unary_expression(lex, result))) {
            goto fail;
        }
        unary = true;
        break;

        // Primary expressions
    case ID:
    case STRING:
    case INTLIT:
    case FLOATLIT:
        if (CCC_OK != (status = par_primary_expression(lex, result))) {
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

            // Type qualitifiers
        case CONST:
        case VOLATILE:
            if (CCC_OK != (status = par_cast_expression(lex, true, result))) {
                goto fail;
            }
            break;

            // Parens
        case ID: {
            // Type specifier only if its a typedef name
            tt_key_t key = { &lex->cur.tab_entry->key, TT_TYPEDEF };
            if (tt_lookup(lex->typetab, &key) != NULL) {
                if (CCC_OK !=
                    (status = par_cast_expression(lex, true, result))) {
                    goto fail;
                }
                break;
            }
        }
        default: {
            expr_t *expr;
            ALLOC_NODE(expr, expr_t);
            expr->type = EXPR_PAREN;
            if (CCC_OK !=
                (status = par_expression(lex, NULL, &expr->paren_base))) {
                goto fail;
            }
            primary = true;
            unary = true;
            LEX_MATCH(lex, RPAREN);
            *result = expr;
            break;
        }
        }
        break;
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
        case LPAREN: {
            expr_t *base = *result;
            if (CCC_OK !=
                (status = par_postfix_expression(lex, base, result))) {
                goto fail;
            }
            break;
        }
        default:
            break;
        }
    }

    *is_unary = unary;
fail:
    return status;
}

status_t par_expression(lex_wrap_t *lex, expr_t *left, expr_t **result) {
    status_t status = CCC_OK;
    bool unary1;

    if (left == NULL) { // Only search for first operand if not provided
        if (CCC_OK !=
            (status = par_non_binary_expression(lex, &unary1, &left))) {
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
                if (CCC_OK !=
                    (status = par_assignment_expression(lex, left, result))) {
                    goto fail;
                }

                return CCC_OK;
            default:
                break;
            }
        }
    }

    bool new_left = true;
    while (new_left) {
        new_left = false;
        oper_t op1 = OP_NOP;

        switch (lex->cur.type) {
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
            expr_t *new_expr_left;
            ALLOC_NODE(new_expr_left, expr_t);
            new_expr_left->type = EXPR_COND;
            new_expr_left->cond.expr1 = left;

            LEX_ADVANCE(lex);
            if (CCC_OK !=
                par_expression(lex, NULL, &new_expr_left->cond.expr2)) {
                goto fail;
            }
            LEX_MATCH(lex, COLON);
            if (CCC_OK !=
                par_expression(lex, NULL, &new_expr_left->cond.expr3)) {
                goto fail;
            }
            left = new_expr_left;
            new_left = true;
            break;
        }
        default:
            *result = left;
            return CCC_OK;
        }

        // Consume the operation
        if (op1 != OP_NOP) {
            LEX_ADVANCE(lex);
        }

        if (new_left) {
            continue;
        }

        expr_t *right;
        bool unary2;
        if (CCC_OK !=
            (status = par_non_binary_expression(lex, &unary2, &right))) {
            goto fail;
        }

        oper_t op2;
        switch (lex->cur.type) {
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

        case COND: { // Cond has lowest precedence
            LEX_ADVANCE(lex);
            expr_t *new_node;
            ALLOC_NODE(new_node, expr_t);
            new_node->type = EXPR_BIN;
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;
            new_node->bin.expr2 = right;
            expr_t *cond_node;
            ALLOC_NODE(cond_node, expr_t);
            cond_node->type = EXPR_COND;
            cond_node->cond.expr1 = new_node;
            if (CCC_OK != par_expression(lex, NULL, &cond_node->cond.expr2)) {
                goto fail;
            }
            LEX_MATCH(lex, COLON);
            if (CCC_OK != par_expression(lex, NULL, &cond_node->cond.expr3)) {
                goto fail;
            }
            *result = new_node;
            return CCC_OK;
        }
        default: { // Not binary 2
            expr_t *new_node;
            ALLOC_NODE(new_node, expr_t);
            new_node->type = EXPR_BIN;
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;
            new_node->bin.expr2 = right;
            *result = new_node;
            return CCC_OK;
        }
        }

        if (par_get_binary_prec(op1) >= par_get_binary_prec(op2)) {
            expr_t *new_node;
            ALLOC_NODE(new_node, expr_t);
            new_node->type = EXPR_BIN;
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;
            new_node->bin.expr2 = right;
            new_left = true;
            left = new_node;
        } else {
            expr_t *new_node;
            ALLOC_NODE(new_node, expr_t);
            new_node->type = EXPR_BIN;
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;

            if (CCC_OK !=
                (status = par_expression(lex, right, &new_node->bin.expr2))) {
                goto fail;
            }

            *result = new_node;
            return CCC_OK;
        }
    }

fail:
    return status;
}

status_t par_unary_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *base;

    switch (lex->cur.type) {
        // Primary expressions
    case ID:
    case STRING:
    case INTLIT:
    case FLOATLIT:
        if (CCC_OK != (status = par_primary_expression(lex, &base))) {
            goto fail;
        }
        if (CCC_OK != (status = par_postfix_expression(lex, base, result))) {
            goto fail;
        }
        break;

    case INC:
    case DEC: {
        ALLOC_NODE(base, expr_t);
        oper_t op = lex->cur.type == INC ? OP_PREINC : OP_PREDEC;
        LEX_ADVANCE(lex);
        base->type = EXPR_UNARY;
        base->unary.op = op;
        if (CCC_OK != (status = par_unary_expression(lex, &base->unary.expr))) {
            goto fail;
        }
        *result = base;
        return CCC_OK;
    }

    case SIZEOF:
        ALLOC_NODE(base, expr_t);
        LEX_ADVANCE(lex);
        base->type = EXPR_SIZEOF;
        if (CCC_BACKTRACK != (status = par_unary_expression(lex, result))) {
            if (status != CCC_OK) {
                goto fail;
            }
            base->sizeof_params.type = NULL;
            base->sizeof_params.expr = *result;
            *result = base;
            return CCC_OK;
        }

        base->sizeof_params.expr = NULL;
        if (CCC_OK !=
            (status = par_type_name(lex, &base->sizeof_params.type))) {
            goto fail;
        }

        *result = base;
        return CCC_OK;

    case BITAND:
    case STAR:
    case PLUS:
    case MINUS:
    case BITNOT:
    case LOGICNOT: {
        ALLOC_NODE(base, expr_t);
        oper_t op;
        switch (lex->cur.type) {
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
        base->type = EXPR_UNARY;
        base->unary.op = op;
        if (CCC_OK !=
            (status = par_cast_expression(lex, false, &base->unary.expr))) {
            goto fail;
        }

        *result = base;
        return CCC_OK;
    }
    default:
        return CCC_BACKTRACK;
    }

fail:
    free(base);
    return status;
}

status_t par_cast_expression(lex_wrap_t *lex, bool skip_paren,
                             expr_t **result) {
    status_t status = CCC_OK;
    if (!skip_paren && lex->cur.type != LPAREN) {
        return par_unary_expression(lex, result);
    }
    if (!skip_paren) {
        LEX_ADVANCE(lex);
    }
    decl_t *type = NULL;
    if (CCC_OK != (status = par_type_name(lex, &type))) {
        goto fail;
    }
    LEX_MATCH(lex, RPAREN);

    expr_t *expr;
    ALLOC_NODE(expr, expr_t);
    expr->type = EXPR_CAST;
    expr->cast.cast = type;
    expr->cast.base = NULL;
    if (CCC_OK !=
        (status = par_cast_expression(lex, false, &expr->cast.base))) {
        goto fail;
    }
    *result = expr;
fail:
    return status;
}

/**
 * Parses postfix expression after the primary expression part
 */
status_t par_postfix_expression(lex_wrap_t *lex, expr_t *base,
                                expr_t **result) {
    status_t status = CCC_OK;
    expr_t *expr = NULL;

    while (base != NULL) {
        switch (lex->cur.type) {
        case LBRACK:
            LEX_ADVANCE(lex);
            ALLOC_NODE(expr, expr_t);
            expr->type = EXPR_BIN;
            expr->bin.op = OP_ARR_ACC;
            expr->bin.expr1 = base;

            if (CCC_OK !=
                (status = par_expression(lex, NULL, &expr->bin.expr2))) {
                goto fail;
            }
            LEX_MATCH(lex, RBRACK);
            base = expr;
            break;

        case LPAREN:
            LEX_ADVANCE(lex);
            ALLOC_NODE(expr, expr_t);
            expr->type = EXPR_CALL;
            expr->call.func = base;
            sl_init(&expr->call.params, offsetof(expr_t, link));

            while (lex->cur.type != RPAREN) {
                expr_t *param;
                if (CCC_OK != (status = par_expression(lex, NULL, &param))) {
                    goto fail;
                }
                sl_append(&expr->call.params, &param->link);
                LEX_MATCH(lex, COMMA);
            }
            LEX_ADVANCE(lex);
            base = expr;
            break;

        case DOT:
        case DEREF: {
            oper_t op = lex->cur.type == DOT ? OP_DOT : OP_ARROW;
            LEX_ADVANCE(lex);
            if (lex->cur.type != ID) {
                goto fail;
            }
            ALLOC_NODE(expr, expr_t);
            expr->type = EXPR_MEM_ACC;
            expr->mem_acc.base = base;
            expr->mem_acc.name = &lex->cur.tab_entry->key;
            expr->mem_acc.op = op;
            LEX_ADVANCE(lex);
            base = expr;
            break;
        }

        case INC:
        case DEC: {
            oper_t op = lex->cur.type == INC ? OP_POSTINC : OP_POSTDEC;
            LEX_ADVANCE(lex);
            ALLOC_NODE(expr, expr_t);
            expr->type = EXPR_UNARY;
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

/**
 * Parses assignment after the assignment operator
 */
status_t par_assignment_expression(lex_wrap_t *lex, expr_t *left,
                                   expr_t **result) {
    status_t status = CCC_OK;
    oper_t op;
    switch (lex->cur.type) {
    case EQ:       op = OP_NOP;    break;
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
        return CCC_ESYNTAX;
    }
    LEX_ADVANCE(lex);
    expr_t *expr;
    ALLOC_NODE(expr, expr_t);
    expr->type = EXPR_ASSIGN;
    expr->assign.dest = left;
    expr->assign.op = op;

    if (CCC_OK != (status = par_expression(lex, NULL, &expr->assign.expr))) {
        goto fail;
    }

    *result = expr;
    return status;

fail:
    free(expr);
    return status;
}

// Excludes parens because they need to be diferrentiated from casts
status_t par_primary_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *base;
    ALLOC_NODE(base, expr_t);

    switch (lex->cur.type) {
    case ID: {
        base->type = EXPR_VAR;
        base->var_id = &lex->cur.tab_entry->key;
        LEX_ADVANCE(lex);
        break;
    }
    case STRING: {
        base->type = EXPR_CONST_STR;
        base->const_val.str_val = &lex->cur.tab_entry->key;
        type_t *type;
        ALLOC_NODE(type, type_t);
        type->type = TYPE_ARR;
        type->size = base->const_val.str_val->len + 1;
        type->align = 1;
        type->dealloc = true;
        type->arr.base = tt_char;
        type->arr.len = NULL;
        LEX_ADVANCE(lex);
        base->const_val.type = type;
        break;
    }
    case INTLIT: {
        base->type = EXPR_CONST_INT;
        base->const_val.int_val = lex->cur.int_params.int_val;
        type_t *type;
        if (lex->cur.int_params.hasLL) {
            type = tt_long; // TODO: Add long long support
        } else if (lex->cur.int_params.hasL) {
            type = tt_long;
        } else {
            type = tt_int;
        }

        if (lex->cur.int_params.hasU) {
            type_t *type_mod;
            ALLOC_NODE(type_mod, type_t);
            type_mod->type = TYPE_MOD;
            type_mod->size = type->size;
            type_mod->align = type->align;
            type_mod->dealloc = true;
            type_mod->mod.type_mod = TMOD_UNSIGNED;
            type_mod->mod.base = type;
            type = type_mod;
        }
        base->const_val.type = type;
        LEX_ADVANCE(lex);
        break;
    }
    case FLOATLIT: {
        base->type = EXPR_CONST_FLOAT;
        base->const_val.float_val = lex->cur.float_params.float_val;
        base->const_val.type =
            lex->cur.float_params.hasF ? tt_float : tt_double;
        LEX_ADVANCE(lex);
        break;
    }
    default:
        return CCC_ESYNTAX;
    }

    *result = base;
fail:
    return status;
}

status_t par_type_name(lex_wrap_t *lex, decl_t **result) {
    status_t status = CCC_OK;
    type_t *base = NULL;
    if (CCC_OK != (status = par_specifier_qualifiers(lex, &base))) {
        if (base == NULL || status != CCC_BACKTRACK) {
            goto fail;
        }
    }
    decl_t *decl;
    ALLOC_NODE(decl, decl_t);
    decl->type = base;
    sl_init(&decl->decls, offsetof(decl_node_t, link));

    if (CCC_BACKTRACK != (status = par_declarator_base(lex, decl))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;

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

    if (lex->cur.type != ELIPSE) {
        return CCC_OK;
    }
    LEX_ADVANCE(lex);
    func->func.varargs = true;

fail:
    return status;
}

status_t par_parameter_list(lex_wrap_t *lex, type_t *func) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_parameter_declaration(lex, func))) {
        goto fail;
    }
    while (CCC_BACKTRACK != (status = par_parameter_declaration(lex, func))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;
fail:
    return status;
}

status_t par_parameter_declaration(lex_wrap_t *lex, type_t *func) {
    status_t status = CCC_OK;
    type_t *type = NULL;
    if (CCC_OK != (status = par_declaration_specifiers(lex, &type))) {
        if (type == NULL || status != CCC_BACKTRACK) {
            goto fail;
        }
    }

    decl_t *decl;
    ALLOC_NODE(decl, decl_t);
    decl->type = type;
    sl_init(&decl->decls, offsetof(decl_node_t, link));

    if (CCC_BACKTRACK != (status = par_declarator_base(lex, decl))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;
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
    while (lex->cur.type == COMMA) {
        if (CCC_OK != (status = par_enumerator(lex, type))) {
            goto fail;
        }
    }

fail:
    return status;
}

status_t par_enumerator(lex_wrap_t *lex, type_t *type) {
    status_t status = CCC_OK;
    if (lex->cur.type != ID) {
        status = CCC_ESYNTAX;
        goto fail;
    }
    enum_id_t *node;
    ALLOC_NODE(node, enum_id_t);
    node->id = &lex->cur.tab_entry->key;
    LEX_ADVANCE(lex);

    if (lex->cur.type == EQ) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_expression(lex, NULL, &node->val))) {
            goto fail;
        }
    }
    sl_append(&type->enum_ids, &node->link);
    return status;
fail:
    free(node);
    return status;
}

status_t par_declaration(lex_wrap_t *lex, decl_t **decl) {
    status_t status = CCC_OK;
    if (*decl == NULL) {
        ALLOC_NODE(*decl, decl_t);
        (*decl)->type = NULL;
        sl_init(&(*decl)->decls, offsetof(decl_node_t, link));

        // Must match at least one declaration specifier
        if (CCC_OK !=
            (status = par_declaration_specifiers(lex, &(*decl)->type))) {
            if ((*decl)->type == NULL || status != CCC_BACKTRACK) {
                goto fail;
            }
        }
    }

    while (CCC_BACKTRACK != (status = par_init_declarator(lex, *decl))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;

fail:
    return status;
}

status_t par_init_declarator(lex_wrap_t *lex, decl_t *decl) {
    status_t status = CCC_OK;
    bool is_typedef = decl->type->type == TYPE_MOD &&
        (decl->type->mod.type_mod & TMOD_TYPEDEF);

    if (CCC_OK != (status = par_declarator_base(lex, decl))) {
        goto fail;
    }
    decl_node_t *decl_node = sl_tail(&decl->decls);

    // Add typedefs to the typetable on the top of the stack
    if (is_typedef) {
        if (CCC_OK !=
            (status = tt_insert(lex->typetab, decl_node->type,
                                TT_TYPEDEF, decl_node->id, NULL))) {
            goto fail;
        }
    }

    if (lex->cur.type == ASSIGN) {
        if (is_typedef) {
            // TODO: report error
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
    if (lex->cur.type != LBRACK) {
        return par_expression(lex, NULL, result);
    }
    LEX_ADVANCE(lex);
    if (CCC_OK != (status = par_initializer_list(lex, result))) {
        goto fail;
    }
    LEX_MATCH(lex, RBRACK);
fail:
    return status;
}

status_t par_initializer_list(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *expr;
    ALLOC_NODE(expr, expr_t);
    expr->type = EXPR_INIT_LIST;
    sl_init(&expr->init_list.exprs, offsetof(expr_t, link));
    expr_t *cur = NULL;
    if (CCC_OK != (status = par_initializer(lex, &cur))) {
        goto fail;
    }
    sl_append(&expr->init_list.exprs, &cur->link);

    while (lex->cur.type == COMMA) {
        LEX_ADVANCE(lex);
        if (lex->cur.type == RBRACK) {
            break;
        }
        if (CCC_OK != (status = par_initializer(lex, &cur))) {
            goto fail;
        }
        sl_append(&expr->init_list.exprs, &cur->link);
    }
    *result = expr;
fail:
    return status;
}


status_t par_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;

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

        // Type qualitifiers
    case CONST:
    case VOLATILE: {
        stmt_t *stmt;
        ALLOC_NODE(stmt, stmt_t);
        stmt->type = STMT_DECL;
        stmt->decl = NULL;
        if(CCC_OK != (status = par_declaration(lex, &stmt->decl))) {
            goto fail;
        }
        LEX_MATCH(lex, SEMI);
        *result = stmt;
        return CCC_OK;
    }
    case ID: {
        // Type specifier only if its a typedef name
        tt_key_t key = { &lex->cur.tab_entry->key, TT_TYPEDEF };
        if (tt_lookup(lex->typetab, &key) != NULL) {
            stmt_t *stmt;
            ALLOC_NODE(stmt, stmt_t);
            stmt->type = STMT_DECL;
            stmt->decl = NULL;
            if (CCC_OK != (status = par_declaration(lex, &stmt->decl))) {
                goto fail;
            }

            *result = stmt;
            return CCC_OK;
        }
    }
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

    case SEMI: // noop
        LEX_ADVANCE(lex);
        ALLOC_NODE(*result, stmt_t);
        (*result)->type = STMT_NOP;
        return CCC_OK;
    case RBRACK:
        return par_compound_statement(lex, result);
    default:
        return par_expression_statement(lex, result);
    }
fail:
    return status;
}

status_t par_labeled_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;
    ALLOC_NODE(stmt, stmt_t);
    switch (lex->cur.type) {
    case ID:
        stmt->type = STMT_LABEL;
        stmt->label.label = &lex->cur.tab_entry->key;
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, COLON);
        if (CCC_OK != (status = par_statement(lex, &stmt->label.stmt))) {
            goto fail;
        }
        break;
    case CASE:
        LEX_ADVANCE(lex);
        stmt->type = STMT_CASE;

        if (CCC_OK !=
            (status = par_expression(lex, NULL, &stmt->case_params.val))) {
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
        stmt->type = STMT_DEFAULT;
        if (CCC_OK !=
            (status = par_statement(lex, &stmt->default_params.stmt))) {
            goto fail;
        }
        break;
    default:
        return CCC_ESYNTAX;
    }

    *result = stmt;
    return status;
fail:
    free(stmt);
    return status;
}

status_t par_selection_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;
    ALLOC_NODE(stmt, stmt_t);
    switch (lex->cur.type) {
    case IF:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        stmt->type = STMT_IF;
        if (CCC_OK !=
            (status = par_expression(lex, NULL, &stmt->if_params.expr))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        if (CCC_OK !=
            (status = par_statement(lex, &stmt->if_params.true_stmt))) {
            goto fail;
        }
        if (lex->cur.type == ELSE) {
            LEX_ADVANCE(lex);
            if (CCC_OK !=
                (status = par_statement(lex, &stmt->if_params.false_stmt))) {
                goto fail;
            }
        } else {
            stmt->if_params.false_stmt = NULL;
        }
        break;

    case SWITCH:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        stmt->type = STMT_SWITCH;
        if (CCC_OK !=
            (status = par_expression(lex, NULL, &stmt->switch_params.expr))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        if (CCC_OK !=
            (status = par_statement(lex, &stmt->switch_params.stmt))) {
            goto fail;
        }
        break;
    default:
        return CCC_ESYNTAX;
    }

    *result = stmt;
    return status;
fail:
    return status;
}

status_t par_iteration_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;
    ALLOC_NODE(stmt, stmt_t);

    switch (lex->cur.type) {
    case DO:
        LEX_ADVANCE(lex);
        stmt->type = STMT_DO;
        if (CCC_OK != (status = par_statement(lex, &stmt->do_params.stmt))) {
            goto fail;
        }
        LEX_MATCH(lex, WHILE);
        LEX_MATCH(lex, LPAREN);
        if (CCC_OK !=
            (status = par_expression(lex, NULL, &stmt->do_params.expr))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);
        LEX_MATCH(lex, SEMI);
        break;
    case WHILE:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        stmt->type = STMT_WHILE;
        if (CCC_OK !=
            (status = par_expression(lex, NULL, &stmt->while_params.expr))) {
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
        stmt->type = STMT_FOR;
        if (lex->cur.type != SEMI) {
            if (CCC_OK !=
                (status = par_expression(lex, NULL, &stmt->for_params.expr1))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);
        if (lex->cur.type != SEMI) {
            if (CCC_OK !=
                (status = par_expression(lex, NULL, &stmt->for_params.expr2))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);
        if (lex->cur.type != SEMI) {
            if (CCC_OK !=
                (status = par_expression(lex, NULL, &stmt->for_params.expr3))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);
        LEX_MATCH(lex, RPAREN);
        if (CCC_OK != (status = par_statement(lex, &stmt->for_params.stmt))) {
            goto fail;
        }
        break;
    default:
        return CCC_ESYNTAX;
    }

    *result = stmt;
fail:
    return status;
}

status_t par_jump_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;
    ALLOC_NODE(stmt, stmt_t);

    switch (lex->cur.type) {
    case GOTO:
        LEX_ADVANCE(lex);
        stmt->type = STMT_GOTO;
        if (lex->cur.type != ID) {
            status = CCC_ESYNTAX;
            goto fail;
        }
        stmt->goto_params.label = &lex->cur.tab_entry->key;
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, SEMI);
        break;
    case CONTINUE:
    case BREAK:
        LEX_ADVANCE(lex);
        stmt->type = STMT_BREAK;
        if (lex->cur.type == CONTINUE) {
            stmt->continue_params.parent = NULL;
        } else {
            stmt->break_params.parent = NULL;
        }
        LEX_MATCH(lex, SEMI);
        break;
    case RETURN:
        LEX_ADVANCE(lex);
        stmt->type = STMT_RETURN;
        if (CCC_OK !=
            (status = par_expression(lex, false, &stmt->return_params.expr))) {
            goto fail;
        }
        LEX_MATCH(lex, SEMI);
        break;
    default:
        return CCC_ESYNTAX;
    }
    *result = stmt;
fail:
    return status;
}


status_t par_compound_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;
    ALLOC_NODE(stmt, stmt_t);

    stmt->type = STMT_COMPOUND;
    sl_init(&stmt->compound.stmts, offsetof(stmt_t, link));
    if (CCC_OK != (tt_init(&stmt->compound.typetab, lex->typetab))) {
        goto fail;
    }
    // Add new typetab table to top of stack
    lex->typetab = &stmt->compound.typetab;

    LEX_MATCH(lex, LBRACE);
    while (lex->cur.type != RBRACE) {
        stmt_t *cur = NULL;
        if (CCC_OK != (status = par_statement(lex, &cur))) {
            goto fail;
        }
        sl_append(&stmt->compound.stmts, &cur->link);
    }
    LEX_ADVANCE(lex);

    // Remove new typetab table to top of stack
    lex->typetab = stmt->compound.typetab.last;

    *result = stmt;
fail:
    return status;
}


status_t par_expression_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;
    ALLOC_NODE(stmt, stmt_t);

    if (lex->cur.type == SEMI) {
        stmt->type = STMT_NOP;
    } else {
        stmt->type = STMT_EXPR;
        if (CCC_OK != (status = par_expression(lex, NULL, &stmt->expr.expr))) {
            goto fail;
        }
    }

    LEX_MATCH(lex, SEMI);

    *result = stmt;
fail:
    return status;
}
