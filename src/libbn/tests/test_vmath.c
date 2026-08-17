/*                     T E S T _ V M A T H . C
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

#include "test_util.h"


static int
test_hcross(void)
{
    int failures = 0;
    const char *test = "hcross";
    const fastf_t tolerance = 1.0e-12;
    hvect_t x = {1.0, 0.0, 0.0, 1.0};
    hvect_t y = {0.0, 1.0, 0.0, 1.0};
    hvect_t scaled_x = {2.0, 0.0, 0.0, 2.0};
    hvect_t scaled_y = {0.0, -3.0, 0.0, -3.0};
    hvect_t a = {2.0, 4.0, 6.0, 2.0};
    hvect_t b = {-8.0, 4.0, -12.0, 4.0};
    vect_t expected = {0.0, 0.0, 1.0};
    vect_t expected_general = {-9.0, -3.0, 5.0};
    vect_t out = VINIT_ZERO;

    HCROSS(out, x, y);
    if (!vect_close(out, expected, tolerance)) {
	report_failure(test, "HCROSS disagrees with VCROSS for unit homogeneous coordinates");
	failures++;
    }

    HCROSS(out, scaled_x, scaled_y);
    if (!vect_close(out, expected, tolerance)) {
	report_failure(test, "HCROSS did not dehomogenize scaled inputs");
	failures++;
    }

    HCROSS(out, a, b);
    if (!vect_close(out, expected_general, tolerance)) {
	report_failure(test, "HCROSS failed the general homogeneous-coordinate case");
	failures++;
    }

    return failures;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_single(argc, argv, "hcross", test_hcross);
}
