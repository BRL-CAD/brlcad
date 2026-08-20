/*                 T E S T _ B B _ T I G H T . C
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
/** @file tests/test_bb_tight.c
 *
 * Regression test for the "bb -t" (tight, subtraction-aware) bounding box.
 *
 * The default AABB (rt_obj_bounds / rt_bound_tree) ignores subtracted
 * (OP_SUBTRACT) material, so the reported box never shrinks to reflect
 * geometry that has been carved away.  The opt-in "bb -t" path bounds the
 * ray-traced, boolean-evaluated geometry instead, so subtractions DO tighten
 * the box.
 *
 * The fixture is a region = big positive box MINUS a large negative box that
 * carves away most of the positive box's X extent.  The test asserts that the
 * tight-path bounding volume is strictly smaller than the default (loose)
 * bounding volume, and that both are non-degenerate.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "ged.h"
#include "wdb.h"


static int g_failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
	fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg)); \
	g_failures++; \
    } \
} while (0)


/* Pull the "Bounding Box Volume: <value> ..." number out of a bb result.
 * Returns -1.0 if not found. */
static double
parse_bb_volume(const char *result)
{
    const char *tag = "Bounding Box Volume:";
    const char *p = strstr(result, tag);
    if (!p)
	return -1.0;
    p += strlen(tag);
    return strtod(p, NULL);
}


/** Open a fresh temporary .g database with a subtraction-dominated region. */
static struct ged *
open_test_db(void)
{
    char tmppath[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(tmppath, MAXPATHLEN);
    struct rt_wdb *wdbp;

    if (!fp)
	return NULL;
    fclose(fp);

    wdbp = wdb_fopen(tmppath);
    if (!wdbp)
	return NULL;

    /* Big positive box: X in [0,100], Y in [0,40], Z in [0,40]. */
    {
	point_t pmin = {0.0, 0.0, 0.0};
	point_t pmax = {100.0, 40.0, 40.0};
	mk_rpp(wdbp, "big.s", pmin, pmax);
    }

    /* Large negative box that carves away the X in [30,110] slab, leaving only
     * X in [0,30] of solid material.  It also over-hangs the positive box in Y
     * and Z so the carve is clean through those axes. */
    {
	point_t pmin = {30.0, -10.0, -10.0};
	point_t pmax = {110.0, 50.0, 50.0};
	mk_rpp(wdbp, "cut.s", pmin, pmax);
    }

    /* region: big.s - cut.s */
    {
	struct wmember wm;
	BU_LIST_INIT(&wm.l);
	(void)mk_addmember("big.s", &wm.l, NULL, WMOP_UNION);
	(void)mk_addmember("cut.s", &wm.l, NULL, WMOP_SUBTRACT);
	mk_comb(wdbp, "carved.r", &wm.l, 1, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0);
    }

    db_close(wdbp->dbip);

    return ged_open("db", tmppath, 1);
}


/* Run "bb [args]" and return the parsed bounding-box volume (or -1.0). */
static double
run_bb_volume(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    bu_vls_trunc(gedp->ged_result_str, 0);
    ret = ged_exec_bb(gedp, argc, argv);
    if (ret != BRLCAD_OK && ret != GED_HELP)
	return -1.0;
    return parse_bb_volume(bu_vls_cstr(gedp->ged_result_str));
}


int
main(int argc, char *argv[])
{
    struct ged *gedp;
    double loose_vol, tight_vol;

    bu_setprogname(argv[0]);
    (void)argc;

    gedp = open_test_db();
    if (!gedp) {
	fprintf(stderr, "ged_test_bb_tight: failed to create test database\n");
	return 1;
    }

    {
	const char *av[] = {"bb", "-v", "carved.r", NULL};
	loose_vol = run_bb_volume(gedp, 3, av);
    }
    {
	const char *av[] = {"bb", "-t", "-v", "carved.r", NULL};
	tight_vol = run_bb_volume(gedp, 4, av);
    }

    printf("ged_test_bb_tight: loose volume=%g tight volume=%g\n", loose_vol, tight_vol);

    CHECK(loose_vol > 0.0, "loose bounding volume should be positive");
    CHECK(tight_vol > 0.0, "tight bounding volume should be positive");
    CHECK(tight_vol < loose_vol,
	    "tight (subtraction-aware) volume must be strictly smaller than the loose volume");

    /* The loose box is the full 100x40x40 positive box (subtraction ignored).
     * The tight box should reflect only the remaining X in [0,30] slab, i.e.
     * roughly 30% of the loose volume.  Allow generous slack for ray sampling
     * resolution: require the tight volume below 60% of loose but above 5%. */
    if (loose_vol > 0.0 && tight_vol > 0.0) {
	double frac = tight_vol / loose_vol;
	CHECK(frac < 0.6,
		"tight volume should be substantially smaller than loose (subtraction accounted for)");
	CHECK(frac > 0.05,
		"tight volume should not be degenerate/near-zero");
    }

    ged_close(gedp);

    if (g_failures) {
	fprintf(stderr, "\nged_test_bb_tight: %d check(s) FAILED\n", g_failures);
	return 1;
    }
    printf("ged_test_bb_tight: all tests PASSED\n");
    return 0;
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
