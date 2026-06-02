/*
 * test_json.c  – self-contained test runner
 * Build:  make check
 */

#include "json.h"
#include "../src/json_schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Tiny test framework */

static int g_pass = 0;
static int g_fail = 0;
static const char *g_suite = "";

#define SUITE(name) do { g_suite = (name); printf("\n[%s]\n", g_suite); } while (0)

#define CHECK(cond) do { \
    if (cond) { g_pass++; printf("  PASS  %s\n", #cond); } \
    else      { g_fail++; printf("  FAIL  %s  (line %d)\n", #cond, __LINE__); } \
} while (0)

#define CHECK_MSG(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS  %s\n", (msg)); } \
    else      { g_fail++; printf("  FAIL  %s  (line %d)\n", (msg), __LINE__); } \
} while (0)

static void report(void)
{
    printf("\n------------------------------\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
}

/* Helpers */

static json_value_t *parse_ok(const char *s)
{
    json_error_t err;
    json_value_t *v = json_parse(s, &err);
    if (!v) printf("  [parse_ok] UNEXPECTED ERROR: %s at %d:%d\n",
                   err.message, err.line, err.col);
    return v;
}

static int parse_fails(const char *s)
{
    json_error_t err;
    json_value_t *v = json_parse(s, &err);
    if (v) { json_free(v); return 0; }
    return 1;
}

/* Primitives */

static void test_primitives(void)
{
    SUITE("Primitives");
    json_value_t *v;

    v = parse_ok("null");
    CHECK(v && json_is_null(v));
    json_free(v);

    v = parse_ok("true");
    CHECK(v && json_is_bool(v) && v->v.boolean == 1);
    json_free(v);

    v = parse_ok("false");
    CHECK(v && json_is_bool(v) && v->v.boolean == 0);
    json_free(v);

    v = parse_ok("42");
    CHECK(v && json_is_number(v) && v->v.number == 42.0);
    json_free(v);

    v = parse_ok("-3.14");
    CHECK(v && json_is_number(v) && fabs(v->v.number - (-3.14)) < 1e-9);
    json_free(v);

    v = parse_ok("1e100");
    CHECK(v && json_is_number(v) && v->v.number > 0);
    json_free(v);

    v = parse_ok("0");
    CHECK(v && json_is_number(v) && v->v.number == 0.0);
    json_free(v);

    v = parse_ok("-0");
    CHECK(v && json_is_number(v));
    json_free(v);

    v = parse_ok("1.5e-3");
    CHECK(v && json_is_number(v) && fabs(v->v.number - 0.0015) < 1e-12);
    json_free(v);
}

/* Strings */

static void test_strings(void)
{
    SUITE("Strings");
    json_value_t *v;

    v = parse_ok("\"hello\"");
    CHECK(v && json_is_string(v) && strcmp(v->v.string, "hello") == 0);
    json_free(v);

    v = parse_ok("\"\"");
    CHECK(v && json_is_string(v) && v->v.string[0] == '\0');
    json_free(v);

    v = parse_ok("\"tab\\there\"");
    CHECK(v && json_is_string(v) && v->v.string[3] == '\t');
    json_free(v);

    v = parse_ok("\"newline\\nhere\"");
    CHECK(v && json_is_string(v) && strchr(v->v.string, '\n') != NULL);
    json_free(v);

    v = parse_ok("\"quote\\\"inside\"");
    CHECK(v && json_is_string(v) && strchr(v->v.string, '"') != NULL);
    json_free(v);

    v = parse_ok("\"backslash\\\\end\"");
    CHECK(v && json_is_string(v) && strchr(v->v.string, '\\') != NULL);
    json_free(v);

    /* Unicode: U+00E9 (é) */
    v = parse_ok("\"caf\\u00E9\"");
    CHECK(v && json_is_string(v) && (unsigned char)v->v.string[3] == 0xC3);
    json_free(v);

    /* Surrogate pair: U+1F600 (😀) */
    v = parse_ok("\"\\uD83D\\uDE00\"");
    CHECK(v && json_is_string(v) && (unsigned char)v->v.string[0] == 0xF0);
    json_free(v);

    /* BMP: U+2764 (❤) */
    v = parse_ok("\"\\u2764\"");
    CHECK(v && json_is_string(v));
    json_free(v);
}

/* Arrays */

static void test_arrays(void)
{
    SUITE("Arrays");
    json_value_t *v;

    v = parse_ok("[]");
    CHECK(v && json_is_array(v) && v->v.array.count == 0);
    json_free(v);

    v = parse_ok("[1, 2, 3]");
    CHECK(v && json_is_array(v) && v->v.array.count == 3);
    if (v) {
        CHECK(json_array_get(v, 0)->v.number == 1.0);
        CHECK(json_array_get(v, 2)->v.number == 3.0);
        CHECK(json_array_get(v, 99) == NULL);
    }
    json_free(v);

    v = parse_ok("[null, true, \"x\", 3.14]");
    CHECK(v && json_is_array(v) && v->v.array.count == 4);
    json_free(v);

    v = parse_ok("[[1,2],[3,4]]");
    CHECK(v && json_is_array(v) && v->v.array.count == 2);
    json_free(v);
}

/* Objects */

static void test_objects(void)
{
    SUITE("Objects");
    json_value_t *v, *child;

    v = parse_ok("{}");
    CHECK(v && json_is_object(v) && v->v.object.count == 0);
    json_free(v);

    v = parse_ok("{\"a\":1}");
    CHECK(v && json_is_object(v) && v->v.object.count == 1);
    child = json_get(v, "a");
    CHECK(child && child->v.number == 1.0);
    json_free(v);

    v = parse_ok("{\"x\":true, \"y\":null, \"z\":\"hi\"}");
    CHECK(v && json_is_object(v) && v->v.object.count == 3);
    CHECK(json_get(v, "x") && json_get(v, "x")->v.boolean == 1);
    CHECK(json_get(v, "y") && json_is_null(json_get(v, "y")));
    CHECK(json_get(v, "z") && strcmp(json_get(v, "z")->v.string, "hi") == 0);
    CHECK(json_get(v, "nope") == NULL);
    json_free(v);
}

/* Nested / dot-path */

static void test_nested(void)
{
    SUITE("Nested / json_get dot-path");
    const char *src =
        "{"
        "  \"user\": {"
        "    \"name\": \"Felix\","
        "    \"address\": {"
        "      \"city\": \"Paris\""
        "    }"
        "  }"
        "}";
    json_value_t *v = parse_ok(src);
    CHECK(v != NULL);
    json_value_t *city = json_get(v, "user.address.city");
    CHECK(city && json_is_string(city) && strcmp(city->v.string, "Paris") == 0);
    CHECK(json_get(v, "user.address.country") == NULL);
    CHECK(json_get(v, "user.name") != NULL);
    json_free(v);
}

/* Array index in json_get */

static void test_get_array_index(void)
{
    SUITE("json_get array index");
    const char *src =
        "{\"items\":[{\"name\":\"a\"},{\"name\":\"b\"},{\"name\":\"c\"}]}";
    json_value_t *v = parse_ok(src);
    CHECK(v != NULL);

    json_value_t *name1 = json_get(v, "items.1.name");
    CHECK(name1 && json_is_string(name1) && strcmp(name1->v.string, "b") == 0);

    json_value_t *name0 = json_get(v, "items.0.name");
    CHECK(name0 && strcmp(name0->v.string, "a") == 0);

    CHECK(json_get(v, "items.99.name") == NULL);
    CHECK(json_get(v, "items.0.nope")  == NULL);
    json_free(v);
}

/* Deep nesting */

static void test_deep_nesting(void)
{
    SUITE("Deep nesting");
    int depth = 400;
    char *s = (char *)malloc((size_t)(depth * 2 + 1));
    for (int i = 0; i < depth; i++) s[i] = '[';
    for (int i = 0; i < depth; i++) s[depth + i] = ']';
    s[depth * 2] = '\0';

    json_value_t *v = parse_ok(s);
    CHECK_MSG(v != NULL, "400 levels of nesting parses ok");
    json_free(v);
    free(s);

    depth = 600;
    s = (char *)malloc((size_t)(depth * 2 + 1));
    for (int i = 0; i < depth; i++) s[i] = '[';
    for (int i = 0; i < depth; i++) s[depth + i] = ']';
    s[depth * 2] = '\0';
    CHECK_MSG(parse_fails(s), "600 levels rejected (exceeds MAX_DEPTH)");
    free(s);
}

/* Whitespace */

static void test_whitespace(void)
{
    SUITE("Whitespace handling");

    json_value_t *v = parse_ok("   \n\t  null   \n");
    CHECK(v && json_is_null(v));
    json_free(v);

    v = parse_ok("  {  \"k\"  :  1  }  ");
    CHECK(v && json_is_object(v));
    json_free(v);
}

/* Error cases */

static void test_error_cases(void)
{
    SUITE("Error / malformed input");

    CHECK(parse_fails("{\"a\":1"));
    CHECK(parse_fails("[1, 2"));
    CHECK(parse_fails("[1, 2,]"));
    CHECK(parse_fails("{\"a\":1,}"));
    CHECK(parse_fails("01"));
    CHECK(parse_fails("1."));
    CHECK(parse_fails("1e"));
    CHECK(parse_fails("True"));
    CHECK(parse_fails("FALSE"));
    CHECK(parse_fails("NULL"));
    CHECK(parse_fails("\"unterminated"));
    CHECK(parse_fails("\"\\q\""));
    CHECK(parse_fails("\"\\uD800\""));
    CHECK(parse_fails("\"\\uDC00\""));
    CHECK(parse_fails("\"\x01\""));
    CHECK(parse_fails("42 extra"));
    CHECK(parse_fails("{}{}"));
    CHECK(parse_fails(""));
    CHECK(parse_fails("   "));
    CHECK(parse_fails("{1:2}"));
    CHECK(parse_fails("[,]"));
    CHECK(parse_fails("[1,,2]"));
}

/* Error location */

static void test_error_location(void)
{
    SUITE("Error location reporting");
    json_error_t err;
    json_parse("{\"a\"\n:\n!}", &err);
    CHECK_MSG(err.line == 3, "error on correct line");
    printf("  (reported: line=%d col=%d msg='%s')\n",
           err.line, err.col, err.message);
}

/* Real-world */

static void test_real_world(void)
{
    SUITE("Real-world-ish JSON");
    const char *src =
        "{"
        "  \"id\": 42,"
        "  \"name\": \"TIGER Compiler\","
        "  \"version\": \"2024.1\","
        "  \"flags\": [\"--tc-parse\", \"--tc-bind\", \"--tc-type\"],"
        "  \"meta\": {"
        "    \"author\": \"EPITA\","
        "    \"debug\": false,"
        "    \"score\": 98.5"
        "  }"
        "}";

    json_value_t *v = parse_ok(src);
    CHECK(v != NULL);
    CHECK(json_get(v, "id") && json_get(v, "id")->v.number == 42.0);
    CHECK(json_get(v, "meta.debug") && json_get(v, "meta.debug")->v.boolean == 0);
    CHECK(json_get(v, "meta.score") &&
          fabs(json_get(v, "meta.score")->v.number - 98.5) < 1e-9);
    json_value_t *flags = json_get(v, "flags");
    CHECK(flags && json_is_array(flags) && flags->v.array.count == 3);
    json_free(v);
}

/* Stringify */

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

/* Schema validator */

static void test_schema(void)
{
    SUITE("json_schema_validate");
    json_schema_error_t serr;
    json_value_t *schema, *val;

    schema = parse_ok("{\"type\":\"string\"}");
    val    = parse_ok("\"hello\"");
    CHECK(json_schema_validate(schema, val, &serr) == 1);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"type\":\"string\"}");
    val    = parse_ok("42");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"type\":\"integer\"}");
    val    = parse_ok("7");
    CHECK(json_schema_validate(schema, val, &serr) == 1);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"type\":\"integer\"}");
    val    = parse_ok("7.5");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"type\":\"number\",\"minimum\":0,\"maximum\":100}");
    val    = parse_ok("50");
    CHECK(json_schema_validate(schema, val, &serr) == 1);
    json_free(val);
    val = parse_ok("150");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"type\":\"string\",\"minLength\":2,\"maxLength\":5}");
    val    = parse_ok("\"hi\"");
    CHECK(json_schema_validate(schema, val, &serr) == 1);
    json_free(val);
    val = parse_ok("\"toolongstring\"");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"type\":\"object\",\"required\":[\"id\",\"name\"]}");
    val    = parse_ok("{\"id\":1,\"name\":\"x\"}");
    CHECK(json_schema_validate(schema, val, &serr) == 1);
    json_free(val);
    val = parse_ok("{\"id\":1}");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    CHECK(strstr(serr.message, "name") != NULL);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"type\":\"object\","
                      "\"properties\":{\"a\":{},\"b\":{}},"
                      "\"additionalProperties\":false}");
    val = parse_ok("{\"a\":1,\"b\":2}");
    CHECK(json_schema_validate(schema, val, &serr) == 1);
    json_free(val);
    val = parse_ok("{\"a\":1,\"b\":2,\"c\":3}");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"enum\":[\"red\",\"green\",\"blue\"]}");
    val    = parse_ok("\"green\"");
    CHECK(json_schema_validate(schema, val, &serr) == 1);
    json_free(val);
    val = parse_ok("\"yellow\"");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"const\":42}");
    val    = parse_ok("42");
    CHECK(json_schema_validate(schema, val, &serr) == 1);
    json_free(val);
    val = parse_ok("43");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    json_free(schema); json_free(val);

    schema = parse_ok("{\"type\":\"array\","
                      "\"minItems\":1,"
                      "\"items\":{\"type\":\"number\"}}");
    val = parse_ok("[1,2,3]");
    CHECK(json_schema_validate(schema, val, &serr) == 1);
    json_free(val);
    val = parse_ok("[1,\"oops\",3]");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    json_free(val);
    val = parse_ok("[]");
    CHECK(json_schema_validate(schema, val, &serr) == 0);
    json_free(schema); json_free(val);
}

