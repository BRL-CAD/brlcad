/*                  R E P R E P _ P R U N E . C
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
#include "rt/prep.h"
#include "wdb.h"

#include "../librt_private.h"


static int
test_hit(struct application *UNUSED(ap), struct partition *UNUSED(part_head), struct seg *UNUSED(segs))
{
    return 1;
}


static int
test_miss(struct application *UNUSED(ap))
{
    return 0;
}


static int
move_sphere(struct db_i *dbip, const char *name, const point_t center)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    struct rt_db_internal intern;
    struct rt_ell_internal *ell;

    if (dp == RT_DIR_NULL)
	return 1;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	return 1;
    if (intern.idb_type != ID_ELL) {
	rt_db_free_internal(&intern);
	return 1;
    }
    ell = (struct rt_ell_internal *)intern.idb_ptr;
    RT_ELL_CK_MAGIC(ell);
    VMOVE(ell->v, center);

    /* On success rt_db_put_internal releases intern. */
    if (rt_db_put_internal(dp, dbip, &intern) < 0) {
	rt_db_free_internal(&intern);
	return 1;
    }
    return 0;
}


int
main(int UNUSED(argc), char **UNUSED(argv))
{
    struct db_i *dbip = db_create_inmem();
    struct rt_wdb *wdbp;
    struct rt_i *rtip = NULL;
    struct resource resp = RT_RESOURCE_INIT_ZERO;
    struct rt_reprep_obj_list objs = {0};
    struct wmember wm;
    struct region *rp;
    struct soltab *left, *right;
    point_t center = VINIT_ZERO;
    char *topobjs[] = {(char *)"test.r"};
    char *unprepped[] = {(char *)"right.s"};
    int failures = 0;

    if (!dbip)
	return 1;
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return 1;
    }

    if (mk_sph(wdbp, "left.s", center, 1.0))
	failures++;
    VSET(center, 10.0, 0.0, 0.0);
    if (mk_sph(wdbp, "right.s", center, 1.0))
	failures++;
    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("left.s", &wm.l, NULL, WMOP_UNION))
	failures++;
    if (!mk_addmember("right.s", &wm.l, NULL, WMOP_SUBTRACT))
	failures++;
    if (mk_lcomb(wdbp, "test.r", &wm, 1, NULL, NULL, NULL, 0))
	failures++;
    if (failures)
	goto done;

    rtip = rt_i_create(dbip);
    if (rt_gettree(rtip, "test.r")) {
	failures++;
	goto done;
    }
    if (rt_find_solid(rtip, "right.s") != RT_SOLTAB_NULL) {
	bu_log("initial disjoint subtractor was not pruned\n");
	failures++;
	goto done;
    }
    rt_prep(rtip);
    rt_init_resource(&resp, 0, rtip);

    objs.ntopobjs = 1;
    objs.topobjs = topobjs;
    objs.nunprepped = 1;
    objs.unprepped = unprepped;
    if (rt_unprep(rtip, &objs)) {
	bu_log("rt_unprep could not invalidate a pruned subtractor path\n");
	failures++;
	goto done;
    }

    VSET(center, 0.5, 0.0, 0.0);
    if (move_sphere(rtip->rti_dbip, "right.s", center)) {
	bu_log("failed to edit the previously pruned subtractor\n");
	failures++;
	goto done;
    }
    if (rt_reprep(rtip, &objs)) {
	bu_log("rt_reprep failed after the subtractor became relevant\n");
	failures++;
	goto done;
    }

    left = rt_find_solid(rtip, "left.s");
    right = rt_find_solid(rtip, "right.s");
    if (!left || !right) {
	bu_log("reprep did not restore both region soltabs\n");
	failures++;
	goto done;
    }
    if (BU_LIST_IS_EMPTY(&rtip->HeadRegion)) {
	bu_log("reprep did not restore the affected region\n");
	failures++;
	goto done;
    }
    rp = BU_LIST_FIRST(region, &rtip->HeadRegion);
    if (!rp->reg_treetop || rp->reg_treetop->tr_op != OP_SUBTRACT || rp->reg_all_unions) {
	bu_log("reprep restored stale Boolean tree metadata\n");
	failures++;
    }
    int solid_id = left->st_id;
    if (right->st_id != solid_id || rtip->stats.nsolids != 2 ||
	rtip->i->rti_nsol_by_type[solid_id] != 2 ||
	!rtip->i->rti_sol_by_type[solid_id]) {
	bu_log("reprep left stale solid count/type metadata\n");
	failures++;
    } else {
	for (size_t i = 0; i < rtip->i->rti_nsol_by_type[solid_id]; i++) {
	    struct soltab *stp = rtip->i->rti_sol_by_type[solid_id][i];
	    if (stp != left && stp != right) {
		bu_log("reprep type table retained a freed soltab\n");
		failures++;
	    }
	}
    }

    /* Exercise the rebuilt BSP and accelerated soltab tables, not just their
     * pointer values.  The edited right sphere removes the back portion of
     * the left sphere, but leaves a nonempty interval for this ray. */
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = &resp;
    ap.a_hit = test_hit;
    ap.a_miss = test_miss;
    VSET(ap.a_ray.r_pt, -2.0, 0.0, 0.0);
    VSET(ap.a_ray.r_dir, 1.0, 0.0, 0.0);
    if (rt_shootray(&ap) != 1) {
	bu_log("ray tracing failed after dynamic reprep\n");
	failures++;
    }

done:
    if (rtip)
	rt_i_destroy(rtip);
    db_close(dbip);
    return failures ? 1 : 0;
}
