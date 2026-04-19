/*                       S K E T C H . C P P
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
/** @file sketch.cpp
 *
 * Test editing of SKETCH (2-D sketch) primitive parameters via
 * the new edsketch.c ECMD suite.
 *
 * Test sketch layout (4 vertices, 1 line segment):
 *
 *   verts[0] = (0, 0)
 *   verts[1] = (10, 0)
 *   verts[2] = (10, 10)
 *   verts[3] = (0, 10)
 *   segments: line 0→1
 *
 * V = (0,0,0), u_vec = (1,0,0), v_vec = (0,1,0)
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/sketch.h"
#include "rt/rt_ecmds.h"

/* ------------------------------------------------------------------ */
/* Build the test sketch                                               */
/* ------------------------------------------------------------------ */

static struct directory *
make_test_sketch(struct rt_wdb *wdbp)
{
    struct rt_sketch_internal *skt;
    BU_ALLOC(skt, struct rt_sketch_internal);
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(skt->V,     0, 0, 0);
    VSET(skt->u_vec, 1, 0, 0);
    VSET(skt->v_vec, 0, 1, 0);

    skt->vert_count = 4;
    skt->verts = (point2d_t *)bu_malloc(4 * sizeof(point2d_t), "sketch verts");
    V2SET(skt->verts[0],  0,  0);
    V2SET(skt->verts[1], 10,  0);
    V2SET(skt->verts[2], 10, 10);
    V2SET(skt->verts[3],  0, 10);

    /* One line segment: vert 0 → vert 1 */
    struct line_seg *ls;
    BU_ALLOC(ls, struct line_seg);
    ls->magic = CURVE_LSEG_MAGIC;
    ls->start = 0;
    ls->end   = 1;

    skt->curve.count   = 1;
    skt->curve.segment = (void **)bu_malloc(sizeof(void *), "skt segs");
    skt->curve.reverse = (int  *)bu_malloc(sizeof(int),     "skt reverse");
    skt->curve.segment[0] = (void *)ls;
    skt->curve.reverse[0] = 0;

    wdb_export(wdbp, "test_sketch", (void *)skt, ID_SKETCH, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, "test_sketch", LOOKUP_QUIET);
    if (!dp)
	bu_exit(1, "ERROR: failed to look up test_sketch\n");
    return dp;
}


