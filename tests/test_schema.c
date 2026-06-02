#include "test_framework.h"
#include "../src/json_schema.h"

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

void suite_schema(void)
{
    test_schema();
}
