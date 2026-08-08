/*                 B O U N D _ I N T E R N A L . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include "raytrace.h"
#include "rt/calc.h"
#include "wdb.h"


int
main(int UNUSED(argc), char **UNUSED(argv))
{
    struct db_i *dbip = db_create_inmem();
    struct rt_wdb *wdbp;
    struct wmember wm;
    struct directory *dp;
    point_t center = VINIT_ZERO;
    point_t bmin, bmax;
    point_t expected_min = {-1.0, -1.0, -1.0};
    point_t expected_max = { 1.0,  1.0,  1.0};
    int ret = 1;

    if (!dbip)
	return 1;
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp)
	goto done;

    if (mk_sph(wdbp, "left.s", center, 1.0))
	goto done;
    VSET(center, 10.0, 0.0, 0.0);
    if (mk_sph(wdbp, "disjoint_right.s", center, 1.0))
	goto done;

    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("left.s", &wm.l, NULL, WMOP_UNION))
	goto done;
    if (!mk_addmember("disjoint_right.s", &wm.l, NULL, WMOP_SUBTRACT))
	goto done;
    if (mk_lcomb(wdbp, "test.r", &wm, 1, NULL, NULL, NULL, 0))
	goto done;

    dp = db_lookup(dbip, "test.r", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	goto done;

    /* rt_gettree prunes the disjoint subtractor from its prepared tree.
     * rt_bound_internal must still obtain bounds while walking the original
     * database tree, where that subtractor remains present. */
    if (rt_bound_internal(dbip, dp, bmin, bmax))
	goto done;
    if (!VNEAR_EQUAL(bmin, expected_min, BN_TOL_DIST))
	goto done;
    if (!VNEAR_EQUAL(bmax, expected_max, BN_TOL_DIST))
	goto done;

    ret = 0;

done:
    db_close(dbip);
    return ret;
}
