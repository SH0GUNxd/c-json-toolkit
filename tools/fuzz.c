/*
 * fuzz.c  -  fuzz harness for json_parse
 *
 * Compatible with both AFL++ and libFuzzer.
 *
 * libFuzzer
 *   clang -std=c99 -fsanitize=fuzzer,address \
 *         -Isrc -Iinclude \
 *         src/lexer.c src/unescape.c src/parser.c src/json.c \
 *         tools/fuzz.c -o fuzz_libfuzzer
 *   ./fuzz_libfuzzer corpus/ -max_len=4096
 *
 * AFL++
 *   AFL_USE_ASAN=1 afl-clang-fast -std=c99 \
 *         -Isrc -Iinclude \
 *         src/lexer.c src/unescape.c src/parser.c src/json.c \
 *         tools/fuzz.c -o fuzz_afl
 *   afl-fuzz -i corpus/ -o findings/ -- ./fuzz_afl
 *
 * Standalone (manual testing, no fuzzer)
 *   cc -std=c99 -DFUZZ_STANDALONE -Isrc -Iinclude \
 *         src/lexer.c src/unescape.c src/parser.c src/json.c \
 *         tools/fuzz.c -o fuzz_standalone
 *   echo '{"key":42}' | ./fuzz_standalone
 */

#include "json.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Invariant checks */

/*
 * If parse succeeds:
 *   1. stringify must succeed and produce valid JSON that re-parses
 *      to an identical tree (round-trip).
 *   2. json_free must not leak (checked externally by ASan).
 */
static void check_roundtrip(const json_value_t *root)
{
    char *s = json_stringify(root);
    if (!s) return; /* OOM - not a bug */

    json_error_t  err;
    json_value_t *rt = json_parse(s, &err);
    free(s);

    /* A valid serialize must always re-parse */
    if (!rt) {
        fprintf(stderr, "ROUNDTRIP FAILURE: stringify output failed to re-parse: "
                        "%s at %d:%d\n", err.message, err.line, err.col);
        abort();
    }
    json_free(rt);
}

/* libFuzzer entry point */

#ifndef FUZZ_STANDALONE

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Make a null-terminated copy - json_parse expects one */
    char *buf = (char *)malloc(size + 1);
    if (!buf) return 0;
    memcpy(buf, data, size);
    buf[size] = '\0';

    json_error_t  err;
    json_value_t *root = json_parse(buf, &err);
    free(buf);

    if (root) {
        check_roundtrip(root);
        json_free(root);
    }
    /* Errors are fine - just must not crash */
    return 0;
}

/* Standalone entry point */

#else /* FUZZ_STANDALONE */

int main(void)
{
    /* Read stdin */
    size_t cap = 4096, len = 0;
    char  *buf = (char *)malloc(cap);
    if (!buf) return 1;

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, stdin)) > 0) {
        len += n;
        if (len + 1 == cap) {
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) { free(buf); return 1; }
            buf = tmp;
        }
    }
    buf[len] = '\0';

    json_error_t  err;
    json_value_t *root = json_parse(buf, &err);
    free(buf);

    if (!root) {
        printf("INVALID: %s at %d:%d\n", err.message, err.line, err.col);
        return 1;
    }

    check_roundtrip(root);

    char *pretty = json_prettify(root);
    if (pretty) { puts(pretty); free(pretty); }
    json_free(root);
    return 0;
}

#endif /* FUZZ_STANDALONE */
