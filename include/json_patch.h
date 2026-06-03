#ifndef JSON_PATCH_H
#define JSON_PATCH_H

#include "json.h"

/*
 * JSON Patch  (RFC 6902)
 *
 * A patch is a JSON array of operation objects, each with an "op" field.
 * Supported operations: add, remove, replace, move, copy, test
 *
 * Example patch:
 *   [
 *     { "op": "replace", "path": "/name", "value": "Bob" },
 *     { "op": "add",     "path": "/tags/-", "value": "admin" },
 *     { "op": "remove",  "path": "/tmp" }
 *   ]
 *
 * json_patch_apply operates on a deep copy of the document so the
 * original is never modified on failure (atomic semantics).
 */

typedef struct
{
    char message[256];
    int op_index; /* index of the failing operation, -1 if pre-flight */
} json_patch_error_t;

/*
 * json_patch_apply  - apply a JSON Patch array to a document.
 *
 * Returns a newly allocated patched document on success.
 * Returns NULL on error and fills *err if provided.
 * The original document is never modified.
 */
json_value_t *json_patch_apply(const json_value_t *doc,
                               const json_value_t *patch,
                               json_patch_error_t *err);

/*
 * json_clone  - deep copy a json_value_t tree. Returns NULL on OOM.
 *               (Also useful standalone.)
 */
json_value_t *json_clone(const json_value_t *val);

#endif /* JSON_PATCH_H */