/* JSON Pointer */

#include "../src/json_pointer.h"
#include "../src/json_patch.h"

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

    /* root */
    r = json_pointer_get(v, "", &perr);
    CHECK(r == v);

    /* /foo */
    r = json_pointer_get(v, "/foo", &perr);
    CHECK(r && json_is_array(r) && r->v.array.count == 2);

    /* /foo/0 */
    r = json_pointer_get(v, "/foo/0", &perr);
    CHECK(r && json_is_string(r) && strcmp(r->v.string, "bar") == 0);

    /* /foo/1 */
    r = json_pointer_get(v, "/foo/1", &perr);
    CHECK(r && json_is_string(r) && strcmp(r->v.string, "baz") == 0);

    /* / (empty key) */
    r = json_pointer_get(v, "/", &perr);
    CHECK(r && json_is_number(r) && r->v.number == 0.0);

    /* /a~1b → key "a/b" */
    r = json_pointer_get(v, "/a~1b", &perr);
    CHECK(r && json_is_number(r) && r->v.number == 1.0);

    /* /c~0d → key "c~d" */
    r = json_pointer_get(v, "/c~0d", &perr);
    CHECK(r && json_is_number(r) && r->v.number == 2.0);

    /* deep path */
    r = json_pointer_get(v, "/x/y/z", &perr);
    CHECK(r && json_is_number(r) && r->v.number == 42.0);

    /* not found */
    r = json_pointer_get(v, "/nope", &perr);
    CHECK(r == NULL);

    /* out of bounds */
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

