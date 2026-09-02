/*                    T R I M E S H _ G C . C
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
/** @file trimesh_gc.c
 *
 */

#include "common.h"

#include "bu.h"
#include "bg.h"


int
main(int argc, char **argv)
{
    const int ifaces[] = {2, 0, 3};
    const point2d_t ipnts[] = {
	{1.0, 1.0},
	{99.0, 99.0},
	{2.0, 3.0},
	{-1.0, 4.0}
    };
    int *ofaces = NULL;
    point2d_t *opnts = NULL;
    int n_opnts = 0;
    int ret;
    int failed = 0;

    bu_setprogname(argv[0]);

    if (argc != 1)
	bu_exit(1, "ERROR: [%s] takes no arguments\n", argv[0]);

    ret = bg_trimesh_2d_gc(&ofaces, &opnts, &n_opnts, ifaces, 1, ipnts);
    if (ret != 1 || n_opnts != 3 || !ofaces || !opnts) {
	failed = 1;
	goto cleanup;
    }

    if (ofaces[0] != 1 || ofaces[1] != 0 || ofaces[2] != 2
	|| !NEAR_EQUAL(opnts[0][X], 1.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[0][Y], 1.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[1][X], 2.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[1][Y], 3.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[2][X], -1.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[2][Y], 4.0, SMALL_FASTF)) {
	failed = 1;
    }

cleanup:
    if (ofaces)
	bu_free(ofaces, "2D mesh faces");
    if (opnts)
	bu_free(opnts, "2D mesh points");

    return failed;
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