/* ------------------------------------------------------------------ */
/* Main test                                                           */
/* ------------------------------------------------------------------ */

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
    struct directory *dp = make_test_sketch(wdbp);

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
    s->mv_context = 1;
    s->local2base = 1.0;

    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;

    /* ================================================================
     * Initial state: ipe_ptr initialized with curr_vert/curr_seg = -1
     * ================================================================*/
    if (!se)
	bu_exit(1, "ERROR: ipe_ptr not allocated\n");
    if (se->curr_vert != -1 || se->curr_seg != -1)
	bu_exit(1, "ERROR: initial state wrong (expected -1,-1)\n");
    bu_log("SKETCH initial state SUCCESS: curr_vert=%d curr_seg=%d\n",
	   se->curr_vert, se->curr_seg);

    /* ================================================================
     * ECMD_SKETCH_PICK_VERTEX  (select vertex 2)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_PICK_VERTEX);
    s->e_inpara = 1;
    s->e_para[0] = 2.0;

    rt_edit_process(s);
    if (se->curr_vert != 2)
	bu_exit(1, "ERROR: ECMD_SKETCH_PICK_VERTEX: expected curr_vert=2, got %d\n",
		se->curr_vert);
    bu_log("ECMD_SKETCH_PICK_VERTEX SUCCESS: curr_vert=%d\n", se->curr_vert);

    /* ================================================================
     * ECMD_SKETCH_MOVE_VERTEX  (move vertex 2 to (20,20))
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_MOVE_VERTEX);
    s->e_inpara = 2;
    s->e_para[0] = 20.0;
    s->e_para[1] = 20.0;

    rt_edit_process(s);
    if (!NEAR_EQUAL(skt->verts[2][0], 20.0, VUNITIZE_TOL) ||
	!NEAR_EQUAL(skt->verts[2][1], 20.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_SKETCH_MOVE_VERTEX: expected (20,20), got (%g,%g)\n",
		skt->verts[2][0], skt->verts[2][1]);
    bu_log("ECMD_SKETCH_MOVE_VERTEX SUCCESS: verts[2]=(%g,%g)\n",
	   skt->verts[2][0], skt->verts[2][1]);

    /* Restore verts[2] */
    V2SET(skt->verts[2], 10, 10);

    /* ================================================================
     * ECMD_SKETCH_PICK_SEGMENT  (select segment 0)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_PICK_SEGMENT);
    s->e_inpara = 1;
    s->e_para[0] = 0.0;

    rt_edit_process(s);
    if (se->curr_seg != 0)
	bu_exit(1, "ERROR: ECMD_SKETCH_PICK_SEGMENT: expected curr_seg=0, got %d\n",
		se->curr_seg);
    bu_log("ECMD_SKETCH_PICK_SEGMENT SUCCESS: curr_seg=%d\n", se->curr_seg);

    /* ================================================================
     * ECMD_SKETCH_MOVE_SEGMENT  (translate line seg 0 by (+5, +5))
     * Segment 0 is line_seg with start=0 (0,0) and end=1 (10,0).
     * After move: verts[0]=(5,5), verts[1]=(15,5).
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_MOVE_SEGMENT);
    s->e_inpara = 2;
    s->e_para[0] = 5.0;
    s->e_para[1] = 5.0;

    rt_edit_process(s);
    if (!NEAR_EQUAL(skt->verts[0][0], 5.0,  VUNITIZE_TOL) ||
	!NEAR_EQUAL(skt->verts[0][1], 5.0,  VUNITIZE_TOL) ||
	!NEAR_EQUAL(skt->verts[1][0], 15.0, VUNITIZE_TOL) ||
	!NEAR_EQUAL(skt->verts[1][1], 5.0,  VUNITIZE_TOL))
	bu_exit(1,
		"ERROR: ECMD_SKETCH_MOVE_SEGMENT: expected verts[0]=(5,5) verts[1]=(15,5), "
		"got (%g,%g) (%g,%g)\n",
		skt->verts[0][0], skt->verts[0][1],
		skt->verts[1][0], skt->verts[1][1]);
    bu_log("ECMD_SKETCH_MOVE_SEGMENT SUCCESS: verts[0]=(%g,%g) verts[1]=(%g,%g)\n",
	   skt->verts[0][0], skt->verts[0][1],
	   skt->verts[1][0], skt->verts[1][1]);

    /* Restore verts */
    V2SET(skt->verts[0],  0,  0);
    V2SET(skt->verts[1], 10,  0);

    /* ================================================================
     * ECMD_SKETCH_APPEND_LINE  (add line 2→3; curve should now have 2 segs)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_LINE);
    s->e_inpara = 2;
    s->e_para[0] = 2.0;
    s->e_para[1] = 3.0;

    rt_edit_process(s);
    if (skt->curve.count != 2)
	bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_LINE: expected 2 segments, got %zu\n",
		skt->curve.count);
    {
	struct line_seg *ls = (struct line_seg *)skt->curve.segment[1];
	if (!ls || ls->magic != CURVE_LSEG_MAGIC || ls->start != 2 || ls->end != 3)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_LINE: new segment wrong\n");
    }
    bu_log("ECMD_SKETCH_APPEND_LINE SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* ================================================================
     * ECMD_SKETCH_APPEND_ARC  (add arc from vert 1 to vert 2, r=8,
     *                          center_is_left=1, ccw=0)
     * e_inpara=5: [0]=start [1]=end [2]=radius [3]=center_is_left [4]=orientation
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_ARC);
    s->e_inpara = 5;
    s->e_para[0] = 1.0;  /* start vert */
    s->e_para[1] = 2.0;  /* end vert */
    s->e_para[2] = 8.0;  /* radius */
    s->e_para[3] = 1.0;  /* center_is_left = 1 */
    s->e_para[4] = 0.0;  /* orientation: ccw */

    rt_edit_process(s);
    if (skt->curve.count != 3)
	bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_ARC: expected 3 segments, got %zu\n",
		skt->curve.count);
    {
	struct carc_seg *cs = (struct carc_seg *)skt->curve.segment[2];
	if (!cs || cs->magic != CURVE_CARC_MAGIC || cs->start != 1 || cs->end != 2 ||
	    !NEAR_EQUAL(cs->radius, 8.0, VUNITIZE_TOL) ||
	    cs->center_is_left != 1 || cs->orientation != 0)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_ARC: new segment wrong\n");
    }
    bu_log("ECMD_SKETCH_APPEND_ARC SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* ================================================================
     * ECMD_SKETCH_APPEND_BEZIER  (quadratic Bezier: verts 0, 1, 2)
     * e_inpara=3 → degree=2; e_para[0..2] are control point indices
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_BEZIER);
    s->e_inpara = 3;
    s->e_para[0] = 0.0;  /* cp 0 */
    s->e_para[1] = 1.0;  /* cp 1 */
    s->e_para[2] = 2.0;  /* cp 2 */

    rt_edit_process(s);
    if (skt->curve.count != 4)
	bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_BEZIER (quad): expected 4 segments, got %zu\n",
		skt->curve.count);
    {
	struct bezier_seg *bs = (struct bezier_seg *)skt->curve.segment[3];
	if (!bs || bs->magic != CURVE_BEZIER_MAGIC || bs->degree != 2 ||
	    bs->ctl_points[0] != 0 || bs->ctl_points[1] != 1 || bs->ctl_points[2] != 2)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_BEZIER (quad): new segment wrong\n");
    }
    bu_log("ECMD_SKETCH_APPEND_BEZIER (degree=2) SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* ================================================================
     * ECMD_SKETCH_APPEND_BEZIER  (cubic Bezier: degree=3, verts 0,1,2,3)
     * e_inpara=4 → degree=3; e_para[0..3] are control point indices
     * This test validates that RT_EDIT_MAXPARA > 3 is properly used.
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_BEZIER);
    s->e_inpara = 4;
    s->e_para[0] = 0.0;  /* cp 0 */
    s->e_para[1] = 1.0;  /* cp 1 */
    s->e_para[2] = 2.0;  /* cp 2 */
    s->e_para[3] = 3.0;  /* cp 3 */

    rt_edit_process(s);
    if (skt->curve.count != 5)
	bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_BEZIER (cubic): expected 5 segments, got %zu\n",
		skt->curve.count);
    {
	struct bezier_seg *bs = (struct bezier_seg *)skt->curve.segment[4];
	if (!bs || bs->magic != CURVE_BEZIER_MAGIC || bs->degree != 3 ||
	    bs->ctl_points[0] != 0 || bs->ctl_points[1] != 1 ||
	    bs->ctl_points[2] != 2 || bs->ctl_points[3] != 3)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_BEZIER (cubic): new segment wrong\n");
    }
    bu_log("ECMD_SKETCH_APPEND_BEZIER (degree=3) SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* ================================================================
     * ECMD_SKETCH_DELETE_VERTEX (should fail: vertex 0 is in use)
     * Verify by checking that vert_count is unchanged after the attempt.
     * ================================================================*/
    se->curr_vert = 0;
    {
	size_t vc_before = skt->vert_count;
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_DELETE_VERTEX);
	rt_edit_process(s);
	if (skt->vert_count != vc_before)
	    bu_exit(1, "ERROR: ECMD_SKETCH_DELETE_VERTEX incorrectly deleted in-use vertex\n");
    }
    bu_log("ECMD_SKETCH_DELETE_VERTEX (in-use vertex) correctly refused\n");
    se->curr_vert = -1;

    /* ================================================================
     * ECMD_SKETCH_DELETE_SEGMENT (delete the last segment, index 4)
     * After: curve.count should be 4
     * ================================================================*/
    se->curr_seg = 4;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_DELETE_SEGMENT);
    s->e_inpara = 0;

    rt_edit_process(s);
    if (skt->curve.count != 4)
	bu_exit(1, "ERROR: ECMD_SKETCH_DELETE_SEGMENT: expected 4 segments, got %zu\n",
		skt->curve.count);
    if (se->curr_seg != -1)
	bu_exit(1, "ERROR: ECMD_SKETCH_DELETE_SEGMENT: curr_seg not reset to -1\n");
    bu_log("ECMD_SKETCH_DELETE_SEGMENT SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE: uniform scale about keypoint
     * ================================================================*/
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    rt_edit_process(s);
    /* V should have been scaled - just verify it ran without crash */
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS\n");

    /* ================================================================
     * RT_MATRIX_EDIT_ROT: matrix rotation
     * ================================================================*/
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_ROT);
    MAT_IDN(s->model_changes);
    MAT_IDN(s->acc_rot_sol);
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

    /* ================================================================
     * RT_MATRIX_EDIT_TRANS_MODEL_XYZ: absolute translation
     * ================================================================*/
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_TRANS_MODEL_XYZ);
    MAT_IDN(s->model_changes);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VSET(s->e_keypoint, 0, 0, 0);
    s->local2base = 1.0;

    rt_edit_process(s);
    {
	point_t kp_world;
	MAT4X3PNT(kp_world, s->model_changes, s->e_keypoint);
	vect_t expected = {10, 20, 30};
	if (!VNEAR_EQUAL(kp_world, expected, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_MATRIX_EDIT_TRANS_MODEL_XYZ failed\n");
	bu_log("RT_MATRIX_EDIT_TRANS_MODEL_XYZ SUCCESS: keypoint maps to (%g,%g,%g)\n",
	       V3ARGS(kp_world));
    }

    /* ================================================================
     * ECMD_SKETCH_MOVE_VERTEX_LIST: move vertices 0 and 2 by delta (5, -3)
     * Before: v0=(0,0), v1=(10,0), v2=(10,10), v3=(0,10)
     * After:  v0=(5,-3), v1=(10,0), v2=(15,7), v3=(0,10)
     * ================================================================*/
    {
	/* Re-read skt after previous edit operations may have modified it */
	struct rt_sketch_internal *skt2 =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;
	/* Reset vertex 0 and 2 to known values first */
	V2SET(skt2->verts[0],  0,  0);
	V2SET(skt2->verts[2], 10, 10);

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_MOVE_VERTEX_LIST);
	s->e_inpara = 4;
	s->e_para[0] = 5.0;   /* U delta */
	s->e_para[1] = -3.0;  /* V delta */
	s->e_para[2] = 0.0;   /* vertex index 0 */
	s->e_para[3] = 2.0;   /* vertex index 2 */
	s->local2base = 1.0;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);

	point2d_t exp0 = { 5, -3};
	point2d_t exp1 = {10,  0};  /* unchanged */
	point2d_t exp2 = {15,  7};
	point2d_t exp3 = { 0, 10};  /* unchanged */

	if (!V2NEAR_EQUAL(skt2->verts[0], exp0, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt2->verts[1], exp1, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt2->verts[2], exp2, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt2->verts[3], exp3, VUNITIZE_TOL))
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_MOVE_VERTEX_LIST: "
		"v0=(%g,%g) v1=(%g,%g) v2=(%g,%g) v3=(%g,%g); "
		"expected (5,-3) (10,0) (15,7) (0,10)\n",
		skt2->verts[0][0], skt2->verts[0][1],
		skt2->verts[1][0], skt2->verts[1][1],
		skt2->verts[2][0], skt2->verts[2][1],
		skt2->verts[3][0], skt2->verts[3][1]);
	bu_log("ECMD_SKETCH_MOVE_VERTEX_LIST SUCCESS: "
	       "v0=(%g,%g) v2=(%g,%g)\n",
	       skt2->verts[0][0], skt2->verts[0][1],
	       skt2->verts[2][0], skt2->verts[2][1]);
    }

    /* ================================================================
     * ECMD_SKETCH_SPLIT_SEGMENT: line segment split at t=0.5
     *
     * Start state after previous tests: segment[0] is line 0→1
     * (verts[0] and verts[1] were restored to (0,0) and (10,0)).
     *
     * After split at t=0.5:
     *   - new vertex midpoint at (5, 0) becomes verts[vc] where vc
     *     was skt->vert_count before the split
     *   - segment[0] becomes line 0 → new_vi
     *   - new  segment[1] becomes line new_vi → 1
     *   - curve.count increases by 1
     * ================================================================*/
    {
	struct rt_sketch_internal *skt3 =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;

	/* Reset the first segment to line 0→1 and restore verts */
	{
	    struct line_seg *ls = (struct line_seg *)skt3->curve.segment[0];
	    ls->start = 0;
	    ls->end   = 1;
	}
	V2SET(skt3->verts[0],  0,  0);
	V2SET(skt3->verts[1], 10,  0);

	size_t vc_before = skt3->vert_count;
	size_t sc_before = skt3->curve.count;

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_SPLIT_SEGMENT);
	s->e_inpara  = 2;
	s->e_para[0] = 0.0;  /* segment index */
	s->e_para[1] = 0.5;  /* t = midpoint */
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): %s\n",
		    bu_vls_cstr(s->log_str));

	/* One new vertex should have been added */
	if (skt3->vert_count != vc_before + 1)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		"vert_count should be %zu, got %zu\n",
		vc_before + 1, skt3->vert_count);

	/* One new segment should have been inserted */
	if (skt3->curve.count != sc_before + 1)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		"curve.count should be %zu, got %zu\n",
		sc_before + 1, skt3->curve.count);

	/* The split vertex (last added) should be at (5, 0) */
	point2d_t exp_mid = { 5.0, 0.0 };
	if (!V2NEAR_EQUAL(skt3->verts[vc_before], exp_mid, VUNITIZE_TOL))
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		"split vertex should be (5,0), got (%g,%g)\n",
		skt3->verts[vc_before][0], skt3->verts[vc_before][1]);

	/* Segment[0] should now end at the split vertex */
	{
	    struct line_seg *ls0 = (struct line_seg *)skt3->curve.segment[0];
	    if (ls0->magic != CURVE_LSEG_MAGIC || ls0->start != 0 ||
		ls0->end   != (int)vc_before)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		    "first half wrong (start=%d end=%d)\n",
		    ls0->start, ls0->end);
	}
	/* Segment[1] should run from split vertex to original end */
	{
	    struct line_seg *ls1 = (struct line_seg *)skt3->curve.segment[1];
	    if (ls1->magic != CURVE_LSEG_MAGIC || ls1->start != (int)vc_before ||
		ls1->end   != 1)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		    "second half wrong (start=%d end=%d)\n",
		    ls1->start, ls1->end);
	}
	bu_log("ECMD_SKETCH_SPLIT_SEGMENT (line, t=0.5) SUCCESS: "
	       "new vertex %zu at (%g,%g), curve.count=%zu\n",
	       vc_before,
	       skt3->verts[vc_before][0], skt3->verts[vc_before][1],
	       skt3->curve.count);
    }

    /* ================================================================
     * ECMD_SKETCH_SPLIT_SEGMENT: Bezier segment split at t=0.5
     *
     * Build a quadratic Bezier (degree=2) with control points:
     *   P0=(0,0) P1=(5,10) P2=(10,0)
     * which are existing verts 0, (new) and 1.
     *
     * First, add a dedicated control-point vertex (5,10):
     * Append it as a new vert, then append the bezier.
     * Then split it at t=0.5.
     *
     * de Casteljau at t=0.5:
     *   Q10 = (1-0.5)*P0 + 0.5*P1 = (2.5, 5)
     *   Q11 = (1-0.5)*P1 + 0.5*P2 = (7.5, 5)
     *   Q20 = (1-0.5)*Q10+ 0.5*Q11= (5,   5)  ← split vertex
     * Left:  P0, Q10, Q20  = (0,0) (2.5,5) (5,5)
     * Right: Q20, Q11, P2  = (5,5) (7.5,5) (10,0)
     * ================================================================*/
    {
	struct rt_sketch_internal *skt4 =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;

	/* Add control-point vertex (5,10) */
	size_t cp_vi = skt4->vert_count;
	skt4->verts = (point2d_t *)bu_realloc(skt4->verts,
		(skt4->vert_count + 1) * sizeof(point2d_t), "verts");
	V2SET(skt4->verts[cp_vi],  5, 10);
	skt4->vert_count++;

	/* Append quadratic Bezier: verts 0, cp_vi, 1 */
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_BEZIER);
	s->e_inpara  = 3;
	s->e_para[0] = 0.0;           /* P0 */
	s->e_para[1] = (fastf_t)cp_vi; /* P1 */
	s->e_para[2] = 1.0;            /* P2 */
	rt_edit_process(s);

	size_t bez_seg_idx = skt4->curve.count - 1; /* just-appended bezier */
	size_t vc_before   = skt4->vert_count;
	size_t sc_before   = skt4->curve.count;

	/* Split the bezier at t=0.5 */
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_SPLIT_SEGMENT);
	s->e_inpara  = 2;
	s->e_para[0] = (fastf_t)bez_seg_idx;
	s->e_para[1] = 0.5;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): %s\n",
		    bu_vls_cstr(s->log_str));

	/* For degree=2, de Casteljau adds 3 new vertices (L[1],L[2]=split,R[1]) */
	if (skt4->vert_count != vc_before + 3)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): "
		"expected vert_count=%zu, got %zu\n",
		vc_before + 3, skt4->vert_count);
	if (skt4->curve.count != sc_before + 1)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): "
		"expected curve.count=%zu, got %zu\n",
		sc_before + 1, skt4->curve.count);

	/* Q20 (split vertex) is L[2] = verts[vc_before+1] */
	int split_vi = vc_before + 1; /* L[d] = Q[d][0] when k reaches d */
	/* Actually L[1]=Q[1][0], L[2]=Q[2][0]=split: vc_before, vc_before+1 */
	/* verts[vc_before]   = Q10 = (2.5, 5) */
	/* verts[vc_before+1] = Q20 = (5,   5) = split vertex */
	/* verts[vc_before+2] = Q11 = (7.5, 5) (right interior) */
	point2d_t exp_q10 = {2.5, 5.0};
	point2d_t exp_q20 = {5.0, 5.0};
	point2d_t exp_q11 = {7.5, 5.0};

	if (!V2NEAR_EQUAL(skt4->verts[vc_before],     exp_q10, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt4->verts[vc_before + 1], exp_q20, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt4->verts[vc_before + 2], exp_q11, VUNITIZE_TOL))
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): "
		"wrong de Casteljau points: "
		"got (%g,%g) (%g,%g) (%g,%g)\n",
		skt4->verts[vc_before][0],     skt4->verts[vc_before][1],
		skt4->verts[vc_before + 1][0], skt4->verts[vc_before + 1][1],
		skt4->verts[vc_before + 2][0], skt4->verts[vc_before + 2][1]);

	/* Left bezier: P0, Q10, Q20 */
	{
	    struct bezier_seg *bsL =
		(struct bezier_seg *)skt4->curve.segment[bez_seg_idx];
	    if (bsL->magic != CURVE_BEZIER_MAGIC || bsL->degree != 2 ||
		bsL->ctl_points[0] != 0 ||
		bsL->ctl_points[1] != (int)vc_before ||
		bsL->ctl_points[2] != (int)(vc_before + 1))
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): left half wrong\n");
	}
	/* Right bezier: Q20, Q11, P2  (P2 = vert[1]) */
	{
	    struct bezier_seg *bsR =
		(struct bezier_seg *)skt4->curve.segment[bez_seg_idx + 1];
	    if (bsR->magic != CURVE_BEZIER_MAGIC || bsR->degree != 2 ||
		bsR->ctl_points[0] != split_vi ||
		bsR->ctl_points[1] != (int)(vc_before + 2) ||
		bsR->ctl_points[2] != 1)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): right half wrong\n");
	}
	bu_log("ECMD_SKETCH_SPLIT_SEGMENT (bezier degree=2, t=0.5) SUCCESS: "
	       "Q10=(%g,%g) split=(%g,%g) Q11=(%g,%g)\n",
	       skt4->verts[vc_before][0],     skt4->verts[vc_before][1],
	       skt4->verts[vc_before + 1][0], skt4->verts[vc_before + 1][1],
	       skt4->verts[vc_before + 2][0], skt4->verts[vc_before + 2][1]);
    }

    /* ================================================================
     * ECMD_SKETCH_PICK_VERTEX via mouse proximity (ft_edit_xy path)
     *
     * Reset model_changes to identity so the projection used internally
     * (model2objview = gv_model2view * model_changes) equals gv_model2view.
     * Then aim the cursor at the view-space projection of vertex 0's 3D
     * position and verify that vertex 0 is selected.
     * ================================================================*/
    {
	/* Use identity model_changes for a clean test */
	MAT_IDN(s->model_changes);

	/* Reset curr_vert */
	se->curr_vert = -1;

	struct rt_sketch_internal *skt5 =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;

	/* Project vertex 0 to view space via model2objview (= gv_model2view now) */
	point_t v0_3d;
	VJOIN2(v0_3d, skt5->V,
	       skt5->verts[0][0], skt5->u_vec,
	       skt5->verts[0][1], skt5->v_vec);
	mat_t m2ov;
	bn_mat_mul(m2ov, v->gv_model2view, s->model_changes);
	point_t v0_view;
	MAT4X3PNT(v0_view, m2ov, v0_3d);

	/* Aim cursor directly at vertex 0's view position */
	vect_t cursor;
	VSET(cursor, v0_view[X], v0_view[Y], 0.0);

	rt_edit_set_edflag(s, ECMD_SKETCH_PICK_VERTEX);
	(*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, cursor);
	rt_edit_process(s);

	if (se->curr_vert < 0)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_PICK_VERTEX (proximity): "
		"curr_vert not set\n");
	if ((size_t)se->curr_vert >= skt5->vert_count)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_PICK_VERTEX (proximity): "
		"curr_vert=%d out of range\n", se->curr_vert);
	/* Vertex 0 is at (0,0) which maps to the same view position as the
	 * cursor, so it must be the closest vertex. */
	if (se->curr_vert != 0)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_PICK_VERTEX (proximity): "
		"expected vertex 0 (at view origin), got vertex %d\n",
		se->curr_vert);

	bu_log("ECMD_SKETCH_PICK_VERTEX (proximity) SUCCESS: "
	       "cursor at view (%g,%g) → nearest vertex %d\n",
	       cursor[X], cursor[Y], se->curr_vert);
    }

    /* ================================================================
     * ECMD_SKETCH_APPEND_NURB: append a quadratic (order=3) NURB with
     * 4 control points (c_size=4, must be >= order=3).
     *
     * Using existing vertices 0..3: (0,0) (10,0) (10,10) (0,10).
     * After the call:
     *   - curve.count increases by 1
     *   - New segment is CURVE_NURB_MAGIC, order=3, c_size=4, rational=0
     *   - k_size = 4 + 3 = 7
     *   - Auto clamped-uniform knot vector:
     *       [0, 0, 0, 1, 2, 2, 2]  (3 repeated zeros, interior at 1,
     *        2 = c_size-order+1 repeated end value)
     * ================================================================*/
    {
	struct rt_sketch_internal *skt_n =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;

	size_t sc_before = skt_n->curve.count;

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_NURB);
	/* e_para[0] = order=3; e_para[1..4] = vert indices 0,1,2,3 */
	s->e_inpara  = 5;
	s->e_para[0] = 3.0;  /* order */
	s->e_para[1] = 0.0;  /* ctrl pt 0 */
	s->e_para[2] = 1.0;  /* ctrl pt 1 */
	s->e_para[3] = 2.0;  /* ctrl pt 2 */
	s->e_para[4] = 3.0;  /* ctrl pt 3 */
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: %s\n",
		    bu_vls_cstr(s->log_str));

	if (skt_n->curve.count != sc_before + 1)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_APPEND_NURB: expected curve.count=%zu, got %zu\n",
		sc_before + 1, skt_n->curve.count);

	size_t nurb_idx = sc_before;  /* index of newly appended segment */
	void *nseg = skt_n->curve.segment[nurb_idx];
	if (!nseg || *(uint32_t *)nseg != CURVE_NURB_MAGIC)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: wrong segment type\n");

	struct nurb_seg *ns = (struct nurb_seg *)nseg;
	if (ns->order != 3 || ns->c_size != 4)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_APPEND_NURB: order=%d c_size=%d (expected 3,4)\n",
		ns->order, ns->c_size);
	if (ns->weights != NULL)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: weights should be NULL\n");
	if (RT_NURB_IS_PT_RATIONAL(ns->pt_type))
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: pt_type should be non-rational\n");

	/* Expected k_size = 3 + 4 = 7 */
	if (ns->k.k_size != 7)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_APPEND_NURB: k_size=%d (expected 7)\n",
		ns->k.k_size);

	/* Expected clamped uniform knot vector: [0,0,0,1,2,2,2] */
	fastf_t expected_kv[7] = {0.0, 0.0, 0.0, 1.0, 2.0, 2.0, 2.0};
	int kv_ok = 1;
	for (int i = 0; i < 7; i++) {
	    if (fabs(ns->k.knots[i] - expected_kv[i]) > VUNITIZE_TOL)
		{ kv_ok = 0; break; }
	}
	if (!kv_ok) {
	    bu_log("ERROR: ECMD_SKETCH_APPEND_NURB: knot vector = [");
	    for (int i = 0; i < 7; i++) bu_log("%g ", ns->k.knots[i]);
	    bu_log("], expected [0,0,0,1,2,2,2]\n");
	    bu_exit(1, "ECMD_SKETCH_APPEND_NURB: wrong knot vector\n");
	}
	/* Control points should be 0,1,2,3 */
	if (ns->ctl_points[0] != 0 || ns->ctl_points[1] != 1 ||
	    ns->ctl_points[2] != 2 || ns->ctl_points[3] != 3)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: wrong control points\n");

	bu_log("ECMD_SKETCH_APPEND_NURB (order=3, c_size=4) SUCCESS: "
	       "kv=[0,0,0,1,2,2,2] curve.count=%zu\n", skt_n->curve.count);

	/* ============================================================
	 * ECMD_SKETCH_NURB_EDIT_KV: replace the knot vector of the
	 * NURB segment we just created.
	 *
	 * Replace [0,0,0,1,2,2,2] with [0,0,0,0.5,2,2,2] — shifting
	 * the single interior knot from 1 to 0.5.
	 * ============================================================*/

	/* Select the NURB segment */
	se->curr_seg = (int)nurb_idx;

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_NURB_EDIT_KV);
	/* e_para[0] = k_size=7; e_para[1..7] = new knot values */
	s->e_inpara  = 8;
	s->e_para[0] = 7.0;   /* k_size */
	s->e_para[1] = 0.0;
	s->e_para[2] = 0.0;
	s->e_para[3] = 0.0;
	s->e_para[4] = 0.5;   /* moved interior knot */
	s->e_para[5] = 2.0;
	s->e_para[6] = 2.0;
	s->e_para[7] = 2.0;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_NURB_EDIT_KV: %s\n",
		    bu_vls_cstr(s->log_str));

	fastf_t expected_kv2[7] = {0.0, 0.0, 0.0, 0.5, 2.0, 2.0, 2.0};
	int kv2_ok = 1;
	for (int i = 0; i < 7; i++) {
	    if (fabs(ns->k.knots[i] - expected_kv2[i]) > VUNITIZE_TOL)
		{ kv2_ok = 0; break; }
	}
	if (!kv2_ok) {
	    bu_log("ERROR: ECMD_SKETCH_NURB_EDIT_KV: knot vector = [");
	    for (int i = 0; i < 7; i++) bu_log("%g ", ns->k.knots[i]);
	    bu_log("], expected [0,0,0,0.5,2,2,2]\n");
	    bu_exit(1, "ECMD_SKETCH_NURB_EDIT_KV: wrong knot vector\n");
	}
	bu_log("ECMD_SKETCH_NURB_EDIT_KV SUCCESS: kv=[0,0,0,0.5,2,2,2]\n");

	/* ============================================================
	 * ECMD_SKETCH_NURB_EDIT_WEIGHTS: make the segment rational by
	 * setting 4 weights = {1, 2, 2, 1}.
	 *
	 * After: pt_type should be rational, weights array filled.
	 * ============================================================*/
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_NURB_EDIT_WEIGHTS);
	/* e_para[0]=seg_index; e_para[1]=c_size=4; e_para[2..5]=weights */
	s->e_inpara  = 6;
	s->e_para[0] = (fastf_t)nurb_idx;  /* segment index */
	s->e_para[1] = 4.0;                /* c_size */
	s->e_para[2] = 1.0;
	s->e_para[3] = 2.0;
	s->e_para[4] = 2.0;
	s->e_para[5] = 1.0;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: %s\n",
		    bu_vls_cstr(s->log_str));

	if (!RT_NURB_IS_PT_RATIONAL(ns->pt_type))
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: pt_type not rational\n");
	if (!ns->weights)
	    bu_exit(1, "ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: weights array is NULL\n");

	fastf_t expected_w[4] = {1.0, 2.0, 2.0, 1.0};
	for (int i = 0; i < 4; i++) {
	    if (fabs(ns->weights[i] - expected_w[i]) > VUNITIZE_TOL)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: weight[%d]=%g "
		    "(expected %g)\n", i, ns->weights[i], expected_w[i]);
	}
	bu_log("ECMD_SKETCH_NURB_EDIT_WEIGHTS SUCCESS: "
	       "weights=[1,2,2,1] rational=%d\n",
	       RT_NURB_IS_PT_RATIONAL(ns->pt_type));

	/* ============================================================
	 * ECMD_SKETCH_DELETE_SEGMENT: delete the NURB segment and verify
	 * that NURB-specific memory (knots, ctl_points, weights) is freed
	 * without a crash (valgrind would catch leaks in CI).
	 * ============================================================*/
	se->curr_seg = (int)nurb_idx;
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_DELETE_SEGMENT);
	s->e_inpara = 0;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_DELETE_SEGMENT (nurb): %s\n",
		    bu_vls_cstr(s->log_str));

	if (skt_n->curve.count != sc_before)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_DELETE_SEGMENT (nurb): "
		"expected count=%zu, got %zu\n",
		sc_before, skt_n->curve.count);
	bu_log("ECMD_SKETCH_DELETE_SEGMENT (nurb) SUCCESS: "
	       "curve.count back to %zu\n", skt_n->curve.count);

	/* ============================================================
	 * Error-path checks for ECMD_SKETCH_APPEND_NURB:
	 *   (a) c_size < order → curve.count must NOT increase
	 *   (b) out-of-range control point index → curve.count must NOT increase
	 * ============================================================*/
	{
	    size_t sc_err = skt_n->curve.count;

	    /* (a) order=4 but only 2 control points (c_size=2 < order=4) */
	    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_NURB);
	    s->e_inpara  = 3;
	    s->e_para[0] = 4.0;  /* order */
	    s->e_para[1] = 0.0;
	    s->e_para[2] = 1.0;
	    rt_edit_process(s);
	    if (skt_n->curve.count != sc_err)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_APPEND_NURB should reject c_size < order "
		    "(count changed from %zu to %zu)\n",
		    sc_err, skt_n->curve.count);
	    bu_log("ECMD_SKETCH_APPEND_NURB (c_size<order) correctly rejected\n");

	    /* (b) order=2, 3 control points but last index is bogus */
	    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_NURB);
	    s->e_inpara  = 4;
	    s->e_para[0] = 2.0;     /* order */
	    s->e_para[1] = 0.0;
	    s->e_para[2] = 1.0;
	    s->e_para[3] = 9999.0;  /* bogus vertex index */
	    rt_edit_process(s);
	    if (skt_n->curve.count != sc_err)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_APPEND_NURB should reject out-of-range vert\n");
	    bu_log("ECMD_SKETCH_APPEND_NURB (bad vert index) correctly rejected\n");
	}
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
