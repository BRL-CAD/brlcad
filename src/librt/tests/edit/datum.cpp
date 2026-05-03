/*                      D A T U M . C P P
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
/** @file datum.cpp
 *
 * Unit tests for DATUM primitive editing via eddatum2.c.
 *
 * Reference DATUM: a PLANE datum at the origin with dir=(0,0,1) and w=1.
 *
 * Tests verify:
 *   - Descriptor is accessible and has the right number of commands
 *   - ECMD_DATUM_SET_TYPE with silent zeroing (PLANE→LINE, PLANE→POINT)
 *   - ECMD_DATUM_SET_PNT sets the position
 *   - ECMD_DATUM_SET_DIR sets the direction
 *   - ECMD_DATUM_SET_W sets the scale factor
 *   - rt_edit_datum_get_params returns correct values
 */

#include "common.h"

#include <cstring>
#include <cmath>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"

/* ECMD numbers from eddatum2.c */
#define ECMD_DATUM_SET_TYPE  44010
#define ECMD_DATUM_SET_PNT   44011
#define ECMD_DATUM_SET_DIR   44012
#define ECMD_DATUM_SET_W     44013

#define DATUM_TYPE_POINT 0
#define DATUM_TYPE_LINE  1
#define DATUM_TYPE_PLANE 2


