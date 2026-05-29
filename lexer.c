#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_internal.h"

/* Error reporting */

void set_error(parser_t *p, int line, int col, const char *fmt, ...)
{
    if (!p->err)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->err->message, sizeof(p->err->message), fmt, ap);
    va_end(ap);
    p->err->line = line;
    p->err->col = col;
}

/* Lexer helpers */

static char cur_ch(const parser_t *p)
{
    return p->src[p->pos];
}

static char advance(parser_t *p)
{
    char c = p->src[p->pos++];
    if (c == '\n')
    {
        p->line++;
        p->col = 1;
    }
    else
    {
        p->col++;
    }
    return c;
}

static void skip_ws(parser_t *p)
{
    while (p->src[p->pos] && isspace((unsigned char)p->src[p->pos]))
        advance(p);
}

static token_t make_tok(tok_type_t t, const char *start, size_t len, int line,
                        int col)
{
    token_t tok;
    tok.type = t;
    tok.start = start;
    tok.len = len;
    tok.line = line;
    tok.col = col;
    return tok;
}

static token_t lex_string(parser_t *p)
{
    int sline = p->line, scol = p->col;
    advance(p); /* consume opening '"' */
    const char *start = p->src + p->pos;
    int escaped = 0;

    while (p->src[p->pos])
    {
        char c = p->src[p->pos];
        if (c == '\\')
        {
            escaped = 1;
            advance(p);
            if (!p->src[p->pos])
                break;
            advance(p);
            continue;
        }
        if (c == '"')
        {
            size_t len = (size_t)(p->src + p->pos - start);
            advance(p); /* consume closing '"' */
            (void)escaped;
            return make_tok(TOK_STRING, start, len, sline, scol);
        }
        /* RFC 8259: unescaped control chars are invalid */
        if ((unsigned char)c < 0x20)
        {
            set_error(p, p->line, p->col,
                      "invalid control character U+%04X in string at %d:%d",
                      (unsigned char)c, p->line, p->col);
            return make_tok(TOK_ERROR, NULL, 0, p->line, p->col);
        }
        advance(p);
    }
    set_error(p, sline, scol, "unterminated string starting at %d:%d", sline,
              scol);
    return make_tok(TOK_ERROR, NULL, 0, sline, scol);
}

static token_t lex_number(parser_t *p)
{
    int sline = p->line, scol = p->col;
    const char *start = p->src + p->pos;

    if (cur_ch(p) == '-')
        advance(p);

    if (!isdigit((unsigned char)cur_ch(p)))
    {
        set_error(p, sline, scol, "invalid number at %d:%d", sline, scol);
        return make_tok(TOK_ERROR, NULL, 0, sline, scol);
    }

    if (cur_ch(p) == '0')
    {
        advance(p);
    }
    else
    {
        while (isdigit((unsigned char)cur_ch(p)))
            advance(p);
    }

    if (cur_ch(p) == '.')
    {
        advance(p);
        if (!isdigit((unsigned char)cur_ch(p)))
        {
            set_error(p, sline, scol,
                      "expected digit after decimal point at %d:%d", sline,
                      scol);
            return make_tok(TOK_ERROR, NULL, 0, sline, scol);
        }
        while (isdigit((unsigned char)cur_ch(p)))
            advance(p);
    }

    if (cur_ch(p) == 'e' || cur_ch(p) == 'E')
    {
        advance(p);
        if (cur_ch(p) == '+' || cur_ch(p) == '-')
            advance(p);
        if (!isdigit((unsigned char)cur_ch(p)))
        {
            set_error(p, sline, scol, "expected digit in exponent at %d:%d",
                      sline, scol);
            return make_tok(TOK_ERROR, NULL, 0, sline, scol);
        }
        while (isdigit((unsigned char)cur_ch(p)))
            advance(p);
    }

    size_t len = (size_t)(p->src + p->pos - start);
    return make_tok(TOK_NUMBER, start, len, sline, scol);
}

static token_t lex_keyword(parser_t *p, const char *kw, tok_type_t t)
{
    int sline = p->line, scol = p->col;
    size_t klen = strlen(kw);
    if (strncmp(p->src + p->pos, kw, klen) == 0
        && !isalpha((unsigned char)p->src[p->pos + klen]))
    {
        const char *start = p->src + p->pos;
        for (size_t i = 0; i < klen; i++)
            advance(p);
        return make_tok(t, start, klen, sline, scol);
    }
    set_error(p, sline, scol, "unexpected token at %d:%d", sline, scol);
    return make_tok(TOK_ERROR, NULL, 0, sline, scol);
}

/* Public lexer API */

void lexer_init(parser_t *p, const char *src, json_error_t *err)
{
    p->src = src;
    p->pos = 0;
    p->line = 1;
    p->col = 1;
    p->err = err;
    p->depth = 0;
    if (err)
    {
        err->message[0] = '\0';
        err->line = 0;
        err->col = 0;
    }
    /* prime the lookahead */
    p->cur = lexer_next(p);
}

token_t lexer_next(parser_t *p)
{
    skip_ws(p);

    int sline = p->line, scol = p->col;
    char c = cur_ch(p);

    if (c == '\0')
        return make_tok(TOK_EOF, NULL, 0, sline, scol);

    switch (c)
    {
    case '{':
        advance(p);
        return make_tok(TOK_LBRACE, NULL, 0, sline, scol);
    case '}':
        advance(p);
        return make_tok(TOK_RBRACE, NULL, 0, sline, scol);
    case '[':
        advance(p);
        return make_tok(TOK_LBRACKET, NULL, 0, sline, scol);
    case ']':
        advance(p);
        return make_tok(TOK_RBRACKET, NULL, 0, sline, scol);
    case ':':
        advance(p);
        return make_tok(TOK_COLON, NULL, 0, sline, scol);
    case ',':
        advance(p);
        return make_tok(TOK_COMMA, NULL, 0, sline, scol);
    case '"':
        return lex_string(p);
    case 't':
        return lex_keyword(p, "true", TOK_TRUE);
    case 'f':
        return lex_keyword(p, "false", TOK_FALSE);
    case 'n':
        return lex_keyword(p, "null", TOK_NULL);
    default:
        if (c == '-' || isdigit((unsigned char)c))
            return lex_number(p);
        set_error(p, sline, scol, "unexpected character '%c' at %d:%d", c,
                  sline, scol);
        return make_tok(TOK_ERROR, NULL, 0, sline, scol);
    }
}

const char *tok_type_name(tok_type_t t)
{
    static const char *names[] = { "{",    "}",      "[",      "]",    ":",
                                   ",",    "string", "number", "true", "false",
                                   "null", "EOF",    "error" };
    if ((int)t < 0 || (int)t > TOK_ERROR)
        return "?";
    return names[(int)t];
}
