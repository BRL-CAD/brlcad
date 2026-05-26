/*                 T E S T _ U N I T S . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <ctype.h>
#ifdef HAVE_INTTYPES_H
#  if defined(__cplusplus)
#    define __STDC_FORMAT_MACROS
#  endif
#  include <inttypes.h>
#else
#  include "pinttypes.h"
#endif

#include "test_api.h"


struct units_test_case {
    const char *label;
    const char *input;
    double expected;
};


struct units_length_holder {
    double value;
};


static int
units_csv_has_token(const char *csv, const char *token)
{
    const char *segment = csv;

    while (segment && *segment) {
	const char *end = strchr(segment, ',');
	const char *start = segment;
	size_t len = 0;

	if (!end) {
	    end = segment + strlen(segment);
	}

	while (start < end && isspace((unsigned char)*start)) {
	    start++;
	}
	while (end > start && isspace((unsigned char)*(end - 1))) {
	    end--;
	}

	len = (size_t)(end - start);
	if (strlen(token) == len && bu_strncmp(start, token, len) == 0) {
	    return 1;
	}

	if (*end == '\0') {
	    break;
	}

	segment = end + 1;
    }

    return 0;
}


static int
test_units_api(void)
{
    int errors = 0;
    size_t i;
    const char *unit = NULL;
    struct bu_vls *units_list = NULL;
    struct units_length_holder holder;
    struct bu_structparse sp = BU_STRUCTPARSE_INIT_ZERO;
    int64_t size = -1;
    static const struct units_test_case conversion_cases[] = {
	{"millimeters", "mm", 1.0},
	{"plural feet", " feet ", 304.8},
	{"upper-case foot", "FT", 304.8},
	{"volume", "cu yards", 764554857.984},
	{"mass", "KG", 1000.0}
    };
    static const struct units_test_case mm_cases[] = {
	{"bare number", "12", 12.0},
	{"concatenated", "5ft", 1524.0},
	{"spaced plural", " 2 feet ", 609.6},
	{"mixed case", "3.5CM", 35.0}
    };
    static const double canonical_lengths[] = {1.0, 25.4, 304.8, 1000.0, 1609344.0};

    for (i = 0; i < ARRAY_LEN(conversion_cases); i++) {
	double value = bu_units_conversion(conversion_cases[i].input);
	TEST_API_CHECK(test_api_close_enough(value, conversion_cases[i].expected, 1.0e-12, 1.0e-9),
		       "unexpected conversion for %s: got %.17g expected %.17g",
		       conversion_cases[i].label, value, conversion_cases[i].expected);
    }

    for (i = 0; i < ARRAY_LEN(mm_cases); i++) {
	double value = bu_mm_value(mm_cases[i].input);
	TEST_API_CHECK(test_api_close_enough(value, mm_cases[i].expected, 1.0e-12, 1.0e-9),
		       "unexpected mm value for %s: got %.17g expected %.17g",
		       mm_cases[i].label, value, mm_cases[i].expected);
    }

    TEST_API_CHECK(bu_mm_value("not-a-unit") < 0.0, "unknown unit strings should fail");

    for (i = 0; i < ARRAY_LEN(canonical_lengths); i++) {
	unit = bu_units_string(canonical_lengths[i]);
	TEST_API_CHECK(unit != NULL, "expected units string for %.17g", canonical_lengths[i]);
	if (unit) {
	    TEST_API_CHECK(test_api_close_enough(bu_units_conversion(unit), canonical_lengths[i], 1.0e-12, 1.0e-9),
			   "round-trip conversion mismatch for %.17g via %s", canonical_lengths[i], unit);
	}
    }

    unit = bu_units_string(25.4 + 1.0e-10);
    TEST_API_CHECK(unit != NULL, "near miss should still resolve to a units string");
    if (unit) {
	TEST_API_CHECK(test_api_close_enough(bu_units_conversion(unit), 25.4, 1.0e-12, 1.0e-9),
		       "near miss should resolve to the inch conversion");
    }

    unit = bu_nearest_units_string(26.0);
    TEST_API_CHECK(unit != NULL, "nearest units should resolve for positive inputs");
    if (unit) {
	TEST_API_CHECK(test_api_close_enough(bu_units_conversion(unit), 25.4, 1.0e-12, 1.0e-9),
		       "unexpected nearest unit for 26.0mm: %s", unit);
    }

    unit = bu_nearest_units_string(0.9);
    TEST_API_CHECK(unit != NULL, "nearest units should resolve small positive distances");
    if (unit) {
	TEST_API_CHECK(test_api_close_enough(bu_units_conversion(unit), 1.0, 1.0e-12, 1.0e-9),
		       "unexpected nearest unit for 0.9mm: %s", unit);
    }

    units_list = bu_units_strings_vls();
    TEST_API_CHECK(units_list != NULL, "bu_units_strings_vls returned NULL");
    if (units_list) {
	size_t list_len = bu_vls_strlen(units_list);
	TEST_API_CHECK(units_csv_has_token(bu_vls_addr(units_list), "mm"), "units list should include mm");
	TEST_API_CHECK(units_csv_has_token(bu_vls_addr(units_list), "cm"), "units list should include cm");
	TEST_API_CHECK(units_csv_has_token(bu_vls_addr(units_list), "m"), "units list should include m");
	TEST_API_CHECK(units_csv_has_token(bu_vls_addr(units_list), "km"), "units list should include km");
	TEST_API_CHECK(units_csv_has_token(bu_vls_addr(units_list), "in"), "units list should include in");
	TEST_API_CHECK(units_csv_has_token(bu_vls_addr(units_list), "ft"), "units list should include ft");
	TEST_API_CHECK(list_len > 0 && bu_vls_addr(units_list)[list_len - 1] != ',',
		       "units list should not end with a comma");
	bu_vls_free(units_list);
	bu_free(units_list, "units list");
    }

    holder.value = 0.0;
    sp.sp_offset = bu_offsetof(struct units_length_holder, value);
    bu_mm_cvt(&sp, "value", &holder, "2in", NULL);
    TEST_API_CHECK(test_api_close_enough(holder.value, 50.8, 1.0e-12, 1.0e-9),
		   "bu_mm_cvt should store converted millimeters");

    TEST_API_CHECK(bu_dehumanize_number("1k", &size) == 0, "dehumanize should accept 1k");
    TEST_API_CHECK(size == 1024, "1k should equal 1024, got %" PRId64, size);
    TEST_API_CHECK(bu_dehumanize_number("3M", &size) == 0, "dehumanize should accept 3M");
    TEST_API_CHECK(size == 3LL * 1024LL * 1024LL, "3M should equal 3*1024*1024, got %" PRId64, size);
    TEST_API_CHECK(bu_dehumanize_number("2T", &size) == -1, "unsupported suffixes should fail");
    TEST_API_CHECK(bu_dehumanize_number("", &size) == -1, "empty strings should fail");

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


int
main(int UNUSED(argc), char *argv[])
{
    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    return test_units_api();
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