static struct directory *
make_datum_plane(struct rt_wdb *wdbp)
{
    struct rt_datum_internal *dp;
    BU_ALLOC(dp, struct rt_datum_internal);
    dp->magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(dp->pnt, 0, 0, 0);
    VSET(dp->dir, 0, 0, 1);
    dp->w = 1.0;   /* PLANE */
    dp->next = NULL;

    wdb_export(wdbp, "datum", (void *)dp, ID_DATUM, 1.0);
    struct directory *dir = db_lookup(wdbp->dbip, "datum", LOOKUP_QUIET);
    if (dir == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create datum object\n");
    return dir;
}

static void
reset_s(struct rt_edit *s, struct rt_datum_internal *dp)
{
    VSET(dp->pnt, 0, 0, 0);
    VSET(dp->dir, 0, 0, 1);
    dp->w = 1.0;
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
    struct directory *dir = make_datum_plane(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dir);

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

    struct rt_datum_internal *dp =
	(struct rt_datum_internal *)s->es_int.idb_ptr;
    RT_DATUM_CK_MAGIC(dp);

    /* ================================================================
     * Test 1: descriptor accessible and has 4 commands
     * ================================================================*/
    const struct rt_edit_prim_desc *desc =
	(*EDOBJ[dir->d_minor_type].ft_edit_desc)();
    if (!desc)
	bu_exit(1, "ERROR: datum edit_desc is NULL\n");
    if (desc->ncmd != 4)
	bu_exit(1, "ERROR: datum descriptor should have 4 cmds, got %d\n",
		desc->ncmd);
    bu_log("TEST 1 PASS: datum descriptor has %d commands\n", desc->ncmd);

    /* ================================================================
     * Test 2: ECMD_DATUM_SET_TYPE PLANE → LINE zeroes w
     * ================================================================*/
    reset_s(s, dp);
    (*EDOBJ[dir->d_minor_type].ft_set_edit_mode)(s, ECMD_DATUM_SET_TYPE);
    s->e_inpara = 1;
    s->e_para[0] = (fastf_t)DATUM_TYPE_LINE;
    rt_edit_process(s);

    if (!ZERO(dp->w))
	bu_exit(1, "ERROR: PLANE->LINE: w not zeroed (w=%g)\n", dp->w);
    if (MAGNITUDE(dp->dir) < SMALL_FASTF)
	bu_exit(1, "ERROR: PLANE->LINE: dir was zeroed (should keep dir)\n");
    bu_log("TEST 2 PASS: PLANE->LINE: w=%.6f dir=(%.3f,%.3f,%.3f)\n",
	   dp->w, V3ARGS(dp->dir));

    /* ================================================================
     * Test 3: ECMD_DATUM_SET_TYPE PLANE → POINT zeroes w and dir
     * ================================================================*/
    reset_s(s, dp);   /* restore to PLANE */
    (*EDOBJ[dir->d_minor_type].ft_set_edit_mode)(s, ECMD_DATUM_SET_TYPE);
    s->e_inpara = 1;
    s->e_para[0] = (fastf_t)DATUM_TYPE_POINT;
    rt_edit_process(s);

    if (!ZERO(dp->w))
	bu_exit(1, "ERROR: PLANE->POINT: w not zeroed\n");
    if (MAGNITUDE(dp->dir) > SMALL_FASTF)
	bu_exit(1, "ERROR: PLANE->POINT: dir not zeroed (%g,%g,%g)\n",
		V3ARGS(dp->dir));
    bu_log("TEST 3 PASS: PLANE->POINT: w=%.6f dir=(%.3f,%.3f,%.3f)\n",
	   dp->w, V3ARGS(dp->dir));

    /* ================================================================
     * Test 4: ECMD_DATUM_SET_PNT
     * ================================================================*/
    reset_s(s, dp);
    (*EDOBJ[dir->d_minor_type].ft_set_edit_mode)(s, ECMD_DATUM_SET_PNT);
    s->e_inpara = 3;
    VSET(s->e_para, 10, 20, 30);
    rt_edit_process(s);

    {
	vect_t expected = {10, 20, 30};
	if (!VNEAR_EQUAL(dp->pnt, expected, SMALL_FASTF))
	    bu_exit(1, "ERROR: set_pnt: got (%g,%g,%g) expected (10,20,30)\n",
		    V3ARGS(dp->pnt));
	bu_log("TEST 4 PASS: pnt = (%g,%g,%g)\n", V3ARGS(dp->pnt));
    }

    /* ================================================================
     * Test 5: ECMD_DATUM_SET_DIR
     * ================================================================*/
    reset_s(s, dp);
    (*EDOBJ[dir->d_minor_type].ft_set_edit_mode)(s, ECMD_DATUM_SET_DIR);
    s->e_inpara = 3;
    VSET(s->e_para, 1, 0, 0);
    rt_edit_process(s);

    {
	vect_t expected = {1, 0, 0};
	if (!VNEAR_EQUAL(dp->dir, expected, SMALL_FASTF))
	    bu_exit(1, "ERROR: set_dir: got (%g,%g,%g) expected (1,0,0)\n",
		    V3ARGS(dp->dir));
	bu_log("TEST 5 PASS: dir = (%g,%g,%g)\n", V3ARGS(dp->dir));
    }

    /* ================================================================
     * Test 6: ECMD_DATUM_SET_W
     * ================================================================*/
    reset_s(s, dp);
    (*EDOBJ[dir->d_minor_type].ft_set_edit_mode)(s, ECMD_DATUM_SET_W);
    s->e_inpara = 1;
    s->e_para[0] = 7.5;
    rt_edit_process(s);

    if (!NEAR_EQUAL(dp->w, 7.5, SMALL_FASTF))
	bu_exit(1, "ERROR: set_w: got %g expected 7.5\n", dp->w);
    bu_log("TEST 6 PASS: w = %g\n", dp->w);

    /* ================================================================
     * Test 7: rt_edit_datum_get_params returns correct pnt
     * ================================================================*/
    reset_s(s, dp);
    VSET(dp->pnt, 5, 6, 7);

    fastf_t vals[4] = {0};
    int nv = (*EDOBJ[dir->d_minor_type].ft_edit_get_params)(s, ECMD_DATUM_SET_PNT, vals);
    if (nv != 3 || !NEAR_EQUAL(vals[0], 5.0, SMALL_FASTF) ||
	!NEAR_EQUAL(vals[1], 6.0, SMALL_FASTF) ||
	!NEAR_EQUAL(vals[2], 7.0, SMALL_FASTF))
	bu_exit(1, "ERROR: get_params(SET_PNT): nv=%d vals=(%g,%g,%g)\n",
		nv, vals[0], vals[1], vals[2]);
    bu_log("TEST 7 PASS: get_params(SET_PNT) = (%g,%g,%g)\n",
	   vals[0], vals[1], vals[2]);

    bu_log("All DATUM edit tests PASSED\n");

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
