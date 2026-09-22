/*                     T E S T _ S C A N . C
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
/** @file test_scan.c
 *
 */

#include "common.h"

#include <string.h>

#include "test_api.h"


static int
scan_fastf_repeated_delimiters(void)
{
    const char *input = "0.0,,0.0,1.0";
    fastf_t first = -1.0;
    fastf_t second = -1.0;
    fastf_t third = -1.0;
    int chars = -1;

    return bu_scan_fastf_t(&chars, input, ",", 3,
	&first, &second, &third) == 3
	&& chars == (int)strlen(input)
	&& ZERO(first)
	&& ZERO(second)
	&& EQUAL(third, 1.0);
}


static int
scan_fastf_mixed_delimiters(void)
{
    const char *input = "1, ;2,, 3";
    fastf_t first = -1.0;
    fastf_t second = -1.0;
    fastf_t third = -1.0;
    int chars = -1;

    return bu_scan_fastf_t(&chars, input, ",; ", 3,
	&first, &second, &third) == 3
	&& chars == (int)strlen(input)
	&& EQUAL(first, 1.0)
	&& EQUAL(second, 2.0)
	&& EQUAL(third, 3.0);
}


static int
scan_fastf_percent_delimiter(void)
{
    const char *input = "1%2%%3";
    fastf_t first = -1.0;
    fastf_t second = -1.0;
    fastf_t third = -1.0;
    int chars = -1;

    return bu_scan_fastf_t(&chars, input, "%", 3,
	&first, &second, &third) == 3
	&& chars == (int)strlen(input)
	&& EQUAL(first, 1.0)
	&& EQUAL(second, 2.0)
	&& EQUAL(third, 3.0);
}


static int
scan_fastf_reports_consumed_prefix(void)
{
    fastf_t first = -1.0;
    fastf_t second = -1.0;
    int chars = -1;

    return bu_scan_fastf_t(&chars, "1,not-a-number", ",", 2,
	&first, &second) == 1
	&& chars == 2
	&& EQUAL(first, 1.0)
	&& EQUAL(second, -1.0);
}


static int
scan_fastf_accepts_null_destinations(void)
{
    fastf_t second = -1.0;
    int chars = -1;

    return bu_scan_fastf_t(&chars, "1,2", ",", 2,
	NULL, &second) == 2
	&& chars == 3
	&& EQUAL(second, 2.0);
}


static int
scan_fastf_rejects_invalid_configuration(void)
{
    fastf_t value = -1.0;
    int chars = -1;

    if (bu_scan_fastf_t(&chars, "1", NULL, 1, &value) != 0
	|| chars != 0 || !EQUAL(value, -1.0))
	return 0;

    chars = -1;
    if (bu_scan_fastf_t(&chars, "1", "", 1, &value) != 0
	|| chars != 0 || !EQUAL(value, -1.0))
	return 0;

    chars = -1;
    return bu_scan_fastf_t(&chars, "1", ",", 0) == 0 && chars == 0;
}


static int
test_scan(void)
{
    int errors = 0;

    TEST_API_CHECK(scan_fastf_repeated_delimiters(),
	"bu_scan_fastf_t should skip repeated delimiters");
    TEST_API_CHECK(scan_fastf_mixed_delimiters(),
	"bu_scan_fastf_t should treat delim as a character set");
    TEST_API_CHECK(scan_fastf_percent_delimiter(),
	"bu_scan_fastf_t should treat percent as delimiter data");
    TEST_API_CHECK(scan_fastf_reports_consumed_prefix(),
	"bu_scan_fastf_t should report the deterministic consumed prefix");
    TEST_API_CHECK(scan_fastf_accepts_null_destinations(),
	"bu_scan_fastf_t should accept NULL value destinations");
    TEST_API_CHECK(scan_fastf_rejects_invalid_configuration(),
	"bu_scan_fastf_t should reset c for invalid configuration");

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


int
main(int UNUSED(argc), char *argv[])
{
    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    return test_scan();
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
