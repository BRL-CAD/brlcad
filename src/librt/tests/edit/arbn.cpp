/*                       A R B N . C P P
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
/** @file arbn.cpp
 *
 * Unit tests for ARBN primitive editing via edarbn.c.
 *
 * Reference ARBN: a unit cube defined by 6 axis-aligned planes.
 *
 * Tests verify:
 *   - ECMD_ARBN_PLANE_SELECT stores plane index correctly
 *   - ECMD_ARBN_PLANE_SET_DIST changes the 4th coefficient
 *   - ECMD_ARBN_PLANE_SET_NORM replaces the normal
 *   - ECMD_ARBN_PLANE_ROTATE rotates the normal
 *   - ECMD_ARBN_PLANE_ADD appends a new plane
 *   - ECMD_ARBN_PLANE_DEL removes a plane
 *   - rt_edit_arbn_get_params returns correct values
 *   - Descriptor is accessible and has the right number of commands
 */

#include "common.h"

#include <cmath>
#include <cstring>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"

/* Mirror the private ECMD numbers from edarbn.c */
#define ECMD_ARBN_PLANE_SELECT    14010
#define ECMD_ARBN_PLANE_SET_DIST  14011
#define ECMD_ARBN_PLANE_SET_NORM  14012
#define ECMD_ARBN_PLANE_ROTATE    14013
#define ECMD_ARBN_PLANE_ADD       14014
#define ECMD_ARBN_PLANE_DEL       14015

/* Mirror the private state struct */
struct rt_arbn_edit_local {
    int plane_index;
};


