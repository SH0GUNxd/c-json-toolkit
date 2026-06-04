# json_parser - JSON Parser in C

**Author:** Félix Vandenbroucke · Dev 2026

A JSON parser written from scratch in strict C99, with no external dependencies. It features a handwritten recursive-descent lexer/parser, full RFC 8259 compliance, serialization, schema validation, JSON Pointer, and JSON Patch support. Includes 152 tests, zero leaks, and GitHub Actions CI.

---

## Features

- Parses all JSON types: `null`, `bool`, `number`, `string`, `array`, `object`
- **RFC 8259 compliance**: rejects leading zeros, trailing commas, bare keywords, isolated surrogates, and control characters
- **Unicode support**: `\uXXXX` escapes, surrogate pair decoding (`\uD83D\uDE00` → UTF-8)
- **Precise errors**: message + line + column
- **Max depth guard**: 512 levels
- **`json_stringify()` / `json_prettify()`**: serialization with round-trip guarantee
- **`json_get("user.address.city")`**: dot-path navigation with array indexing (`"items.2.name"`)
- **`json_schema_validate()`**: JSON Schema draft-07 subset
- **JSON Pointer** (RFC 6901): get / set / remove via `/foo/0/bar`
- **JSON Patch** (RFC 6902): `add`, `remove`, `replace`, `move`, `copy`, `test` — atomic
- **`json_clone()`**: deep copy
- **`jsonlint` CLI**: validate and pretty-print JSON files
- **Benchmark**: `json_parse` + `json_stringify` throughput in MB/s
- **Fuzz harness**: AFL++ and libFuzzer compatible, checks round-trip behavior

---

## Stack

| Layer       | Technology                                      |
|-------------|--------------------------------------------------|
| Language    | Strict C99, `-pedantic -Wall -Wextra -Werror`    |
| Parser      | Handwritten recursive-descent lexer/parser       |
| Tests       | Custom runner, no external dependencies         |
| Fuzzing     | AFL++ / libFuzzer                                |
| CI          | GitHub Actions (gcc + clang + valgrind)          |

---

## Project Structure

```text
json_parser/
├── include/                Public API
│   ├── json.h
│   ├── json_schema.h
│   ├── json_pointer.h
│   └── json_patch.h
├── src/                    Internal implementation
│   ├── json_internal.h     internal types (token_t, parser_t)
│   ├── lexer.c             tokenizer with line/column tracking
│   ├── unescape.c          string decoding, UTF-8, surrogate pairs
│   ├── parser.c            recursive-descent parser
│   ├── json.c              json_parse / json_get / json_free
│   ├── stringify.c         json_stringify / json_prettify
│   ├── json_schema.c       schema validator
│   ├── json_pointer.c      JSON Pointer (RFC 6901)
│   └── json_patch.c        JSON Patch (RFC 6902) + json_clone()
├── tests/
│   ├── test_framework.h    tiny testing framework macros and shared state
│   ├── test_parser.c       primitives, strings, arrays, objects, errors, whitespace
│   ├── test_get.c          dot-path, array index, nested
│   ├── test_stringify.c    stringify, prettify, round-trip
│   ├── test_schema.c       schema validator
│   ├── test_pointer.c      JSON Pointer
│   ├── test_patch.c        JSON Patch, clone
│   └── test_runner.c       main() that calls all suites
├── tools/
│   ├── jsonlint.c          CLI: validate and pretty-print
│   ├── bench.c             throughput benchmark
│   └── fuzz.c              AFL++ / libFuzzer harness
├── corpus/                 seeds for the fuzzer
├── .github/workflows/      GitHub Actions CI
├── Makefile
└── README.md
```

---

## Requirements

**GCC or Clang** with C99 support.

```bash
cc --version
make --version
```

---

## Build

```bash
# Build the library
make

# Run all 152 tests
make check

# Build the jsonlint CLI
make jsonlint

# Build and run the benchmark
make bench
```

---

## API

```c
// Parser
json_value_t *json_parse(const char *input, json_error_t *err);

// Navigation: dot-path, numeric segments for arrays
json_value_t *json_get(const json_value_t *root, const char *path);
json_value_t *json_array_get(const json_value_t *arr, size_t index);

// Serialization - must be freed with free()
char *json_stringify(const json_value_t *val);
char *json_prettify(const json_value_t *val);

// Schema validation (draft-07 subset)
int json_schema_validate(const json_value_t *schema,
                         const json_value_t *value,
                         json_schema_error_t *err);

// JSON Pointer (RFC 6901)
json_value_t *json_pointer_get(const json_value_t *root, const char *pointer, json_pointer_error_t *err);
int           json_pointer_set(json_value_t *root, const char *pointer, json_value_t *value, json_pointer_error_t *err);
int           json_pointer_remove(json_value_t *root, const char *pointer, json_pointer_error_t *err);

// JSON Patch (RFC 6902)
json_value_t *json_patch_apply(const json_value_t *doc, const json_value_t *patch, json_patch_error_t *err);

// Deep copy / cleanup
json_value_t *json_clone(const json_value_t *val);
void          json_free(json_value_t *val);
```

### Example

```c
#include "json.h"

json_error_t err;
json_value_t *root = json_parse("{\"name\":\"Felix\",\"scores\":}", &err);
if (!root) {
    fprintf(stderr, "%d:%d: %s\n", err.line, err.col, err.message);
    return 1;
}

json_value_t *name = json_get(root, "name");
json_value_t *second = json_get(root, "scores.1");

char *out = json_prettify(root);
puts(out);
free(out);

json_free(root);
```

---

## jsonlint CLI

```bash
./jsonlint file.json          # validate + pretty-print
./jsonlint -c file.json       # compact output
./jsonlint -q file.json       # quiet, exit code only
echo '{"x":1}' | ./jsonlint    # stdin
```

Exit codes: `0` valid · `1` parse error · `2` IO/usage error.

---

## Schema Validation

Supported keywords: `type`, `properties`, `required`, `additionalProperties`,
`items`, `minItems`, `maxItems`, `minimum`, `maximum`, `minLength`, `maxLength`,
`enum`, `const`.

```c
json_value_t *schema = json_parse(
    "{\"type\":\"object\","
    " \"required\":[\"id\",\"name\"],"
    " \"properties\":{"
    "   \"id\":{\"type\":\"integer\"},"
    "   \"name\":{\"type\":\"string\"}"
    " }}", NULL);

json_schema_error_t serr;
if (!json_schema_validate(schema, value, &serr))
    fprintf(stderr, "%s: %s\n", serr.path, serr.message);
```

---

## Tests

**152 assertions, no external dependencies.**

```bash
make check
```

Covers: primitives, strings, arrays, objects, dot-path, array index, deep nesting (400 OK / 600 rejected), whitespace, 22 error cases, error localization, stringify round-trip, schema validation, JSON Pointer, JSON Patch, clone.

---

## Fuzzing

```bash
# Standalone (reads stdin)
make fuzz-standalone
echo '{"x":1}' | ./fuzz_standalone

# libFuzzer (clang required)
make fuzz
./fuzz_libfuzzer corpus/ -max_len=4096

# AFL++
make fuzz-afl
afl-fuzz -i corpus/ -o findings/ -- ./fuzz_afl
```

The harness checks two invariants for every input:
1. No crash on arbitrary bytes
2. Round-trip: `stringify(parse(x))` must always parse again without error