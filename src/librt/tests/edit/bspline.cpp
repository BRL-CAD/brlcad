/*                      B S P L I N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file bspline.cpp
 *
 * Test BSPLINE (NURBS surface) editing via the ECMD suite.
 *
 * Test surface: bilinear (order 2) 3×3 control mesh, non-rational XYZ.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "nmg.h"
#include "nmg/nurb.h"
#include "wdb.h"
#include "rt/rt_ecmds.h"

/* ECMD constants (file-local in edbspline.c) */
#define ECMD_VTRANS          9017
#define ECMD_SPLINE_VPICK    9018
#define ECMD_BSPLINE_PICK_CP 9019
#define ECMD_BSPLINE_PICK_KNOT 9020
#define ECMD_BSPLINE_SET_KNOT  9021

/* The rt_bspline_edit struct (also file-local in edbspline.c) */
struct rt_bspline_edit {
    int spl_surfno;
    int spl_ui;
    int spl_vi;
    point_t v_pos;
    int knot_dir;
    int knot_idx;
};


/* Build a 3×3 bilinear (order=2) NURBS surface (order 2 = linear in each direction) */
static struct directory *
make_test_bspline(struct rt_wdb *wdbp)
{
    /* order 2 (linear), 3 control pts in each direction
     * knot vector for order=2, 3 pts: [0,0,1,1] for clamped ends → n+order knots
     * n=3 pts, order=2 → n_knots=3+2=5; but we need uniform: [0,0,1,2,2] */

    /* Use order 2 (linear) to keep it simple: 3 ctrl pts, 5 knots each dir */
    struct face_g_snurb *srf = nmg_nurb_new_snurb(
	    2, 2,  /* u_order, v_order */
	    5, 5,  /* n_u_knots, n_v_knots */
	    3, 3,  /* n_rows (v), n_cols (u) */
	    RT_NURB_MAKE_PT_TYPE(3, RT_NURB_PT_XYZ, 0) /* non-rational XYZ */
    );

    /* Knot vectors: uniform clamped [0,0,1,2,2] */
    srf->u.knots[0] = 0.0; srf->u.knots[1] = 0.0;
    srf->u.knots[2] = 1.0;
    srf->u.knots[3] = 2.0; srf->u.knots[4] = 2.0;

    srf->v.knots[0] = 0.0; srf->v.knots[1] = 0.0;
    srf->v.knots[2] = 1.0;
    srf->v.knots[3] = 2.0; srf->v.knots[4] = 2.0;

    /* 3×3 = 9 control points, laid out row-major (v outer, u inner) */
    fastf_t *cp = srf->ctl_points;
    /* row 0 */
    VSET(cp +  0,  0,  0, 0);
    VSET(cp +  3,  5,  0, 0);
    VSET(cp +  6, 10,  0, 0);
    /* row 1 */
    VSET(cp +  9,  0,  5, 0);
    VSET(cp + 12,  5,  5, 2);  /* elevated centre */
    VSET(cp + 15, 10,  5, 0);
    /* row 2 */
    VSET(cp + 18,  0, 10, 0);
    VSET(cp + 21,  5, 10, 0);
    VSET(cp + 24, 10, 10, 0);

    struct face_g_snurb *surfs[2];
    surfs[0] = srf;
    surfs[1] = NULL;

    /* Build rt_nurb_internal directly and export via wdb_export */
    struct rt_nurb_internal *ni;
    BU_ALLOC(ni, struct rt_nurb_internal);
    ni->magic = RT_NURB_INTERNAL_MAGIC;
    ni->nsrf = 1;
    ni->srfs = (struct face_g_snurb **)bu_malloc(sizeof(struct face_g_snurb *), "srfs");
    ni->srfs[0] = surfs[0];

    wdb_export(wdbp, "test_bspline", (void *)ni, ID_BSPLINE, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, "test_bspline", LOOKUP_QUIET);
    if (!dp)
	bu_exit(1, "ERROR: failed to look up test_bspline\n");
    return dp;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return BRLCAD_ERROR;

    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create in-memory database\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = make_test_bspline(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size = 100.0;
    v->gv_isize = 1.0 / v->gv_size;
    v->gv_scale = 50.0;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width  = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 0;
    s->local2base = 1.0;

    struct rt_bspline_edit *b = (struct rt_bspline_edit *)s->ipe_ptr;

    /* ================================================================
     * Initial state: prim_edit_create sets spl_surfno/ui/vi to the
     * middle control point of surface 0.
     * Surface has s_size[0]=3 (v), s_size[1]=3 (u), so middle = (1,1).
     * ================================================================*/
    if (!b)
	bu_exit(1, "ERROR: ipe_ptr not allocated\n");
    if (b->spl_surfno != 0 || b->spl_ui != 1 || b->spl_vi != 1)
	bu_exit(1, "ERROR: initial state wrong: surf=%d u=%d v=%d (expected 0,1,1)\n",
		b->spl_surfno, b->spl_ui, b->spl_vi);
    bu_log("BSPLINE initial state SUCCESS: surf=%d u=%d v=%d\n",
	   b->spl_surfno, b->spl_ui, b->spl_vi);

    /* ================================================================
     * ECMD_BSPLINE_PICK_CP  (select CP at surf=0, u=2, v=0)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BSPLINE_PICK_CP);
    s->e_inpara = 3;
    s->e_para[0] = 0.0;  /* surf 0 */
    s->e_para[1] = 2.0;  /* u = 2 */
    s->e_para[2] = 0.0;  /* v = 0 */

    rt_edit_process(s);
    if (b->spl_surfno != 0 || b->spl_ui != 2 || b->spl_vi != 0)
	bu_exit(1, "ERROR: ECMD_BSPLINE_PICK_CP: expected surf=0 u=2 v=0, got %d %d %d\n",
		b->spl_surfno, b->spl_ui, b->spl_vi);
    bu_log("ECMD_BSPLINE_PICK_CP SUCCESS: surf=%d u=%d v=%d\n",
	   b->spl_surfno, b->spl_ui, b->spl_vi);

    /* ================================================================
     * ECMD_BSPLINE_PICK_CP out-of-range should fail gracefully
     * (surface index out of range)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BSPLINE_PICK_CP);
    s->e_inpara = 3;
    s->e_para[0] = 5.0;  /* surf 5 - out of range */
    s->e_para[1] = 0.0;
    s->e_para[2] = 0.0;
    bu_vls_trunc(s->log_str, 0);

    rt_edit_process(s);
    /* b->spl_surfno should NOT have changed to 5 */
    if (b->spl_surfno == 5)
	bu_exit(1, "ERROR: ECMD_BSPLINE_PICK_CP incorrectly accepted out-of-range surf index\n");
    bu_log("ECMD_BSPLINE_PICK_CP out-of-range correctly refused\n");

    /* ================================================================
     * ECMD_VTRANS  (translate the currently selected CP to (10,0,5))
     * ================================================================*/

    /* Re-select middle CP (1,1) so we translate the elevated point */
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BSPLINE_PICK_CP);
    s->e_inpara = 3;
    s->e_para[0] = 0.0;
    s->e_para[1] = 1.0;
    s->e_para[2] = 1.0;
    rt_edit_process(s);
    if (b->spl_ui != 1 || b->spl_vi != 1)
	bu_exit(1, "ERROR: re-select centre CP failed\n");

    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_VTRANS);
    s->e_inpara = 1;          /* 1 = keyboard param valid */
    VSET(s->e_para, 5.0, 5.0, 9.0);  /* new position (local units) */

    rt_edit_process(s);

    {
	struct rt_nurb_internal *sip =
	    (struct rt_nurb_internal *)s->es_int.idb_ptr;
	struct face_g_snurb *surf = sip->srfs[b->spl_surfno];
	fastf_t *cp = &RT_NURB_GET_CONTROL_POINT(surf, b->spl_ui, b->spl_vi);
	vect_t expected = {5.0, 5.0, 9.0};
	if (!VNEAR_EQUAL(cp, expected, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_VTRANS: expected (5,5,9), got (%g,%g,%g)\n",
		    V3ARGS(cp));
	bu_log("ECMD_VTRANS SUCCESS: CP(%d,%d) moved to (%g,%g,%g)\n",
	       b->spl_ui, b->spl_vi, V3ARGS(cp));
    }

    /* ================================================================
     * ECMD_BSPLINE_PICK_KNOT: pick U knot at index 2 of surface 0
     *
     * The surface has u.k_size = 5, knots = [0,0,1,2,2].
     * Picking dir=0 (U), index=2 should store knot_dir=0, knot_idx=2
     * and the value at [2] is 1.0.
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BSPLINE_PICK_KNOT);
    s->e_inpara = 3;
    s->e_para[0] = 0.0;  /* surface 0 */
    s->e_para[1] = 0.0;  /* direction U */
    s->e_para[2] = 2.0;  /* knot index 2 */

    rt_edit_process(s);
    if (b->knot_dir != 0 || b->knot_idx != 2)
	bu_exit(1, "ERROR: ECMD_BSPLINE_PICK_KNOT: expected dir=0 idx=2, got %d/%d\n",
		b->knot_dir, b->knot_idx);
    {
	struct rt_nurb_internal *sip2 =
	    (struct rt_nurb_internal *)s->es_int.idb_ptr;
	struct face_g_snurb *surf2 = sip2->srfs[b->spl_surfno];
	fastf_t kval = surf2->u.knots[b->knot_idx];
	if (!NEAR_EQUAL(kval, 0.5, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_BSPLINE_PICK_KNOT: knot[2]=%g, expected 0.5 (normalized)\n",
		    kval);
	bu_log("ECMD_BSPLINE_PICK_KNOT SUCCESS: dir=%d idx=%d val=%g\n",
	       b->knot_dir, b->knot_idx, kval);
    }

    /* ================================================================
     * ECMD_BSPLINE_PICK_KNOT out-of-range: index >= k_size should fail
     * knot_dir and knot_idx must remain unchanged
     * ================================================================*/
    {
	int prev_dir = b->knot_dir;
	int prev_idx = b->knot_idx;
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BSPLINE_PICK_KNOT);
	s->e_inpara = 3;
	s->e_para[0] = 0.0;
	s->e_para[1] = 0.0;
	s->e_para[2] = 9999.0;  /* out of range */
	bu_vls_trunc(s->log_str, 0);
	rt_edit_process(s);
	if (b->knot_idx != prev_idx || b->knot_dir != prev_dir)
	    bu_exit(1, "ERROR: ECMD_BSPLINE_PICK_KNOT out-of-range changed selection\n");
	bu_log("ECMD_BSPLINE_PICK_KNOT out-of-range correctly refused\n");
    }

    /* ================================================================
     * ECMD_BSPLINE_SET_KNOT: change the selected knot (U[2]) from 1.0 to 1.5
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BSPLINE_SET_KNOT);
    s->e_inpara = 1;
    s->e_para[0] = 1.5;  /* new value */

    rt_edit_process(s);
    {
	struct rt_nurb_internal *sip3 =
	    (struct rt_nurb_internal *)s->es_int.idb_ptr;
	struct face_g_snurb *surf3 = sip3->srfs[b->spl_surfno];
	fastf_t kval = surf3->u.knots[b->knot_idx];
	if (!NEAR_EQUAL(kval, 1.5, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_BSPLINE_SET_KNOT: knot[2]=%g, expected 1.5\n",
		    kval);
	bu_log("ECMD_BSPLINE_SET_KNOT SUCCESS: U knot[%d] = %g\n",
	       b->knot_idx, kval);
    }

    /* ================================================================
     * ECMD_BSPLINE_PICK_KNOT + ECMD_BSPLINE_SET_KNOT for V direction
     * Pick V knot at index 3 (value 2.0) and change it to 3.0
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BSPLINE_PICK_KNOT);
    s->e_inpara = 3;
    s->e_para[0] = 0.0;  /* surface 0 */
    s->e_para[1] = 1.0;  /* direction V */
    s->e_para[2] = 3.0;  /* knot index 3 */

    rt_edit_process(s);
    if (b->knot_dir != 1 || b->knot_idx != 3)
	bu_exit(1, "ERROR: V knot pick: expected dir=1 idx=3, got %d/%d\n",
		b->knot_dir, b->knot_idx);

    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BSPLINE_SET_KNOT);
    s->e_inpara = 1;
    s->e_para[0] = 3.0;

    rt_edit_process(s);
    {
	struct rt_nurb_internal *sip4 =
	    (struct rt_nurb_internal *)s->es_int.idb_ptr;
	struct face_g_snurb *surf4 = sip4->srfs[b->spl_surfno];
	fastf_t kval = surf4->v.knots[b->knot_idx];
	if (!NEAR_EQUAL(kval, 3.0, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: V ECMD_BSPLINE_SET_KNOT: V knot[3]=%g, expected 3.0\n",
		    kval);
	bu_log("ECMD_BSPLINE_SET_KNOT SUCCESS: V knot[%d] = %g\n",
	       b->knot_idx, kval);
    }

    /* ================================================================
     * ECMD_SPLINE_VPICK: mouse-proximity control-point pick
     *
     * The test surface has 9 control points (3×3).  With an identity
     * model_changes and the default view (AET 45/35), point at mouse
     * position (0,0) should pick the control point closest to the
     * centre of the view.
     *
     * Strategy: first pre-select CP(2,0) by index so the selection is
     * at a known location.  Then fire a VPICK with mouse near that CP's
     * projected position and verify the selection changes to the closest
     * point (may or may not be (2,0) but must be a valid index).
     * ================================================================*/
    {
	/* Pre-select CP at surf=0, u=0, v=0 */
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_BSPLINE_PICK_CP);
	s->e_inpara = 3;
	s->e_para[0] = 0.0; s->e_para[1] = 0.0; s->e_para[2] = 0.0;
	rt_edit_process(s);
	int old_u = b->spl_ui, old_v = b->spl_vi;

	/* Fire VPICK at screen centre (0,0) via ft_edit_xy + ft_edit */
	MAT_IDN(s->model_changes);
	rt_edit_set_edflag(s, ECMD_SPLINE_VPICK);
	vect_t vp = {0.0, 0.0, 0.0};
	(*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, vp);
	rt_edit_process(s);

	/* After VPICK the selection must be a valid (in-range) index */
	struct rt_nurb_internal *sip5 =
	    (struct rt_nurb_internal *)s->es_int.idb_ptr;
	struct face_g_snurb *surf5 = sip5->srfs[b->spl_surfno];
	if (b->spl_ui < 0 || b->spl_ui >= surf5->s_size[1] ||
	    b->spl_vi < 0 || b->spl_vi >= surf5->s_size[0])
	    bu_exit(1, "ERROR: ECMD_SPLINE_VPICK: out-of-range selection u=%d v=%d\n",
		    b->spl_ui, b->spl_vi);
	bu_log("ECMD_SPLINE_VPICK SUCCESS: old=(%d,%d) new=(%d,%d)\n",
	       old_u, old_v, b->spl_ui, b->spl_vi);
    }

    /* ================================================================
     * RT_MATRIX_EDIT_ROT: matrix rotation should update model_changes
     * ================================================================*/
    MAT_IDN(s->model_changes);
    MAT_IDN(s->acc_rot_sol);
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 30, 0, 0);
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	mat_t ident;
	MAT_IDN(ident);
	if (bn_mat_is_equal(s->model_changes, ident, &tol))
	    bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT did not rotate model_changes\n");
	bu_log("RT_MATRIX_EDIT_ROT SUCCESS\n");
    }

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
