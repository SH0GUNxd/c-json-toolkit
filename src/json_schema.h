#ifndef JSON_SCHEMA_H
#define JSON_SCHEMA_H

#include "json.h"

/*
 * Minimal JSON Schema validator (subset of draft-07).
 *
 * Supported keywords:
 *   type, properties, required, additionalProperties,
 *   items, minItems, maxItems,
 *   minimum, maximum, minLength, maxLength,
 *   enum, const
 *
 * Usage:
 *   json_schema_error_t err;
 *   int ok = json_schema_validate(schema, value, &err);
 */

typedef struct {
    char path[256];     /* JSON pointer to the failing node, e.g. "/user/age" */
    char message[256];
} json_schema_error_t;

/*
 * Returns 1 if value conforms to schema, 0 otherwise.
 * Fills *err on failure (may be NULL).
 */
int json_schema_validate(const json_value_t *schema,
                         const json_value_t *value,
                         json_schema_error_t *err);

#endif /* JSON_SCHEMA_H */
