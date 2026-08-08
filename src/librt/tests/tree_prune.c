/*                    T R E E _ P R U N E . C
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
#include "wdb.h"

#include "../librt_private.h"


struct callback_data {
    int calls;
    int expect_pruned;
    int failures;
    union tree *retained_tree;
};


static void
check_final_tree_callback(struct rt_i *rtip, struct db_tree_state *UNUSED(tsp), struct region *rp)
{
    struct callback_data *data = (struct callback_data *)rtip->rti_udata;

    data->calls++;
    data->retained_tree = rp->reg_treetop;
    if (!rp->reg_treetop) {
	data->failures++;
	return;
    }

    if (data->expect_pruned) {
	if (rp->reg_treetop->tr_op != OP_SOLID || !rp->reg_all_unions)
	    data->failures++;
	if (rt_find_solid(rtip, "right.s") != RT_SOLTAB_NULL)
	    data->failures++;
    } else {
	if (rp->reg_treetop->tr_op != OP_SUBTRACT || rp->reg_all_unions)
	    data->failures++;
	if (rt_find_solid(rtip, "right.s") == RT_SOLTAB_NULL)
	    data->failures++;
    }
}


static int
check_separation(const char *label, fastf_t gap, int expect_pruned)
{
    struct db_i *dbip = db_create_inmem();
    struct rt_wdb *wdbp;
    struct rt_i *rtip = NULL;
    struct wmember wm;
    struct region *rp;
    struct callback_data data = {0, expect_pruned, 0, TREE_NULL};
    point_t center = VINIT_ZERO;
    fastf_t tol;
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
    VSET(center, 2.0 + gap, 0.0, 0.0);
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
    tol = rtip->rti_tol.dist;
    rtip->rti_udata = &data;
    rtip->rti_gettrees_clbk = check_final_tree_callback;
    if (rt_gettree(rtip, "test.r")) {
	bu_log("%s: rt_gettree failed\n", label);
	failures++;
	goto done;
    }

    if (data.calls != 1 || data.failures) {
	bu_log("%s: callback did not observe finalized tree metadata\n", label);
	failures++;
    }
    rp = BU_LIST_FIRST(region, &rtip->HeadRegion);
    if (rp == REGION_NULL || data.retained_tree != rp->reg_treetop) {
	bu_log("%s: callback tree pointer was not valid after rt_gettree\n", label);
	failures++;
    }
    if (expect_pruned != (rt_find_solid(rtip, "right.s") == RT_SOLTAB_NULL)) {
	bu_log("%s: subtractor pruning result was not conservative\n", label);
	failures++;
    }
    if (!NEAR_EQUAL(rtip->mdl_min[X], -1.0, tol) ||
	!NEAR_EQUAL(rtip->mdl_max[X], 1.0, tol)) {
	bu_log("%s: stale model bounds after pruning\n", label);
	failures++;
    }

done:
    if (rtip)
	rt_i_destroy(rtip);
    db_close(dbip);
    return failures;
}


static void
mark_piece_callback(struct rt_i *UNUSED(rtip), struct db_tree_state *UNUSED(tsp), struct region *rp)
{
    if (rp->reg_treetop && rp->reg_treetop->tr_op == OP_SOLID)
	rp->reg_treetop->tr_a.tu_stp->st_npieces = 2;
}


static int
check_repeated_piece_metadata(void)
{
    struct db_i *dbip = db_create_inmem();
    struct rt_wdb *wdbp;
    struct rt_i *rtip = NULL;
    struct wmember wm;
    struct soltab *one, *two;
    point_t center = VINIT_ZERO;
    int failures = 0;

    if (!dbip)
	return 1;
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return 1;
    }

    if (mk_sph(wdbp, "one.s", center, 1.0))
	failures++;
    VSET(center, 10.0, 0.0, 0.0);
    if (mk_sph(wdbp, "two.s", center, 1.0))
	failures++;
    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("one.s", &wm.l, NULL, WMOP_UNION) ||
	mk_lcomb(wdbp, "one.r", &wm, 1, NULL, NULL, NULL, 0))
	failures++;
    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("two.s", &wm.l, NULL, WMOP_UNION) ||
	mk_lcomb(wdbp, "two.r", &wm, 1, NULL, NULL, NULL, 0))
	failures++;
    if (failures)
	goto done;

    rtip = rt_i_create(dbip);
    rtip->rti_gettrees_clbk = mark_piece_callback;
    if (rt_gettree(rtip, "one.r") || rtip->i->rti_nsolids_with_pieces != 1) {
	bu_log("piece metadata: first gettree count is stale\n");
	failures++;
    }
    if (rt_gettree(rtip, "two.r") || rtip->i->rti_nsolids_with_pieces != 2) {
	bu_log("piece metadata: repeated gettree duplicated piece indices\n");
	failures++;
    }
    one = rt_find_solid(rtip, "one.s");
    two = rt_find_solid(rtip, "two.s");
    if (!one || !two || one->st_piecestate_num == two->st_piecestate_num) {
	bu_log("piece metadata: surviving solids lack unique piece indices\n");
	failures++;
    }
    if (one)
	one->st_npieces = 0;
    if (two)
	two->st_npieces = 0;

done:
    if (rtip)
	rt_i_destroy(rtip);
    db_close(dbip);
    return failures;
}


int
main(int UNUSED(argc), char **UNUSED(argv))
{
    struct db_i *tol_dbip = db_create_inmem();
    struct rt_i *tol_rtip;
    fastf_t tol;
    int failures = 0;

    if (!tol_dbip)
	return 1;
    tol_rtip = rt_i_create(tol_dbip);
    tol = tol_rtip->rti_tol.dist;
    rt_i_destroy(tol_rtip);
    db_close(tol_dbip);

    failures += check_separation("separated", 2.0 * tol, 1);
    failures += check_separation("within tolerance", 0.5 * tol, 0);
    failures += check_separation("touching", 0.0, 0);
    failures += check_separation("overlapping", -0.5, 0);
    failures += check_repeated_piece_metadata();

    return failures ? 1 : 0;
}
