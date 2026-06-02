#include "json_pointer.h"
#include "json_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

/* Error helper */

static void ptr_err(json_pointer_error_t *err, const char *fmt, ...)
{
    if (!err) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, ap);
    va_end(ap);
}

/* Token unescaping: ~1 → / ~0 → ~ */

static char *unescape_token(const char *tok, size_t len)
{
    char  *out = (char *)malloc(len + 1);
    size_t wi  = 0;
    if (!out) return NULL;

    for (size_t i = 0; i < len; i++) {
        if (tok[i] == '~' && i + 1 < len) {
            if      (tok[i+1] == '1') { out[wi++] = '/'; i++; }
            else if (tok[i+1] == '0') { out[wi++] = '~'; i++; }
            else                       { out[wi++] = tok[i]; }
        } else {
            out[wi++] = tok[i];
        }
    }
    out[wi] = '\0';
    return out;
}

/* Parse a single numeric token → array index */

static int parse_index(const char *tok, size_t *out)
{
    if (!tok || !*tok) return 0;
    /* RFC 6901: no leading zeros except "0" itself */
    if (tok[0] == '0' && tok[1] != '\0') return 0;
    size_t v = 0;
    for (const char *p = tok; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
        v = v * 10 + (size_t)(*p - '0');
    }
    *out = v;
    return 1;
}

/* Segment iterator */

typedef struct {
    const char *ptr;   /* current position in the pointer string */
} seg_iter_t;

/* Returns heap-allocated unescaped token, sets *is_end, NULL on done */
static char *next_segment(seg_iter_t *it, int *is_end)
{
    *is_end = 0;
    if (!it->ptr || *it->ptr == '\0') { *is_end = 1; return NULL; }
    /* skip leading '/' */
    if (*it->ptr == '/') it->ptr++;

    const char *start = it->ptr;
    while (*it->ptr && *it->ptr != '/') it->ptr++;
    size_t len = (size_t)(it->ptr - start);
    *is_end = (*it->ptr == '\0');
    return unescape_token(start, len);
}

/* json_pointer_get */

json_value_t *json_pointer_get(const json_value_t   *root,
                               const char           *pointer,
                               json_pointer_error_t *err)
{
    if (!root || !pointer) {
        ptr_err(err, "null argument");
        return NULL;
    }
    /* empty pointer = root */
    if (*pointer == '\0') return (json_value_t *)root;

    if (*pointer != '/') {
        ptr_err(err, "JSON Pointer must start with '/' or be empty");
        return NULL;
    }

    const json_value_t *cur = root;
    seg_iter_t          it  = { pointer };
    int                 end = 0;

    for (;;) {
        char *tok = next_segment(&it, &end);
        if (!tok) break;

        if (json_is_array(cur)) {
            size_t idx;
            if (!parse_index(tok, &idx)) {
                ptr_err(err, "invalid array index '%s'", tok);
                free(tok); return NULL;
            }
            if (idx >= cur->v.array.count) {
                ptr_err(err, "array index %zu out of bounds (len=%zu)",
                        idx, cur->v.array.count);
                free(tok); return NULL;
            }
            cur = cur->v.array.items[idx];
        } else if (json_is_object(cur)) {
            const json_pair_t *p = cur->v.object.head;
            cur = NULL;
            while (p) {
                if (strcmp(p->key, tok) == 0) { cur = p->value; break; }
                p = p->next;
            }
            if (!cur) {
                ptr_err(err, "key '%s' not found", tok);
                free(tok); return NULL;
            }
        } else {
            ptr_err(err, "cannot index into non-container at '%s'", tok);
            free(tok); return NULL;
        }

        free(tok);
        if (end) break;
    }

    return (json_value_t *)cur;
}

/* json_pointer_set */

