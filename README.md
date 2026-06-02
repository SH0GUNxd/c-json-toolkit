# json_parser - Parseur JSON en C

**Auteur :** Félix Vandenbroucke · Dev 2026

Parseur JSON écrit from scratch en C99 strict, sans dépendance externe. Lexer/parser récursif descendant, conformité RFC 8259 complète, sérialisation, validation de schéma, JSON Pointer et JSON Patch. 152 tests, zéro leak, CI GitHub Actions.

---

## Fonctionnalités

- Parse tous les types JSON : `null`, `bool`, `number`, `string`, `array`, `object`
- **Conformité RFC 8259** : rejette les leading zeros, virgules finales, mots-clés nus, surrogates isolés, caractères de contrôle
- **Unicode** : escapes `\uXXXX`, décodage des surrogate pairs (`\uD83D\uDE00` → UTF-8)
- **Erreurs précises** : message + ligne + colonne
- **Profondeur max** : guard à 512 niveaux
- **`json_stringify()` / `json_prettify()`** : sérialisation avec garantie de round-trip
- **`json_get("user.address.city")`** : navigation dot-path avec index tableau (`"items.2.name"`)
- **`json_schema_validate()`** : sous-ensemble JSON Schema draft-07
- **JSON Pointer** (RFC 6901) : get / set / remove via `/foo/0/bar`
- **JSON Patch** (RFC 6902) : `add`, `remove`, `replace`, `move`, `copy`, `test` - atomique
- **`json_clone()`** : copie profonde
- **CLI `jsonlint`** : valider et pretty-printer des fichiers JSON
- **Benchmark** : throughput `json_parse` + `json_stringify` en MB/s
- **Fuzz harness** : compatible AFL++ et libFuzzer, vérifie le round-trip

---

## Stack

| Couche       | Technologie                                      |
|--------------|--------------------------------------------------|
| Langage      | C99 strict, `-pedantic -Wall -Wextra -Werror`    |
| Parser       | Lexer/parser récursif descendant maison          |
| Tests        | Runner maison, zéro dépendance externe           |
| Fuzz         | AFL++ / libFuzzer                                |
| CI           | GitHub Actions (gcc + clang + valgrind)          |

---

## Structure du projet

```text
json_parser/
├── include/                API publique
│   ├── json.h              
│   ├── json_schema.h       
│   ├── json_pointer.h      
│   └── json_patch.h        
├── src/                    Implémentation interne
│   ├── json_internal.h     types internes (token_t, parser_t)
│   ├── lexer.c             tokenizer avec suivi ligne/colonne
│   ├── unescape.c          décodage strings, UTF-8, surrogate pairs
│   ├── parser.c            parser récursif descendant
│   ├── json.c              json_parse / json_get / json_free
│   ├── stringify.c         json_stringify / json_prettify
│   ├── json_schema.c       validateur de schéma
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
│   ├── jsonlint.c          CLI : valider et pretty-printer
│   ├── bench.c             benchmark throughput
│   └── fuzz.c              harness AFL++ / libFuzzer
├── corpus/                 seeds pour le fuzzer
├── .github/workflows/      CI GitHub Actions
├── Makefile
└── README.md
```

---

## Prérequis

**GCC ou Clang** avec support C99.

```bash
cc --version
make --version
```

---

## Build

```bash
# Compiler la bibliothèque
make

# Lancer les 152 tests
make check

# Compiler le CLI jsonlint
make jsonlint

# Compiler et lancer le benchmark
make bench
```

---

## API

```c
// Parser
json_value_t *json_parse(const char *input, json_error_t *err);

// Navigation : dot-path, segments numériques pour les tableaux
json_value_t *json_get(const json_value_t *root, const char *path);
json_value_t *json_array_get(const json_value_t *arr, size_t index);

// Sérialisation - à libérer avec free()
char *json_stringify(const json_value_t *val);
char *json_prettify(const json_value_t *val);

// Validation de schéma (draft-07 subset)
int json_schema_validate(const json_value_t *schema,
                         const json_value_t *value,
                         json_schema_error_t *err);

// JSON Pointer (RFC 6901)
json_value_t *json_pointer_get(const json_value_t *root, const char *pointer, json_pointer_error_t *err);
int           json_pointer_set(json_value_t *root, const char *pointer, json_value_t *value, json_pointer_error_t *err);
int           json_pointer_remove(json_value_t *root, const char *pointer, json_pointer_error_t *err);

// JSON Patch (RFC 6902)
json_value_t *json_patch_apply(const json_value_t *doc, const json_value_t *patch, json_patch_error_t *err);

// Copie profonde / libération
json_value_t *json_clone(const json_value_t *val);
void          json_free(json_value_t *val);
```

### Exemple

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

---

## CLI `jsonlint`

```bash
./jsonlint file.json          # valider + pretty-print
./jsonlint -c file.json       # sortie compacte
./jsonlint -q file.json       # silencieux, exit code seulement
echo '{"x":1}' | ./jsonlint   # stdin
```

Codes de retour : `0` valide · `1` erreur de parsing · `2` erreur IO/usage.

---

## Validation de schéma

Mots-clés supportés : `type`, `properties`, `required`, `additionalProperties`,
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

**152 assertions, zéro dépendance externe.**

```bash
make check
```

Couvre : primitifs, strings, tableaux, objets, dot-path, index tableau, nesting profond (400 ok / 600 rejeté), whitespace, 22 cas d'erreur, localisation d'erreur, round-trip stringify, validation de schéma, JSON Pointer, JSON Patch, clone.

---

## Fuzz

```bash
# Standalone (lit stdin)
make fuzz-standalone
echo '{"x":1}' | ./fuzz_standalone

# libFuzzer (clang requis)
make fuzz
./fuzz_libfuzzer corpus/ -max_len=4096

# AFL++
make fuzz-afl
afl-fuzz -i corpus/ -o findings/ -- ./fuzz_afl
```

Le harness vérifie deux invariants sur chaque entrée :
1. Pas de crash sur des octets arbitraires
2. Round-trip : `stringify(parse(x))` doit toujours re-parser sans erreur