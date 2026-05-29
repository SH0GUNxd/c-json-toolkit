#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_internal.h"

/* Lookahead helpers */

static token_t peek(const parser_t *p)
{
    return p->cur;
}

static token_t consume(parser_t *p)
{
    token_t t = p->cur;
    p->cur = lexer_next(p);
    return t;
}

/* expect: consume current token if it matches, else set error and return 0 */
static int expect(parser_t *p, tok_type_t t)
{
    if (p->cur.type != t)
    {
        set_error(p, p->cur.line, p->cur.col,
                  "expected '%s' but got '%s' at %d:%d", tok_type_name(t),
                  tok_type_name(p->cur.type), p->cur.line, p->cur.col);
        return 0;
    }
    consume(p);
    return 1;
}

/* Value allocation helpers */

static json_value_t *alloc_value(json_type_t t)
{
    json_value_t *v = (json_value_t *)calloc(1, sizeof(json_value_t));
    if (v)
        v->type = t;
    return v;
}

/* Forward declaration */

json_value_t *parse_value(parser_t *p);

/* parse_string */

static json_value_t *parse_string(parser_t *p)
{
    token_t tok = consume(p); /* already confirmed TOK_STRING */
    char *s = unescape_string(p, tok.start, tok.len);
    if (!s)
        return NULL;

    json_value_t *v = alloc_value(JSON_STRING);
    if (!v)
    {
        free(s);
        set_error(p, 0, 0, "out of memory");
        return NULL;
    }
    v->v.string = s;
    return v;
}

/* parse_number */

static json_value_t *parse_number(parser_t *p)
{
    token_t tok = consume(p);
    /* strtod needs a null-terminated string; raw is not. copy it. */
    char buf[64];
    size_t len = tok.len < sizeof(buf) - 1 ? tok.len : sizeof(buf) - 1;
    memcpy(buf, tok.start, len);
    buf[len] = '\0';

    char *end;
    double d = strtod(buf, &end);
    if (end != buf + len)
    {
        set_error(p, tok.line, tok.col, "malformed number '%s' at %d:%d", buf,
                  tok.line, tok.col);
        return NULL;
    }

    json_value_t *v = alloc_value(JSON_NUMBER);
    if (!v)
    {
        set_error(p, 0, 0, "out of memory");
        return NULL;
    }
    v->v.number = d;
    return v;
}

/* parse_array */

#define ARRAY_INIT_CAP 8

static json_value_t *parse_array(parser_t *p)
{
    if (!expect(p, TOK_LBRACKET))
        return NULL;

    json_value_t *arr = alloc_value(JSON_ARRAY);
    if (!arr)
    {
        set_error(p, 0, 0, "out of memory");
        return NULL;
    }

    if (peek(p).type == TOK_RBRACKET)
    {
        consume(p);
        return arr;
    }

    size_t cap = ARRAY_INIT_CAP;
    json_value_t **items = (json_value_t **)malloc(cap * sizeof(*items));
    if (!items)
    {
        free(arr);
        set_error(p, 0, 0, "out of memory");
        return NULL;
    }

    size_t count = 0;
    for (;;)
    {
        json_value_t *elem = parse_value(p);
        if (!elem)
            goto err;

        if (count == cap)
        {
            cap *= 2;
            json_value_t **tmp =
                (json_value_t **)realloc(items, cap * sizeof(*items));
            if (!tmp)
            {
                json_free(elem);
                set_error(p, 0, 0, "out of memory");
                goto err;
            }
            items = tmp;
        }
        items[count++] = elem;

        if (peek(p).type == TOK_RBRACKET)
            break;
        if (!expect(p, TOK_COMMA))
            goto err;

        /* trailing comma check */
        if (peek(p).type == TOK_RBRACKET)
        {
            set_error(p, p->cur.line, p->cur.col,
                      "trailing comma in array at %d:%d", p->cur.line,
                      p->cur.col);
            goto err;
        }
    }
    consume(p); /* ] */

    arr->v.array.items = items;
    arr->v.array.count = count;
    return arr;

err:
    for (size_t i = 0; i < count; i++)
        json_free(items[i]);
    free(items);
    free(arr);
    return NULL;
}

