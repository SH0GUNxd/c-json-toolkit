#include "test_framework.h"
#include "json_pointer.h"

static void test_json_pointer(void)
{
    SUITE("JSON Pointer (RFC 6901)");
    json_pointer_error_t perr;
    json_value_t *v, *r;

    const char *src =
        "{"
        "  \"foo\": [\"bar\", \"baz\"],"
        "  \"\":    0,"
        "  \"a/b\": 1,"
        "  \"c~d\": 2,"
        "  \"e\\\\f\": 3,"
        "  \"x\": {\"y\": {\"z\": 42}}"
        "}";
    v = parse_ok(src);
    CHECK(v != NULL);

    r = json_pointer_get(v, "", &perr);
    CHECK(r == v);

    r = json_pointer_get(v, "/foo", &perr);
    CHECK(r && json_is_array(r) && r->v.array.count == 2);

    r = json_pointer_get(v, "/foo/0", &perr);
    CHECK(r && json_is_string(r) && strcmp(r->v.string, "bar") == 0);

    r = json_pointer_get(v, "/foo/1", &perr);
    CHECK(r && json_is_string(r) && strcmp(r->v.string, "baz") == 0);

    r = json_pointer_get(v, "/", &perr);
    CHECK(r && json_is_number(r) && r->v.number == 0.0);

    /* ~1 → / */
    r = json_pointer_get(v, "/a~1b", &perr);
    CHECK(r && json_is_number(r) && r->v.number == 1.0);

    /* ~0 → ~ */
    r = json_pointer_get(v, "/c~0d", &perr);
    CHECK(r && json_is_number(r) && r->v.number == 2.0);

    r = json_pointer_get(v, "/x/y/z", &perr);
    CHECK(r && json_is_number(r) && r->v.number == 42.0);

    r = json_pointer_get(v, "/nope", &perr);
    CHECK(r == NULL);

    r = json_pointer_get(v, "/foo/5", &perr);
    CHECK(r == NULL);

    json_free(v);

    /* set: update existing key */
    v = parse_ok("{\"a\":1,\"b\":2}");
    json_value_t *newval = parse_ok("99");
    CHECK(json_pointer_set(v, "/a", newval, &perr) == 1);
    r = json_pointer_get(v, "/a", &perr);
    CHECK(r && r->v.number == 99.0);
    json_free(v);

    /* set: add new key */
    v = parse_ok("{\"a\":1}");
    newval = parse_ok("\"hello\"");
    CHECK(json_pointer_set(v, "/b", newval, &perr) == 1);
    r = json_pointer_get(v, "/b", &perr);
    CHECK(r && json_is_string(r) && strcmp(r->v.string, "hello") == 0);
    json_free(v);

    /* set: append to array with /- */
    v = parse_ok("[1,2,3]");
    newval = parse_ok("4");
    CHECK(json_pointer_set(v, "/-", newval, &perr) == 1);
    CHECK(v->v.array.count == 4);
    CHECK(v->v.array.items[3]->v.number == 4.0);
    json_free(v);

    /* remove: object key */
    v = parse_ok("{\"a\":1,\"b\":2}");
    CHECK(json_pointer_remove(v, "/a", &perr) == 1);
    CHECK(v->v.object.count == 1);
    CHECK(json_pointer_get(v, "/a", &perr) == NULL);
    json_free(v);

    /* remove: array element */
    v = parse_ok("[10,20,30]");
    CHECK(json_pointer_remove(v, "/1", &perr) == 1);
    CHECK(v->v.array.count == 2);
    CHECK(v->v.array.items[0]->v.number == 10.0);
    CHECK(v->v.array.items[1]->v.number == 30.0);
    json_free(v);
}

void suite_pointer(void)
{
    test_json_pointer();
}
