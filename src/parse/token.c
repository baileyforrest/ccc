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
 * Token function implmenetation
 */

#include "token.h"

#include <assert.h>
#include <stdio.h>

#include "parse/symtab.h"

#define CASE_BASIC_PRINT(token) \
    case token: printf(#token "\n"); break

void token_print(lexeme_t *token) {
    assert(token != NULL);

    switch (token->type) {
        // Other
    case ID:
        printf("%s", token->tab_entry->key.str);
        break;
    case STRING:
        printf("\"%s\"", token->tab_entry->key.str);
        break;
    case INTLIT:
        printf("%lld", token->int_params.int_val);
        if (token->int_params.hasU) {
            printf("U");
        }
        if (token->int_params.hasL) {
            printf("L");
        }
        if (token->int_params.hasLL) {
            printf("LL");
        }
        break;
    case FLOATLIT:
        printf("%f", token->float_params.float_val);
        if (token->float_params.hasF) {
            printf("F");
        }
        break;
    default:
        printf("%s", token_str(token->type));
    }
    printf("\n");
}

#define CASE_TOK_STR(tok, str) \
    case tok: str

const char *token_str(token_t token) {
    switch (token) {
    case TOKEN_EOF:     return "EOF";
    case LBRACE:        return "{";
    case RBRACE:        return "}";
    case LPAREN:        return "(";
    case RPAREN:        return ")";
    case SEMI:          return ";";
    case COMMA:         return ":";
    case LBRACK:        return "[";
    case RBRACK:        return "]";
    case DEREF:         return "->";
    case DOT:           return ".";
    case ELIPSE:        return "...";

    case COND:          return "?";
    case COLON:         return ":";

    case ASSIGN:        return "=";
    case PLUSEQ:        return "+=";
    case MINUSEQ:       return "-=";
    case STAREQ:        return "*=";
    case DIVEQ:         return "/=";
    case MODEQ:         return "%=";
    case BITXOREQ:      return "^=";
    case BITOREQ:       return "|=";
    case BITANDEQ:      return "&=";
    case RSHIFTEQ:      return ">>=";
    case LSHIFTEQ:      return "<<=";

    case EQ:            return "==";
    case NE:            return "!=";
    case LT:            return "<";
    case GT:            return ">";
    case LE:            return "<=";
    case GE:            return ">=";

    case RSHIFT:        return ">>";
    case LSHIFT:        return "<<";

    case LOGICAND:      return "&&";
    case LOGICOR:       return "||";
    case LOGICNOT:      return "!";

    case PLUS:          return "+";
    case MINUS:         return "-";
    case STAR:          return "*";
    case DIV:           return "/";
    case MOD:           return "%";

    case BITAND:        return "&";
    case BITOR:         return "|";
    case BITXOR:        return "^";
    case BITNOT:        return "~";

    case INC:           return "++";
    case DEC:           return "--";

    case AUTO:          return "auto";
    case BREAK:         return "break";
    case CASE:          return "case";
    case CONST:         return "const";
    case CONTINUE:      return "continue";
    case DEFAULT:       return "default";
    case DO:            return "do";
    case ELSE:          return "else";
    case ENUM:          return "enum";
    case EXTERN:        return "extern";
    case FOR:           return "for";
    case GOTO:          return "goto";
    case IF:            return "if";
    case INLINE:        return "inline";
    case REGISTER:      return "register";
    case RESTRICT:      return "restrict";
    case RETURN:        return "return";
    case SIZEOF:        return "sizeof";
    case STATIC:        return "static";
    case STRUCT:        return "struct";
    case SWITCH:        return "switch";
    case TYPEDEF:       return "typedef";
    case UNION:         return "union";
    case VOLATILE:      return "volatile";
    case WHILE:         return "while";

    case ALIGNAS:       return "_Alignas";
    case ALIGNOF:       return "_Alignof";
    case BOOL:          return "_Bool";
    case COMPLEX:       return "_Complex";
    case GENERIC:       return "_Generic";
    case IMAGINARY:     return "_Imaginary";
    case NORETURN:      return "_Noreturn";
    case STATIC_ASSERT: return "_Static_assert";
    case THREAD_LOCAL:  return "_Thread_local";

    case VOID:          return "void";
    case CHAR:          return "char";
    case SHORT:         return "short";
    case INT:           return "int";
    case LONG:          return "long";
    case UNSIGNED:      return "unsigned";
    case SIGNED:        return "signed";

    case DOUBLE:        return "double";
    case FLOAT:         return "float";

    case ID:            return "<identifier>";
    case STRING:        return "<string literal>";
    case INTLIT:        return "<integer literal>";
    case FLOATLIT:      return "<float literal>";
    }
    assert(false);
    return NULL;
}