#include "test_framework.h"

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

void suite_get(void)
{
    test_nested();
    test_get_array_index();
}