/* JSON Patch */

static void test_json_patch(void)
{
    SUITE("JSON Patch (RFC 6902)");
    json_patch_error_t perr;
    json_value_t *doc, *patch, *result;

    /* add */
    doc   = parse_ok("{\"a\":1}");
    patch = parse_ok("[{\"op\":\"add\",\"path\":\"/b\",\"value\":2}]");
    result = json_patch_apply(doc, patch, &perr);
    CHECK(result != NULL);
    CHECK(json_pointer_get(result, "/b", NULL) &&
          json_pointer_get(result, "/b", NULL)->v.number == 2.0);
    CHECK(doc->v.object.count == 1); /* original untouched */
    json_free(doc); json_free(patch); json_free(result);

    /* remove */
    doc   = parse_ok("{\"a\":1,\"b\":2}");
    patch = parse_ok("[{\"op\":\"remove\",\"path\":\"/a\"}]");
    result = json_patch_apply(doc, patch, &perr);
    CHECK(result != NULL);
    CHECK(json_pointer_get(result, "/a", NULL) == NULL);
    CHECK(json_pointer_get(result, "/b", NULL) != NULL);
    json_free(doc); json_free(patch); json_free(result);

    /* replace */
    doc   = parse_ok("{\"x\":\"old\"}");
    patch = parse_ok("[{\"op\":\"replace\",\"path\":\"/x\",\"value\":\"new\"}]");
    result = json_patch_apply(doc, patch, &perr);
    CHECK(result != NULL);
    json_value_t *x = json_pointer_get(result, "/x", NULL);
    CHECK(x && strcmp(x->v.string, "new") == 0);
    json_free(doc); json_free(patch); json_free(result);

    /* move */
    doc   = parse_ok("{\"a\":42,\"b\":0}");
    patch = parse_ok("[{\"op\":\"move\",\"from\":\"/a\",\"path\":\"/b\"}]");
    result = json_patch_apply(doc, patch, &perr);
    CHECK(result != NULL);
    CHECK(json_pointer_get(result, "/a", NULL) == NULL);
    CHECK(json_pointer_get(result, "/b", NULL)->v.number == 42.0);
    json_free(doc); json_free(patch); json_free(result);

    /* copy */
    doc   = parse_ok("{\"a\":99}");
    patch = parse_ok("[{\"op\":\"copy\",\"from\":\"/a\",\"path\":\"/b\"}]");
    result = json_patch_apply(doc, patch, &perr);
    CHECK(result != NULL);
    CHECK(json_pointer_get(result, "/a", NULL)->v.number == 99.0);
    CHECK(json_pointer_get(result, "/b", NULL)->v.number == 99.0);
    json_free(doc); json_free(patch); json_free(result);

    /* test: pass */
    doc   = parse_ok("{\"v\":1}");
    patch = parse_ok("[{\"op\":\"test\",\"path\":\"/v\",\"value\":1}]");
    result = json_patch_apply(doc, patch, &perr);
    CHECK(result != NULL);
    json_free(doc); json_free(patch); json_free(result);

    /* test: fail → whole patch returns NULL, original untouched */
    doc   = parse_ok("{\"v\":1}");
    patch = parse_ok("[{\"op\":\"test\",\"path\":\"/v\",\"value\":999}]");
    result = json_patch_apply(doc, patch, &perr);
    CHECK(result == NULL);
    CHECK(perr.op_index == 0);
    json_free(doc); json_free(patch);

    /* atomic: failure mid-patch leaves original untouched */
    doc   = parse_ok("{\"a\":1}");
    patch = parse_ok("["
                     "{\"op\":\"add\",\"path\":\"/b\",\"value\":2},"
                     "{\"op\":\"remove\",\"path\":\"/nope\"}"
                     "]");
    result = json_patch_apply(doc, patch, &perr);
    CHECK(result == NULL);
    CHECK(doc->v.object.count == 1); /* original still has only "a" */
    json_free(doc); json_free(patch);

    /* array append with /- */
    doc   = parse_ok("{\"tags\":[\"a\",\"b\"]}");
    patch = parse_ok("[{\"op\":\"add\",\"path\":\"/tags/-\",\"value\":\"c\"}]");
    result = json_patch_apply(doc, patch, &perr);
    CHECK(result != NULL);
    json_value_t *tags = json_pointer_get(result, "/tags", NULL);
    CHECK(tags && tags->v.array.count == 3);
    CHECK(strcmp(tags->v.array.items[2]->v.string, "c") == 0);
    json_free(doc); json_free(patch); json_free(result);
}

/* json_clone */

static void test_clone(void)
{
    SUITE("json_clone");
    const char *src = "{\"a\":1,\"b\":[1,2,{\"c\":true}]}";
    json_value_t *orig  = parse_ok(src);
    json_value_t *clone = json_clone(orig);
    CHECK(clone != NULL);
    CHECK(clone != orig);

    /* deep independence: mutate clone, original unchanged */
    json_pointer_error_t perr;
    json_value_t *newval = parse_ok("99");
    json_pointer_set(clone, "/a", newval, &perr);
    CHECK(json_pointer_get(orig,  "/a", NULL)->v.number == 1.0);
    CHECK(json_pointer_get(clone, "/a", NULL)->v.number == 99.0);

    json_free(orig);
    json_free(clone);
}

/* main */

int main(void)
{
    printf("=== JSON Parser Test Suite ===\n");

    test_primitives();
    test_strings();
    test_arrays();
    test_objects();
    test_nested();
    test_get_array_index();
    test_deep_nesting();
    test_whitespace();
    test_error_cases();
    test_error_location();
    test_real_world();
    test_stringify();
    test_schema();
    test_json_pointer();
    test_json_patch();
    test_clone();

    report();
    return g_fail > 0 ? 1 : 0;
}
