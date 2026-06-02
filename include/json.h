#ifndef JSON_H
#define JSON_H

#include <stddef.h>

/* Types */

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

typedef struct json_value json_value_t;
typedef struct json_pair  json_pair_t;

struct json_pair {
    char         *key;
    json_value_t *value;
    json_pair_t  *next;
};

struct json_value {
    json_type_t type;
    union {
        int           boolean;   /* JSON_BOOL   */
        double        number;    /* JSON_NUMBER */
        char         *string;    /* JSON_STRING */
        struct {                 /* JSON_ARRAY  */
            json_value_t **items;
            size_t         count;
        } array;
        struct {                 /* JSON_OBJECT */
            json_pair_t *head;
            size_t       count;
        } object;
    } v;
};

/* Error */

typedef struct {
    char   message[256];
    int    line;
    int    col;
} json_error_t;

/* API */

/*
 * json_parse  - Parse a null-terminated JSON string.
 *               Returns a heap-allocated json_value_t on success, NULL on error.
 *               Fills *err if err != NULL.
 */
json_value_t *json_parse(const char *input, json_error_t *err);

/*
 * json_get    - Navigate an object tree with a dot-separated key path.
 *               e.g. json_get(root, "user.address.city")
 *               Returns NULL if path not found or intermediate node isn't an object.
 */
json_value_t *json_get(const json_value_t *root, const char *path);

/*
 * json_array_get - Access array element by index. Returns NULL if out of bounds.
 */
json_value_t *json_array_get(const json_value_t *arr, size_t index);

/*
 * json_free   - Recursively free a json_value_t tree.
 */
void json_free(json_value_t *val);

/*
 * json_stringify - Serialize to compact JSON string. Caller must free().
 * json_prettify  - Serialize with 2-space indentation. Caller must free().
 */
char *json_stringify(const json_value_t *val);
char *json_prettify(const json_value_t *val);

/* Convenience predicates */
#define json_is_null(v)   ((v) && (v)->type == JSON_NULL)
#define json_is_bool(v)   ((v) && (v)->type == JSON_BOOL)
#define json_is_number(v) ((v) && (v)->type == JSON_NUMBER)
#define json_is_string(v) ((v) && (v)->type == JSON_STRING)
#define json_is_array(v)  ((v) && (v)->type == JSON_ARRAY)
#define json_is_object(v) ((v) && (v)->type == JSON_OBJECT)

#endif /* JSON_H */
