/*                          A R S . C P P
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
/** @file ars.cpp
 *
 * Test editing of ARS primitive parameters.
 *
 * Reference ARS: 2 curves, 2 pts_per_curve
 *   curve 0: (0,0,0), (1,0,0)  [+ duplicate of first]
 *   curve 1: (0,0,1), (1,0,1)  [+ duplicate of first]
 *
 * Keypoint: first point of first curve = (0,0,0) when no vertex
 * selected (es_ars_crv = -1).
 *
 * rt_ars_mat is alias-safe (copies each point via tmp_pnt before
 * MAT4X3PNT, then writes back to the same array).
 *
 * Scale(s=2) about (0,0,0): all points multiply by s.
 * Translate by (5,5,5): all points shift by (5,5,5).
 * ECMD_ARS_MOVE_PT (e_inpara=3, mv_context=1): moves selected vertex
 *   to the absolute coordinate given in e_para (after e_invmat apply).
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


/* ECMD constants for new ARS operations (file-local in edars.c) */
#define ECMD_ARS_SCALE_CRV	5048
#define ECMD_ARS_SCALE_COL	5049
#define ECMD_ARS_INSERT_CRV	5050

/* Internal representation of the ARS edit state (mirrors rt_ars_edit in
 * edars.c).  Used to pre-select a vertex without exposing private struct. */
struct rt_ars_edit_local {
    int es_ars_crv;
    int es_ars_col;
    point_t es_pt;
};

/* Convenience macro to access a specific curve/point in the ARS */
#define ARS_PT(ars, crv, col)  (&(ars)->curves[crv][(col)*ELEMENTS_PER_POINT])

