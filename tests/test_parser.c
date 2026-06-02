#include "test_framework.h"

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

    /* Surrogate pair: U+1F600 */
    v = parse_ok("\"\\uD83D\\uDE00\"");
    CHECK(v && json_is_string(v) && (unsigned char)v->v.string[0] == 0xF0);
    json_free(v);

    /* BMP: U+2764 */
    v = parse_ok("\"\\u2764\"");
    CHECK(v && json_is_string(v));
    json_free(v);
}

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

static void test_error_location(void)
{
    SUITE("Error location reporting");
    json_error_t err;
    json_parse("{\"a\"\n:\n!}", &err);
    CHECK_MSG(err.line == 3, "error on correct line");
    printf("  (reported: line=%d col=%d msg='%s')\n",
           err.line, err.col, err.message);
}

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

void suite_parser(void)
{
    test_primitives();
    test_strings();
    test_arrays();
    test_objects();
    test_deep_nesting();
    test_whitespace();
    test_error_cases();
    test_error_location();
    test_real_world();
}