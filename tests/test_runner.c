#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_framework.h"

/* Global test framework state */
int g_pass = 0;
int g_fail = 0;
const char *g_suite = "";

/* External suite definitions */
extern void suite_parser(void);
extern void suite_get(void);
extern void suite_stringify(void);
extern void suite_schema(void);
extern void suite_pointer(void);
extern void suite_patch(void);

/* Helpers */
json_value_t *parse_ok(const char *s)
{
    json_error_t err;
    json_value_t *v = json_parse(s, &err);
    if (!v)
    {
        printf("  [parse_ok] UNEXPECTED ERROR: %s at %d:%d\n", err.message,
               err.line, err.col);
    }
    return v;
}

int parse_fails(const char *s)
{
    json_error_t err;
    json_value_t *v = json_parse(s, &err);
    if (v)
    {
        json_free(v);
        return 0;
    }
    return 1;
}

static void report(void)
{
    printf("\n------------------------------\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
}

/* main runner */
int main(void)
{
    printf("--- JSON Parser Test Suite ---\n");

    suite_parser();
    suite_get();
    suite_stringify();
    suite_schema();
    suite_pointer();
    suite_patch();

    report();
    return g_fail > 0 ? 1 : 0;
}