struct directory *
make_ars(struct rt_wdb *wdbp)
{
    const char *objname = "ars";
    struct rt_ars_internal *ars;
    BU_ALLOC(ars, struct rt_ars_internal);
    ars->magic = RT_ARS_INTERNAL_MAGIC;
    ars->ncurves       = 2;
    ars->pts_per_curve = 2;

    /* allocate ncurves+1 pointers (last NULL) */
    ars->curves = (fastf_t **)bu_calloc(ars->ncurves + 1,
					sizeof(fastf_t *), "ars curves");

    /* curve 0: (0,0,0), (1,0,0) */
    ars->curves[0] = (fastf_t *)bu_calloc(
	(ars->pts_per_curve + 1) * ELEMENTS_PER_POINT,
	sizeof(fastf_t), "ars crv0");
    VSET(ARS_PT(ars, 0, 0), 0, 0, 0);
    VSET(ARS_PT(ars, 0, 1), 1, 0, 0);
    VMOVE(ARS_PT(ars, 0, 2), ARS_PT(ars, 0, 0));  /* duplicate first */

    /* curve 1: (0,0,1), (1,0,1) */
    ars->curves[1] = (fastf_t *)bu_calloc(
	(ars->pts_per_curve + 1) * ELEMENTS_PER_POINT,
	sizeof(fastf_t), "ars crv1");
    VSET(ARS_PT(ars, 1, 0), 0, 0, 1);
    VSET(ARS_PT(ars, 1, 1), 1, 0, 1);
    VMOVE(ARS_PT(ars, 1, 2), ARS_PT(ars, 1, 0));  /* duplicate first */

    /* wdb_export owns ars and frees it via rt_ars_ifree after export */
    wdb_export(wdbp, objname, (void *)ars, ID_ARS, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create ars object\n");

    return dp;
}

static void
ars_reset(struct rt_edit *s, struct rt_ars_internal *edit_ars,
	  struct rt_ars_edit_local *ae)
{
    /* Restore reference values */
    VSET(ARS_PT(edit_ars, 0, 0), 0, 0, 0);
    VSET(ARS_PT(edit_ars, 0, 1), 1, 0, 0);
    VMOVE(ARS_PT(edit_ars, 0, 2), ARS_PT(edit_ars, 0, 0));
    VSET(ARS_PT(edit_ars, 1, 0), 0, 0, 1);
    VSET(ARS_PT(edit_ars, 1, 1), 1, 0, 1);
    VMOVE(ARS_PT(edit_ars, 1, 2), ARS_PT(edit_ars, 1, 0));

    ae->es_ars_crv = -1;
    ae->es_ars_col = -1;
    VSETALL(ae->es_pt, 0.0);

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

    struct directory *dp = make_ars(wdbp);

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

    struct rt_ars_internal *edit_ars =
	(struct rt_ars_internal *)s->es_int.idb_ptr;
    struct rt_ars_edit_local *ae =
	(struct rt_ars_edit_local *)s->ipe_ptr;

    vect_t mousevec;

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE (scale=2 about keypoint (0,0,0))
     *
     * Keypoint = first point = (0,0,0).  All points multiply by 2:
     *   curve0 pt0: (0,0,0)→(0,0,0), pt1: (1,0,0)→(2,0,0)
     *   curve1 pt0: (0,0,1)→(0,0,2), pt1: (1,0,1)→(2,0,2)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    rt_edit_process(s);
    {
	point_t exp00 = {0, 0, 0}, exp01 = {2, 0, 0};
	point_t exp10 = {0, 0, 2}, exp11 = {2, 0, 2};
	if (!VNEAR_EQUAL(ARS_PT(edit_ars,0,0), exp00, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,0,1), exp01, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,1,0), exp10, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,1,1), exp11, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n"
		    "  c0p0=%g,%g,%g c0p1=%g,%g,%g\n"
		    "  c1p0=%g,%g,%g c1p1=%g,%g,%g\n",
		    V3ARGS(ARS_PT(edit_ars,0,0)), V3ARGS(ARS_PT(edit_ars,0,1)),
		    V3ARGS(ARS_PT(edit_ars,1,0)), V3ARGS(ARS_PT(edit_ars,1,1)));
	bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: c0p0=%g,%g,%g c0p1=%g,%g,%g\n",
	       V3ARGS(ARS_PT(edit_ars,0,0)), V3ARGS(ARS_PT(edit_ars,0,1)));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS (translate; keypoint (0,0,0) → e_para)
     *
     * Translation = e_para - keypoint = (5,5,5) - (0,0,0) = (5,5,5)
     * All points shift by (5,5,5):
     *   curve0: (0,0,0)→(5,5,5), (1,0,0)→(6,5,5)
     *   curve1: (0,0,1)→(5,5,6), (1,0,1)→(6,5,6)
     * ================================================================*/
    ars_reset(s, edit_ars, ae);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	point_t exp00 = {5, 5, 5}, exp01 = {6, 5, 5};
	point_t exp10 = {5, 5, 6}, exp11 = {6, 5, 6};
	if (!VNEAR_EQUAL(ARS_PT(edit_ars,0,0), exp00, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,0,1), exp01, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,1,0), exp10, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,1,1), exp11, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n"
		    "  c0p0=%g,%g,%g c0p1=%g,%g,%g\n"
		    "  c1p0=%g,%g,%g c1p1=%g,%g,%g\n",
		    V3ARGS(ARS_PT(edit_ars,0,0)), V3ARGS(ARS_PT(edit_ars,0,1)),
		    V3ARGS(ARS_PT(edit_ars,1,0)), V3ARGS(ARS_PT(edit_ars,1,1)));
	bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: c0p0=%g,%g,%g c0p1=%g,%g,%g\n",
	       V3ARGS(ARS_PT(edit_ars,0,0)), V3ARGS(ARS_PT(edit_ars,0,1)));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_ROT (rotate about keypoint (0,0,0), angles 5,5,5)
     *
     * R = bn_mat_angles(5,5,5).  Keypoint (0,0,0).
     * v     (0,0,0) → R*(0,0,0) = (0,0,0)
     * (1,0,0) → R[:,0] = (0.99240387650610407, 0.09439130678413448, -0.07889757346864876)
     * (0,0,1) → R[:,2] = (0.08715574274765817, -0.08682408883346517, 0.99240387650610407)
     * (1,0,1) → R[:,0]+R[:,2] = (1.07956.., 0.00757.., 0.91350..)
     * ================================================================*/
    ars_reset(s, edit_ars, ae);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	/* curve0 pt0: (0,0,0) → (0,0,0) */
	point_t exp00 = {0, 0, 0};
	/* curve0 pt1: (1,0,0) → R[:,0] */
	point_t exp01 = {0.99240387650610407, 0.09439130678413448, -0.07889757346864876};
	/* curve1 pt0: (0,0,1) → R[:,2] */
	point_t exp10 = {0.08715574274765817, -0.08682408883346517, 0.99240387650610407};
	if (!VNEAR_EQUAL(ARS_PT(edit_ars,0,0), exp00, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,0,1), exp01, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,1,0), exp10, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n"
		    "  c0p0=%0.10f,%0.10f,%0.10f (exp 0,0,0)\n"
		    "  c0p1=%0.10f,%0.10f,%0.10f\n"
		    "  c1p0=%0.10f,%0.10f,%0.10f\n",
		    V3ARGS(ARS_PT(edit_ars,0,0)),
		    V3ARGS(ARS_PT(edit_ars,0,1)),
		    V3ARGS(ARS_PT(edit_ars,1,0)));
	bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: c0p0=%g,%g,%g c0p1=%g,%g,%g\n",
	       V3ARGS(ARS_PT(edit_ars,0,0)), V3ARGS(ARS_PT(edit_ars,0,1)));
    }

    /* ================================================================
     * ECMD_ARS_MOVE_PT  (move selected vertex to absolute coord)
     *
     * Pre-select curve=0, col=1 (point (1,0,0)).
     * e_inpara=3, e_para=(7,8,9), mv_context=1.
     * new_pt = MAT4X3PNT(e_invmat=(IDN), e_para=(7,8,9)) = (7,8,9)
     * ars->curves[0][col*3] = (7,8,9)
     * Only this specific vertex changes; others remain.
     * ================================================================*/
    ars_reset(s, edit_ars, ae);
    ae->es_ars_crv = 0;
    ae->es_ars_col = 1;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ARS_MOVE_PT);
    s->e_inpara = 3;
    VSET(s->e_para, 7, 8, 9);

    rt_edit_process(s);
    {
	point_t exp01 = {7, 8, 9};
	point_t exp00 = {0, 0, 0};  /* unchanged */
	point_t exp10 = {0, 0, 1};  /* unchanged */
	if (!VNEAR_EQUAL(ARS_PT(edit_ars,0,1), exp01, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,0,0), exp00, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,1,0), exp10, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_ARS_MOVE_PT failed\n"
		    "  c0p0=%g,%g,%g  c0p1=%g,%g,%g  c1p0=%g,%g,%g\n",
		    V3ARGS(ARS_PT(edit_ars,0,0)),
		    V3ARGS(ARS_PT(edit_ars,0,1)),
		    V3ARGS(ARS_PT(edit_ars,1,0)));
	bu_log("ECMD_ARS_MOVE_PT SUCCESS: c0p1=%g,%g,%g\n",
	       V3ARGS(ARS_PT(edit_ars,0,1)));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE XY: verify scale happened
     * ================================================================*/
    ars_reset(s, edit_ars, ae);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    {
	int xpos = 1372, ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }
    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE(xy) failed: %s\n",
		bu_vls_cstr(s->log_str));

    rt_edit_process(s);
    {
	/* (1,0,0) should be scaled */
	point_t orig = {1, 0, 0};
	if (VNEAR_EQUAL(ARS_PT(edit_ars,0,1), orig, SQRT_SMALL_FASTF))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE(xy) did not scale c0p1\n");
    }
    bu_log("RT_PARAMS_EDIT_SCALE(xy) SUCCESS: c0p1=%g,%g,%g (was 1,0,0)\n",
	   V3ARGS(ARS_PT(edit_ars,0,1)));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS XY: verify points moved
     * ================================================================*/
    ars_reset(s, edit_ars, ae);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, ARS_PT(edit_ars,0,0));
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
	point_t orig = {0, 0, 0};
	if (VNEAR_EQUAL(ARS_PT(edit_ars,0,0), orig, SQRT_SMALL_FASTF))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not translate c0p0\n");
    }
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: c0p0=%g,%g,%g (was 0,0,0)\n",
	   V3ARGS(ARS_PT(edit_ars,0,0)));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    ars_reset(s, edit_ars, ae);
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
    ars_reset(s, edit_ars, ae);
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
    ars_reset(s, edit_ars, ae);
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
     * ECMD_ARS_SCALE_CRV: scale curve 0 by factor 2 about its centroid
     * curve 0 before reset: (0,0,0), (1,0,0)  centroid = (0.5,0,0)
     * After scale 2: (0-0.5)*2+0.5=-0.5, (1-0.5)*2+0.5=1.5
     * => (−0.5,0,0) and (1.5,0,0)
     * ================================================================*/
    ars_reset(s, edit_ars, ae);
    ae->es_ars_crv = 0;
    ae->es_ars_col = 0;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ARS_SCALE_CRV);
    s->e_inpara = 1;
    s->e_para[0] = 2.0;
    rt_edit_process(s);
    {
	point_t exp0 = {-0.5, 0, 0};
	point_t exp1 = { 1.5, 0, 0};
	if (!VNEAR_EQUAL(ARS_PT(edit_ars,0,0), exp0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,0,1), exp1, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_ARS_SCALE_CRV failed: "
		    "c0p0=%g,%g,%g c0p1=%g,%g,%g (expected -0.5,0,0 and 1.5,0,0)\n",
		    V3ARGS(ARS_PT(edit_ars,0,0)), V3ARGS(ARS_PT(edit_ars,0,1)));
	bu_log("ECMD_ARS_SCALE_CRV SUCCESS: c0p0=%g,%g,%g c0p1=%g,%g,%g\n",
	       V3ARGS(ARS_PT(edit_ars,0,0)), V3ARGS(ARS_PT(edit_ars,0,1)));
    }

    /* ================================================================
     * ECMD_ARS_SCALE_COL: scale column 0 by factor 3 about its centroid
     * column 0: curve0=(0,0,0), curve1=(0,0,1)  centroid=(0,0,0.5)
     * After scale 3 about centroid:
     *   curve0: z=(0-0.5)*3+0.5 = -1.0  => (0,0,-1)
     *   curve1: z=(1-0.5)*3+0.5 =  2.0  => (0,0,2)
     * ================================================================*/
    ars_reset(s, edit_ars, ae);
    ae->es_ars_crv = 0;
    ae->es_ars_col = 0;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ARS_SCALE_COL);
    s->e_inpara = 1;
    s->e_para[0] = 3.0;
    rt_edit_process(s);
    {
	point_t exp_c0 = {0, 0, -1.0};
	point_t exp_c1 = {0, 0,  2.0};
	if (!VNEAR_EQUAL(ARS_PT(edit_ars,0,0), exp_c0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,1,0), exp_c1, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_ARS_SCALE_COL failed: "
		    "c0p0=%g,%g,%g c1p0=%g,%g,%g (expected 0,0,-1 and 0,0,2)\n",
		    V3ARGS(ARS_PT(edit_ars,0,0)), V3ARGS(ARS_PT(edit_ars,1,0)));
	bu_log("ECMD_ARS_SCALE_COL SUCCESS: c0p0=%g,%g,%g c1p0=%g,%g,%g\n",
	       V3ARGS(ARS_PT(edit_ars,0,0)), V3ARGS(ARS_PT(edit_ars,1,0)));
    }

    /* ================================================================
     * ECMD_ARS_INSERT_CRV: insert interpolated curve after curve 0
     * curve 0: (0,0,0),(1,0,0)  curve 1: (0,0,1),(1,0,1)
     * New curve (midpoint): (0,0,0.5),(1,0,0.5)
     * ncurves becomes 3; selection advances to index 1 (new curve)
     * ================================================================*/
    ars_reset(s, edit_ars, ae);
    ae->es_ars_crv = 0;
    ae->es_ars_col = 0;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ARS_INSERT_CRV);
    s->e_inpara = 0;
    rt_edit_process(s);
    {
	if ((size_t)edit_ars->ncurves != 3)
	    bu_exit(1, "ERROR: ECMD_ARS_INSERT_CRV: expected 3 curves, got %zu\n",
		    edit_ars->ncurves);
	point_t exp_new0 = {0, 0, 0.5};
	point_t exp_new1 = {1, 0, 0.5};
	if (!VNEAR_EQUAL(ARS_PT(edit_ars,1,0), exp_new0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ARS_PT(edit_ars,1,1), exp_new1, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_ARS_INSERT_CRV: new curve wrong: "
		    "c1p0=%g,%g,%g c1p1=%g,%g,%g (expected 0,0,0.5 and 1,0,0.5)\n",
		    V3ARGS(ARS_PT(edit_ars,1,0)), V3ARGS(ARS_PT(edit_ars,1,1)));
	bu_log("ECMD_ARS_INSERT_CRV SUCCESS: ncurves=%zu new c1=(%g,%g,%g),(%g,%g,%g)\n",
	       edit_ars->ncurves,
	       V3ARGS(ARS_PT(edit_ars,1,0)), V3ARGS(ARS_PT(edit_ars,1,1)));
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
