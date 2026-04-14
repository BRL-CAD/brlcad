/*                    M E T A B A L L . C P P
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
/** @file metaball.cpp
 *
 * Test editing of METABALL primitive parameters.
 *
 * Reference METABALL: method=0 (METABALL_METABALL), threshold=0.5
 *   pt1 = (1, 0, 0), fldstr=1.0, sweat=0.2
 *   pt2 = (-1, 0, 0), fldstr=2.0, sweat=0.3
 *
 * Expected values derived from MGED edsol.c (MENU_METABALL_*) and
 * analytical tracing.  rt_metaball_mat is alias-safe (copies coord
 * to a temporary before MAT4X3PNT).
 *
 * rt_metaball_mat: applies MAT4X3PNT to each control point coord, and
 * scales fldstr by 1/mat[15].  For scale=s: mat[15]=1/s, so
 * fldstr → fldstr * s.  For rotation: mat[15]=1, fldstr unchanged.
 *
 * Keypoint when no point selected: (0,0,0)  (rt_edit_metaball_keypoint
 * returns VSETALL(0) when es_metaball_pnt is NULL).
 *
 * struct rt_metaball_edit (from edmetaball.c) — mirrored locally so
 * that the test can set es_metaball_pnt for point-level operations.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"


/* Mirror the private struct from edmetaball.c so we can set the
 * currently-selected metaball point from test code. */
struct rt_metaball_edit_local {
    struct wdb_metaball_pnt *es_metaball_pnt;
};

