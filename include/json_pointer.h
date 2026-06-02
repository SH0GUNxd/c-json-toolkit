#ifndef JSON_POINTER_H
#define JSON_POINTER_H

#include "json.h"

/*
 * JSON Pointer  (RFC 6901)
 *
 * Syntax:  "" | "/" *( token "/" ) token
 * Examples:
 *   ""              → root document
 *   "/foo"          → root["foo"]
 *   "/foo/0"        → root["foo"][0]
 *   "/a~1b"         → key "a/b"   (~1 = /)
 *   "/a~0b"         → key "a~b"   (~0 = ~)
 *   "/-"            → (resolve only) past-the-end of array (write context)
 */

typedef struct {
    char message[256];
} json_pointer_error_t;

/*
 * json_pointer_get  - resolve a JSON Pointer against a document.
 *                     Returns NULL if pointer is invalid or path not found.
 */
json_value_t *json_pointer_get(const json_value_t *root,
                               const char         *pointer,
                               json_pointer_error_t *err);

/*
 * json_pointer_set  - set the value at the given pointer path.
 *                     The previous value is freed. "/-" appends to arrays.
 *                     Returns 1 on success, 0 on error.
 *                     root must be non-NULL; cannot replace the root itself.
 */
int json_pointer_set(json_value_t        *root,
                     const char          *pointer,
                     json_value_t        *value,
                     json_pointer_error_t *err);

/*
 * json_pointer_remove - remove the value at the given pointer path.
 *                       Returns 1 on success, 0 on error.
 */
int json_pointer_remove(json_value_t        *root,
                        const char          *pointer,
                        json_pointer_error_t *err);

#endif /* JSON_POINTER_H */
