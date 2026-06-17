/*                 T E S T _ M E A S U R E . C
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
/** @file libbsg/tests/test_measure.c
 *
 * Unit tests for bsg/measure.h API.
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bsg/interaction.h"
#include "bsg/measure.h"

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    (void)argc;

    struct bsg_measure_result out;
    point_t a = {0.0, 0.0, 0.0};
    point_t b = {3.0, 4.0, 0.0};
    if (!bsg_measure_candidates(NULL, a, b, &out)) {
	printf("FAIL: expected valid measure result\n");
	return 1;
    }
    if (!out.mr_valid) {
	printf("FAIL: result marked invalid\n");
	return 1;
    }
    if (out.mr_distance < 4.999 || out.mr_distance > 5.001) {
	printf("FAIL: expected distance 5, got %g\n", out.mr_distance);
	return 1;
    }
    struct bsg_interaction_record *rec = bsg_interaction_from_measure_result(NULL, a, b, &out);
    if (!rec || rec->kind != BSG_INTERACTION_MEASURE_CANDIDATE ||
	    !rec->valid || rec->distance < 4.999 || rec->distance > 5.001) {
	printf("FAIL: measure interaction record mismatch\n");
	return 1;
    }
    bsg_interaction_record_free(rec);
    printf("PASS test_measure\n");
    return 0;
}
