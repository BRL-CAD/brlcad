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

#include <math.h>

#include "bu/app.h"
#include "raytrace.h"
#include "rt/calc.h"
#include "wdb.h"


int
main(int UNUSED(argc), const char **argv)
{
    struct db_i *dbip = db_create_inmem();
    struct rt_wdb *wdbp;
    struct wmember wm;
    struct directory *dp;
    point_t center = VINIT_ZERO;
    point_t bmin, bmax;
    point_t expected_min = {-1.0, -1.0, -1.0};
    point_t expected_max = { 1.0,  1.0,  1.0};
    point_t rpp_min = {-1.0, -2.0, -3.0};
    point_t rpp_max = { 1.0,  2.0,  3.0};
    mat_t translated;
    mat_t rotated;
    const char *objects[] = {"box.r"};
    const char *bounded_and_unbounded[] = {"box.r", "half.r"};
    const char *empty_and_bounded[] = {"empty.r", "box.r"};
    const char *disjoint_and_bounded[] = {"disjoint.r", "box.r"};
    int ret = 1;

    bu_setprogname(argv[0]);

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

    dp = db_lookup(dbip, "left.s", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || rt_bound_internal(dbip, dp, bmin, bmax))
	goto done;
    if (!VNEAR_EQUAL(bmin, expected_min, BN_TOL_DIST) ||
	!VNEAR_EQUAL(bmax, expected_max, BN_TOL_DIST))
	goto done;
    if (mk_rpp(wdbp, "box.s", rpp_min, rpp_max))
	goto done;
    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("box.s", &wm.l, NULL, WMOP_UNION))
	goto done;
    if (mk_lcomb(wdbp, "box.r", &wm, 1, NULL, NULL, NULL, 0))
	goto done;

    VSET(expected_min, -1.0, -2.0, -3.0);
    VSET(expected_max, 1.0, 2.0, 3.0);
    if (rt_obj_bounds_mat(NULL, dbip, 1, objects, 0, NULL, 0, bmin, bmax) != BRLCAD_OK) {
	bu_log("rt_obj_bounds_mat identity call failed\n");
	goto done;
    }
    if (!VNEAR_EQUAL(bmin, expected_min, BN_TOL_DIST) ||
	!VNEAR_EQUAL(bmax, expected_max, BN_TOL_DIST)) {
	bu_log("rt_obj_bounds_mat identity bounds incorrect: (%g %g %g) (%g %g %g)\n",
		V3ARGS(bmin), V3ARGS(bmax));
	goto done;
    }

    MAT_IDN(rotated);
    rotated[0] = 0.0;
    rotated[1] = -1.0;
    rotated[4] = 1.0;
    rotated[5] = 0.0;
    VSET(expected_min, -2.0, -1.0, -3.0);
    VSET(expected_max, 2.0, 1.0, 3.0);
    if (rt_obj_bounds_mat(NULL, dbip, 1, objects, 0, rotated, 0, bmin, bmax) != BRLCAD_OK) {
	bu_log("rt_obj_bounds_mat rotation call failed\n");
	goto done;
    }
    if (!VNEAR_EQUAL(bmin, expected_min, BN_TOL_DIST) ||
	!VNEAR_EQUAL(bmax, expected_max, BN_TOL_DIST)) {
	bu_log("rt_obj_bounds_mat rotation bounds incorrect: (%g %g %g) (%g %g %g)\n",
		V3ARGS(bmin), V3ARGS(bmax));
	goto done;
    }
    if (rt_obj_bounds_mat(NULL, dbip, 1, objects, 0, NULL,
	RT_OBJ_BOUNDS_IGNORE_INFINITE << 1, bmin, bmax) != BRLCAD_ERROR)
	goto done;

    VSET(center, 0.0, 0.0, 1.0);
    if (mk_half(wdbp, "half.s", center, 0.0))
	goto done;
    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("half.s", &wm.l, NULL, WMOP_UNION))
	goto done;
    if (mk_lcomb(wdbp, "half.r", &wm, 1, NULL, NULL, NULL, 0))
	goto done;
    if (rt_obj_bounds_mat(NULL, dbip, 2, bounded_and_unbounded, 0, NULL,
	0, bmin, bmax) != BRLCAD_OK ||
	isfinite(bmin[0]) || isfinite(bmin[1]) || isfinite(bmin[2]) ||
	isfinite(bmax[0]) || isfinite(bmax[1]) || isfinite(bmax[2]))
	goto done;
    VSET(expected_min, -1.0, -2.0, -3.0);
    VSET(expected_max, 1.0, 2.0, 3.0);
    if (rt_obj_bounds_mat(NULL, dbip, 2, bounded_and_unbounded, 0, NULL,
	RT_OBJ_BOUNDS_IGNORE_INFINITE, bmin, bmax) != BRLCAD_OK)
	goto done;
    if (!VNEAR_EQUAL(bmin, expected_min, BN_TOL_DIST) ||
	!VNEAR_EQUAL(bmax, expected_max, BN_TOL_DIST))
	goto done;
    if (rt_obj_bounds_mat(NULL, dbip, 1, &bounded_and_unbounded[1], 0,
	NULL, RT_OBJ_BOUNDS_IGNORE_INFINITE, bmin, bmax) != BRLCAD_ERROR)
	goto done;

    BU_LIST_INIT(&wm.l);
    if (mk_comb(wdbp, "empty.c", &wm.l, 0, NULL, NULL, NULL,
	0, 0, 0, 0, 0, 0, 0))
	goto done;

    BU_LIST_INIT(&wm.l);
    if (mk_lcomb(wdbp, "empty.r", &wm, 1, NULL, NULL, NULL, 0))
	goto done;
    VSET(expected_min, -1.0, -2.0, -3.0);
    VSET(expected_max, 1.0, 2.0, 3.0);
    if (rt_obj_bounds_mat(NULL, dbip, 2, empty_and_bounded, 0, NULL,
	0, bmin, bmax) != BRLCAD_OK ||
	!VNEAR_EQUAL(bmin, expected_min, BN_TOL_DIST) ||
	!VNEAR_EQUAL(bmax, expected_max, BN_TOL_DIST))
	goto done;
    if (rt_obj_bounds_mat(NULL, dbip, 1, empty_and_bounded, 0, NULL,
	0, bmin, bmax) != BRLCAD_ERROR)
	goto done;

    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("left.s", &wm.l, NULL, WMOP_UNION) ||
	!mk_addmember("disjoint_right.s", &wm.l, NULL, WMOP_INTERSECT))
	goto done;
    if (mk_lcomb(wdbp, "disjoint.r", &wm, 1, NULL, NULL, NULL, 0))
	goto done;
    if (rt_obj_bounds_mat(NULL, dbip, 2, disjoint_and_bounded, 0, NULL,
	0, bmin, bmax) != BRLCAD_OK ||
	!VNEAR_EQUAL(bmin, expected_min, BN_TOL_DIST) ||
	!VNEAR_EQUAL(bmax, expected_max, BN_TOL_DIST))
	goto done;
    if (rt_obj_bounds_mat(NULL, dbip, 1, disjoint_and_bounded, 0, NULL,
	0, bmin, bmax) != BRLCAD_ERROR)
	goto done;

    dp = db_lookup(dbip, "empty.c", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL ||
	rt_bound_internal(dbip, dp, bmin, bmax) == 0)
	goto done;

    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("empty.c", &wm.l, NULL, WMOP_UNION))
	goto done;
    if (mk_lcomb(wdbp, "nested.c", &wm, 0, NULL, NULL, NULL, 0))
	goto done;
    dp = db_lookup(dbip, "nested.c", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL ||
	rt_bound_internal(dbip, dp, bmin, bmax) == 0)
	goto done;

    MAT_IDN(translated);
    MAT_DELTAS(translated, 20.0, 0.0, 0.0);
    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("left.s", &wm.l, NULL, WMOP_UNION) ||
	!mk_addmember("left.s", &wm.l, translated, WMOP_UNION))
	goto done;
    if (mk_lcomb(wdbp, "instanced.r", &wm, 1, NULL, NULL, NULL, 0))
	goto done;
    dp = db_lookup(dbip, "instanced.r", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || rt_bound_internal(dbip, dp, bmin, bmax))
	goto done;
    VSET(expected_min, -1.0, -1.0, -1.0);
    VSET(expected_max, 21.0, 1.0, 1.0);
    if (!VNEAR_EQUAL(bmin, expected_min, BN_TOL_DIST) ||
	!VNEAR_EQUAL(bmax, expected_max, BN_TOL_DIST))
	goto done;

    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("left.s", &wm.l, translated, WMOP_UNION))
	goto done;
    if (mk_lcomb(wdbp, "child.c", &wm, 0, NULL, NULL, NULL, 0))
	goto done;
    MAT_DELTAS(translated, 10.0, 0.0, 0.0);
    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("child.c", &wm.l, translated, WMOP_UNION))
	goto done;
    if (mk_lcomb(wdbp, "parent.r", &wm, 1, NULL, NULL, NULL, 0))
	goto done;
    dp = db_lookup(dbip, "parent.r", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || rt_bound_internal(dbip, dp, bmin, bmax))
	goto done;
    VSET(expected_min, 29.0, -1.0, -1.0);
    VSET(expected_max, 31.0, 1.0, 1.0);
    if (!VNEAR_EQUAL(bmin, expected_min, BN_TOL_DIST) ||
	!VNEAR_EQUAL(bmax, expected_max, BN_TOL_DIST))
	goto done;

    ret = 0;

done:
    db_close(dbip);
    return ret;
}
