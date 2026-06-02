#include "test_framework.h"

static void test_stringify(void)
{
    SUITE("json_stringify / json_prettify");
    json_value_t *v;
    char *s;

    /* round-trip */
    v = parse_ok("{\"a\":1,\"b\":true,\"c\":null}");
    s = json_stringify(v);
    CHECK(s != NULL);
    json_value_t *rt = parse_ok(s);
    CHECK(rt != NULL);
    CHECK(json_get(rt, "a") && json_get(rt, "a")->v.number == 1.0);
    free(s);
    json_free(rt);
    json_free(v);

    /* array compact output */
    v = parse_ok("[1,\"two\",false,null]");
    s = json_stringify(v);
    CHECK(s && strcmp(s, "[1,\"two\",false,null]") == 0);
    free(s);
    json_free(v);

    /* escape roundtrip */
    v = parse_ok("\"tab\\there\"");
    s = json_stringify(v);
    CHECK(s && strstr(s, "\\t") != NULL);
    free(s);
    json_free(v);

    /* prettify produces newlines */
    v = parse_ok("{\"x\":1}");
    s = json_prettify(v);
    CHECK(s && strchr(s, '\n') != NULL);
    free(s);
    json_free(v);

    /* integer: no trailing .0 */
    v = parse_ok("42");
    s = json_stringify(v);
    CHECK(s && strcmp(s, "42") == 0);
    free(s);
    json_free(v);
}

void suite_stringify(void)
{
    test_stringify();
}
