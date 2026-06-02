#include "json_internal.h"
#include "json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* json_parse */

json_value_t *json_parse(const char *input, json_error_t *err)
{
    if (!input) {
        if (err) {
            snprintf(err->message, sizeof(err->message), "null input");
            err->line = 0; err->col = 0;
        }
        return NULL;
    }

    parser_t p;
    lexer_init(&p, input, err);

    if (p.cur.type == TOK_ERROR) return NULL;

    json_value_t *root = parse_value(&p);
    if (!root) return NULL;

    /* Consume trailing whitespace; root value must be the entire input */
    if (p.cur.type != TOK_EOF) {
        set_error(&p, p.cur.line, p.cur.col,
                  "unexpected trailing content '%s' at %d:%d",
                  tok_type_name(p.cur.type), p.cur.line, p.cur.col);
        json_free(root);
        return NULL;
    }

    return root;
}

/* json_get
 *  Dot-path navigation. Segments split on '.'.
 *  Numeric segments index into arrays: "items.2.name"
*/

static int is_uint(const char *s, size_t *out)
{
    if (!*s) return 0;
    size_t v = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (size_t)(*s - '0');
    }
    *out = v;
    return 1;
}

json_value_t *json_get(const json_value_t *root, const char *path)
{
    if (!root || !path) return NULL;

    char *buf = (char *)malloc(strlen(path) + 1);
    if (!buf) return NULL;
    strcpy(buf, path);

    const json_value_t *cur = root;
    char               *seg = buf;

    while (seg && *seg) {
        char *dot = strchr(seg, '.');
        if (dot) *dot = '\0';

        size_t idx;
        if (is_uint(seg, &idx)) {
            if (cur->type != JSON_ARRAY || idx >= cur->v.array.count) {
                cur = NULL; break;
            }
            cur = cur->v.array.items[idx];
        } else {
            if (cur->type != JSON_OBJECT) { cur = NULL; break; }
            const json_pair_t *pair = cur->v.object.head;
            cur = NULL;
            while (pair) {
                if (strcmp(pair->key, seg) == 0) { cur = pair->value; break; }
                pair = pair->next;
            }
        }
        if (!cur) break;
        seg = dot ? dot + 1 : NULL;
    }

    free(buf);
    return (json_value_t *)cur;
}

/* json_array_get */

json_value_t *json_array_get(const json_value_t *arr, size_t index)
{
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    if (index >= arr->v.array.count)     return NULL;
    return arr->v.array.items[index];
}

/* json_free */

void json_free(json_value_t *val)
{
    if (!val) return;

    switch (val->type) {
        case JSON_STRING:
            free(val->v.string);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < val->v.array.count; i++)
                json_free(val->v.array.items[i]);
            free(val->v.array.items);
            break;
        case JSON_OBJECT: {
            json_pair_t *pair = val->v.object.head;
            while (pair) {
                json_pair_t *next = pair->next;
                free(pair->key);
                json_free(pair->value);
                free(pair);
                pair = next;
            }
            break;
        }
        case JSON_NULL:
        case JSON_BOOL:
        case JSON_NUMBER:
            break;
    }

    free(val);
}
