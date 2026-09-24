/*                      B S P L I N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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

static int check_bspline_operation_matrix(void);


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
rt_edit_test_bspline(void)
{
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
    s->e_inpara = 3;
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

    const fastf_t inch_to_mm = 25.4;
    s->local2base = inch_to_mm;
    s->base2local = 1.0 / inch_to_mm;
    s->mv_context = 0;
    MAT_IDN(s->e_invmat);
    b->spl_surfno = 0;
    b->spl_ui = 1;
    b->spl_vi = 1;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_VTRANS);
    s->e_inpara = 3;
    VSET(s->e_para, 0.5, 0.25, 0.1);
    struct rt_nurb_internal *sip =
	(struct rt_nurb_internal *)s->es_int.idb_ptr;
    struct face_g_snurb *surf = sip->srfs[0];
    fastf_t *cp = &RT_NURB_GET_CONTROL_POINT(surf, 1, 1);
    vect_t expected;
    vect_t entered;
    VMOVE(entered, s->e_para);
    VSET(expected, 0.5 * inch_to_mm, 0.25 * inch_to_mm, 0.1 * inch_to_mm);
    if (rt_edit_process(s) != BRLCAD_OK ||
	!VNEAR_EQUAL(cp, expected, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(s->e_para, entered, VUNITIZE_TOL))
	bu_exit(1, "ERROR: B-spline inch control-point move changed input or point\n");
    s->e_inpara = 3;
    if (rt_edit_process(s) != BRLCAD_OK || !VNEAR_EQUAL(cp, expected, VUNITIZE_TOL))
	bu_exit(1, "ERROR: repeated B-spline inch control-point move compounded\n");

    VSET(s->e_para, 9, 9, 9);
    VSET(s->e_mparam, 5, 6, 7);
    s->e_mvalid = 1;
    VSET(expected, 5, 6, 7);
    if (rt_edit_process(s) != BRLCAD_OK ||
	!VNEAR_EQUAL(cp, expected, VUNITIZE_TOL) ||
	!NEAR_EQUAL(s->e_para[X], 9, VUNITIZE_TOL))
	bu_exit(1, "ERROR: B-spline mouse point changed numeric input\n");

    rt_edit_destroy(s);
    db_free_full_path(&fp);
    bv_free(v);
    db_close(dbip);
    return check_bspline_operation_matrix();
}

enum {
    BSPLINE_CP_COUNT = 9,
    BSPLINE_KNOT_COUNT = 5
};

static const fastf_t BSPLINE_INITIAL_POINTS[BSPLINE_CP_COUNT * 3] = {
     0,  0, 0,   5,  0, 0,  10,  0, 0,
     0,  5, 0,   5,  5, 2,  10,  5, 0,
     0, 10, 0,   5, 10, 0,  10, 10, 0
};
static const fastf_t BSPLINE_INITIAL_KNOTS[BSPLINE_KNOT_COUNT] = {
    0, 0, 0.5, 1, 1
};

struct bspline_expected {
    fastf_t points[BSPLINE_CP_COUNT * 3];
    fastf_t u_knots[BSPLINE_KNOT_COUNT];
    fastf_t v_knots[BSPLINE_KNOT_COUNT];
    int surface;
    int u;
    int v;
    int knot_dir;
    int knot_idx;
};

static void
bspline_initial_state(struct bspline_expected *expected)
{
    memcpy(expected->points, BSPLINE_INITIAL_POINTS, sizeof(expected->points));
    memcpy(expected->u_knots, BSPLINE_INITIAL_KNOTS,
	sizeof(expected->u_knots));
    memcpy(expected->v_knots, BSPLINE_INITIAL_KNOTS,
	sizeof(expected->v_knots));
    expected->surface = 0;
    expected->u = 1;
    expected->v = 1;
    expected->knot_dir = 0;
    expected->knot_idx = 0;
}

static bool
bspline_same_state(const struct rt_edit *edit,
		   const struct bspline_expected *expected,
		   const char *name)
{
    const struct rt_nurb_internal *nurb =
	(const struct rt_nurb_internal *)edit->es_int.idb_ptr;
    const struct rt_bspline_edit *selection =
	(const struct rt_bspline_edit *)edit->ipe_ptr;
    if (!nurb || nurb->nsrf != 1 || !selection)
	return false;
    const struct face_g_snurb *surface = nurb->srfs[0];
    if (!surface || surface->s_size[0] != 3 || surface->s_size[1] != 3 ||
	surface->u.k_size != BSPLINE_KNOT_COUNT ||
	surface->v.k_size != BSPLINE_KNOT_COUNT)
	return false;
    for (int i = 0; i < BSPLINE_CP_COUNT * 3; ++i) {
	if (!NEAR_EQUAL(surface->ctl_points[i], expected->points[i],
		VUNITIZE_TOL)) {
	    bu_log("%s control value %d: expected %.17g, got %.17g\n",
		name, i, expected->points[i], surface->ctl_points[i]);
	    return false;
	}
    }
    for (int i = 0; i < BSPLINE_KNOT_COUNT; ++i) {
	if (!NEAR_EQUAL(surface->u.knots[i], expected->u_knots[i],
		VUNITIZE_TOL) ||
	    !NEAR_EQUAL(surface->v.knots[i], expected->v_knots[i],
		VUNITIZE_TOL)) {
	    bu_log("%s knot %d differs from expected\n", name, i);
	    return false;
	}
    }
    if (selection->spl_surfno != expected->surface ||
	selection->spl_ui != expected->u ||
	selection->spl_vi != expected->v ||
	selection->knot_dir != expected->knot_dir ||
	selection->knot_idx != expected->knot_idx) {
	bu_log("%s selection differs from expected\n", name);
	return false;
    }
    const fastf_t *keypoint = &expected->points[
	(expected->v * surface->s_size[1] + expected->u) * 3];
    point_t model_keypoint;
    MAT4X3PNT(model_keypoint, edit->e_mat, keypoint);
    if (!VNEAR_EQUAL(edit->e_keypoint, model_keypoint, VUNITIZE_TOL)) {
	bu_log("%s keypoint differs from selected control point\n", name);
	return false;
    }
    return true;
}

static int
bspline_run_case(struct db_full_path *path, struct db_i *dbip,
		 struct bn_tol *tol, struct bview *view,
		 const char *unit, const char *name, int command,
		 const fastf_t *values, int count,
		 int setup_command, const fastf_t *setup_values,
		 int setup_count, bool xy,
		 const struct bspline_expected *expected,
		 bool expect_error = false, bool path_transform = false)
{
    struct rt_edit *edit = rt_edit_create(path, dbip, tol, view);
    if (!edit)
	return 1;
    struct bspline_expected initial;
    bspline_initial_state(&initial);
    int failures = 0;
    if (!bspline_same_state(edit, &initial, name)) {
	++failures;
	goto done;
    }
    if (path_transform) {
	vect_t path_delta = {10, 0, 0};
	vect_t inverse_delta = {-10, 0, 0};
	MAT_DELTAS_VEC(edit->e_mat, path_delta);
	MAT_DELTAS_VEC(edit->e_invmat, inverse_delta);
	edit->mv_context = 1;
	rt_get_solid_keypoint(edit, &edit->e_keypoint,
	    &edit->e_keytag, edit->e_mat);
    }
    if (xy)
	VMOVE(edit->curr_e_axes_pos, edit->e_keypoint);
    if (setup_command) {
	rt_edit_set_edflag(edit, setup_command);
	edit->e_inpara = setup_count;
	for (int i = 0; i < setup_count; ++i)
	    edit->e_para[i] = setup_values[i];
	if (rt_edit_process(edit) != BRLCAD_OK) {
	    bu_log("BSpline %s %s setup failed: %s\n", unit, name,
		bu_vls_cstr(edit->log_str));
	    ++failures;
	    goto done;
	}
    }
    rt_edit_set_edflag(edit, command);
    int result;
    if (xy) {
	result = EDOBJ[ID_BSPLINE].ft_edit_xy(edit, values);
	if (result == BRLCAD_OK)
	    result = rt_edit_process(edit);
    } else {
	edit->e_inpara = count;
	for (int i = 0; i < count; ++i)
	    edit->e_para[i] = values[i];
	result = rt_edit_process(edit);
    }

    if ((result == BRLCAD_OK) == expect_error ||
	!bspline_same_state(edit, expected, name)) {
	bu_log("BSpline %s %s failed: %s\n", unit, name,
	    bu_vls_cstr(edit->log_str));
	++failures;
    }
    for (int i = 0; i < count; ++i) {
	if (!(isnan(values[i]) ? isnan(edit->e_para[i]) :
	    NEAR_EQUAL(edit->e_para[i], values[i], VUNITIZE_TOL))) {
	    bu_log("BSpline %s %s changed numeric input %d\n", unit, name, i);
	    ++failures;
	}
    }

done:
    rt_edit_destroy(edit);
    return failures;
}

static int
bspline_check_unit(fastf_t local2base, const char *unit)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	return 1;
    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return 1;
    }
    struct directory *dp = make_test_bspline(wdbp);
    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bview *view;
    BU_GET(view, struct bview);
    bv_init(view, NULL);
    view->gv_size = 100.0;
    view->gv_isize = 1.0 / view->gv_size;
    view->gv_scale = 0.5 * view->gv_size;
    bv_update(view);
    MAT_IDN(view->gv_model2view);
    MAT_IDN(view->gv_view2model);

    struct bspline_expected expected;
    int failures = 0;
    const fastf_t pick_cp[] = {0, 2, 0};
    bspline_initial_state(&expected);
    expected.u = 2;
    expected.v = 0;
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"pick control point", ECMD_BSPLINE_PICK_CP, pick_cp, 3,
	0, NULL, 0, false, &expected);

    const fastf_t move_cp[] = {0.5, 0.25, 0.1};
    bspline_initial_state(&expected);
    for (int i = 0; i < 3; ++i)
	expected.points[12 + i] = move_cp[i] * local2base;
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"move control point", ECMD_VTRANS, move_cp, 3,
	0, NULL, 0, false, &expected);

    const fastf_t pick_u_knot[] = {0, 0, 2};
    bspline_initial_state(&expected);
    expected.knot_idx = 2;
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"pick U knot", ECMD_BSPLINE_PICK_KNOT, pick_u_knot, 3,
	0, NULL, 0, false, &expected);

    const fastf_t pick_v_knot[] = {0, 1, 2};
    bspline_initial_state(&expected);
    expected.knot_dir = 1;
    expected.knot_idx = 2;
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"pick V knot", ECMD_BSPLINE_PICK_KNOT, pick_v_knot, 3,
	0, NULL, 0, false, &expected);

    const fastf_t set_u_knot[] = {0.75};
    bspline_initial_state(&expected);
    expected.knot_idx = 2;
    expected.u_knots[2] = set_u_knot[0];
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"set U knot", ECMD_BSPLINE_SET_KNOT, set_u_knot, 1,
	ECMD_BSPLINE_PICK_KNOT, pick_u_knot, 3, false, &expected);

    const fastf_t set_v_knot[] = {0.25};
    bspline_initial_state(&expected);
    expected.knot_dir = 1;
    expected.knot_idx = 2;
    expected.v_knots[2] = set_v_knot[0];
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"set V knot", ECMD_BSPLINE_SET_KNOT, set_v_knot, 1,
	ECMD_BSPLINE_PICK_KNOT, pick_v_knot, 3, false, &expected);

    const fastf_t pick_xy[] = {10, 0, 0};
    bspline_initial_state(&expected);
    expected.u = 2;
    expected.v = 0;
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"pick vertex XY", ECMD_SPLINE_VPICK, pick_xy, 0,
	0, NULL, 0, true, &expected);

    const fastf_t move_xy[] = {8, 9, 0};
    bspline_initial_state(&expected);
    expected.points[12] = 8;
    expected.points[13] = 9;
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"move control point XY", ECMD_VTRANS, move_xy, 0,
	0, NULL, 0, true, &expected);

    const fastf_t move_xy_instance[] = {18, 9, 0};
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"move control point XY in instance", ECMD_VTRANS,
	move_xy_instance, 0, 0, NULL, 0, true, &expected,
	false, true);

    const fastf_t short_cp[] = {0, 1};
    bspline_initial_state(&expected);
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"short control-point selection", ECMD_BSPLINE_PICK_CP,
	short_cp, 2, 0, NULL, 0, false, &expected, true);

    const fastf_t fractional_cp[] = {0, 1.5, 1};
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"fractional control-point index", ECMD_BSPLINE_PICK_CP,
	fractional_cp, 3, 0, NULL, 0, false, &expected, true);

    const fastf_t nan_cp[] = {0, NAN, 1};
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"nonfinite control-point index", ECMD_BSPLINE_PICK_CP,
	nan_cp, 3, 0, NULL, 0, false, &expected, true);

    const fastf_t short_move[] = {0.5, 0.25};
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"short control-point move", ECMD_VTRANS,
	short_move, 2, 0, NULL, 0, false, &expected, true);

    const fastf_t nan_move[] = {0.5, NAN, 0.1};
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"nonfinite control-point move", ECMD_VTRANS,
	nan_move, 3, 0, NULL, 0, false, &expected, true);

    const fastf_t short_knot[] = {0, 1};
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"short knot selection", ECMD_BSPLINE_PICK_KNOT,
	short_knot, 2, 0, NULL, 0, false, &expected, true);

    const fastf_t fractional_knot[] = {0, 0.5, 2};
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"fractional knot direction", ECMD_BSPLINE_PICK_KNOT,
	fractional_knot, 3, 0, NULL, 0, false, &expected, true);

    const fastf_t nan_knot[] = {0, 1, NAN};
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"nonfinite knot index", ECMD_BSPLINE_PICK_KNOT,
	nan_knot, 3, 0, NULL, 0, false, &expected, true);

    const fastf_t nan_knot_value[] = {NAN};
    expected.knot_idx = 2;
    failures += bspline_run_case(&path, dbip, &tol, view, unit,
	"nonfinite knot value", ECMD_BSPLINE_SET_KNOT,
	nan_knot_value, 1, ECMD_BSPLINE_PICK_KNOT,
	pick_u_knot, 3, false, &expected, true);

    bv_free(view);
    db_free_full_path(&path);
    db_close(dbip);
    bu_log("BSpline operation matrix %s: %s\n", unit,
	failures ? "fail" : "pass");
    return failures;
}

static int
check_bspline_operation_matrix(void)
{
    const struct rt_edit_prim_desc *desc = EDOBJ[ID_BSPLINE].ft_edit_desc();
    if (!desc || desc->ncmd != 5)
	return BRLCAD_ERROR;
    const int commands[] = {
	ECMD_SPLINE_VPICK, ECMD_BSPLINE_PICK_CP, ECMD_VTRANS,
	ECMD_BSPLINE_PICK_KNOT, ECMD_BSPLINE_SET_KNOT
    };
    const int parameter_counts[] = {0, 3, 1, 3, 1};
    int failures = 0;
    for (int i = 0; i < desc->ncmd; ++i) {
	if (desc->cmds[i].cmd_id != commands[i] ||
	    desc->cmds[i].nparam != parameter_counts[i]) {
	    bu_log("BSpline descriptor command %d is inconsistent\n", i);
	    ++failures;
	}
    }
    failures += bspline_check_unit(1.0, "mm");
    failures += bspline_check_unit(25.4, "in");
    return failures ? BRLCAD_ERROR : BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
