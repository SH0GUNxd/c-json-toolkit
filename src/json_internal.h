#ifndef JSON_INTERNAL_H
#define JSON_INTERNAL_H

#include <stddef.h>

#include "json.h"

/* Lexer tokens */

typedef enum
{
    TOK_LBRACE, /* { */
    TOK_RBRACE, /* } */
    TOK_LBRACKET, /* [ */
    TOK_RBRACKET, /* ] */
    TOK_COLON,
    TOK_COMMA,
    TOK_STRING,
    TOK_NUMBER,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NULL,
    TOK_EOF,
    TOK_ERROR
} tok_type_t;

typedef struct
{
    tok_type_t type;
    const char *start;
    size_t len;
    int line;
    int col;
} token_t;

/* Parser state */

typedef struct
{
    const char *src;
    size_t pos;
    int line;
    int col;
    token_t cur;
    json_error_t *err;
    int depth;
} parser_t;

#define MAX_DEPTH 512

/* Internal functions */

/* lexer */
void lexer_init(parser_t *p, const char *src, json_error_t *err);
token_t lexer_next(parser_t *p);
const char *tok_type_name(tok_type_t t);

/* parser */

json_value_t *parse_value(parser_t *p);

/* helpers */

void set_error(parser_t *p, int line, int col, const char *fmt, ...);
char *unescape_string(parser_t *p, const char *raw, size_t len);

#endif /* JSON_INTERNAL_H */
