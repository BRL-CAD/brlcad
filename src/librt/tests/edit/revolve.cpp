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

#include "vmath.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"

#include "test_utils.h"

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
    s->e_inpara = 0;
    VSETALL(s->e_para, 0.0);
}

static void
expect_edit_success(struct rt_edit *s, const char *operation)
{
    if (rt_edit_process(s) != BRLCAD_OK)
	bu_exit(1, "ERROR: %s failed: %s\n", operation,
		bu_vls_cstr(s->log_str));
}

int
rt_edit_test_revolve(void)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create database\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (edit_test_make_sketch(wdbp, "unit_sketch", 1.0) != 0 ||
	edit_test_make_sketch(wdbp, "my_sketch", 2.0) != 0)
	bu_exit(1, "ERROR: Unable to create revolve sketch fixtures\n");
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
    expect_edit_success(s, "set_v");

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
    expect_edit_success(s, "set_axis");

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
    expect_edit_success(s, "set_r");

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
    expect_edit_success(s, "set_ang");

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
    rt_edit_set_str(s, 0, "my_sketch");
    if (rt_edit_process(s) != BRLCAD_OK ||
	!BU_STR_EQUAL(bu_vls_cstr(&rip->sketch_name), "my_sketch") ||
	!rip->skt ||
	!NEAR_EQUAL(rip->skt->verts[0][X], 2.0, SMALL_FASTF))
	bu_exit(1, "ERROR: set_skt did not load the new sketch\n");

    rt_edit_set_str(s, 0, "missing_sketch");
    if (rt_edit_process(s) != BRLCAD_ERROR ||
	!BU_STR_EQUAL(bu_vls_cstr(&rip->sketch_name), "my_sketch") ||
	!rip->skt ||
	!NEAR_EQUAL(rip->skt->verts[0][X], 2.0, SMALL_FASTF))
	bu_exit(1, "ERROR: missing sketch changed the revolve reference\n");

    rt_edit_set_str(s, 0, "revolve");
    if (rt_edit_process(s) != BRLCAD_ERROR ||
	!BU_STR_EQUAL(bu_vls_cstr(&rip->sketch_name), "my_sketch") ||
	!rip->skt ||
	!NEAR_EQUAL(rip->skt->verts[0][X], 2.0, SMALL_FASTF))
	bu_exit(1, "ERROR: non-sketch reference changed the revolve\n");
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

    const fastf_t inch = 25.4;
    s->local2base = inch;
    s->base2local = 1.0 / inch;
    reset_s(s, rip);

    rt_edit_set_edflag(s, ECMD_REVOLVE_SET_V);
    s->e_inpara = 3;
    VSET(s->e_para, 1.0, 2.0, 3.0);
    if (rt_edit_process(s) != BRLCAD_OK ||
	!NEAR_EQUAL(rip->v3d[X], inch, SMALL_FASTF) ||
	!NEAR_EQUAL(rip->v3d[Y], 2.0 * inch, SMALL_FASTF) ||
	!NEAR_EQUAL(rip->v3d[Z], 3.0 * inch, SMALL_FASTF))
	bu_exit(1, "ERROR: set_v did not convert local coordinates\n");

    rt_edit_set_edflag(s, ECMD_REVOLVE_SET_AXIS);
    s->e_inpara = 3;
    VSET(s->e_para, 0.0, 0.0, 2.0);
    if (rt_edit_process(s) != BRLCAD_OK ||
	!NEAR_EQUAL(rip->axis3d[Z], 2.0 * inch, SMALL_FASTF))
	bu_exit(1, "ERROR: set_axis did not convert local length\n");

    rt_edit_set_edflag(s, ECMD_REVOLVE_SET_R);
    s->e_inpara = 3;
    VSET(s->e_para, 3.0, 0.0, 0.0);
    if (rt_edit_process(s) != BRLCAD_OK ||
	!NEAR_EQUAL(rip->r[X], 3.0 * inch, SMALL_FASTF))
	bu_exit(1, "ERROR: set_r did not convert local length\n");

    nv = (*EDOBJ[dp->d_minor_type].ft_edit_get_params)(s, ECMD_REVOLVE_SET_R, vals);
    if (nv != 3 || !NEAR_EQUAL(vals[X], 3.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: get_params(SET_R) returned count %d, x %g, base x %g, factor %g\n",
		nv, vals[X], rip->r[X], s->base2local);

    rt_edit_set_edflag(s, ECMD_REVOLVE_SET_AXIS);
    s->e_inpara = 2;
    if (rt_edit_process(s) != BRLCAD_ERROR ||
	!NEAR_EQUAL(rip->axis3d[Z], 2.0 * inch, SMALL_FASTF) ||
	!bu_vls_strlen(s->log_str))
	bu_exit(1, "ERROR: invalid axis input did not fail without mutation\n");

    struct rt_db_internal copied;
    RT_DB_INTERNAL_INIT(&copied);
    if (OBJ[ID_REVOLVE].ft_xform(&copied, bn_mat_identity,
	    &s->es_int, 0, dbip) != BRLCAD_OK)
	bu_exit(1, "ERROR: revolve copy transform failed\n");
    struct rt_revolve_internal *copy =
	(struct rt_revolve_internal *)copied.idb_ptr;
    if (!copy || copy->skt == rip->skt || !copy->skt ||
	!NEAR_EQUAL(copy->skt->verts[0][X], 2.0, SMALL_FASTF) ||
	!NEAR_EQUAL(copy->r[X], 3.0 * inch, SMALL_FASTF) ||
	!NEAR_EQUAL(copy->ang, rip->ang, SMALL_FASTF))
	bu_exit(1, "ERROR: revolve copy lost its sketch, start vector, or angle\n");
    rt_db_free_internal(&copied);

    struct rt_sketch_internal *current_sketch = rip->skt;
    if (OBJ[ID_REVOLVE].ft_xform(&s->es_int, bn_mat_identity,
	    &s->es_int, 0, dbip) != BRLCAD_OK ||
	rip->skt != current_sketch ||
	!NEAR_EQUAL(rip->r[X], 3.0 * inch, SMALL_FASTF))
	bu_exit(1, "ERROR: in-place revolve transform replaced the sketch\n");

    struct rt_db_internal source, moved;
    RT_DB_INTERNAL_INIT(&source);
    RT_DB_INTERNAL_INIT(&moved);
    if (rt_db_get_internal(&source, dp, dbip, NULL) != ID_REVOLVE)
	bu_exit(1, "ERROR: revolve transfer fixture could not be imported\n");
    struct rt_revolve_internal *source_rip =
	(struct rt_revolve_internal *)source.idb_ptr;
    struct rt_sketch_internal *transferred_sketch = source_rip->skt;
    if (OBJ[ID_REVOLVE].ft_xform(&moved, bn_mat_identity,
	    &source, 1, dbip) != BRLCAD_OK)
	bu_exit(1, "ERROR: revolve transfer transform failed\n");
    struct rt_revolve_internal *moved_rip =
	(struct rt_revolve_internal *)moved.idb_ptr;
    if (!moved_rip || !moved_rip->skt ||
	moved_rip->skt != transferred_sketch ||
	!NEAR_EQUAL(moved_rip->skt->verts[0][X], 1.0, SMALL_FASTF) ||
	!NEAR_EQUAL(moved_rip->r[X], 1.0, SMALL_FASTF) ||
	!NEAR_EQUAL(moved_rip->ang, M_2PI, SMALL_FASTF))
	bu_exit(1, "ERROR: revolve transfer lost geometry or sketch ownership\n");
    rt_db_free_internal(&moved);

    struct rt_db_internal prepared_ip;
    RT_DB_INTERNAL_INIT(&prepared_ip);
    if (rt_db_get_internal(&prepared_ip, dp, dbip, NULL) != ID_REVOLVE)
	bu_exit(1, "ERROR: revolve prep fixture could not be imported\n");
    struct rt_revolve_internal *prepared_rip =
	(struct rt_revolve_internal *)prepared_ip.idb_ptr;
    struct soltab st = RT_SOLTAB_INIT_ZERO;
    st.l.magic = RT_SOLTAB_MAGIC;
    st.l2.magic = RT_SOLTAB2_MAGIC;
    if (OBJ[ID_REVOLVE].ft_prep(&st, &prepared_ip, NULL) != 0 ||
	!st.st_specific || !prepared_rip->skt)
	bu_exit(1, "ERROR: revolve prep did not preserve its input sketch\n");
    rt_db_free_internal(&prepared_ip);
    OBJ[ID_REVOLVE].ft_print(&st);
    OBJ[ID_REVOLVE].ft_free(&st);
    if (st.st_specific)
	bu_exit(1, "ERROR: revolve free retained prepared state\n");

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
