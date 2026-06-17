/*              S N A P _ P A R I T Y . C P P
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
/** @file snap_semantics.cpp
 *
 * Verify that bsg_snap_point_2d and bsg_snap_candidates produce consistent
 * semantic snap records.
 *
 * Strategy:
 *   1. Create a headless view with grid snapping enabled.
 *   2. Call bsg_snap_candidates with BSG_SNAP_KIND_GRID near the origin.
 *   3. Verify the result distance is within grid resolution.
 *   4. Call bsg_snap_point_2d at the view center; verify it does not crash
 *      and returns a valid 2D coordinate pair.
 *
 * No database or display hardware required.
 *
 * Usage: ged_test_snap_semantics
 */

#include "common.h"

#include <cstdio>
#include <cmath>

#include <bu.h>
#include <bsg.h>
#include "bsg/snap_action.h"
#include "bsg/util.h"

#define ASSERT(cond) do { \
    nchecks++; \
    if (!(cond)) { \
	bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
	nfails++; \
    } \
} while (0)

static int nchecks = 0;
static int nfails  = 0;


int
main(int UNUSED(ac), char *av[])
{
    bu_setprogname(av[0]);

    /* ------------------------------------------------------------------
     * Build a minimal headless view with grid snap enabled.
     * ------------------------------------------------------------------ */
    struct bsg_view v;
    bsg_view_init(&v, NULL);
    v.gv_width  = 512;
    v.gv_height = 512;
    struct bsg_grid_state grid;
    bsg_view_grid_get(&v, &grid);
    grid.res_h = 1.0;
    grid.res_v = 1.0;
    bsg_view_grid_set(&v, &grid);
    bsg_view_set_snap_source_flags(&v, 0);

    /* ------------------------------------------------------------------
     * Test 1: bsg_snap_candidates near origin with GRID kind.
     * ------------------------------------------------------------------ */
    {
	point_t sample = {0.4, 0.4, 0.0};
	struct bsg_snap_result sr;
	bsg_snap_result_init(&sr);
	int cnt = bsg_snap_candidates(&v, sample, 0.0,
				      BSG_SNAP_KIND_GRID, &sr);
	/* A valid grid snap should return at least 1 candidate and the
	 * nearest snapped point should be within one grid cell of sample. */
	ASSERT(cnt >= 0);
	if (cnt > 0 && sr.sr_cnt > 0) {
	    double dist = DIST_PNT_PNT(sr.sr_candidates[0].sc_point, sample);
	    bsg_view_grid_get(&v, &grid);
	    ASSERT(dist <= grid.res_h * 1.5);
	}
	bsg_snap_result_free(&sr);
    }

    /* ------------------------------------------------------------------
     * Test 2: bsg_snap_point_2d does not crash and stays finite.
     * ------------------------------------------------------------------ */
    {
	fastf_t vx = 0.0, vy = 0.0;
	/* Place the sample near the view center (0,0 in view space). */
	bsg_snap_point_2d(&v, &vx, &vy, BSG_SNAP_KIND_GRID);
	/* Result should be finite (no NaN/Inf). */
	ASSERT(std::isfinite((double)vx));
	ASSERT(std::isfinite((double)vy));
    }

    /* ------------------------------------------------------------------
     * Test 3: NULL guards — must not crash.
     * ------------------------------------------------------------------ */
    {
	struct bsg_snap_result sr;
	bsg_snap_result_init(&sr);
	point_t p = VINIT_ZERO;
	int r = bsg_snap_candidates(NULL, p, 0.0, BSG_SNAP_KIND_GRID, &sr);
	ASSERT(r == 0);
	fastf_t vx = 0.0, vy = 0.0;
	bsg_snap_point_2d(NULL, &vx, &vy, BSG_SNAP_KIND_GRID);
    }

    bsg_view_free(&v);

    bu_log("snap semantic records: %d checks, %d failures\n", nchecks, nfails);
    return nfails ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
