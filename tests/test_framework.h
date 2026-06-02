#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Global test framework state (defined in test_runner.c) */
extern int g_pass;
extern int g_fail;
extern const char *g_suite;

/* Tiny test framework macros */
#define SUITE(name) do { \
    g_suite = (name); \
    printf("\n[%s]\n", g_suite); \
} while (0)

#define CHECK(cond) do { \
    if (cond) { \
        g_pass++; \
        printf("  PASS  %s\n", #cond); \
    } else { \
        g_fail++; \
        printf("  FAIL  %s  (line %d)\n", #cond, __LINE__); \
    } \
} while (0)

#define CHECK_MSG(cond, msg) do { \
    if (cond) { \
        g_pass++; \
        printf("  PASS  %s\n", (msg)); \
    } else { \
        g_fail++; \
        printf("  FAIL  %s  (line %d)\n", (msg), __LINE__); \
    } \
} while (0)

/* Shared test helper functions (implemented in test_runner.c) */
json_value_t *parse_ok(const char *s);
int parse_fails(const char *s);

#endif /* TEST_FRAMEWORK_H */
