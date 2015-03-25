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
 *
 * Error handling memory ownership strategy:
 * Functions which perform allocations are responsible for deallocations on
 * errors, except in subtle cases where this is not possible.
 *
 * All AST destructor functions ignore NULL, so child nodes should be
 * initialized to NULL.
 */

#include "parser.h"
#include "parser_priv.h"

#include <assert.h>
#include <stdlib.h>

#include "util/htable.h"
#include "util/logger.h"
#include "util/util.h"

status_t parser_parse(lexer_t *lexer, trans_unit_t **result) {
    assert(lexer != NULL);
    assert(result != NULL);
    status_t status = CCC_OK;

    lex_wrap_t lex;
    lex.lexer = lexer;
    for (int i = 0; i < LEX_LOOKAHEAD; ++i) {
        if (CCC_OK != (status = lexer_next_token(lex.lexer, &lex.lexemes[i]))) {
            goto fail;
        }
    }
    lex.lex_idx = 0;

    status = par_translation_unit(&lex, result);

fail:
    return status;
}

status_t parser_parse_expr(lexer_t *lexer, expr_t **result) {
    assert(lexer != NULL);
    assert(result != NULL);
    status_t status = CCC_OK;
    lex_wrap_t lex;

    lex.lexer = lexer;
    for (int i = 0; i < LEX_LOOKAHEAD; ++i) {
        if (CCC_OK != (status = lexer_next_token(lex.lexer, &lex.lexemes[i]))) {
            goto fail;
        }
    }
    lex.lex_idx = 0;

    status = par_expression(&lex, NULL, result);

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

status_t par_translation_unit(lex_wrap_t *lex, trans_unit_t **result) {
    status_t status = CCC_OK;
    trans_unit_t *tunit = NULL;
    ALLOC_NODE(lex, tunit, trans_unit_t);
    sl_init(&tunit->gdecls, offsetof(gdecl_t, link));
    if (CCC_OK != (status = tt_init(&tunit->typetab, NULL))) {
        goto fail;
    }

    lex->typetab = &tunit->typetab; // Set top type table to translation units

    while (LEX_CUR(lex).type != TOKEN_EOF) {
        gdecl_t *gdecl;
        if (CCC_OK != (status = par_external_declaration(lex, &gdecl))) {
            goto fail;
        }
        sl_append(&tunit->gdecls, &gdecl->link);
    }
    *result = tunit;
    return status;

fail:
    ast_trans_unit_destroy(tunit);
    return status;
}

status_t par_external_declaration(lex_wrap_t *lex, gdecl_t **result) {
    status_t status = CCC_OK;
    gdecl_t *gdecl = NULL;
    ALLOC_NODE(lex, gdecl, gdecl_t);
    gdecl->type = GDECL_DECL;
    gdecl->decl = NULL;

    ALLOC_NODE(lex, gdecl->decl, decl_t);
    gdecl->decl->type = NULL;
    sl_init(&gdecl->decl->decls, offsetof(decl_node_t, link));

    if (CCC_OK !=
        (status = par_declaration_specifiers(lex, &gdecl->decl->type))) {
        if (status != CCC_BACKTRACK) {
            goto fail;
        }
    }
    if (gdecl->decl->type == NULL) {
        logger_log(&LEX_CUR(lex).mark,
                   "Data definition has no type or storage class",
                   LOG_WARN);
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
    if (LEX_CUR(lex).type == SEMI) {
        LEX_ADVANCE(lex);
        goto done;
    }

    decl_node_t *decl_node = sl_tail(&gdecl->decl->decls);
    if (decl_node->type->type == TYPE_FUNC) {
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
    sl_init(&gdecl->fdefn.gotos, offsetof(stmt_t, goto_params.link));

    static const ht_params_t s_gdecl_ht_params = {
        0,                              // Size estimate
        offsetof(stmt_t, label.label),  // Offset of key
        offsetof(stmt_t, label.link),   // Offset of ht link
        ind_strhash,                    // Hash function
        ind_vstrcmp,                    // void string compare
    };

    if (CCC_OK !=
        (status = ht_init(&gdecl->fdefn.labels, &s_gdecl_ht_params))) {
        goto fail;
    }

    /* TODO: Handle old style function signature
    while (LEX_CUR(lex).type != RPAREN) {
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
        switch (LEX_CUR(lex).type) {
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
            tt_key_t key = { &LEX_CUR(lex).tab_entry->key, TT_TYPEDEF };
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
        ALLOC_NODE(lex, new_type, type_t);
        new_type->type = TYPE_MOD;
        if (*type != NULL) {
            new_type->size = (*type)->size;
            new_type->align = (*type)->align;
        } else {
            new_type->size = 0;
            new_type->align = 0;
        }
        new_type->mod.base = *type;
        new_type->mod.type_mod = TMOD_NONE;
        *type = new_type;
    }
    type_mod_t tmod;
    switch (LEX_CUR(lex).type) {
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
        logger_log(&LEX_CUR(lex).mark, logger_fmt_buf, LOG_WARN);
    }

    (*type)->mod.type_mod |= tmod;
    LEX_ADVANCE(lex);
fail:
    return status;
}

status_t par_type_specifier(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;

    type_t *new_node = NULL;
    type_t *old_type = *type;

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

    // Handle repeat end nodes
    if (*end_node != NULL) {
        bool okay = false;
        switch ((*end_node)->type) {
        case TYPE_INT:
            // Just have int overwritten with the correct type
            if (LEX_CUR(lex).type == SHORT) {
                *end_node = tt_short;
                okay = true;
            }
            if (LEX_CUR(lex).type == LONG) {
                *end_node = tt_long;
                okay = true;
            }
            break;
        case TYPE_SHORT:
            // Skip the int on the short
            if (LEX_CUR(lex).type == INT) {
                okay = true;
            }
            break;
        case TYPE_LONG:
            // Skip the int on the long
            if (LEX_CUR(lex).type == INT) {
                okay = true;
            }
            if (LEX_CUR(lex).type == LONG) { // Create long long int
                *end_node = tt_long_long;
                okay = true;
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
        logger_log(&LEX_CUR(lex).mark, "Multiple type specifers",
                   LOG_ERR);
        status = CCC_ESYNTAX;
        goto fail;
    }

    switch (LEX_CUR(lex).type) {
    case ID: { // typedef name
        // Type specifier only if its a typedef name
        tt_key_t key = { &LEX_CUR(lex).tab_entry->key , TT_TYPEDEF };
        typetab_entry_t *entry = tt_lookup(lex->typetab, &key);
        assert(entry != NULL); // Must be checked before calling
        ALLOC_NODE(lex, new_node, type_t);
        new_node->type = TYPE_TYPEDEF;
        new_node->typedef_params.name = &LEX_CUR(lex).tab_entry->key;
        new_node->typedef_params.base = entry->type;
        new_node->typedef_params.type = TYPE_VOID;
        *end_node = new_node;
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
        type_mod_t mod = LEX_CUR(lex).type == SIGNED ? TMOD_SIGNED : TMOD_UNSIGNED;
        if (mod_node == NULL) {
            ALLOC_NODE(lex, mod_node, type_t);
            new_node = mod_node;
            mod_node->type = TYPE_MOD;
            if (*type == NULL) {
                mod_node->size = 0;
                mod_node->align = 0;
            } else {
                mod_node->size = (*type)->size;
                mod_node->align = (*type)->align;
            }
            mod_node->mod.base = *type;
            mod_node->mod.type_mod = TMOD_NONE;
            *type = mod_node;
        }

        if (mod_node->mod.type_mod & mod) {
            snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                     "Duplicate type specifer: %s",
                     ast_type_mod_str(mod));
            logger_log(&LEX_CUR(lex).mark, logger_fmt_buf, LOG_ERR);
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
    return status;

fail:
    ast_type_destroy(new_node);
    *type = old_type;
    return status;
}

status_t par_struct_or_union_or_enum_specifier(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;

    type_t *new_type = NULL;
    len_str_t *name = NULL;

    basic_type_t btype;
    switch (LEX_CUR(lex).type) {
    case STRUCT: btype = TYPE_STRUCT; break;
    case UNION:  btype = TYPE_UNION;  break;
    case ENUM:   btype = TYPE_ENUM;   break;
    default:
        assert(false);
    }
    LEX_ADVANCE(lex);

    typetab_entry_t *entry = NULL;
    if (LEX_CUR(lex).type == ID) {
        name = &LEX_CUR(lex).tab_entry->key;
        tt_key_t key = { name, TT_COMPOUND };
        entry = tt_lookup(lex->typetab, &key);

        LEX_ADVANCE(lex);

        // Not a definition
        if (LEX_CUR(lex).type != LBRACE && entry != NULL) {
            if (entry->type->type != btype) {
                snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                         "Incorrect type specifer %s. Expected: %s.",
                         ast_basic_type_str(entry->type->type),
                         ast_basic_type_str(btype));
                logger_log(&LEX_CUR(lex).mark, logger_fmt_buf, LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail;
            }
            type_t *typedef_type;
            ALLOC_NODE(lex, typedef_type, type_t);
            typedef_type->type = TYPE_TYPEDEF;
            typedef_type->size = entry->type->size;
            typedef_type->align = entry->type->align;
            typedef_type->typedef_params.name = name;
            typedef_type->typedef_params.base = entry->type;
            typedef_type->typedef_params.type = btype;

            *type = typedef_type;
            return CCC_OK;
        }
    }

    if (entry == NULL) { // Allocate a new type if it doesn't exist
        ALLOC_NODE(lex, new_type, type_t);
        new_type->type = btype;
        new_type->size = 0; // Mark as unitialized
        new_type->align = 0;
        if (btype == TYPE_ENUM) {
            new_type->enum_params.name = name;
            new_type->enum_params.type = tt_int;
            sl_init(&new_type->enum_params.ids, offsetof(decl_node_t, link));
        } else {
            new_type->struct_params.name = name;
            sl_init(&new_type->struct_params.decls,
                    offsetof(decl_t, link));
        }
    }

    if (LEX_CUR(lex).type != LBRACE) {
        if (name != NULL) { // Create a new declaration in the table
            if (entry == NULL) {
                if (CCC_OK != (status = tt_insert(lex->typetab, new_type,
                                                  TT_COMPOUND, name, &entry))) {
                    goto fail;
                }
            }

            type_t *typedef_type;
            ALLOC_NODE(lex, typedef_type, type_t);
            typedef_type->type = TYPE_TYPEDEF;
            typedef_type->size = 0; // Mark as uninitialized
            typedef_type->align = 0;
            typedef_type->typedef_params.name = name;
            typedef_type->typedef_params.base = new_type;
            typedef_type->typedef_params.type = btype;

            *type = typedef_type;
            return CCC_OK;
        } else { // Can't have a compound type without a name or definition
            logger_log(&LEX_CUR(lex).mark,
                       "Compound type without name or definition", LOG_ERR);
            status = CCC_ESYNTAX;
            goto fail;
        }
    }

    LEX_MATCH(lex, LBRACE);

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
    if (name == NULL) {
        ast_type_destroy(new_type);
    } else {
        ast_type_protected_destroy(new_type);
    }
    return status;
}

status_t par_struct_declaration(lex_wrap_t *lex, type_t *type) {
    status_t status = CCC_OK;
    type_t *decl_type = NULL;
    if (CCC_OK != (status = par_specifier_qualifiers(lex, &decl_type))) {
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

status_t par_specifier_qualifiers(lex_wrap_t *lex, type_t **type) {
    status_t status = CCC_OK;;
    *type = NULL; // Set to NULL so sub functions will allocate

    while (true) {
        switch (LEX_CUR(lex).type) {
            // Type specifiers:
        case ID: {
            // Type specifier only if its a typedef name
            tt_key_t key = { &LEX_CUR(lex).tab_entry->key , TT_TYPEDEF };
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

status_t par_struct_declarator_list(lex_wrap_t *lex, type_t *base,
                                    type_t *decl_type) {
    status_t status = CCC_OK;
    decl_t *decl = NULL;
    ALLOC_NODE(lex, decl, decl_t);
    decl->type = decl_type;
    sl_init(&decl->decls, offsetof(decl_node_t, link));

    if (CCC_OK != (status = par_struct_declarator(lex, base, decl))) {
        goto fail;
    }
    while (LEX_CUR(lex).type == COMMA) {
        if (CCC_OK != (status = par_struct_declarator(lex, base, decl))) {
            goto fail;
        }
    }

    sl_append(&base->struct_params.decls, &decl->link);
    return status;

fail:
    ast_decl_destroy(decl);
    return status;
}

status_t par_struct_declarator(lex_wrap_t *lex, type_t *base,
                               decl_t *decl) {
    status_t status = CCC_OK;

    if (CCC_OK != (status = par_declarator_base(lex, decl))) {
        goto fail;
    }
    decl_node_t *dnode = sl_tail(&decl->decls);

    if (LEX_CUR(lex).type == COLON) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_expression(lex, NULL, &dnode->expr))) {
            goto fail;
        }
    }

    base->align = MAX(base->align, dnode->type->align);

    // TODO: Handle bitfields
    if (base->type == TYPE_STRUCT) {
        base->size += dnode->type->size;
    } else { // base->type == TYPE_UNION
        base->size = MAX(base->size, dnode->type->size);
    }

fail:
    return status;
}

status_t par_declarator_base(lex_wrap_t *lex, decl_t *decl) {
    switch (LEX_CUR(lex).type) {
    case STAR:
    case ID:
    case LPAREN:
        break;
    default:
        return CCC_BACKTRACK;
    }
    status_t status = CCC_OK;
    decl_node_t *decl_node;
    ALLOC_NODE(lex, decl_node, decl_node_t);
    decl_node->type = decl->type;
    decl_node->id = NULL;
    decl_node->expr = NULL;
    if (CCC_OK != (status = par_declarator(lex, decl_node, NULL))) {
        goto fail;
    }
    sl_append(&decl->decls, &decl_node->link);

    bool is_typedef = decl->type->type == TYPE_MOD &&
        (decl->type->mod.type_mod & TMOD_TYPEDEF);

    // Add typedefs to the typetable on the top of the stack
    if (is_typedef) {
        if (CCC_OK !=
            (status = tt_insert(lex->typetab, decl_node->type,
                                TT_TYPEDEF, decl_node->id, NULL))) {
            goto fail;
        }
    }

    return status;

fail:
    ast_decl_node_destroy(decl_node);
    return status;
}

status_t par_declarator(lex_wrap_t *lex, decl_node_t *decl_node,
                        type_t ***patch) {
    status_t status = CCC_OK;

    type_t **lpatch = &decl_node->type;
    while (LEX_CUR(lex).type == STAR) {
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

    return status;

fail:
    ast_decl_node_destroy(decl_node);
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
        ALLOC_NODE(lex, new_type, type_t);
    }
    new_type->type = TYPE_PTR;
    new_type->size = PTR_SIZE;
    new_type->align = PTR_ALIGN;
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
    switch (LEX_CUR(lex).type) {
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
        ALLOC_NODE(lex, mod_node, type_t);
        mod_node->type = TYPE_MOD;
        if (*type != NULL) {
            mod_node->size = (*type)->size;
            mod_node->align = (*type)->align;
        } else {
            mod_node->size = 0;
            mod_node->align = 0;
        }
        mod_node->mod.type_mod = TMOD_NONE;
        mod_node->mod.base = *type;
        *type = mod_node;
    }
    mod_node->mod.type_mod |= mod;

    return status;

fail:
    // Nothing to clean up here
    return status;
}

status_t par_direct_declarator(lex_wrap_t *lex, decl_node_t *node,
                               type_t ***patch) {
    status_t status = CCC_OK;
    type_t *base = node->type;
    type_t **lpatch = NULL;

    switch (LEX_CUR(lex).type) {
    case LPAREN: {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_declarator(lex, node, &lpatch))) {
            goto fail;
        }
        LEX_MATCH(lex, RPAREN);

        type_t *paren_type;
        ALLOC_NODE(lex, paren_type, type_t);
        paren_type->type = TYPE_PAREN;
        paren_type->paren_base = *lpatch;
        *lpatch = paren_type;
        lpatch = &paren_type->paren_base;
        paren_type->size = paren_type->paren_base->size;
        paren_type->align = paren_type->paren_base->align;
        break;
    }

    case ID:
        node->id = &LEX_CUR(lex).tab_entry->key;
        LEX_ADVANCE(lex);
        break;

    default:
        // Default is not an error, because may be abstract without an
        // identifier or parens
        break;
    }

    type_t **last_node = &base;
    bool done = false;
    while (!done) {
        switch (LEX_CUR(lex).type) {
        case LBRACK: // Array dimension
            LEX_ADVANCE(lex);
            type_t *arr_type;
            ALLOC_NODE(lex, arr_type, type_t);
            arr_type->type = TYPE_ARR;
            arr_type->align = base->align;
            arr_type->size = 0;
            arr_type->arr.len = NULL;

            arr_type->arr.base = *last_node;
            *last_node = arr_type;
            last_node = &arr_type->arr.base;

            if (LEX_CUR(lex).type == RBRACK) {
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
            ALLOC_NODE(lex, func_type, type_t);
            func_type->type = TYPE_FUNC;
            func_type->align = PTR_ALIGN;
            func_type->size = PTR_SIZE;
            func_type->func.num_params = 0;
            func_type->func.varargs = false;
            sl_init(&func_type->func.params, offsetof(stmt_t, link));

            func_type->func.type = *last_node;
            *last_node = func_type;
            last_node = &func_type->func.type;

            /* TODO: Support old decl syntax
            if (LEX_CUR(lex).type == ID) {
                while (LEX_CUR(lex).type == ID) {
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

fail:
    return status;
}

status_t par_non_binary_expression(lex_wrap_t *lex, bool *is_unary,
                                   expr_t **result) {
    status_t status = CCC_OK;

    bool primary = false;
    bool unary = false;

    switch (LEX_CUR(lex).type) {
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
        switch (LEX_CUR(lex).type) {
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
            tt_key_t key = { &LEX_CUR(lex).tab_entry->key, TT_TYPEDEF };
            if (tt_lookup(lex->typetab, &key) != NULL) {
                if (CCC_OK !=
                    (status = par_cast_expression(lex, true, result))) {
                    goto fail;
                }
                break;
            }
            // FALL THROUGH
        }
        default: { // Paren expression
            expr_t *expr;
            ALLOC_NODE(lex, expr, expr_t);
            expr->type = EXPR_PAREN;
            expr->paren_base = NULL;
            if (CCC_OK !=
                (status = par_expression(lex, NULL, &expr->paren_base))) {
                ast_expr_destroy(expr);
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
        switch (LEX_CUR(lex).type) {
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

    if (is_unary != NULL) {
        *is_unary = unary;
    }
fail:
    return status;
}

status_t par_expression(lex_wrap_t *lex, expr_t *left, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *new_node = NULL; // Newly allocated expression

    if (left == NULL) { // Only search for first operand if not provided
        bool unary1;
        if (CCC_OK !=
            (status = par_non_binary_expression(lex, &unary1, &left))) {
            goto fail;
        }

        if (unary1) {
            // Search for assignment operators
            switch (LEX_CUR(lex).type) {
            case ASSIGN:
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

    // This loop runs to combine binary expressions
    bool new_left = true;
    while (new_left) {
        new_left = false;
        oper_t op1 = OP_NOP;

        switch (LEX_CUR(lex).type) {
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
            LEX_ADVANCE(lex);
            ALLOC_NODE(lex, new_node, expr_t);
            new_node->type = EXPR_COND;
            new_node->cond.expr1 = left;
            new_node->cond.expr2 = NULL;
            new_node->cond.expr3 = NULL;

            if (CCC_OK !=
                par_expression(lex, NULL, &new_node->cond.expr2)) {
                goto fail;
            }
            LEX_MATCH(lex, COLON);
            if (CCC_OK !=
                par_expression(lex, NULL, &new_node->cond.expr3)) {
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
        if (CCC_OK != (status = par_non_binary_expression(lex, NULL, &right))) {
            goto fail;
        }

        oper_t op2 = OP_NOP;
        switch (LEX_CUR(lex).type) {
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
            LEX_ADVANCE(lex);
            ALLOC_NODE(lex, new_node, expr_t);
            new_node->type = EXPR_BIN;
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;
            new_node->bin.expr2 = right;

            expr_t *cond_node;
            ALLOC_NODE(lex, cond_node, expr_t);
            cond_node->type = EXPR_COND;
            cond_node->cond.expr1 = new_node;
            cond_node->cond.expr2 = NULL;
            cond_node->cond.expr3 = NULL;

            // Set new_node to cond_node so it is properly freed on error
            new_node = cond_node;
            if (CCC_OK != par_expression(lex, NULL, &cond_node->cond.expr2)) {
                goto fail;
            }
            LEX_MATCH(lex, COLON);
            if (CCC_OK != par_expression(lex, NULL, &cond_node->cond.expr3)) {
                goto fail;
            }

            // We're done because cond has lowest precedence, so any preceeding
            // binary operators are in cond.expr3
            *result = cond_node;
            return CCC_OK;
        }
        default: { // Not binary 2, just combine the left and light
            ALLOC_NODE(lex, new_node, expr_t);
            new_node->type = EXPR_BIN;
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
            ALLOC_NODE(lex, new_node, expr_t);
            new_node->type = EXPR_BIN;
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;
            new_node->bin.expr2 = right;
            new_left = true;
            left = new_node;
        } else {
            // op2 has greater precedence, parse expression with right as the
            // left side, then combine with the current left
            ALLOC_NODE(lex, new_node, expr_t);
            new_node->type = EXPR_BIN;
            new_node->bin.op = op1;
            new_node->bin.expr1 = left;
            new_node->bin.expr2 = NULL;

            if (CCC_OK !=
                (status = par_expression(lex, right, &new_node->bin.expr2))) {
                goto fail;
            }

            *result = new_node;
            return CCC_OK;
        }
    }

    return status;

fail:
    ast_expr_destroy(new_node);
    return status;
}

status_t par_unary_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *base = NULL;

    switch (LEX_CUR(lex).type) {
        // Primary expressions, followed by postfix modifers
    case ID:
    case STRING:
    case INTLIT:
    case FLOATLIT:
        if (CCC_OK != (status = par_primary_expression(lex, &base))) {
            goto fail;
        }
        if (CCC_OK != (status = par_postfix_expression(lex, base, &base))) {
            goto fail;
        }
        break;

    case INC:
    case DEC: {
        ALLOC_NODE(lex, base, expr_t);
        oper_t op = LEX_CUR(lex).type == INC ? OP_PREINC : OP_PREDEC;
        LEX_ADVANCE(lex);
        base->type = EXPR_UNARY;
        base->unary.op = op;
        base->unary.expr = NULL;
        if (CCC_OK != (status = par_unary_expression(lex, &base->unary.expr))) {
            goto fail;
        }
        break;
    }

    case SIZEOF:
        LEX_ADVANCE(lex);
        ALLOC_NODE(lex, base, expr_t);
        base->type = EXPR_SIZEOF;
        base->sizeof_params.type = NULL;
        base->sizeof_params.expr = NULL;
        if (CCC_BACKTRACK !=
            (status = par_unary_expression(lex, &base->sizeof_params.expr))) {
            if (status != CCC_OK) {
                goto fail;
            }
        } else { // Parsing unary expression failed, sizeof typename
            base->sizeof_params.expr = NULL;
            LEX_MATCH(lex, LPAREN);
            if (CCC_OK !=
                (status = par_type_name(lex, &base->sizeof_params.type))) {
                goto fail;
            }
            LEX_MATCH(lex, RPAREN);
        }
        break;

    case BITAND:
    case STAR:
    case PLUS:
    case MINUS:
    case BITNOT:
    case LOGICNOT: {
        oper_t op;
        switch (LEX_CUR(lex).type) {
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
        ALLOC_NODE(lex, base, expr_t);
        base->type = EXPR_UNARY;
        base->unary.op = op;
        base->unary.expr = NULL;
        if (CCC_OK !=
            (status = par_cast_expression(lex, false, &base->unary.expr))) {
            goto fail;
        }

        break;
    }
    default:
        return CCC_BACKTRACK;
    }

    *result = base;
    return status;

fail:
    ast_expr_destroy(base);
    return status;
}

status_t par_cast_expression(lex_wrap_t *lex, bool skip_paren,
                             expr_t **result) {
    status_t status = CCC_OK;
    if (!skip_paren && LEX_CUR(lex).type != LPAREN) {
        // No paren, just parse a unary expression
        return par_unary_expression(lex, result);
    }
    if (!skip_paren) {
        LEX_ADVANCE(lex);
    }
    decl_t *type = NULL;
    expr_t *expr = NULL;
    if (CCC_OK != (status = par_type_name(lex, &type))) {
        goto fail;
    }
    LEX_MATCH(lex, RPAREN);

    ALLOC_NODE(lex, expr, expr_t);
    expr->type = EXPR_CAST;
    expr->cast.cast = type;
    expr->cast.base = NULL;
    if (CCC_OK !=
        (status = par_cast_expression(lex, false, &expr->cast.base))) {
        goto fail;
    }
    *result = expr;
    return status;

fail:
    if (expr != NULL) { // If expr is allocated, type will be destroyed with it
        ast_expr_destroy(expr);
    } else {
        ast_decl_destroy(type);
    }
    return status;
}

status_t par_postfix_expression(lex_wrap_t *lex, expr_t *base,
                                expr_t **result) {
    status_t status = CCC_OK;
    expr_t **orig_base = NULL;
    expr_t *expr = base;

    while (base != NULL) {
        switch (LEX_CUR(lex).type) {
        case LBRACK: // Array index
            LEX_ADVANCE(lex);
            ALLOC_NODE(lex, expr, expr_t);
            expr->type = EXPR_BIN;
            expr->bin.op = OP_ARR_ACC;
            expr->bin.expr1 = base;
            expr->bin.expr2 = NULL;

            if (CCC_OK !=
                (status = par_expression(lex, NULL, &expr->bin.expr2))) {
                goto fail;
            }
            LEX_MATCH(lex, RBRACK);
            base = expr;
            if (orig_base == NULL) {
                orig_base = &expr->bin.expr2;
            }
            break;

        case LPAREN: // Function call
            LEX_ADVANCE(lex);
            ALLOC_NODE(lex, expr, expr_t);
            expr->type = EXPR_CALL;
            expr->call.func = base;
            sl_init(&expr->call.params, offsetof(expr_t, link));

            while (LEX_CUR(lex).type != RPAREN) {
                expr_t *param;
                if (CCC_OK != (status = par_expression(lex, NULL, &param))) {
                    goto fail;
                }
                sl_append(&expr->call.params, &param->link);
                if (LEX_CUR(lex).type == RPAREN) {
                    break;
                }
                LEX_MATCH(lex, COMMA);
            }
            LEX_ADVANCE(lex);
            base = expr;
            if (orig_base == NULL) {
                orig_base = &expr->call.func;
            }
            break;

        case DOT: // Structure access direct or indirect
        case DEREF: {
            oper_t op = LEX_CUR(lex).type == DOT ? OP_DOT : OP_ARROW;
            LEX_ADVANCE(lex);

            if (LEX_CUR(lex).type != ID) { // Not a name
                snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                         "Parse Error: Expected <identifer>, Found: %s.",
                         token_str(LEX_CUR(lex).type));
                logger_log(&LEX_CUR(lex).mark, logger_fmt_buf, LOG_ERR);
                status = CCC_ESYNTAX;
                goto fail;
            }
            ALLOC_NODE(lex, expr, expr_t);
            expr->type = EXPR_MEM_ACC;
            expr->mem_acc.base = base;
            expr->mem_acc.name = &LEX_CUR(lex).tab_entry->key;
            expr->mem_acc.op = op;
            LEX_ADVANCE(lex);
            base = expr;
            if (orig_base == NULL) {
                orig_base = &expr->mem_acc.base;
            }
            break;
        }

        case INC:
        case DEC: {
            oper_t op = LEX_CUR(lex).type == INC ? OP_POSTINC : OP_POSTDEC;
            LEX_ADVANCE(lex);
            ALLOC_NODE(lex, expr, expr_t);
            expr->type = EXPR_UNARY;
            expr->unary.op = op;
            expr->unary.expr = base;
            base = expr;
            if (orig_base == NULL) {
                orig_base = &expr->unary.expr;
            }
            break;
        }
        default: // Found a terminating character
            base = NULL;
        }
    }

    *result = expr;
    return status;

fail:
    // Expr points to newest node, unless its allocation failed, then base
    // points to newest node
    if (expr == NULL) {
        expr = base;
    }

    // Destroy expr, but make sure base is not deallocated since this function
    // did not allocate base
    if (orig_base != NULL) {
        *orig_base = NULL;
        ast_expr_destroy(expr);
    }

    return status;
}

status_t par_assignment_expression(lex_wrap_t *lex, expr_t *left,
                                   expr_t **result) {
    status_t status = CCC_OK;
    oper_t op;
    switch (LEX_CUR(lex).type) {
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
        assert(false);
    }
    LEX_ADVANCE(lex);

    expr_t *expr = NULL;
    ALLOC_NODE(lex, expr, expr_t);
    expr->type = EXPR_ASSIGN;
    expr->assign.dest = left;
    expr->assign.op = op;
    expr->assign.expr = NULL;

    if (CCC_OK != (status = par_expression(lex, NULL, &expr->assign.expr))) {
        goto fail;
    }

    *result = expr;
    return status;

fail:
    ast_expr_destroy(expr);
    return status;
}

// Excludes parens because they need to be diferrentiated from casts
status_t par_primary_expression(lex_wrap_t *lex, expr_t **result) {
    status_t status = CCC_OK;
    expr_t *base = NULL;
    ALLOC_NODE(lex, base, expr_t);
    base->type = EXPR_VOID; // For safe destruction

    switch (LEX_CUR(lex).type) {
    case ID:
        base->type = EXPR_VAR;
        base->var_id = &LEX_CUR(lex).tab_entry->key;
        LEX_ADVANCE(lex);
        break;

    case STRING: {
        base->type = EXPR_CONST_STR;
        base->const_val.str_val = &LEX_CUR(lex).tab_entry->key;
        base->const_val.type = NULL;
        type_t *type;
        ALLOC_NODE(lex, type, type_t);
        type->type = TYPE_ARR;
        type->size = base->const_val.str_val->len + 1;
        type->align = 1;
        type->arr.base = tt_char;
        type->arr.len = NULL;
        base->const_val.type = type;
        expr_t *len;
        ALLOC_NODE(lex, len, expr_t);
        len->type = EXPR_CONST_INT;
        len->const_val.int_val = base->const_val.str_val->len + 1;
        len->const_val.type = tt_long;
        type->arr.len = len;
        LEX_ADVANCE(lex);
        break;
    }
    case INTLIT: {
        base->type = EXPR_CONST_INT;
        base->const_val.int_val = LEX_CUR(lex).int_params.int_val;
        type_t *type;
        if (LEX_CUR(lex).int_params.hasLL) {
            type = tt_long_long;
        } else if (LEX_CUR(lex).int_params.hasL) {
            type = tt_long;
        } else {
            type = tt_int;
        }

        if (LEX_CUR(lex).int_params.hasU) {
            type_t *type_mod;
            ALLOC_NODE(lex, type_mod, type_t);
            type_mod->type = TYPE_MOD;
            type_mod->size = type->size;
            type_mod->align = type->align;
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
        base->const_val.float_val = LEX_CUR(lex).float_params.float_val;
        base->const_val.type =
            LEX_CUR(lex).float_params.hasF ? tt_float : tt_double;
        LEX_ADVANCE(lex);
        break;
    }
    default:
        assert(false);
    }

    *result = base;
    return status;

fail:
    ast_expr_destroy(base);
    return status;
}

status_t par_type_name(lex_wrap_t *lex, decl_t **result) {
    status_t status = CCC_OK;
    type_t *base = NULL;
    decl_t *decl = NULL;

    if (CCC_OK != (status = par_specifier_qualifiers(lex, &base))) {
        if (base == NULL || status != CCC_BACKTRACK) {
            goto fail;
        }
    }
    ALLOC_NODE(lex, decl, decl_t);
    decl->type = base;
    sl_init(&decl->decls, offsetof(decl_node_t, link));

    if (CCC_BACKTRACK != (status = par_declarator_base(lex, decl))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;

    *result = decl;
    return status;

fail:
    if (decl == NULL) {
        ast_type_destroy(base);
    } else {
        ast_decl_destroy(decl);
    }
    return status;
}

status_t par_parameter_type_list(lex_wrap_t *lex, type_t *func) {
    status_t status = CCC_OK;
    if (CCC_OK != (status = par_parameter_list(lex, func))) {
        if (status != CCC_BACKTRACK) {
            goto fail;
        }
    }

    if (LEX_CUR(lex).type != ELIPSE) {
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
        func->func.num_params++;
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

    ALLOC_NODE(lex, decl, decl_t);
    decl->type = type;
    sl_init(&decl->decls, offsetof(decl_node_t, link));

    // Declarators are optional
    if (CCC_BACKTRACK != (status = par_declarator_base(lex, decl))) {
        if (status != CCC_OK) {
            goto fail;
        }
    }
    status = CCC_OK;

    // Add declaration to the function
    sl_append(&func->func.params, &decl->link);
    return status;

fail:
    if (decl == NULL) {
        ast_type_destroy(type);
    } else {
        ast_decl_destroy(decl);
    }
    return status;
}

status_t par_enumerator_list(lex_wrap_t *lex, type_t *type) {
    assert(type->type == TYPE_ENUM);
    status_t status = CCC_OK;

    if (CCC_OK != (status = par_enumerator(lex, type))) {
        goto fail;
    }

    // Trailing comma on last entry is allowed
    while (LEX_CUR(lex).type == COMMA) {
        if (CCC_OK != (status = par_enumerator(lex, type))) {
            if (status != CCC_BACKTRACK) {
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
    if (LEX_CUR(lex).type != ID) {
        return CCC_BACKTRACK;
    }
    decl_node_t *node = NULL;
    ALLOC_NODE(lex, node, decl_node_t);
    node->type = type->enum_params.type;
    node->id = &LEX_CUR(lex).tab_entry->key;
    node->expr = NULL;
    LEX_ADVANCE(lex);

    // Parse enum value if there is one
    if (LEX_CUR(lex).type == ASSIGN) {
        LEX_ADVANCE(lex);
        if (CCC_OK != (status = par_expression(lex, NULL, &node->expr))) {
            goto fail;
        }
    }
    sl_append(&type->enum_params.ids, &node->link);
    return status;

fail:
    ast_decl_node_destroy(node);
    return status;
}

status_t par_declaration(lex_wrap_t *lex, decl_t **decl, bool partial) {
    status_t status = CCC_OK;
    if (*decl == NULL) {
        ALLOC_NODE(lex, *decl, decl_t);
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

    if (CCC_BACKTRACK == (status = par_init_declarator(lex, *decl, partial))) {
        // No init declarators
        return CCC_OK;
    }

    while (LEX_CUR(lex).type == COMMA) {
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

    bool is_typedef = decl->type->type == TYPE_MOD &&
        (decl->type->mod.type_mod & TMOD_TYPEDEF);
    if (LEX_CUR(lex).type == ASSIGN) {
        if (is_typedef) {
            snprintf(logger_fmt_buf, LOG_FMT_BUF_SIZE,
                     "Typedef '%s' is initialized",
                     decl_node->id->str);
            logger_log(&LEX_CUR(lex).mark, logger_fmt_buf, LOG_WARN);
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
    if (LEX_CUR(lex).type != LBRACE) {
        return par_expression(lex, NULL, result);
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
    expr_t *expr;
    ALLOC_NODE(lex, expr, expr_t);
    expr->type = EXPR_INIT_LIST;
    sl_init(&expr->init_list.exprs, offsetof(expr_t, link));

    while (true) {
        if (LEX_CUR(lex).type == COMMA) {
            LEX_ADVANCE(lex);
        }
        if (LEX_CUR(lex).type == RBRACE) { // Trailing commas allowed
            break;
        }
        expr_t *cur = NULL;
        if (CCC_OK != (status = par_initializer(lex, &cur))) {
            goto fail;
        }
        sl_append(&expr->init_list.exprs, &cur->link);
    }

    *result = expr;
    return status;

fail:
    ast_expr_destroy(expr);
    return status;
}


status_t par_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt = NULL;

    switch (LEX_CUR(lex).type) {
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
        ALLOC_NODE(lex, stmt, stmt_t);
        stmt->type = STMT_DECL;
        stmt->decl = NULL;
        if(CCC_OK != (status = par_declaration(lex, &stmt->decl, false))) {
            goto fail;
        }
        LEX_MATCH(lex, SEMI);
        break;
    }
    case ID: {
        if (LEX_NEXT(lex).type != COLON) {
            // Type specifier only if its a typedef name
            tt_key_t key = { &LEX_CUR(lex).tab_entry->key, TT_TYPEDEF };
            if (tt_lookup(lex->typetab, &key) != NULL) {
                ALLOC_NODE(lex, stmt, stmt_t);
                stmt->type = STMT_DECL;
                stmt->decl = NULL;
                if (CCC_OK != (status = par_declaration(lex, &stmt->decl,
                                                        false))) {
                    goto fail;
                }

                goto done;
            } else {
                return par_expression_statement(lex, result);
            }
        }
        // FALL THROUGH
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
    case LBRACE:
        return par_compound_statement(lex, result);
    default:
        return par_expression_statement(lex, result);
    }

done:
    *result = stmt;
    return status;

fail:
    ast_stmt_destroy(stmt);
    return status;
}

status_t par_labeled_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt = NULL;
    ALLOC_NODE(lex, stmt, stmt_t);
    stmt->type = STMT_NOP; // Initialize to NOP for safe destruction

    switch (LEX_CUR(lex).type) {
    case ID:
        stmt->type = STMT_LABEL;
        stmt->label.label = &LEX_CUR(lex).tab_entry->key;
        stmt->label.stmt = NULL;
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, COLON);
        if (CCC_OK != (status = par_statement(lex, &stmt->label.stmt))) {
            goto fail;
        }
        break;

    case CASE:
        LEX_ADVANCE(lex);
        stmt->type = STMT_CASE;
        stmt->case_params.val = NULL;
        stmt->case_params.stmt = NULL;

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
        stmt->default_params.stmt = NULL;

        if (CCC_OK !=
            (status = par_statement(lex, &stmt->default_params.stmt))) {
            goto fail;
        }
        break;

    default:
        assert(false);
    }

    *result = stmt;
    return status;

fail:
    ast_stmt_destroy(stmt);
    return status;
}

status_t par_selection_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt = NULL;
    ALLOC_NODE(lex, stmt, stmt_t);
    stmt->type = STMT_NOP; // Initialize to NOP for safe destruction

    switch (LEX_CUR(lex).type) {
    case IF:
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, LPAREN);
        stmt->type = STMT_IF;
        stmt->if_params.expr = NULL;
        stmt->if_params.true_stmt = NULL;
        stmt->if_params.false_stmt = NULL;

        if (CCC_OK !=
            (status = par_expression(lex, NULL, &stmt->if_params.expr))) {
            goto fail;
        }

        LEX_MATCH(lex, RPAREN);
        if (CCC_OK !=
            (status = par_statement(lex, &stmt->if_params.true_stmt))) {
            goto fail;
        }

        if (LEX_CUR(lex).type == ELSE) {
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
        stmt->type = STMT_SWITCH;
        stmt->switch_params.expr = NULL;
        stmt->switch_params.stmt = NULL;
        stmt->switch_params.default_stmt = NULL;
        sl_init(&stmt->switch_params.cases, offsetof(stmt_t, case_params.link));

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
        assert(false);
    }

    *result = stmt;
    return status;

fail:
    ast_stmt_destroy(stmt);
    return status;
}

status_t par_iteration_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt = NULL;
    ALLOC_NODE(lex, stmt, stmt_t);
    stmt->type = STMT_NOP; // Initialize to NOP for safe destruction

    switch (LEX_CUR(lex).type) {
    case DO:
        LEX_ADVANCE(lex);
        stmt->type = STMT_DO;
        stmt->do_params.stmt = NULL;
        stmt->do_params.expr = NULL;

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
        stmt->while_params.expr = NULL;
        stmt->while_params.stmt = NULL;

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
        stmt->for_params.expr1 = NULL;
        stmt->for_params.expr2 = NULL;
        stmt->for_params.expr3 = NULL;
        stmt->for_params.stmt = NULL;

        if (LEX_CUR(lex).type != SEMI) {
            if (CCC_OK !=
                (status = par_expression(lex, NULL, &stmt->for_params.expr1))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);

        if (LEX_CUR(lex).type != SEMI) {
            if (CCC_OK !=
                (status = par_expression(lex, NULL, &stmt->for_params.expr2))) {
                goto fail;
            }
        }
        LEX_MATCH(lex, SEMI);

        if (LEX_CUR(lex).type != SEMI) {
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
        assert(false);
    }

    *result = stmt;
    return status;

fail:
    ast_stmt_destroy(stmt);
    return status;
}

status_t par_jump_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt = NULL;
    ALLOC_NODE(lex, stmt, stmt_t);
    stmt->type = STMT_NOP; // Initialize to NOP for safe destruction

    switch (LEX_CUR(lex).type) {
    case GOTO:
        LEX_ADVANCE(lex);
        stmt->type = STMT_GOTO;
        if (LEX_CUR(lex).type != ID) {
            status = CCC_ESYNTAX;
            goto fail;
        }
        stmt->goto_params.label = &LEX_CUR(lex).tab_entry->key;
        LEX_ADVANCE(lex);
        LEX_MATCH(lex, SEMI);
        break;

    case CONTINUE:
    case BREAK:
        LEX_ADVANCE(lex);
        stmt->type = STMT_BREAK;
        if (LEX_CUR(lex).type == CONTINUE) {
            stmt->continue_params.parent = NULL;
        } else { // LEX_CUR(lex).type == BREAK
            stmt->break_params.parent = NULL;
        }
        LEX_MATCH(lex, SEMI);
        break;

    case RETURN:
        LEX_ADVANCE(lex);
        stmt->type = STMT_RETURN;
        stmt->return_params.expr = NULL;

        if (CCC_OK !=
            (status = par_expression(lex, false, &stmt->return_params.expr))) {
            goto fail;
        }
        LEX_MATCH(lex, SEMI);
        break;

    default:
        assert(false);
    }
    *result = stmt;
    return status;

fail:
    ast_stmt_destroy(stmt);
    return status;
}


status_t par_compound_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;
    ALLOC_NODE(lex, stmt, stmt_t);
    stmt->type = STMT_COMPOUND;
    sl_init(&stmt->compound.stmts, offsetof(stmt_t, link));

    if (CCC_OK != (tt_init(&stmt->compound.typetab, lex->typetab))) {
        goto fail;
    }
    // Add new typetab table to top of stack
    lex->typetab = &stmt->compound.typetab;

    LEX_MATCH(lex, LBRACE);
    while (LEX_CUR(lex).type != RBRACE) {
        stmt_t *cur = NULL;
        if (CCC_OK != (status = par_statement(lex, &cur))) {
            goto fail;
        }
        sl_append(&stmt->compound.stmts, &cur->link);
    }
    LEX_ADVANCE(lex); // Consume RBRACE

    // Restore old type table to top of stack
    lex->typetab = stmt->compound.typetab.last;

    *result = stmt;
    return status;

fail:
    ast_stmt_destroy(stmt);
    return status;
}


status_t par_expression_statement(lex_wrap_t *lex, stmt_t **result) {
    status_t status = CCC_OK;
    stmt_t *stmt;
    ALLOC_NODE(lex, stmt, stmt_t);

    if (LEX_CUR(lex).type == SEMI) {
        stmt->type = STMT_NOP;
    } else {
        stmt->type = STMT_EXPR;
        stmt->expr.expr = NULL;
        if (CCC_OK != (status = par_expression(lex, NULL, &stmt->expr.expr))) {
            goto fail;
        }
    }

    LEX_MATCH(lex, SEMI);

    *result = stmt;
    return status;

fail:
    ast_stmt_destroy(stmt);
    return status;
}