struct directory *
make_metaball(struct rt_wdb *wdbp)
{
    const char *objname = "metaball";

    /* Build rt_metaball_internal by hand to avoid needing libwdb */
    struct rt_metaball_internal *mb;
    BU_ALLOC(mb, struct rt_metaball_internal);
    mb->magic     = RT_METABALL_INTERNAL_MAGIC;
    mb->threshold = 0.5;
    mb->method    = METABALL_METABALL;
    mb->initstep  = 0.0;
    mb->finalstep = 0.0;
    BU_LIST_INIT(&mb->metaball_ctrl_head);

    /* pt1: (1, 0, 0), fldstr=1.0, sweat=0.2 */
    struct wdb_metaball_pnt *p1;
    BU_GET(p1, struct wdb_metaball_pnt);
    p1->l.magic = WDB_METABALLPT_MAGIC;
    VSET(p1->coord,  1, 0, 0);
    p1->fldstr = 1.0;
    p1->sweat  = 0.2;
    BU_LIST_INSERT(&mb->metaball_ctrl_head, &p1->l);

    /* pt2: (-1, 0, 0), fldstr=2.0, sweat=0.3 */
    struct wdb_metaball_pnt *p2;
    BU_GET(p2, struct wdb_metaball_pnt);
    p2->l.magic = WDB_METABALLPT_MAGIC;
    VSET(p2->coord, -1, 0, 0);
    p2->fldstr = 2.0;
    p2->sweat  = 0.3;
    BU_LIST_INSERT(&mb->metaball_ctrl_head, &p2->l);

    /* wdb_export owns mb and frees it via rt_metaball_ifree after export */
    wdb_export(wdbp, objname, (void *)mb, ID_METABALL, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create metaball object\n");

    return dp;
}

/* Get the first/second control point of the metaball in the edit state */
static struct wdb_metaball_pnt *
mb_first_pt(struct rt_edit *s)
{
    struct rt_metaball_internal *mb =
	(struct rt_metaball_internal *)s->es_int.idb_ptr;
    return BU_LIST_FIRST(wdb_metaball_pnt, &mb->metaball_ctrl_head);
}

static struct wdb_metaball_pnt *
mb_second_pt(struct rt_edit *s)
{
    struct wdb_metaball_pnt *first = mb_first_pt(s);
    return BU_LIST_NEXT(wdb_metaball_pnt, &first->l);
}

/* Count control points */
static int
mb_count(struct rt_edit *s)
{
    struct rt_metaball_internal *mb =
	(struct rt_metaball_internal *)s->es_int.idb_ptr;
    int n = 0;
    struct wdb_metaball_pnt *pt;
    for (BU_LIST_FOR(pt, wdb_metaball_pnt, &mb->metaball_ctrl_head))
	n++;
    return n;
}

static void
mb_reset(struct rt_edit *s, struct rt_metaball_internal *edit_mb)
{
    /* Remove all existing points and re-add the two reference points */
    while (BU_LIST_NON_EMPTY(&edit_mb->metaball_ctrl_head)) {
	struct wdb_metaball_pnt *pt =
	    BU_LIST_FIRST(wdb_metaball_pnt, &edit_mb->metaball_ctrl_head);
	BU_LIST_DQ(&pt->l);
	BU_PUT(pt, struct wdb_metaball_pnt);   /* matches BU_GET in rt_metaball_import5 */
    }

    /* Add pt1 */
    struct wdb_metaball_pnt *n1;
    BU_GET(n1, struct wdb_metaball_pnt);
    n1->l.magic = WDB_METABALLPT_MAGIC;
    VSET(n1->coord,  1, 0, 0);
    n1->fldstr = 1.0;
    n1->sweat  = 0.2;
    BU_LIST_INSERT(&edit_mb->metaball_ctrl_head, &n1->l);

    /* Add pt2 */
    struct wdb_metaball_pnt *n2;
    BU_GET(n2, struct wdb_metaball_pnt);
    n2->l.magic = WDB_METABALLPT_MAGIC;
    VSET(n2->coord, -1, 0, 0);
    n2->fldstr = 2.0;
    n2->sweat  = 0.3;
    BU_LIST_INSERT(&edit_mb->metaball_ctrl_head, &n2->l);

    /* Reset threshold/method */
    edit_mb->threshold = 0.5;
    edit_mb->method    = METABALL_METABALL;

    /* Clear selection and reset keypoint to origin */
    struct rt_metaball_edit_local *m =
	(struct rt_metaball_edit_local *)s->ipe_ptr;
    m->es_metaball_pnt = NULL;
    VSETALL(s->e_keypoint, 0.0);

    MAT_IDN(s->acc_rot_sol);
    MAT_IDN(s->incr_change);
    s->acc_sc_sol = 1.0;
    s->e_inpara   = 0;
    s->es_scale   = 0.0;
    s->mv_context = 1;
}

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return BRLCAD_ERROR;

    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create database instance\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);

    struct directory *dp = make_metaball(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size  = 73.3197;
    v->gv_isize = 1.0 / v->gv_size;
    v->gv_scale = 0.5 * v->gv_size;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width  = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;

    struct rt_metaball_internal *edit_mb =
	(struct rt_metaball_internal *)s->es_int.idb_ptr;
    struct rt_metaball_edit_local *m =
	(struct rt_metaball_edit_local *)s->ipe_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_METABALL_SET_THRESHOLD
     *
     * e_inpara=1, e_para[0]=0.7 (> 0): threshold → 0.7
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_METABALL_SET_THRESHOLD);
    s->e_inpara = 1;
    s->e_para[0] = 0.7;

    rt_edit_process(s);
    if (!NEAR_EQUAL(edit_mb->threshold, 0.7, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_METABALL_SET_THRESHOLD failed: got %g\n",
		edit_mb->threshold);
    bu_log("ECMD_METABALL_SET_THRESHOLD SUCCESS: threshold=%g\n",
	   edit_mb->threshold);

    /* ================================================================
     * ECMD_METABALL_SET_METHOD
     *
     * e_inpara=1, e_para[0]=1 → method → 1 (METABALL_ISOPOTENTIAL)
     * ================================================================*/
    mb_reset(s, edit_mb);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_METABALL_SET_METHOD);
    s->e_inpara = 1;
    s->e_para[0] = 1.0;

    rt_edit_process(s);
    if (edit_mb->method != 1)
	bu_exit(1, "ERROR: ECMD_METABALL_SET_METHOD failed: got %d\n",
		edit_mb->method);
    bu_log("ECMD_METABALL_SET_METHOD SUCCESS: method=%d\n", edit_mb->method);

    /* ================================================================
     * ECMD_METABALL_PT_MOV  (move selected point by delta)
     *
     * Select pt1 (1,0,0) BEFORE calling set_edit_mode — set_edit_mode
     * rejects ECMD_METABALL_PT_MOV if es_metaball_pnt is NULL.
     * Move by e_para=(2,3,4).
     * pt1.coord → (1+2, 0+3, 0+4) = (3,3,4)
     * ================================================================*/
    mb_reset(s, edit_mb);
    /* Pre-select first point before set_edit_mode checks it */
    m->es_metaball_pnt = mb_first_pt(s);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_METABALL_PT_MOV);
    s->e_inpara = 3;
    VSET(s->e_para, 2, 3, 4);

    rt_edit_process(s);
    {
	struct wdb_metaball_pnt *pt1 = mb_first_pt(s);
	point_t exp = {3, 3, 4};
	if (!VNEAR_EQUAL(pt1->coord, exp, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_METABALL_PT_MOV failed: got %g,%g,%g\n",
		    V3ARGS(pt1->coord));
	bu_log("ECMD_METABALL_PT_MOV SUCCESS: pt1.coord=%g,%g,%g\n",
	       V3ARGS(pt1->coord));
    }

    /* ================================================================
     * ECMD_METABALL_PT_ADD  (add a new point at e_para coords)
     *
     * Start with 2 points.  Add at (5,5,5) → 3 points.
     * The new point becomes the selected point.
     * ================================================================*/
    mb_reset(s, edit_mb);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_METABALL_PT_ADD);
    s->e_inpara = 3;
    VSET(s->e_para, 5, 5, 5);

    rt_edit_process(s);
    {
	int n = mb_count(s);
	if (n != 3)
	    bu_exit(1, "ERROR: ECMD_METABALL_PT_ADD failed: count=%d\n", n);
	bu_log("ECMD_METABALL_PT_ADD SUCCESS: count=%d\n", n);
    }

    /* ================================================================
     * ECMD_METABALL_PT_DEL  (delete selected point)
     *
     * Pre-select pt1, then set_edit_mode and delete.
     * → only 1 point remains.
     * ================================================================*/
    mb_reset(s, edit_mb);
    m->es_metaball_pnt = mb_first_pt(s);   /* pre-select before set_edit_mode */
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_METABALL_PT_DEL);

    rt_edit_process(s);
    {
	int n = mb_count(s);
	if (n != 1)
	    bu_exit(1, "ERROR: ECMD_METABALL_PT_DEL failed: count=%d\n", n);
	bu_log("ECMD_METABALL_PT_DEL SUCCESS: count=%d\n", n);
    }

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale=2 about keypoint (0,0,0))
     *
     * rt_metaball_mat with scale=2 (mat[15]=1/2):
     *   coord → coord * 2  (MAT4X3PNT divides by mat[15])
     *   fldstr → fldstr / mat[15] = fldstr * 2
     * pt1: (1,0,0)→(2,0,0), fldstr 1.0→2.0
     * pt2: (-1,0,0)→(-2,0,0), fldstr 2.0→4.0
     * ================================================================*/
    mb_reset(s, edit_mb);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    rt_edit_process(s);
    {
	struct wdb_metaball_pnt *pt1 = mb_first_pt(s);
	struct wdb_metaball_pnt *pt2 = mb_second_pt(s);
	point_t exp1 = {2, 0, 0}, exp2 = {-2, 0, 0};
	if (!VNEAR_EQUAL(pt1->coord, exp1, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(pt1->fldstr, 2.0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(pt2->coord, exp2, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(pt2->fldstr, 4.0, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n"
		    "  pt1: coord=%g,%g,%g fldstr=%g\n"
		    "  pt2: coord=%g,%g,%g fldstr=%g\n",
		    V3ARGS(pt1->coord), pt1->fldstr,
		    V3ARGS(pt2->coord), pt2->fldstr);
	bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: pt1=%g,%g,%g fldstr=%g  pt2=%g,%g,%g fldstr=%g\n",
	       V3ARGS(pt1->coord), pt1->fldstr,
	       V3ARGS(pt2->coord), pt2->fldstr);
    }

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; keypoint (0,0,0) → e_para)
     *
     * With mv_context=1, e_invmat=IDN, keypoint=(0,0,0):
     *   delta = keypoint - e_para = (0,0,0)-(10,20,30) = (-10,-20,-30)
     *   MAT_DELTAS_VEC_NEG → translation = (10,20,30)
     *   rt_metaball_mat MAT4X3PNT: coord += (10,20,30)
     * pt1: (1,0,0) → (11,20,30)
     * pt2: (-1,0,0) → (9,20,30)
     * fldstr unchanged (mat[15]=1 for translation matrix)
     * ================================================================*/
    mb_reset(s, edit_mb);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	struct wdb_metaball_pnt *pt1 = mb_first_pt(s);
	struct wdb_metaball_pnt *pt2 = mb_second_pt(s);
	point_t exp1 = {11, 20, 30}, exp2 = {9, 20, 30};
	if (!VNEAR_EQUAL(pt1->coord, exp1, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(pt2->coord, exp2, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n"
		    "  pt1: coord=%g,%g,%g\n"
		    "  pt2: coord=%g,%g,%g\n",
		    V3ARGS(pt1->coord), V3ARGS(pt2->coord));
	bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: pt1=%g,%g,%g  pt2=%g,%g,%g\n",
	       V3ARGS(pt1->coord), V3ARGS(pt2->coord));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint (0,0,0), angles=5,5,5)
     *
     * R = bn_mat_angles(5,5,5).  mat[15]=1 for pure rotation.
     * pt1 (1,0,0) → R*(1,0,0) = R[:,0]
     *   = (0.99240387650610407, 0.09439130678413448, -0.07889757346864876)
     * pt2 (-1,0,0) → -R[:,0]
     *   = (-0.99240387650610407, -0.09439130678413448, 0.07889757346864876)
     * fldstr unchanged.
     * ================================================================*/
    mb_reset(s, edit_mb);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	struct wdb_metaball_pnt *pt1 = mb_first_pt(s);
	struct wdb_metaball_pnt *pt2 = mb_second_pt(s);
	point_t exp1 = { 0.99240387650610407,  0.09439130678413448, -0.07889757346864876};
	point_t exp2 = {-0.99240387650610407, -0.09439130678413448,  0.07889757346864876};
	if (!VNEAR_EQUAL(pt1->coord, exp1, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(pt2->coord, exp2, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(pt1->fldstr, 1.0, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(pt2->fldstr, 2.0, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT failed\n"
		    "  pt1: coord=%0.17f,%0.17f,%0.17f fldstr=%g\n"
		    "  pt2: coord=%0.17f,%0.17f,%0.17f fldstr=%g\n",
		    V3ARGS(pt1->coord), pt1->fldstr,
		    V3ARGS(pt2->coord), pt2->fldstr);
	bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: pt1=%g,%g,%g  pt2=%g,%g,%g\n",
	       V3ARGS(pt1->coord), V3ARGS(pt2->coord));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS XY: verify points moved
     * ================================================================*/
    mb_reset(s, edit_mb);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VSET(s->curr_e_axes_pos, 0, 0, 0);   /* keypoint in model space */

    {
	int xpos = 1482, ypos = 762;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) failed: %s\n",
		bu_vls_cstr(s->log_str));

    rt_edit_process(s);
    {
	struct wdb_metaball_pnt *pt1 = mb_first_pt(s);
	point_t orig = {1, 0, 0};
	if (VNEAR_EQUAL(pt1->coord, orig, SQRT_SMALL_FASTF))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move pt1\n");
	bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: pt1=%g,%g,%g (moved from 1,0,0)\n",
	       V3ARGS(pt1->coord));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    mb_reset(s, edit_mb);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    mousevec[X] = 0.1; mousevec[Y] = -0.05; mousevec[Z] = 0;
    bu_vls_trunc(s->log_str, 0);
    int rot_xy_ret = (*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec);
    if (rot_xy_ret != BRLCAD_OK)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(xy) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(xy) SUCCESS: rotation applied via knob path\n");


    /* ================================================================
     * RT_MATRIX_EDIT_ROT: absolute matrix rotation from keyboard angles
     *
     * Set model_changes to a 30-deg rotation about X. Keypoint is at
     * the primitive's vertex (varies per primitive).
     * After the edit model_changes must differ from identity.
     * ================================================================*/
    mb_reset(s, edit_mb);
    MAT_IDN(s->model_changes);
    MAT_IDN(s->acc_rot_sol);
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 30, 0, 0);   /* 30-deg rotation about X axis */
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
mat_t ident;
MAT_IDN(ident);
if (bn_mat_is_equal(s->model_changes, ident, &tol))
    bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT did not rotate model_changes\n");
mat_t expected_rot;
MAT_IDN(expected_rot);
bn_mat_angles(expected_rot, 30, 0, 0);
if (!bn_mat_is_equal(s->acc_rot_sol, expected_rot, &tol))
    bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT: acc_rot_sol not updated\n");
bu_log("RT_MATRIX_EDIT_ROT SUCCESS: model_changes rotated, acc_rot_sol updated\n");
    }

    /* ================================================================
     * RT_MATRIX_EDIT_TRANS_MODEL_XYZ: absolute model-space translation
     *
     * Start with identity model_changes, keypoint at origin.
     * After translating to (10,20,30): model_changes * (0,0,0) == (10,20,30).
     * ================================================================*/
    mb_reset(s, edit_mb);
    MAT_IDN(s->model_changes);
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_TRANS_MODEL_XYZ);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VSET(s->e_keypoint, 0, 0, 0);
    s->local2base = 1.0;

    {
point_t kp_saved;
VMOVE(kp_saved, s->e_keypoint);

rt_edit_process(s);

point_t kp_world;
MAT4X3PNT(kp_world, s->model_changes, kp_saved);
vect_t expected = {10, 20, 30};
if (!VNEAR_EQUAL(kp_world, expected, VUNITIZE_TOL))
    bu_exit(1, "ERROR: RT_MATRIX_EDIT_TRANS_MODEL_XYZ failed: "
    "keypoint maps to (%g,%g,%g), expected (10,20,30)\n",
    V3ARGS(kp_world));
bu_log("RT_MATRIX_EDIT_TRANS_MODEL_XYZ SUCCESS: "
       "keypoint maps to (%g,%g,%g)\n", V3ARGS(kp_world));
    }

    /* ================================================================
     * ECMD_METABALL_PT_SWEAT  (set sweat to absolute value)
     *
     * Pre-select pt1.  Set sweat to 0.75.
     * pt1.sweat → 0.75 (regardless of prior value)
     * ================================================================*/
    mb_reset(s, edit_mb);
    m->es_metaball_pnt = mb_first_pt(s);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_METABALL_PT_SWEAT);
    s->e_inpara = 1;
    s->e_para[0] = 0.75;

    rt_edit_process(s);
    {
	struct wdb_metaball_pnt *pt1 = mb_first_pt(s);
	if (!NEAR_EQUAL(pt1->sweat, 0.75, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_METABALL_PT_SWEAT failed: sweat=%g\n",
		    pt1->sweat);
	bu_log("ECMD_METABALL_PT_SWEAT SUCCESS: sweat=%g\n", pt1->sweat);
    }

    /* ECMD_METABALL_PT_SWEAT error: no point selected.
     * The error is caught in ft_set_edit_mode (before rt_edit_process).
     * Check the log right after ft_set_edit_mode. */
    mb_reset(s, edit_mb);
    /* m->es_metaball_pnt is NULL after mb_reset */
    bu_vls_trunc(s->log_str, 0);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_METABALL_PT_SWEAT);
    if (bu_vls_strlen(s->log_str) == 0)
	bu_exit(1, "ERROR: ECMD_METABALL_PT_SWEAT with no point: expected error in log from set_edit_mode\n");
    bu_log("ECMD_METABALL_PT_SWEAT no-point correctly refused\n");

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