/* parse_object */

static json_value_t *parse_object(parser_t *p)
{
    if (!expect(p, TOK_LBRACE))
        return NULL;

    json_value_t *obj = alloc_value(JSON_OBJECT);
    if (!obj)
    {
        set_error(p, 0, 0, "out of memory");
        return NULL;
    }

    if (peek(p).type == TOK_RBRACE)
    {
        consume(p);
        return obj;
    }

    json_pair_t *head = NULL;
    json_pair_t *tail = NULL;
    size_t count = 0;

    for (;;)
    {
        /* key must be a string */
        if (peek(p).type != TOK_STRING)
        {
            set_error(p, p->cur.line, p->cur.col,
                      "expected string key in object at %d:%d", p->cur.line,
                      p->cur.col);
            goto err;
        }
        token_t ktok = consume(p);
        char *key = unescape_string(p, ktok.start, ktok.len);
        if (!key)
            goto err;

        if (!expect(p, TOK_COLON))
        {
            free(key);
            goto err;
        }

        json_value_t *val = parse_value(p);
        if (!val)
        {
            free(key);
            goto err;
        }

        json_pair_t *pair = (json_pair_t *)calloc(1, sizeof(json_pair_t));
        if (!pair)
        {
            free(key);
            json_free(val);
            set_error(p, 0, 0, "out of memory");
            goto err;
        }
        pair->key = key;
        pair->value = val;

        if (!head)
            head = pair;
        else
            tail->next = pair;
        tail = pair;
        count++;

        if (peek(p).type == TOK_RBRACE)
            break;
        if (!expect(p, TOK_COMMA))
            goto err;

        /* trailing comma check */
        if (peek(p).type == TOK_RBRACE)
        {
            set_error(p, p->cur.line, p->cur.col,
                      "trailing comma in object at %d:%d", p->cur.line,
                      p->cur.col);
            goto err;
        }
    }
    consume(p); /* } */

    obj->v.object.head = head;
    obj->v.object.count = count;
    return obj;

err:
    for (json_pair_t *pr = head; pr;)
    {
        json_pair_t *nx = pr->next;
        free(pr->key);
        json_free(pr->value);
        free(pr);
        pr = nx;
    }
    free(obj);
    return NULL;
}

/* parse_value (dispatcher) */

json_value_t *parse_value(parser_t *p)
{
    if (p->depth > MAX_DEPTH)
    {
        set_error(p, p->cur.line, p->cur.col,
                  "maximum nesting depth %d exceeded at %d:%d", MAX_DEPTH,
                  p->cur.line, p->cur.col);
        return NULL;
    }
    p->depth++;

    json_value_t *v = NULL;
    tok_type_t t = peek(p).type;

    switch (t)
    {
    case TOK_NULL: {
        token_t tok = consume(p);
        v = alloc_value(JSON_NULL);
        if (!v)
            set_error(p, tok.line, tok.col, "out of memory");
        break;
    }
    case TOK_TRUE: {
        token_t tok = consume(p);
        v = alloc_value(JSON_BOOL);
        if (!v)
            set_error(p, tok.line, tok.col, "out of memory");
        else
            v->v.boolean = 1;
        break;
    }
    case TOK_FALSE: {
        token_t tok = consume(p);
        v = alloc_value(JSON_BOOL);
        if (!v)
            set_error(p, tok.line, tok.col, "out of memory");
        else
            v->v.boolean = 0;
        break;
    }
    case TOK_NUMBER:
        v = parse_number(p);
        break;
    case TOK_STRING:
        v = parse_string(p);
        break;
    case TOK_LBRACKET:
        v = parse_array(p);
        break;
    case TOK_LBRACE:
        v = parse_object(p);
        break;
    default:
        set_error(p, p->cur.line, p->cur.col, "unexpected token '%s' at %d:%d",
                  tok_type_name(t), p->cur.line, p->cur.col);
        break;
    }

    p->depth--;
    return v;
}
