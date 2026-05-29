# json_parser

A JSON parser written from scratch in C99 strict. No external dependencies.

## Features

- All JSON types: `null`, `bool`, `number`, `string`, `array`, `object`
- Full RFC 8259 compliance: rejects leading zeros, trailing commas, bare keywords, lone surrogates, control characters in strings
- Unicode: `\uXXXX` escapes, surrogate pair decoding (`\uD83D\uDE00` → UTF-8)
- Precise error reporting with line and column
- Nesting depth guard (MAX_DEPTH = 512)
- Zero leaks - `json_free()` is fully recursive
- 69 tests covering edge cases, malformed input, deep nesting, and real-world JSON

## API

```c
// Parse a null-terminated JSON string.
// Returns NULL on error and fills *err if provided.
json_value_t *json_parse(const char *input, json_error_t *err);

// Navigate an object tree with a dot-separated key path.
// e.g. json_get(root, "user.address.city")
json_value_t *json_get(const json_value_t *root, const char *path);

// Access array element by index.
json_value_t *json_array_get(const json_value_t *arr, size_t index);

// Recursively free a json_value_t tree.
void json_free(json_value_t *val);
```

## Usage

```c
#include "json.h"

json_error_t err;
json_value_t *root = json_parse("{\"name\":\"Felix\",\"score\":98.5}", &err);
if (!root) {
    fprintf(stderr, "parse error at %d:%d: %s\n", err.line, err.col, err.message);
    return 1;
}

json_value_t *name = json_get(root, "name");
if (json_is_string(name))
    printf("%s\n", name->v.string); // Felix

json_free(root);
```

## Build

```sh
make        # builds libjson.a
make check  # compiles and runs the test suite
```

Requires: `cc` with C99 support, `make`.

## Structure

```
src/json.h              public API
src/json_internal.h     internal types (token_t, parser_t)
src/lexer.c             tokenizer with line/col tracking
src/unescape.c          string decoding, UTF-8 encoding, surrogate pairs
src/parser.c            recursive descent parser
src/json.c              json_parse / json_get / json_free
tests/test_json.c       test runner (self-contained)
```
