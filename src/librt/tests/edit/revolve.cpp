/*                    R E V O L V E . C P P
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
/** @file revolve.cpp
 *
 * Unit tests for REVOLVE primitive editing via edrevolve.c.
 *
 * Reference REVOLVE: 360-degree solid of revolution from a unit sketch.
 *   v3d   = (0,0,0)
 *   axis3d= (0,0,1)
 *   r     = (1,0,0)
 *   ang   = M_PI*2 (360 deg)
 *   sketch_name = "unit_sketch"
 *
 * Tests verify:
 *   - Descriptor is accessible and has the right number of commands
 *   - ECMD_REVOLVE_SET_V sets vertex
 *   - ECMD_REVOLVE_SET_AXIS sets axis
 *   - ECMD_REVOLVE_SET_R sets start vector
 *   - ECMD_REVOLVE_SET_ANG sets angle (degrees in, radians stored)
 *   - ECMD_REVOLVE_SET_SKT sets sketch name
 *   - rt_edit_revolve_get_params returns correct values
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

/* ECMD numbers from edrevolve.c */
#define ECMD_REVOLVE_SET_V    40010
#define ECMD_REVOLVE_SET_AXIS 40011
#define ECMD_REVOLVE_SET_R    40012
#define ECMD_REVOLVE_SET_ANG  40013
#define ECMD_REVOLVE_SET_SKT  40014


