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
 * Token function implmenetation
 */

#include "token.h"

#include <assert.h>
#include <stdio.h>

#include "lex/symtab.h"

#include "util/string_builder.h"
#include "util/logger.h"

#define INT_PARAM_LIT(val) { false, false, false, val }

#define INT_TOK_LIT(param) \
    { INTLIT, 0, NULL, &fmark_built_in, STR_SET_LIT, { .int_params = &param } }

static token_int_params_t zero_params = INT_PARAM_LIT(0);
token_t token_int_zero = INT_TOK_LIT(zero_params);

static token_int_params_t one_params = INT_PARAM_LIT(1);
token_t token_int_one = INT_TOK_LIT(one_params);

token_t token_eof = { TOKEN_EOF, 0, NULL, &fmark_built_in, STR_SET_LIT, {} };


typedef struct token_node_t {
    sl_link_t link;
    token_t token;
} token_node_t;

void token_man_init(token_man_t *tm) {
    sl_init(&tm->tokens, offsetof(token_node_t, link));
}

void token_node_destroy(token_node_t *token_node) {
    str_set_destroy(token_node->token.hideset);
    switch (token_node->token.type) {
    case INTLIT: free(token_node->token.int_params); break;
    case FLOATLIT: free(token_node->token.float_params); break;
    default:
        break;
    }
    free(token_node);
}

void token_man_destroy(token_man_t *tm) {
    SL_DESTROY_FUNC(&tm->tokens, token_node_destroy);
}

token_t *token_create(token_man_t *tm) {
    token_node_t *result = emalloc(sizeof(token_node_t));
    sl_append(&tm->tokens, &result->link);

    // Set to safe values for printing and destruction
    result->token.type = TOKEN_EOF;
    result->token.start = NULL;
    result->token.len = 0;
    result->token.hideset = str_set_empty();

    return &result->token;
}

token_t *token_copy(token_man_t *tm, token_t *token) {
    token_t *result = token_create(tm);
    memcpy(result, token, sizeof(token_t));
    switch (token->type) {
    case INTLIT:
        result->int_params = emalloc(sizeof(token_int_params_t));
        memcpy(result->int_params, token->int_params,
               sizeof(token_int_params_t));
        break;
    case FLOATLIT:
        result->float_params = emalloc(sizeof(token_float_params_t));
        memcpy(result->float_params, token->float_params,
               sizeof(token_float_params_t));
        break;
    default:
        break;
    }
    result->hideset = str_set_copy(token->hideset);

    return result;
}

bool token_equal(const token_t *t1, const token_t *t2) {
    if (t1 == t2) {
        return true;
    }

    if (t1->type != t2->type) {
        return false;
    }

    switch (t1->type) {
    case ID: return strcmp(t1->id_name, t2->id_name) == 0;
    case STRING: return strcmp(t1->str_val, t2->str_val) == 0;
    case INTLIT:
        return t1->int_params->int_val == t2->int_params->int_val &&
            t1->int_params->hasU == t2->int_params->hasU &&
            t1->int_params->hasL == t2->int_params->hasL &&
            t1->int_params->hasLL == t2->int_params->hasLL;
    case FLOATLIT:
        return t1->float_params->float_val == t2->float_params->float_val &&
            t1->float_params->hasF == t2->float_params->hasF &&
            t1->float_params->hasL == t2->float_params->hasL;
    default:
        break;
    }
    return true;
}


void token_print_helper(token_t *token, string_builder_t *sb, FILE *file) {
    assert(token != NULL);

    switch (token->type) {
    case TOK_WARN: logger_log(token->mark, LOG_WARN, token->str_val); break;
    case TOK_ERR:  logger_log(token->mark, LOG_ERR, token->str_val); break;

    case ID:
        directed_print(sb, file, "%s", token->id_name);
        break;
    case STRING:
        if (token->start != NULL) {
            directed_print(sb, file, "%.*s", (int)token->len, token->start);
            break;
        }
        directed_print(sb, file, "\"%s\"", token->str_val);
        break;
    case INTLIT:
        if (token->start != NULL) {
            directed_print(sb, file, "%.*s", (int)token->len, token->start);
            break;
        }
        directed_print(sb, file, "%lld", token->int_params->int_val);
        if (token->int_params->hasU) {
            directed_print(sb, file, "U");
        }
        if (token->int_params->hasL) {
            directed_print(sb, file, "L");
        } else if (token->int_params->hasLL) {
            directed_print(sb, file, "LL");
        }
        break;
    case FLOATLIT:
        if (token->start != NULL) {
            directed_print(sb, file, "%.*s", (int)token->len, token->start);
            break;
        }
        directed_print(sb, file, "%Lf", token->float_params->float_val);
        if (token->float_params->hasL) {
            directed_print(sb, file, "L");
        }
        if (token->float_params->hasF) {
            directed_print(sb, file, "F");
        }
        break;
    default:
        directed_print(sb, file, "%s", token_type_str(token->type));
    }
}

void token_print(FILE *file, token_t *token) {
    token_print_helper(token, NULL, file);
}

char *token_str(token_t *token) {
    string_builder_t sb;
    sb_init(&sb, 0);

    token_print_helper(token, &sb, NULL);
    sb_compact(&sb);

    return sb_buf(&sb);
}

void token_str_append_sb(string_builder_t *sb, token_t *token) {
    token_print_helper(token, sb, NULL);
}

#define CASE_TOK_STR(tok, str) \
    case tok: str

const char *token_type_str(token_type_t token) {
    switch (token) {
    case TOK_WARN:
    case TOK_ERR:       return "";
    case TOKEN_EOF:     return "";
    case HASH:          return "#";
    case HASHHASH:      return "##";
    case SPACE:         return " ";
    case NEWLINE:       return "\n";
    case BACKSLASH:     return "\\";

    case LBRACE:        return "{";
    case RBRACE:        return "}";
    case LPAREN:        return "(";
    case RPAREN:        return ")";
    case SEMI:          return ";";
    case COMMA:         return ",";
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
    case STATIC_ASSERT: return "_Static_assert";
    case THREAD_LOCAL:  return "_Thread_local";

    case OFFSETOF:      return "__builtin_offsetof";

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

    case VA_LIST:       return "__builtin_va_list";
    case VA_START:      return "__builtin_va_start";
    case VA_ARG:        return "__builtin_va_arg";
    case VA_END:        return "__builtin_va_end";
    case VA_COPY:       return "__builtin_va_copy";

    case FUNC:          return "__func__";
    }
    assert(false);
    return NULL;
}
