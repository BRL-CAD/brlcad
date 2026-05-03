/*                     A N N O T . C P P
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
/** @file annot.cpp
 *
 * Unit tests for ANNOT primitive editing via edannot.c.
 *
 * Reference ANNOT: a simple annotation with a single ANN_TSEG_MAGIC
 * text segment labelling "Hello" at vertex 0, positioned at (1,2,0).
 *
 * Tests verify:
 *   - Descriptor is accessible and has the right number of commands
 *   - ECMD_ANNOT_SET_TEXT replaces the label text
 *   - ECMD_ANNOT_SET_POS sets the 3-D anchor V
 *   - ECMD_ANNOT_SET_TSEG_TXT_SIZE changes the text size
 *   - ECMD_ANNOT_VERT_MOVE moves a 2-D control vertex
 *   - rt_edit_annot_get_params returns correct values
 */

#include "common.h"

#include <cstring>
#include <cmath>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/magic.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

/* ECMD numbers from edannot.c */
#define ECMD_ANNOT_SET_TEXT          42010
#define ECMD_ANNOT_SET_POS           42011
#define ECMD_ANNOT_SET_TSEG_TXT_SIZE 42012
#define ECMD_ANNOT_VERT_MOVE         42013


static struct directory *
make_annot(struct rt_wdb *wdbp)
{
    /* Build the internal struct by hand */
    struct rt_annot_internal *aip;
    BU_ALLOC(aip, struct rt_annot_internal);
    aip->magic = RT_ANNOT_INTERNAL_MAGIC;
    VSET(aip->V, 1, 2, 0);

    /* One 2-D vertex */
    aip->vert_count = 1;
    aip->verts = (point2d_t *)bu_calloc(1, sizeof(point2d_t), "verts");
    V2SET(aip->verts[0], 0.1, 0.2);

    /* One text segment */
    aip->ant.count = 1;
    aip->ant.reverse = (int *)bu_calloc(1, sizeof(int), "reverse");
    aip->ant.segments = (void **)bu_calloc(1, sizeof(void *), "segments");

    struct txt_seg *tsg;
    BU_ALLOC(tsg, struct txt_seg);
    tsg->magic = ANN_TSEG_MAGIC;
    tsg->ref_pt = 0;
    tsg->rel_pos = RT_TXT_POS_BL;
    bu_vls_init(&tsg->label);
    bu_vls_strcpy(&tsg->label, "Hello");
    tsg->txt_size = 10.0;
    tsg->txt_rot_angle = 0.0;
    aip->ant.segments[0] = (void *)tsg;

    int ret = mk_annot(wdbp, "annot", aip);
    if (ret)
	bu_exit(1, "ERROR: mk_annot failed\n");

    /* mk_annot copied the struct; free our temporaries */
    bu_vls_free(&tsg->label);
    BU_PUT(tsg, struct txt_seg);
    bu_free(aip->ant.segments, "segments");
    bu_free(aip->ant.reverse, "reverse");
    bu_free(aip->verts, "verts");
    BU_PUT(aip, struct rt_annot_internal);

    struct directory *dp = db_lookup(wdbp->dbip, "annot", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to find annot object\n");
    return dp;
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
    struct directory *dp = make_annot(wdbp);

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

    struct rt_annot_internal *aip =
	(struct rt_annot_internal *)s->es_int.idb_ptr;
    RT_ANNOT_CK_MAGIC(aip);

    /* ================================================================
     * Test 1: descriptor accessible and has 4 commands
     * ================================================================*/
    const struct rt_edit_prim_desc *desc =
	(*EDOBJ[dp->d_minor_type].ft_edit_desc)();
    if (!desc)
	bu_exit(1, "ERROR: annot edit_desc is NULL\n");
    if (desc->ncmd != 4)
	bu_exit(1, "ERROR: annot descriptor should have 4 cmds, got %d\n",
		desc->ncmd);
    bu_log("TEST 1 PASS: annot descriptor has %d commands\n", desc->ncmd);

    /* ================================================================
     * Test 2: ECMD_ANNOT_SET_TEXT
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ANNOT_SET_TEXT);
    bu_strlcpy(s->e_str[0], "World", RT_EDIT_MAXSTR_LEN);
    s->e_nstr = 1;
    s->e_inpara = 0;
    rt_edit_process(s);

    {
	struct txt_seg *tsg = (struct txt_seg *)aip->ant.segments[0];
	if (bu_strcmp(bu_vls_cstr(&tsg->label), "World") != 0)
	    bu_exit(1, "ERROR: set_text: got '%s' expected 'World'\n",
		    bu_vls_cstr(&tsg->label));
	bu_log("TEST 2 PASS: text = '%s'\n", bu_vls_cstr(&tsg->label));
    }

    /* ================================================================
     * Test 3: ECMD_ANNOT_SET_POS
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ANNOT_SET_POS);
    s->e_inpara = 3;
    VSET(s->e_para, 10, 20, 30);
    rt_edit_process(s);

    {
	vect_t expected = {10, 20, 30};
	if (!VNEAR_EQUAL(aip->V, expected, SMALL_FASTF))
	    bu_exit(1, "ERROR: set_pos: got (%g,%g,%g) expected (10,20,30)\n",
		    V3ARGS(aip->V));
	bu_log("TEST 3 PASS: V = (%g,%g,%g)\n", V3ARGS(aip->V));
    }

    /* ================================================================
     * Test 4: ECMD_ANNOT_SET_TSEG_TXT_SIZE
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ANNOT_SET_TSEG_TXT_SIZE);
    s->e_inpara = 1;
    s->e_para[0] = 24.0;
    rt_edit_process(s);

    {
	struct txt_seg *tsg = (struct txt_seg *)aip->ant.segments[0];
	if (!NEAR_EQUAL(tsg->txt_size, 24.0, SMALL_FASTF))
	    bu_exit(1, "ERROR: set_txt_size: got %g expected 24\n", tsg->txt_size);
	bu_log("TEST 4 PASS: txt_size = %g\n", tsg->txt_size);
    }

    /* ================================================================
     * Test 5: ECMD_ANNOT_VERT_MOVE (move vertex 0 by (+5, +3))
     * ================================================================*/
    point2d_t orig;
    V2MOVE(orig, aip->verts[0]);

    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ANNOT_VERT_MOVE);
    s->e_inpara = 3;
    s->e_para[0] = 0.0;   /* vertex index */
    s->e_para[1] = 5.0;   /* du */
    s->e_para[2] = 3.0;   /* dv */
    rt_edit_process(s);

    {
	point2d_t expected;
	point2d_t delta;
	V2SET(delta, 5, 3);
	V2ADD2(expected, orig, delta);
	if (!V2NEAR_EQUAL(aip->verts[0], expected, SMALL_FASTF))
	    bu_exit(1, "ERROR: vert_move: got (%g,%g) expected (%g,%g)\n",
		    aip->verts[0][0], aip->verts[0][1],
		    expected[0], expected[1]);
	bu_log("TEST 5 PASS: verts[0] = (%g,%g)\n",
	       aip->verts[0][0], aip->verts[0][1]);
    }

    /* ================================================================
     * Test 6: rt_edit_annot_get_params returns anchor position
     * ================================================================*/
    VSET(aip->V, 3, 4, 5);
    fastf_t vals[4] = {0};
    int nv = (*EDOBJ[dp->d_minor_type].ft_edit_get_params)(s, ECMD_ANNOT_SET_POS, vals);
    if (nv != 3 || !NEAR_EQUAL(vals[0], 3.0, SMALL_FASTF) ||
	!NEAR_EQUAL(vals[1], 4.0, SMALL_FASTF) ||
	!NEAR_EQUAL(vals[2], 5.0, SMALL_FASTF))
	bu_exit(1, "ERROR: get_params(SET_POS): nv=%d vals=(%g,%g,%g)\n",
		nv, vals[0], vals[1], vals[2]);
    bu_log("TEST 6 PASS: get_params(SET_POS) = (%g,%g,%g)\n",
	   vals[0], vals[1], vals[2]);

    bu_log("All ANNOT edit tests PASSED\n");

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
