#include "json_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Dynamic buffer */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} buf_t;

static int buf_init(buf_t *b)
{
    b->cap  = 256;
    b->len  = 0;
    b->data = (char *)malloc(b->cap);
    return b->data ? 0 : -1;
}

static int buf_grow(buf_t *b, size_t need)
{
    if (b->len + need <= b->cap) return 0;
    while (b->cap < b->len + need) b->cap *= 2;
    char *tmp = (char *)realloc(b->data, b->cap);
    if (!tmp) return -1;
    b->data = tmp;
    return 0;
}

static int buf_push(buf_t *b, char c)
{
    if (buf_grow(b, 1) < 0) return -1;
    b->data[b->len++] = c;
    return 0;
}

static int buf_write(buf_t *b, const char *s, size_t n)
{
    if (buf_grow(b, n) < 0) return -1;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    return 0;
}

static int buf_puts(buf_t *b, const char *s)
{
    return buf_write(b, s, strlen(s));
}

/* String escaping */

static int write_escaped_string(buf_t *b, const char *s)
{
    if (buf_push(b, '"') < 0) return -1;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '"':  if (buf_puts(b, "\\\"") < 0) return -1; break;
            case '\\': if (buf_puts(b, "\\\\") < 0) return -1; break;
            case '\b': if (buf_puts(b, "\\b")  < 0) return -1; break;
            case '\f': if (buf_puts(b, "\\f")  < 0) return -1; break;
            case '\n': if (buf_puts(b, "\\n")  < 0) return -1; break;
            case '\r': if (buf_puts(b, "\\r")  < 0) return -1; break;
            case '\t': if (buf_puts(b, "\\t")  < 0) return -1; break;
            default:
                if (c < 0x20) {
                    char esc[7];
                    snprintf(esc, sizeof(esc), "\\u%04X", c);
                    if (buf_puts(b, esc) < 0) return -1;
                } else {
                    if (buf_push(b, (char)c) < 0) return -1;
                }
                break;
        }
    }
    return buf_push(b, '"');
}

/* Recursive serializer */

static int serialize(buf_t *b, const json_value_t *v, int indent, int depth)
{
    if (!v) return buf_puts(b, "null");

    switch (v->type) {
        case JSON_NULL:
            return buf_puts(b, "null");

        case JSON_BOOL:
            return buf_puts(b, v->v.boolean ? "true" : "false");

        case JSON_NUMBER: {
            char tmp[64];
            /* Use %g to avoid unnecessary trailing zeros, but ensure no
             * scientific notation for integers that fit in 64 bits */
            double d = v->v.number;
            if (d == (long long)d && d >= -1e15 && d <= 1e15)
                snprintf(tmp, sizeof(tmp), "%.0f", d);
            else
                snprintf(tmp, sizeof(tmp), "%.17g", d);
            return buf_puts(b, tmp);
        }

        case JSON_STRING:
            return write_escaped_string(b, v->v.string);

        case JSON_ARRAY: {
            if (buf_push(b, '[') < 0) return -1;
            for (size_t i = 0; i < v->v.array.count; i++) {
                if (i > 0 && buf_push(b, ',') < 0) return -1;
                if (indent >= 0) {
                    if (buf_push(b, '\n') < 0) return -1;
                    for (int d = 0; d <= depth; d++)
                        if (buf_puts(b, "  ") < 0) return -1;
                }
                if (serialize(b, v->v.array.items[i], indent, depth + 1) < 0)
                    return -1;
            }
            if (indent >= 0 && v->v.array.count > 0) {
                if (buf_push(b, '\n') < 0) return -1;
                for (int d = 0; d < depth; d++)
                    if (buf_puts(b, "  ") < 0) return -1;
            }
            return buf_push(b, ']');
        }

        case JSON_OBJECT: {
            if (buf_push(b, '{') < 0) return -1;
            const json_pair_t *pair = v->v.object.head;
            int first = 1;
            while (pair) {
                if (!first && buf_push(b, ',') < 0) return -1;
                first = 0;
                if (indent >= 0) {
                    if (buf_push(b, '\n') < 0) return -1;
                    for (int d = 0; d <= depth; d++)
                        if (buf_puts(b, "  ") < 0) return -1;
                }
                if (write_escaped_string(b, pair->key) < 0) return -1;
                if (buf_push(b, ':') < 0) return -1;
                if (indent >= 0 && buf_push(b, ' ') < 0) return -1;
                if (serialize(b, pair->value, indent, depth + 1) < 0) return -1;
                pair = pair->next;
            }
            if (indent >= 0 && v->v.object.count > 0) {
                if (buf_push(b, '\n') < 0) return -1;
                for (int d = 0; d < depth; d++)
                    if (buf_puts(b, "  ") < 0) return -1;
            }
            return buf_push(b, '}');
        }
    }
    return -1;
}

/* Public API */

/*
 * json_stringify  - serialize to compact JSON.
 * json_prettify   - serialize with 2-space indentation.
 * Both return a heap-allocated null-terminated string; caller must free().
 * Return NULL on OOM.
 */
char *json_stringify(const json_value_t *val)
{
    buf_t b;
    if (buf_init(&b) < 0) return NULL;
    if (serialize(&b, val, -1, 0) < 0) { free(b.data); return NULL; }
    if (buf_push(&b, '\0') < 0)        { free(b.data); return NULL; }
    return b.data;
}

char *json_prettify(const json_value_t *val)
{
    buf_t b;
    if (buf_init(&b) < 0) return NULL;
    if (serialize(&b, val, 0, 0) < 0) { free(b.data); return NULL; }
    if (buf_push(&b, '\0') < 0)       { free(b.data); return NULL; }
    return b.data;
}
