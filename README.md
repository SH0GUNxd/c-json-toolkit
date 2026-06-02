# json_parser

A JSON parser written from scratch in C99. No external dependencies, zero leaks.

## Features

- All JSON types: `null`, `bool`, `number`, `string`, `array`, `object`
- Full RFC 8259 compliance - rejects leading zeros, trailing commas, bare keywords, lone surrogates, control characters
- Unicode: `\uXXXX` escapes, surrogate pair decoding (`\uD83D\uDE00` → UTF-8)
- Precise error reporting with line and column
- Nesting depth guard (MAX_DEPTH = 512)
- `json_stringify()` / `json_prettify()` - serialization with round-trip guarantee
- `json_get("user.address.city")` - dot-path navigation including array indices (`"items.2.name"`)
- `json_schema_validate()` - subset of JSON Schema draft-07
- CLI tool: `jsonlint` - validate and pretty-print files
- Benchmark: parse + stringify throughput in MB/s
- Fuzz harness: AFL++ and libFuzzer compatible, with round-trip invariant checking
- 101 tests

## API

```c
// Parse
json_value_t *json_parse(const char *input, json_error_t *err);

// Navigate: dot-path, numeric segments index arrays
json_value_t *json_get(const json_value_t *root, const char *path);
json_value_t *json_array_get(const json_value_t *arr, size_t index);

// Serialize - caller must free()
char *json_stringify(const json_value_t *val);
char *json_prettify(const json_value_t *val);

// Schema validation (subset of draft-07)
int json_schema_validate(const json_value_t *schema,
                         const json_value_t *value,
                         json_schema_error_t *err);

// Free
void json_free(json_value_t *val);
```

## Usage

```c
#include "json.h"

json_error_t  err;
json_value_t *root = json_parse("{\"name\":\"Felix\",\"scores\":[98,75,88]}", &err);
if (!root) {
    fprintf(stderr, "%d:%d: %s\n", err.line, err.col, err.message);
    return 1;
}

json_value_t *name   = json_get(root, "name");
json_value_t *second = json_get(root, "scores.1");

char *out = json_prettify(root);
puts(out);
free(out);

json_free(root);
```

## Build

```sh
make              # builds libjson.a
make check        # 101 tests
make jsonlint     # CLI validator/pretty-printer
make bench        # throughput benchmark
make fuzz-standalone  # fuzz harness (reads stdin)
make fuzz         # libFuzzer (requires clang)
make fuzz-afl     # AFL++ (requires afl-clang-fast)
```

## CLI

```sh
./jsonlint file.json          # validate + pretty-print
./jsonlint -c file.json       # compact output
./jsonlint -q file.json       # quiet, exit code only
echo '{"x":1}' | ./jsonlint   # stdin
```

## Schema validation

Supported keywords: `type`, `properties`, `required`, `additionalProperties`,
`items`, `minItems`, `maxItems`, `minimum`, `maximum`, `minLength`, `maxLength`,
`enum`, `const`.

```c
json_value_t *schema = json_parse(
    "{\"type\":\"object\","
    " \"required\":[\"id\",\"name\"],"
    " \"properties\":{\"id\":{\"type\":\"integer\"},\"name\":{\"type\":\"string\"}}}", NULL);

json_schema_error_t serr;
if (!json_schema_validate(schema, value, &serr))
    fprintf(stderr, "%s: %s\n", serr.path, serr.message);
```

## Structure

```
src/json.h              public API
src/json_internal.h     internal types (token_t, parser_t)
src/json_schema.h       schema validator API
src/lexer.c             tokenizer with line/col tracking
src/unescape.c          string decoding, UTF-8, surrogate pairs
src/parser.c            recursive descent parser
src/json.c              json_parse / json_get / json_free
src/stringify.c         json_stringify / json_prettify
src/json_schema.c       schema validator
tools/jsonlint.c        CLI tool
tools/bench.c           throughput benchmark
tools/fuzz.c            AFL++ / libFuzzer harness
tests/test_json.c       101-test runner
corpus/                 fuzz seed inputs
```
