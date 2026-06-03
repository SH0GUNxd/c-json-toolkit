#include "json_patch.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_internal.h"
#include "json_pointer.h"

/* json_clone */

json_value_t *json_clone(const json_value_t *val)
{
    if (!val)
        return NULL;

    json_value_t *out = (json_value_t *)calloc(1, sizeof(json_value_t));
    if (!out)
        return NULL;
    out->type = val->type;

    switch (val->type)
    {
    case JSON_NULL:
    case JSON_BOOL:
        out->v.boolean = val->v.boolean;
        break;

    case JSON_NUMBER:
        out->v.number = val->v.number;
        break;

    case JSON_STRING:
        out->v.string = (char *)malloc(strlen(val->v.string) + 1);
        if (!out->v.string)
        {
            free(out);
            return NULL;
        }
        strcpy(out->v.string, val->v.string);
        break;

    case JSON_ARRAY: {
        size_t n = val->v.array.count;
        out->v.array.items =
            (json_value_t **)calloc(n ? n : 1, sizeof(json_value_t *));
        if (!out->v.array.items)
        {
            free(out);
            return NULL;
        }
        out->v.array.count = n;
        for (size_t i = 0; i < n; i++)
        {
            out->v.array.items[i] = json_clone(val->v.array.items[i]);
            if (!out->v.array.items[i])
            {
                for (size_t j = 0; j < i; j++)
                    json_free(out->v.array.items[j]);
                free(out->v.array.items);
                free(out);
                return NULL;
            }
        }
        break;
    }

    case JSON_OBJECT: {
        json_pair_t *tail = NULL;
        for (const json_pair_t *p = val->v.object.head; p; p = p->next)
        {
            json_pair_t *np = (json_pair_t *)calloc(1, sizeof(json_pair_t));
            if (!np)
            {
                json_free(out);
                return NULL;
            }
            np->key = (char *)malloc(strlen(p->key) + 1);
            if (!np->key)
            {
                free(np);
                json_free(out);
                return NULL;
            }
            strcpy(np->key, p->key);
            np->value = json_clone(p->value);
            if (!np->value)
            {
                free(np->key);
                free(np);
                json_free(out);
                return NULL;
            }
            if (!out->v.object.head)
                out->v.object.head = np;
            else
                tail->next = np;
            tail = np;
            out->v.object.count++;
        }
        break;
    }
    }

    return out;
}

/* Error helper */

static void patch_err(json_patch_error_t *err, int idx, const char *fmt, ...)
{
    if (!err)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, ap);
    va_end(ap);
    err->op_index = idx;
}

/* Object key lookup */

static const json_value_t *obj_get(const json_value_t *obj, const char *key)
{
    if (!json_is_object(obj))
        return NULL;
    for (const json_pair_t *p = obj->v.object.head; p; p = p->next)
        if (strcmp(p->key, key) == 0)
            return p->value;
    return NULL;
}

/* Deep equality (reused from schema, exposed here) */

static int values_equal(const json_value_t *a, const json_value_t *b)
{
    if (!a || !b || a->type != b->type)
        return 0;
    switch (a->type)
    {
    case JSON_NULL:
        return 1;
    case JSON_BOOL:
        return a->v.boolean == b->v.boolean;
    case JSON_NUMBER:
        return a->v.number == b->v.number;
    case JSON_STRING:
        return strcmp(a->v.string, b->v.string) == 0;
    case JSON_ARRAY:
        if (a->v.array.count != b->v.array.count)
            return 0;
        for (size_t i = 0; i < a->v.array.count; i++)
            if (!values_equal(a->v.array.items[i], b->v.array.items[i]))
                return 0;
        return 1;
    case JSON_OBJECT: {
        if (a->v.object.count != b->v.object.count)
            return 0;
        for (const json_pair_t *p = a->v.object.head; p; p = p->next)
        {
            int found = 0;
            for (const json_pair_t *q = b->v.object.head; q; q = q->next)
                if (strcmp(p->key, q->key) == 0
                    && values_equal(p->value, q->value))
                {
                    found = 1;
                    break;
                }
            if (!found)
                return 0;
        }
        return 1;
    }
    }
    return 0;
}

/* Apply one operation */

