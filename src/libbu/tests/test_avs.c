/*                      T E S T _ A V S . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include "test_api.h"


static size_t
avs_count_attribute_named(const struct bu_attribute_value_set *avs, const char *name)
{
    size_t count = 0;
    struct bu_attribute_value_pair *avpp = NULL;

    for (BU_AVS_FOR(avpp, avs)) {
	if (BU_STR_EQUAL(avpp->name, name)) {
	    count++;
	}
    }

    return count;
}


static struct bu_attribute_value_pair *
avs_find_attribute(struct bu_attribute_value_set *avs, const char *name)
{
    struct bu_attribute_value_pair *avpp = NULL;

    for (BU_AVS_FOR(avpp, avs)) {
	if (BU_STR_EQUAL(avpp->name, name)) {
	    return avpp;
	}
    }

    return NULL;
}


static int
test_avs_api(void)
{
    int errors = 0;
    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    struct bu_attribute_value_set src = BU_AVS_INIT_ZERO;
    struct bu_attribute_value_set *heap_avs = NULL;
    struct bu_vls value = BU_VLS_INIT_ZERO;
    struct bu_attribute_value_pair *nullable = NULL;
    struct test_api_log_capture capture;
    int ret = 0;

    bu_avs_init_empty(&avs);
    bu_avs_init(&src, 2, "api avs src");

    TEST_API_CHECK(BU_AVS_IS_INITIALIZED(&avs), "bu_avs_init_empty did not initialize the set");
    TEST_API_CHECK(avs.count == 0, "expected empty avs count, got %zu", avs.count);
    TEST_API_CHECK(bu_avs_get(&avs, "missing") == NULL, "missing key should not exist");

    test_api_log_capture_begin(&capture);
    ret = bu_avs_add(&avs, "", "invalid");
    test_api_log_capture_end(&capture);
    TEST_API_CHECK(ret == 0, "empty attribute name should be rejected");
    TEST_API_CHECK(avs.count == 0, "empty attribute name should not change count");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "zero length"),
		   "expected warning for zero-length attribute name");
    test_api_log_capture_free(&capture);

    ret = bu_avs_add(&avs, "color", "red");
    TEST_API_CHECK(ret == 2, "expected insert return code 2, got %d", ret);
    TEST_API_CHECK(avs.count == 1, "expected one attribute after insert, got %zu", avs.count);
    TEST_API_CHECK(BU_STR_EQUAL(bu_avs_get(&avs, "color"), "red"), "unexpected value after insert");

    ret = bu_avs_add(&avs, "color", "blue");
    TEST_API_CHECK(ret == 1, "expected update return code 1, got %d", ret);
    TEST_API_CHECK(avs.count == 1, "update should not change attribute count");
    TEST_API_CHECK(BU_STR_EQUAL(bu_avs_get(&avs, "color"), "blue"), "unexpected value after update");

    bu_vls_sprintf(&value, "%s", "active");
    ret = bu_avs_add_vls(&avs, "status", &value);
    TEST_API_CHECK(ret == 2, "expected add_vls insert return code 2, got %d", ret);
    bu_vls_sprintf(&value, "%s", "mutated");
    TEST_API_CHECK(BU_STR_EQUAL(bu_avs_get(&avs, "status"), "active"),
		   "add_vls should copy source text");

    ret = bu_avs_add(&avs, "nullable", NULL);
    TEST_API_CHECK(ret == 2, "expected insert of null-valued attribute to succeed");
    nullable = avs_find_attribute(&avs, "nullable");
    TEST_API_CHECK(nullable != NULL, "expected nullable attribute to exist");
    TEST_API_CHECK(nullable && nullable->value == NULL,
		   "expected nullable attribute to preserve NULL value");

    bu_avs_add_nonunique(&avs, "dup", "first");
    bu_avs_add_nonunique(&avs, "dup", "second");
    TEST_API_CHECK(avs_count_attribute_named(&avs, "dup") == 2,
		   "expected duplicate attributes to be preserved");

    ret = bu_avs_remove(&avs, "dup");
    TEST_API_CHECK(ret == 0, "expected duplicate attribute removal to succeed");
    TEST_API_CHECK(avs_count_attribute_named(&avs, "dup") == 0,
		   "expected all duplicate attributes to be removed");
    TEST_API_CHECK(bu_avs_remove(&avs, "ghost") == -1,
		   "missing attribute removal should report not found");

    bu_avs_add(&src, "color", "green");
    bu_avs_add(&src, "density", "42");
    bu_avs_merge(&avs, &src);
    TEST_API_CHECK(BU_STR_EQUAL(bu_avs_get(&avs, "color"), "green"),
		   "merge should replace existing values");
    TEST_API_CHECK(BU_STR_EQUAL(bu_avs_get(&avs, "density"), "42"),
		   "merge should add new keys");
    TEST_API_CHECK(BU_STR_EQUAL(bu_avs_get(&avs, "status"), "active"),
		   "merge should preserve unrelated keys");

    test_api_log_capture_begin(&capture);
    bu_avs_print(&avs, "api avs");
    test_api_log_capture_end(&capture);
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "api avs"),
		   "print output should include the title");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "color"),
		   "print output should include key names");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "green"),
		   "print output should include values");
    TEST_API_CHECK(test_api_text_contains(bu_vls_addr(&capture.text), "density"),
		   "print output should include merged keys");
    test_api_log_capture_free(&capture);

    heap_avs = bu_avs_new(1, "api avs heap");
    TEST_API_CHECK(heap_avs != NULL, "bu_avs_new returned NULL");
    TEST_API_CHECK(BU_AVS_IS_INITIALIZED(heap_avs), "heap avs should be initialized");
    TEST_API_CHECK(bu_avs_add(heap_avs, "mode", "heap") == 2, "heap avs insert failed");
    TEST_API_CHECK(BU_STR_EQUAL(bu_avs_get(heap_avs, "mode"), "heap"), "heap avs lookup failed");

    bu_avs_free(&src);
    bu_avs_free(&avs);
    bu_vls_free(&value);

    if (heap_avs) {
	bu_avs_free(heap_avs);
	bu_free(heap_avs, "api avs heap");
    }

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


int
main(int UNUSED(argc), char *argv[])
{
    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    return test_avs_api();
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