static struct directory *
make_arbn(struct rt_wdb *wdbp)
{
    /* Unit cube: 6 axis-aligned half-spaces */
    struct rt_arbn_internal *aip;
    BU_ALLOC(aip, struct rt_arbn_internal);
    aip->magic = RT_ARBN_INTERNAL_MAGIC;
    aip->neqn  = 6;
    aip->eqn   = (plane_t *)bu_calloc(6, sizeof(plane_t), "arbn eqn");

    /* +X plane: x <=  1  →  N=(1,0,0) d=1 */
    HSET(aip->eqn[0],  1,  0,  0,  1);
    /* -X plane: x >= -1  →  N=(-1,0,0) d=1 */
    HSET(aip->eqn[1], -1,  0,  0,  1);
    /* +Y plane */
    HSET(aip->eqn[2],  0,  1,  0,  1);
    /* -Y plane */
    HSET(aip->eqn[3],  0, -1,  0,  1);
    /* +Z plane */
    HSET(aip->eqn[4],  0,  0,  1,  1);
    /* -Z plane */
    HSET(aip->eqn[5],  0,  0, -1,  1);

    wdb_export(wdbp, "arbn", (void *)aip, ID_ARBN, 1.0);
    struct directory *dp = db_lookup(wdbp->dbip, "arbn", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create arbn object\n");
    return dp;
}

/* reset_sel is available for future test additions */
static void __attribute__((unused))
reset_sel(struct rt_edit *s)
{
    struct rt_arbn_edit_local *e = (struct rt_arbn_edit_local *)s->ipe_ptr;
    e->plane_index = -1;
    s->e_inpara = 0;
    VSETALL(s->e_para, 0.0);
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc != 1) return BRLCAD_ERROR;

    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create database\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = make_arbn(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size = 10.0;
    v->gv_isize = 0.1;
    v->gv_scale = 5.0;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width  = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;
    s->local2base = 1.0;
    s->base2local = 1.0;

    struct rt_arbn_internal *aip =
	(struct rt_arbn_internal *)s->es_int.idb_ptr;
    RT_ARBN_CK_MAGIC(aip);

    /* ================================================================
     * Test 1: descriptor is accessible and has 6 commands
     * ================================================================*/
    const struct rt_edit_prim_desc *desc =
	(*EDOBJ[dp->d_minor_type].ft_edit_desc)();
    if (!desc)
	bu_exit(1, "ERROR: arbn edit_desc is NULL\n");
    if (desc->ncmd != 6)
	bu_exit(1, "ERROR: arbn descriptor should have 6 cmds, got %d\n",
		desc->ncmd);
    bu_log("TEST 1 PASS: arbn descriptor has %d commands\n", desc->ncmd);

    /* ================================================================
     * Test 2: ECMD_ARBN_PLANE_SELECT — select plane 2
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_SELECT);
    s->e_inpara = 1;
    s->e_para[0] = 2.0;
    rt_edit_process(s);

    {
	struct rt_arbn_edit_local *e = (struct rt_arbn_edit_local *)s->ipe_ptr;
	if (e->plane_index != 2)
	    bu_exit(1, "ERROR: plane_select stored %d (expected 2)\n",
		    e->plane_index);
	bu_log("TEST 2 PASS: plane_index = %d\n", e->plane_index);
    }

    /* ================================================================
     * Test 3: ECMD_ARBN_PLANE_SET_DIST — change d of plane 2 to 5
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_SET_DIST);
    s->e_inpara = 1;
    s->e_para[0] = 5.0;
    rt_edit_process(s);

    if (!NEAR_EQUAL(aip->eqn[2][3], 5.0, SMALL_FASTF))
	bu_exit(1, "ERROR: plane_set_dist: d=%g (expected 5)\n", aip->eqn[2][3]);
    bu_log("TEST 3 PASS: plane 2 d = %g\n", aip->eqn[2][3]);

    /* ================================================================
     * Test 4: ECMD_ARBN_PLANE_SET_NORM — replace normal of plane 2
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_SET_NORM);
    s->e_inpara = 3;
    VSET(s->e_para, 0.0, 1.0, 0.0);
    rt_edit_process(s);

    {
	vect_t expected = {0, 1, 0};
	vect_t got;
	VSET(got, aip->eqn[2][0], aip->eqn[2][1], aip->eqn[2][2]);
	if (!VNEAR_EQUAL(got, expected, SMALL_FASTF))
	    bu_exit(1, "ERROR: plane_set_norm: N=(%g,%g,%g) expected (0,1,0)\n",
		    V3ARGS(got));
	bu_log("TEST 4 PASS: plane 2 normal = (%g,%g,%g)\n", V3ARGS(got));
    }

    /* ================================================================
     * Test 5: ECMD_ARBN_PLANE_ROTATE — rotate plane 2 normal by 90 deg around Z
     * Normal is currently (0,1,0); after 90-deg Z rotation → (-1,0,0).
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_ROTATE);
    s->e_inpara = 3;
    VSET(s->e_para, 0.0, 0.0, 90.0);   /* rx=0, ry=0, rz=90 */
    rt_edit_process(s);

    {
	vect_t expected = {-1, 0, 0};
	vect_t got;
	VSET(got, aip->eqn[2][0], aip->eqn[2][1], aip->eqn[2][2]);
	if (!VNEAR_EQUAL(got, expected, 1e-9))
	    bu_exit(1, "ERROR: plane_rotate: N=(%g,%g,%g) expected (-1,0,0)\n",
		    V3ARGS(got));
	bu_log("TEST 5 PASS: plane 2 rotated normal = (%g,%g,%g)\n", V3ARGS(got));
    }

    /* ================================================================
     * Test 6: ECMD_ARBN_PLANE_ADD — append a 7th plane
     * ================================================================*/
    size_t before_n = aip->neqn;
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_ADD);
    s->e_inpara = 4;
    s->e_para[0] = 1.0;   /* nx */
    s->e_para[1] = 1.0;   /* ny */
    s->e_para[2] = 1.0;   /* nz */
    s->e_para[3] = 3.0;   /* d  */
    rt_edit_process(s);

    if (aip->neqn != before_n + 1)
	bu_exit(1, "ERROR: plane_add: neqn=%zu (expected %zu)\n",
		aip->neqn, before_n + 1);
    bu_log("TEST 6 PASS: neqn now %zu (was %zu)\n", aip->neqn, before_n);

    /* ================================================================
     * Test 7: ECMD_ARBN_PLANE_DEL — remove the newly-selected plane
     * After PLANE_ADD, the new plane is auto-selected.
     * ================================================================*/
    size_t before_del = aip->neqn;
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_DEL);
    s->e_inpara = 0;
    rt_edit_process(s);

    if (aip->neqn != before_del - 1)
	bu_exit(1, "ERROR: plane_del: neqn=%zu (expected %zu)\n",
		aip->neqn, before_del - 1);
    bu_log("TEST 7 PASS: neqn now %zu (was %zu)\n", aip->neqn, before_del);

    /* ================================================================
     * Test 8: rt_edit_arbn_get_params returns plane index
     * ================================================================*/
    /* Re-select plane 0 so we have something to query */
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_SELECT);
    s->e_inpara = 1;
    s->e_para[0] = 0.0;
    rt_edit_process(s);

    fastf_t vals[4] = {0};
    int nv = (*EDOBJ[dp->d_minor_type].ft_edit_get_params)(s, ECMD_ARBN_PLANE_SELECT, vals);
    if (nv != 1 || (int)vals[0] != 0)
	bu_exit(1, "ERROR: get_params(SELECT): nv=%d vals[0]=%g\n", nv, vals[0]);
    bu_log("TEST 8 PASS: get_params(SELECT) returns plane_index=%g\n", vals[0]);

    bu_log("All ARBN edit tests PASSED\n");

    rt_edit_destroy(s);
    db_close(dbip);
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
