#include "test_framework.h"
#include "json_pointer.h"
#include "json_patch.h"

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

void suite_patch(void)
{
    test_json_patch();
    test_clone();
}