int json_pointer_set(json_value_t        *root,
                     const char          *pointer,
                     json_value_t        *value,
                     json_pointer_error_t *err)
{
    if (!root || !pointer || !value) {
        ptr_err(err, "null argument");
        return 0;
    }
    if (*pointer == '\0') {
        ptr_err(err, "cannot replace root via json_pointer_set");
        return 0;
    }
    if (*pointer != '/') {
        ptr_err(err, "JSON Pointer must start with '/'");
        return 0;
    }

    /* Walk to the parent, then apply the last token */
    json_value_t *cur   = root;
    seg_iter_t    it    = { pointer };
    char         *prev  = NULL;   /* previous token (= last key/index) */
    int           end   = 0;

    for (;;) {
        char *tok = next_segment(&it, &end);
        if (!tok) break;
        free(prev);
        prev = tok;
        if (end) break;

        /* advance cur */
        if (json_is_array(cur)) {
            size_t idx;
            if (!parse_index(tok, &idx) || idx >= cur->v.array.count) {
                ptr_err(err, "array index '%s' invalid or out of bounds", tok);
                free(prev); return 0;
            }
            cur = cur->v.array.items[idx];
        } else if (json_is_object(cur)) {
            json_pair_t *p = cur->v.object.head;
            cur = NULL;
            while (p) {
                if (strcmp(p->key, tok) == 0) { cur = p->value; break; }
                p = p->next;
            }
            if (!cur) {
                ptr_err(err, "key '%s' not found", tok);
                free(prev); return 0;
            }
        } else {
            ptr_err(err, "cannot index into non-container");
            free(prev); return 0;
        }
    }

    if (!prev) { ptr_err(err, "empty pointer after parsing"); return 0; }

    /* Apply at parent=cur, key=prev */
    if (json_is_array(cur)) {
        if (strcmp(prev, "-") == 0) {
            /* append */
            size_t nc = cur->v.array.count + 1;
            json_value_t **tmp = (json_value_t **)realloc(
                cur->v.array.items, nc * sizeof(json_value_t *));
            if (!tmp) { ptr_err(err, "out of memory"); free(prev); return 0; }
            cur->v.array.items = tmp;
            cur->v.array.items[cur->v.array.count] = value;
            cur->v.array.count = nc;
        } else {
            size_t idx;
            if (!parse_index(prev, &idx) || idx >= cur->v.array.count) {
                ptr_err(err, "array index '%s' out of bounds", prev);
                free(prev); return 0;
            }
            json_free(cur->v.array.items[idx]);
            cur->v.array.items[idx] = value;
        }
    } else if (json_is_object(cur)) {
        /* update existing key */
        for (json_pair_t *p = cur->v.object.head; p; p = p->next) {
            if (strcmp(p->key, prev) == 0) {
                json_free(p->value);
                p->value = value;
                free(prev);
                return 1;
            }
        }
        /* add new key */
        json_pair_t *pair = (json_pair_t *)calloc(1, sizeof(json_pair_t));
        if (!pair) { ptr_err(err, "out of memory"); free(prev); return 0; }
        pair->key   = prev;  /* transfer ownership */
        pair->value = value;
        /* append to tail */
        if (!cur->v.object.head) {
            cur->v.object.head = pair;
        } else {
            json_pair_t *tail = cur->v.object.head;
            while (tail->next) tail = tail->next;
            tail->next = pair;
        }
        cur->v.object.count++;
        return 1;
    } else {
        ptr_err(err, "cannot set on non-container");
        free(prev); return 0;
    }

    free(prev);
    return 1;
}

/* json_pointer_remove */

int json_pointer_remove(json_value_t        *root,
                        const char          *pointer,
                        json_pointer_error_t *err)
{
    if (!root || !pointer) { ptr_err(err, "null argument"); return 0; }
    if (*pointer == '\0')  { ptr_err(err, "cannot remove root"); return 0; }
    if (*pointer != '/')   { ptr_err(err, "pointer must start with '/'"); return 0; }

    json_value_t *cur  = root;
    seg_iter_t    it   = { pointer };
    char         *prev = NULL;
    int           end  = 0;

    for (;;) {
        char *tok = next_segment(&it, &end);
        if (!tok) break;
        free(prev);
        prev = tok;
        if (end) break;

        if (json_is_array(cur)) {
            size_t idx;
            if (!parse_index(tok, &idx) || idx >= cur->v.array.count) {
                ptr_err(err, "array index '%s' invalid", tok);
                free(prev); return 0;
            }
            cur = cur->v.array.items[idx];
        } else if (json_is_object(cur)) {
            json_pair_t *p = cur->v.object.head;
            cur = NULL;
            while (p) {
                if (strcmp(p->key, tok) == 0) { cur = p->value; break; }
                p = p->next;
            }
            if (!cur) { ptr_err(err, "key '%s' not found", tok); free(prev); return 0; }
        } else {
            ptr_err(err, "cannot index into non-container");
            free(prev); return 0;
        }
    }

    if (!prev) { ptr_err(err, "empty pointer"); return 0; }

    if (json_is_array(cur)) {
        size_t idx;
        if (!parse_index(prev, &idx) || idx >= cur->v.array.count) {
            ptr_err(err, "array index '%s' out of bounds", prev);
            free(prev); return 0;
        }
        json_free(cur->v.array.items[idx]);
        /* shift left */
        for (size_t i = idx; i + 1 < cur->v.array.count; i++)
            cur->v.array.items[i] = cur->v.array.items[i+1];
        cur->v.array.count--;
    } else if (json_is_object(cur)) {
        json_pair_t *p    = cur->v.object.head;
        json_pair_t *prev_p = NULL;
        while (p) {
            if (strcmp(p->key, prev) == 0) {
                if (prev_p) prev_p->next = p->next;
                else        cur->v.object.head = p->next;
                free(p->key);
                json_free(p->value);
                free(p);
                cur->v.object.count--;
                free(prev);
                return 1;
            }
            prev_p = p;
            p = p->next;
        }
        ptr_err(err, "key '%s' not found", prev);
        free(prev); return 0;
    } else {
        ptr_err(err, "cannot remove from non-container");
        free(prev); return 0;
    }

    free(prev);
    return 1;
}
