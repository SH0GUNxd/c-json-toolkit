#include "json_schema.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "json_internal.h"

/* Error helpers */

static int fail(json_schema_error_t *err, const char *path, const char *fmt,
                ...)
{
    if (!err)
        return 0;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, ap);
    va_end(ap);
    snprintf(err->path, sizeof(err->path), "%s", path);
    return 0;
}

/* Forward declaration */

static int validate(const json_value_t *schema, const json_value_t *val,
                    json_schema_error_t *err, char *path);

/* Type check */

static int type_matches(const char *t, const json_value_t *val)
{
    if (strcmp(t, "null") == 0)
        return json_is_null(val);
    if (strcmp(t, "boolean") == 0)
        return json_is_bool(val);
    if (strcmp(t, "number") == 0)
        return json_is_number(val);
    if (strcmp(t, "integer") == 0)
        return json_is_number(val) && val->v.number == floor(val->v.number);
    if (strcmp(t, "string") == 0)
        return json_is_string(val);
    if (strcmp(t, "array") == 0)
        return json_is_array(val);
    if (strcmp(t, "object") == 0)
        return json_is_object(val);
    return 1;
}

/* Deep equality */

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

/* Schema key lookup */

static const json_value_t *sget(const json_value_t *schema, const char *key)
{
    if (!json_is_object(schema))
        return NULL;
    for (const json_pair_t *p = schema->v.object.head; p; p = p->next)
        if (strcmp(p->key, key) == 0)
            return p->value;
    return NULL;
}

/* Core validator */

static int validate(const json_value_t *schema, const json_value_t *val,
                    json_schema_error_t *err, char *path)
{
    if (!json_is_object(schema))
        return 1;

    const json_value_t *node;

    /* type */
    if ((node = sget(schema, "type")) && json_is_string(node))
        if (!type_matches(node->v.string, val))
            return fail(err, path, "expected type '%s'", node->v.string);

    /* const */
    if ((node = sget(schema, "const")))
        if (!values_equal(val, node))
            return fail(err, path, "value does not match const");

    /* enum */
    if ((node = sget(schema, "enum")) && json_is_array(node))
    {
        int found = 0;
        for (size_t i = 0; i < node->v.array.count; i++)
            if (values_equal(val, node->v.array.items[i]))
            {
                found = 1;
                break;
            }
        if (!found)
            return fail(err, path, "value not in enum");
    }

    /* string: minLength / maxLength */
    if (json_is_string(val))
    {
        size_t slen = strlen(val->v.string);
        if ((node = sget(schema, "minLength")) && json_is_number(node)
            && slen < (size_t)node->v.number)
            return fail(err, path, "string length %zu < minLength %.0f", slen,
                        node->v.number);
        if ((node = sget(schema, "maxLength")) && json_is_number(node)
            && slen > (size_t)node->v.number)
            return fail(err, path, "string length %zu > maxLength %.0f", slen,
                        node->v.number);
    }

    /* number: minimum / maximum */
    if (json_is_number(val))
    {
        if ((node = sget(schema, "minimum")) && json_is_number(node)
            && val->v.number < node->v.number)
            return fail(err, path, "%.17g < minimum %.17g", val->v.number,
                        node->v.number);
        if ((node = sget(schema, "maximum")) && json_is_number(node)
            && val->v.number > node->v.number)
            return fail(err, path, "%.17g > maximum %.17g", val->v.number,
                        node->v.number);
    }

    /* array: minItems / maxItems / items */
    if (json_is_array(val))
    {
        size_t count = val->v.array.count;
        if ((node = sget(schema, "minItems")) && json_is_number(node)
            && count < (size_t)node->v.number)
            return fail(err, path, "array length %zu < minItems %.0f", count,
                        node->v.number);
        if ((node = sget(schema, "maxItems")) && json_is_number(node)
            && count > (size_t)node->v.number)
            return fail(err, path, "array length %zu > maxItems %.0f", count,
                        node->v.number);
        if ((node = sget(schema, "items")))
        {
            for (size_t i = 0; i < count; i++)
            {
                char cp[512];
                snprintf(cp, sizeof(cp), "%s/%zu", path, i);
                if (!validate(node, val->v.array.items[i], err, cp))
                    return 0;
            }
        }
    }

    /* object: required / properties / additionalProperties */
    if (json_is_object(val))
    {
        const json_value_t *req = sget(schema, "required");
        if (req && json_is_array(req))
        {
            for (size_t i = 0; i < req->v.array.count; i++)
            {
                const json_value_t *rk = req->v.array.items[i];
                if (!json_is_string(rk))
                    continue;
                int found = 0;
                for (const json_pair_t *p = val->v.object.head; p; p = p->next)
                    if (strcmp(p->key, rk->v.string) == 0)
                    {
                        found = 1;
                        break;
                    }
                if (!found)
                    return fail(err, path, "missing required property '%s'",
                                rk->v.string);
            }
        }

        const json_value_t *props = sget(schema, "properties");
        if (props && json_is_object(props))
        {
            for (const json_pair_t *vp = val->v.object.head; vp; vp = vp->next)
            {
                const json_value_t *ps = NULL;
                for (const json_pair_t *sp = props->v.object.head; sp;
                     sp = sp->next)
                    if (strcmp(sp->key, vp->key) == 0)
                    {
                        ps = sp->value;
                        break;
                    }
                if (ps)
                {
                    char cp[512];
                    snprintf(cp, sizeof(cp), "%s/%s", path, vp->key);
                    if (!validate(ps, vp->value, err, cp))
                        return 0;
                }
            }
        }

        const json_value_t *addl = sget(schema, "additionalProperties");
        if (addl && json_is_bool(addl) && !addl->v.boolean && props)
        {
            for (const json_pair_t *vp = val->v.object.head; vp; vp = vp->next)
            {
                int known = 0;
                for (const json_pair_t *sp = props->v.object.head; sp;
                     sp = sp->next)
                    if (strcmp(sp->key, vp->key) == 0)
                    {
                        known = 1;
                        break;
                    }
                if (!known)
                    return fail(err, path,
                                "additional property '%s' not allowed",
                                vp->key);
            }
        }
    }

    return 1;
}

/* Public API */

int json_schema_validate(const json_value_t *schema, const json_value_t *value,
                         json_schema_error_t *err)
{
    if (err)
    {
        err->path[0] = '\0';
        err->message[0] = '\0';
    }
    char root_path[2] = "";
    return validate(schema, value, err, root_path);
}