static int apply_op(json_value_t *doc, const json_value_t *op, int idx,
                    json_patch_error_t *err)
{
    const json_value_t *op_node = obj_get(op, "op");
    const json_value_t *path_node = obj_get(op, "path");

    if (!op_node || !json_is_string(op_node))
    {
        patch_err(err, idx, "operation %d: missing or non-string 'op'", idx);
        return 0;
    }
    if (!path_node || !json_is_string(path_node))
    {
        patch_err(err, idx, "operation %d: missing or non-string 'path'", idx);
        return 0;
    }

    const char *op_str = op_node->v.string;
    const char *path_str = path_node->v.string;
    json_pointer_error_t perr;

    /* add */
    if (strcmp(op_str, "add") == 0)
    {
        const json_value_t *val = obj_get(op, "value");
        if (!val)
        {
            patch_err(err, idx, "op 'add': missing 'value'");
            return 0;
        }
        json_value_t *clone = json_clone(val);
        if (!clone)
        {
            patch_err(err, idx, "op 'add': out of memory");
            return 0;
        }
        if (*path_str == '\0')
        {
            patch_err(err, idx, "op 'add': cannot add to root with empty path");
            json_free(clone);
            return 0;
        }
        if (!json_pointer_set(doc, path_str, clone, &perr))
        {
            patch_err(err, idx, "op 'add': %s", perr.message);
            json_free(clone);
            return 0;
        }
        return 1;
    }

    /* remove */
    if (strcmp(op_str, "remove") == 0)
    {
        if (!json_pointer_remove(doc, path_str, &perr))
        {
            patch_err(err, idx, "op 'remove': %s", perr.message);
            return 0;
        }
        return 1;
    }

    /* replace */
    if (strcmp(op_str, "replace") == 0)
    {
        const json_value_t *val = obj_get(op, "value");
        if (!val)
        {
            patch_err(err, idx, "op 'replace': missing 'value'");
            return 0;
        }
        /* target must exist */
        if (!json_pointer_get(doc, path_str, &perr))
        {
            patch_err(err, idx, "op 'replace': target not found: %s",
                      perr.message);
            return 0;
        }
        json_value_t *clone = json_clone(val);
        if (!clone)
        {
            patch_err(err, idx, "op 'replace': out of memory");
            return 0;
        }
        if (!json_pointer_set(doc, path_str, clone, &perr))
        {
            patch_err(err, idx, "op 'replace': %s", perr.message);
            json_free(clone);
            return 0;
        }
        return 1;
    }

    /* move */
    if (strcmp(op_str, "move") == 0)
    {
        const json_value_t *from_node = obj_get(op, "from");
        if (!from_node || !json_is_string(from_node))
        {
            patch_err(err, idx, "op 'move': missing 'from'");
            return 0;
        }
        const char *from_str = from_node->v.string;
        json_value_t *src = json_pointer_get(doc, from_str, &perr);
        if (!src)
        {
            patch_err(err, idx, "op 'move': source not found: %s",
                      perr.message);
            return 0;
        }
        json_value_t *clone = json_clone(src);
        if (!clone)
        {
            patch_err(err, idx, "op 'move': out of memory");
            return 0;
        }
        if (!json_pointer_remove(doc, from_str, &perr))
        {
            patch_err(err, idx, "op 'move': remove failed: %s", perr.message);
            json_free(clone);
            return 0;
        }
        if (!json_pointer_set(doc, path_str, clone, &perr))
        {
            patch_err(err, idx, "op 'move': set failed: %s", perr.message);
            json_free(clone);
            return 0;
        }
        return 1;
    }

    /* copy */
    if (strcmp(op_str, "copy") == 0)
    {
        const json_value_t *from_node = obj_get(op, "from");
        if (!from_node || !json_is_string(from_node))
        {
            patch_err(err, idx, "op 'copy': missing 'from'");
            return 0;
        }
        json_value_t *src = json_pointer_get(doc, from_node->v.string, &perr);
        if (!src)
        {
            patch_err(err, idx, "op 'copy': source not found: %s",
                      perr.message);
            return 0;
        }
        json_value_t *clone = json_clone(src);
        if (!clone)
        {
            patch_err(err, idx, "op 'copy': out of memory");
            return 0;
        }
        if (!json_pointer_set(doc, path_str, clone, &perr))
        {
            patch_err(err, idx, "op 'copy': %s", perr.message);
            json_free(clone);
            return 0;
        }
        return 1;
    }

    /* test */
    if (strcmp(op_str, "test") == 0)
    {
        const json_value_t *val = obj_get(op, "value");
        if (!val)
        {
            patch_err(err, idx, "op 'test': missing 'value'");
            return 0;
        }
        json_value_t *target = json_pointer_get(doc, path_str, &perr);
        if (!target)
        {
            patch_err(err, idx, "op 'test': path not found: %s", perr.message);
            return 0;
        }
        if (!values_equal(target, val))
        {
            patch_err(err, idx, "op 'test': value mismatch at '%s'", path_str);
            return 0;
        }
        return 1;
    }

    patch_err(err, idx, "unknown op '%s'", op_str);
    return 0;
}

/* json_patch_apply */

json_value_t *json_patch_apply(const json_value_t *doc,
                               const json_value_t *patch,
                               json_patch_error_t *err)
{
    if (err)
    {
        err->message[0] = '\0';
        err->op_index = -1;
    }

    if (!json_is_array(patch))
    {
        patch_err(err, -1, "patch must be a JSON array");
        return NULL;
    }

    /* Work on a deep copy - atomic: original untouched on failure */
    json_value_t *working = json_clone(doc);
    if (!working)
    {
        patch_err(err, -1, "out of memory");
        return NULL;
    }

    for (size_t i = 0; i < patch->v.array.count; i++)
    {
        const json_value_t *op = patch->v.array.items[i];
        if (!json_is_object(op))
        {
            patch_err(err, (int)i, "operation %zu is not an object", i);
            json_free(working);
            return NULL;
        }
        if (!apply_op(working, op, (int)i, err))
        {
            json_free(working);
            return NULL;
        }
    }

    return working;
}
