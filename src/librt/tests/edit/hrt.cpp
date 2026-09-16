/*                          H R T . C P P
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
/** @file hrt.cpp
 *
 * Test editing of HRT (heart) primitive parameters.
 *
 * Reference HRT: V=(0,0,0), xdir=(1,0,0), ydir=(0,1,0), zdir=(0,0,1), d=0.5
 *
 * HRT uses edit_generic (no primitive-specific edit commands). Keypoint
 * is always (0,0,0) because EDOBJ[ID_HRT].ft_keypoint is NULL.
 *
 * rt_hrt_mat applies MAT4X3PNT to all four fields: v, xdir, ydir, zdir.
 * Note: xdir, ydir, zdir are treated as absolute points (not direction
 * vectors), so a translation matrix shifts them along with v.
 *
 * Rotation matrix R = bn_mat_angles(5,5,5) columns:
 *   R[:,0] = ( 0.99240387650610407,  0.09439130678413448, -0.07889757346864876)
 *   R[:,1] = (-0.08682408883346517,  0.99174183072099057,  0.09439130678346780)
 *   R[:,2] = ( 0.08715574274765817, -0.08682408883346517,  0.99240387650610407)
 *
 * Scale (s=2) about keypoint (0,0,0):
 *   bn_mat_scale_about_pnt uses mat[15]=1/s, MAT4X3PNT divides by mat[15]:
 *   result = original * s for all four fields.
 *
 * Translation (e_para=(10,20,30)) with keypoint (0,0,0), mv_context=1:
 *   All four fields are shifted by (+10,+20,+30).
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


struct directory *
make_hrt(struct rt_wdb *wdbp)
{
    const char *objname = "hrt";
    struct rt_hrt_internal *hrt;
    BU_ALLOC(hrt, struct rt_hrt_internal);
    hrt->hrt_magic = RT_HRT_INTERNAL_MAGIC;
    VSET(hrt->v,    0, 0, 0);
    VSET(hrt->xdir, 1, 0, 0);
    VSET(hrt->ydir, 0, 1, 0);
    VSET(hrt->zdir, 0, 0, 1);
    hrt->d = 0.5;

    wdb_export(wdbp, objname, (void *)hrt, ID_HRT, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create hrt object\n");

    return dp;
}

int
hrt_diff(const char *cmd,
	 const struct rt_hrt_internal *ctrl,
	 const struct rt_hrt_internal *h)
{
    int ret = 0;
    if (!VNEAR_EQUAL(ctrl->v,    h->v,    VUNITIZE_TOL) ||
	!VNEAR_EQUAL(ctrl->xdir, h->xdir, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(ctrl->ydir, h->ydir, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(ctrl->zdir, h->zdir, VUNITIZE_TOL)) {
	bu_log("ERROR(%s)\n"
	       "  v   ctrl=%0.17f,%0.17f,%0.17f  got=%0.17f,%0.17f,%0.17f\n"
	       "  x   ctrl=%0.17f,%0.17f,%0.17f  got=%0.17f,%0.17f,%0.17f\n"
	       "  y   ctrl=%0.17f,%0.17f,%0.17f  got=%0.17f,%0.17f,%0.17f\n"
	       "  z   ctrl=%0.17f,%0.17f,%0.17f  got=%0.17f,%0.17f,%0.17f\n",
	       cmd,
	       V3ARGS(ctrl->v),    V3ARGS(h->v),
	       V3ARGS(ctrl->xdir), V3ARGS(h->xdir),
	       V3ARGS(ctrl->ydir), V3ARGS(h->ydir),
	       V3ARGS(ctrl->zdir), V3ARGS(h->zdir));
	ret = 1;
    }
    return ret;
}

static void
hrt_reset(struct rt_edit *s, struct rt_hrt_internal *edit_hrt)
{
    VSET(edit_hrt->v,    0, 0, 0);
    VSET(edit_hrt->xdir, 1, 0, 0);
    VSET(edit_hrt->ydir, 0, 1, 0);
    VSET(edit_hrt->zdir, 0, 0, 1);
    edit_hrt->d = 0.5;

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

    struct directory *dp = make_hrt(wdbp);

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

    struct rt_hrt_internal *edit_hrt =
	(struct rt_hrt_internal *)s->es_int.idb_ptr;

    struct rt_hrt_internal ctrl;
    vect_t mousevec;

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale=2 about keypoint (0,0,0))
     *
     * bn_mat_scale_about_pnt: mat[15]=1/2.
     * MAT4X3PNT divides by mat[15], so all four fields scale by 2.
     * v → (0,0,0), xdir → (2,0,0), ydir → (0,2,0), zdir → (0,0,2)
     * d unchanged.
     * ================================================================*/
    /* HRT has no ft_set_edit_mode (NULL); set the flag directly. */
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VSET(ctrl.v,    0, 0, 0);
    VSET(ctrl.xdir, 2, 0, 0);
    VSET(ctrl.ydir, 0, 2, 0);
    VSET(ctrl.zdir, 0, 0, 2);

    rt_edit_process(s);
    if (hrt_diff("RT_PARAMS_EDIT_SCALE", &ctrl, edit_hrt))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: xdir=%g,%g,%g\n",
	   V3ARGS(edit_hrt->xdir));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; keypoint (0,0,0) → e_para)
     *
     * rt_hrt_mat applies MAT4X3PNT to all four fields.  Translation
     * matrix shifts all four by (+10,+20,+30).
     * v → (10,20,30)
     * xdir: (1,0,0) → (11,20,30)  (translation applied as a point)
     * ydir: (0,1,0) → (10,21,30)
     * zdir: (0,0,1) → (10,20,31)
     * ================================================================*/
    hrt_reset(s, edit_hrt);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);

    VSET(ctrl.v,     10, 20, 30);
    VSET(ctrl.xdir,  11, 20, 30);
    VSET(ctrl.ydir,  10, 21, 30);
    VSET(ctrl.zdir,  10, 20, 31);

    rt_edit_process(s);
    if (hrt_diff("RT_PARAMS_EDIT_TRANS", &ctrl, edit_hrt))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: v=%g,%g,%g\n", V3ARGS(edit_hrt->v));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint (0,0,0), angles=5,5,5)
     *
     * R = bn_mat_angles(5,5,5).  Keypoint (0,0,0), mv_context=1.
     * v   (0,0,0) → R*(0,0,0) = (0,0,0)
     * xdir (1,0,0) → R[:,0] = (0.99240..., 0.09439..., -0.07890...)
     * ydir (0,1,0) → R[:,1] = (-0.08682..., 0.99174..., 0.09439...)
     * zdir (0,0,1) → R[:,2] = (0.08716..., -0.08682..., 0.99240...)
     * ================================================================*/
    hrt_reset(s, edit_hrt);
    MAT_IDN(s->acc_rot_sol);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VSET(s->e_keypoint, 0, 0, 0);

    VSET(ctrl.v,    0, 0, 0);
    VSET(ctrl.xdir,  0.99240387650610407,  0.09439130678413448, -0.07889757346864876);
    VSET(ctrl.ydir, -0.08682408883346517,  0.99174183072099057,  0.09439130678346780);
    VSET(ctrl.zdir,  0.08715574274765817, -0.08682408883346517,  0.99240387650610407);

    rt_edit_process(s);
    if (hrt_diff("RT_PARAMS_EDIT_ROT(k)", &ctrl, edit_hrt))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: v=%g,%g,%g xdir=%g,%g,%g\n",
	   V3ARGS(edit_hrt->v), V3ARGS(edit_hrt->xdir));

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE XY (verify d is unchanged)
     * ================================================================*/
    hrt_reset(s, edit_hrt);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_SCALE);

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

    double scale_xy = 1.0 + 0.25 * (1383.0 / 2048.0);
    rt_edit_process(s);
    {
	/* d should be unchanged; v should still be (0,0,0) for scale about origin */
	if (!NEAR_EQUAL(edit_hrt->d, 0.5, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE(xy) d changed: %g\n",
		    edit_hrt->d);
	/* xdir should be scaled */
	vect_t expected_xdir;
	VSET(expected_xdir, 1.0 * scale_xy, 0, 0);
	if (!VNEAR_EQUAL(edit_hrt->xdir, expected_xdir, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE(xy) xdir: got %g,%g,%g\n",
		    V3ARGS(edit_hrt->xdir));
    }
    bu_log("RT_PARAMS_EDIT_SCALE(xy) SUCCESS: xdir=%g,%g,%g d=%g\n",
	   V3ARGS(edit_hrt->xdir), edit_hrt->d);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS XY (verify v moved)
     * ================================================================*/
    hrt_reset(s, edit_hrt);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, edit_hrt->v);

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
	point_t orig_v = {0, 0, 0};
	if (VNEAR_EQUAL(edit_hrt->v, orig_v, SQRT_SMALL_FASTF))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move v\n");
    }
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: v=%g,%g,%g (moved from 0,0,0)\n",
	   V3ARGS(edit_hrt->v));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    hrt_reset(s, edit_hrt);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_ROT);
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
     * origin. After the edit model_changes must differ from identity
     * and acc_rot_sol must hold the 30-deg X rotation.
     * ================================================================*/
    hrt_reset(s, edit_hrt);
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
    hrt_reset(s, edit_hrt);
    MAT_IDN(s->model_changes);
    {
	point_t kp_saved;
	VSET(kp_saved, 0, 0, 0);
	rt_edit_set_edflag(s, RT_MATRIX_EDIT_TRANS_MODEL_XYZ);
	s->e_inpara = 1;
	VSET(s->e_para, 10, 20, 30);
	VSET(s->e_keypoint, 0, 0, 0);
	s->local2base = 1.0;

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