static struct directory *
make_revolve(struct rt_wdb *wdbp)
{
    struct rt_revolve_internal *rip;
    BU_ALLOC(rip, struct rt_revolve_internal);
    rip->magic = RT_REVOLVE_INTERNAL_MAGIC;
    VSET(rip->v3d,    0, 0, 0);
    VSET(rip->axis3d, 0, 0, 1);
    VSET(rip->r,      1, 0, 0);
    rip->ang = M_2PI;
    bu_vls_init(&rip->sketch_name);
    bu_vls_strcpy(&rip->sketch_name, "unit_sketch");
    rip->skt = NULL;

    wdb_export(wdbp, "revolve", (void *)rip, ID_REVOLVE, 1.0);
    struct directory *dp = db_lookup(wdbp->dbip, "revolve", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create revolve object\n");
    return dp;
}

static void
reset_s(struct rt_edit *s, struct rt_revolve_internal *rip)
{
    VSET(rip->v3d,    0, 0, 0);
    VSET(rip->axis3d, 0, 0, 1);
    VSET(rip->r,      1, 0, 0);
    rip->ang = M_2PI;
    bu_vls_trunc(&rip->sketch_name, 0);
    bu_vls_strcpy(&rip->sketch_name, "unit_sketch");
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
    struct directory *dp = make_revolve(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    v->gv_size = 10.0; v->gv_isize = 0.1; v->gv_scale = 5.0;
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width = 512; v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;
    s->local2base = 1.0;
    s->base2local = 1.0;

    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    /* ================================================================
     * Test 1: descriptor accessible and has 5 commands
     * ================================================================*/
    const struct rt_edit_prim_desc *desc =
	(*EDOBJ[dp->d_minor_type].ft_edit_desc)();
    if (!desc)
	bu_exit(1, "ERROR: revolve edit_desc is NULL\n");
    if (desc->ncmd != 5)
	bu_exit(1, "ERROR: revolve descriptor should have 5 cmds, got %d\n",
		desc->ncmd);
    bu_log("TEST 1 PASS: revolve descriptor has %d commands\n", desc->ncmd);

    /* ================================================================
     * Test 2: ECMD_REVOLVE_SET_V
     * ================================================================*/
    reset_s(s, rip);
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_REVOLVE_SET_V);
    s->e_inpara = 3;
    VSET(s->e_para, 5, 6, 7);
    rt_edit_process(s);

    {
	vect_t expected = {5, 6, 7};
	if (!VNEAR_EQUAL(rip->v3d, expected, SMALL_FASTF))
	    bu_exit(1, "ERROR: set_v: got (%g,%g,%g) expected (5,6,7)\n",
		    V3ARGS(rip->v3d));
	bu_log("TEST 2 PASS: v3d = (%g,%g,%g)\n", V3ARGS(rip->v3d));
    }

    /* ================================================================
     * Test 3: ECMD_REVOLVE_SET_AXIS
     * ================================================================*/
    reset_s(s, rip);
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_REVOLVE_SET_AXIS);
    s->e_inpara = 3;
    VSET(s->e_para, 0, 1, 0);
    rt_edit_process(s);

    {
	vect_t expected = {0, 1, 0};
	if (!VNEAR_EQUAL(rip->axis3d, expected, SMALL_FASTF))
	    bu_exit(1, "ERROR: set_axis: got (%g,%g,%g) expected (0,1,0)\n",
		    V3ARGS(rip->axis3d));
	bu_log("TEST 3 PASS: axis3d = (%g,%g,%g)\n", V3ARGS(rip->axis3d));
    }

    /* ================================================================
     * Test 4: ECMD_REVOLVE_SET_R
     * ================================================================*/
    reset_s(s, rip);
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_REVOLVE_SET_R);
    s->e_inpara = 3;
    VSET(s->e_para, 0, 1, 0);
    rt_edit_process(s);

    {
	vect_t expected = {0, 1, 0};
	if (!VNEAR_EQUAL(rip->r, expected, SMALL_FASTF))
	    bu_exit(1, "ERROR: set_r: got (%g,%g,%g) expected (0,1,0)\n",
		    V3ARGS(rip->r));
	bu_log("TEST 4 PASS: r = (%g,%g,%g)\n", V3ARGS(rip->r));
    }

    /* ================================================================
     * Test 5: ECMD_REVOLVE_SET_ANG (degrees in, radians stored)
     * ================================================================*/
    reset_s(s, rip);
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_REVOLVE_SET_ANG);
    s->e_inpara = 1;
    s->e_para[0] = 180.0;   /* degrees */
    rt_edit_process(s);

    if (!NEAR_EQUAL(rip->ang, M_PI, 1e-9))
	bu_exit(1, "ERROR: set_ang: got %g rad expected %g rad (180 deg)\n",
		rip->ang, M_PI);
    bu_log("TEST 5 PASS: ang = %g rad (%.1f deg)\n",
	   rip->ang, rip->ang * RAD2DEG);

    /* ================================================================
     * Test 6: ECMD_REVOLVE_SET_SKT
     * ================================================================*/
    reset_s(s, rip);
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_REVOLVE_SET_SKT);
    bu_strlcpy(s->e_str[0], "my_sketch", RT_EDIT_MAXSTR_LEN);
    s->e_nstr = 1;
    s->e_inpara = 0;
    rt_edit_process(s);

    if (bu_strcmp(bu_vls_cstr(&rip->sketch_name), "my_sketch") != 0)
	bu_exit(1, "ERROR: set_skt: got '%s' expected 'my_sketch'\n",
		bu_vls_cstr(&rip->sketch_name));
    bu_log("TEST 6 PASS: sketch_name = '%s'\n", bu_vls_cstr(&rip->sketch_name));

    /* ================================================================
     * Test 7: rt_edit_revolve_get_params returns angle in degrees
     * ================================================================*/
    reset_s(s, rip);   /* ang = M_2PI */
    fastf_t vals[4] = {0};
    int nv = (*EDOBJ[dp->d_minor_type].ft_edit_get_params)(s, ECMD_REVOLVE_SET_ANG, vals);
    if (nv != 1 || !NEAR_EQUAL(vals[0], 360.0, 1e-9))
	bu_exit(1, "ERROR: get_params(SET_ANG): nv=%d vals[0]=%g\n", nv, vals[0]);
    bu_log("TEST 7 PASS: get_params(SET_ANG) = %g deg\n", vals[0]);

    bu_log("All REVOLVE edit tests PASSED\n");

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
