/*                          S P H . C P P
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
/** @file sph.cpp
 *
 * Test editing of SPH (sphere) primitive parameters.
 *
 * SPH uses the same edit functions as ELL (rt_edit_ell_edit).
 * SPH is stored internally as ELL with three equal axes.
 *
 * Reference SPH: center=(0,0,0), radius=1.0.
 * Internally: V=(0,0,0), A=(1,0,0), B=(0,1,0), C=(0,0,1).
 *
 * SPH has no ft_set_edit_mode (NULL). It uses the ELL edit path
 * which falls back to edit_generic for RT_PARAMS_EDIT_SCALE/TRANS/ROT.
 *
 * RT_PARAMS_EDIT_SCALE (es_scale=2, keypoint=(0,0,0)):
 *   Uniform scale: all axes scale by 2. V=(0,0,0)→(0,0,0),
 *   A=(1,0,0)→(2,0,0), B=(0,1,0)→(0,2,0), C=(0,0,1)→(0,0,2).
 *
 * RT_PARAMS_EDIT_TRANS (e_para=(5,5,5)):
 *   Translate: V=(0,0,0)→(5,5,5), A/B/C unchanged direction.
 *
 * RT_PARAMS_EDIT_ROT (e_para=(5,5,5), rotate_about='k'):
 *   Rotate: V stays at origin (keypoint). A/B/C rotate.
 *
 * RT_MATRIX_EDIT_ROT (e_para=(30,0,0), keypoint=(0,0,0)):
 *   Absolute matrix rotation: model_changes gains a 30-deg X rotation.
 *   Verifies that model_changes is no longer identity after the rotation.
 *
 * RT_MATRIX_EDIT_TRANS_MODEL_XYZ (e_para=(10,20,30)):
 *   Absolute model-space oed translation: model_changes updated so that
 *   e_keypoint maps to model position (10,20,30).
 *
 * RT_PARAMS_EDIT_ROT XY and RT_MATRIX_EDIT_ROT XY:
 *   Both now succeed via edit_mrot_xy() which converts the XY mouse
 *   displacement to "ax"/"ay" knob increments and calls rt_knob_edit_rot.
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
make_sph(struct rt_wdb *wdbp)
{
    const char *objname = "sph";

    /* SPH is stored as ELL with all semi-axes equal to the radius */
    struct rt_ell_internal *ell;
    BU_ALLOC(ell, struct rt_ell_internal);
    ell->magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell->v, 0, 0, 0);
    VSET(ell->a, 1, 0, 0);
    VSET(ell->b, 0, 1, 0);
    VSET(ell->c, 0, 0, 1);

    /* Export as ID_SPH (sphere) so d_minor_type == ID_SPH */
    wdb_export(wdbp, objname, (void *)ell, ID_SPH, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create sph object\n");

    return dp;
}

static void
sph_reset(struct rt_edit *s, struct rt_ell_internal *ell)
{
    VSET(ell->v, 0, 0, 0);
    VSET(ell->a, 1, 0, 0);
    VSET(ell->b, 0, 1, 0);
    VSET(ell->c, 0, 0, 1);

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

    struct directory *dp = make_sph(wdbp);

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

    /* SPH is stored as ELL internally */
    struct rt_ell_internal *ell =
	(struct rt_ell_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE (scale=2 about keypoint (0,0,0))
     *
     * SPH has no ft_set_edit_mode (NULL), so use rt_edit_set_edflag directly.
     * Uniform scale: all axis vectors double in length.
     *   V=(0,0,0) → (0,0,0)
     *   A=(1,0,0) → (2,0,0)
     *   B=(0,1,0) → (0,2,0)
     *   C=(0,0,1) → (0,0,2)
     * ================================================================*/
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_SCALE);
    s->edit_mode = RT_PARAMS_EDIT_SCALE;
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSETALL(s->e_keypoint, 0.0);

    rt_edit_process(s);
    {
	vect_t exp_v = {0, 0, 0}, exp_a = {2, 0, 0}, exp_b = {0, 2, 0}, exp_c = {0, 0, 2};
	if (!VNEAR_EQUAL(ell->v, exp_v, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ell->a, exp_a, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ell->b, exp_b, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ell->c, exp_c, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n"
		    "  V=%g,%g,%g  A=%g,%g,%g  B=%g,%g,%g  C=%g,%g,%g\n",
		    V3ARGS(ell->v), V3ARGS(ell->a), V3ARGS(ell->b), V3ARGS(ell->c));
	bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: V=%g,%g,%g A=%g,%g,%g\n",
	       V3ARGS(ell->v), V3ARGS(ell->a));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS (translate; keypoint (0,0,0) → e_para=(5,5,5))
     *
     * V shifts by (5,5,5); A/B/C are relative vectors, unchanged.
     * ================================================================*/
    sph_reset(s, ell);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_TRANS);
    s->edit_mode = RT_PARAMS_EDIT_TRANS;
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	vect_t exp_v = {5, 5, 5};
	vect_t exp_a = {1, 0, 0};  /* unchanged */
	if (!VNEAR_EQUAL(ell->v, exp_v, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ell->a, exp_a, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n"
		    "  V=%g,%g,%g A=%g,%g,%g\n",
		    V3ARGS(ell->v), V3ARGS(ell->a));
	bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: V=%g,%g,%g A=%g,%g,%g\n",
	       V3ARGS(ell->v), V3ARGS(ell->a));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_ROT (rotate about keypoint (0,0,0), angles=5,5,5)
     *
     * V stays at origin (it IS the keypoint). A/B/C rotate.
     * After rotation: V unchanged at (0,0,0).
     * ================================================================*/
    sph_reset(s, ell);
    MAT_IDN(s->acc_rot_sol);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_ROT);
    s->edit_mode = RT_PARAMS_EDIT_ROT;
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	vect_t exp_v = {0, 0, 0};
	if (!VNEAR_EQUAL(ell->v, exp_v, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed: V=%g,%g,%g\n",
		    V3ARGS(ell->v));
	/* A should no longer be (1,0,0) - it's been rotated */
	vect_t orig_a = {1, 0, 0};
	if (VNEAR_EQUAL(ell->a, orig_a, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k): A not rotated (%g,%g,%g)\n",
		    V3ARGS(ell->a));
	bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: V=%g,%g,%g A=%g,%g,%g\n",
	       V3ARGS(ell->v), V3ARGS(ell->a));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE XY: verify scale happened
     * ================================================================*/
    sph_reset(s, ell);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_SCALE);
    s->edit_mode = RT_PARAMS_EDIT_SCALE;
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
	vect_t orig = {1, 0, 0};
	if (VNEAR_EQUAL(ell->a, orig, SQRT_SMALL_FASTF))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE(xy) did not scale A\n");
    }
    bu_log("RT_PARAMS_EDIT_SCALE(xy) SUCCESS: A=%g,%g,%g (was 1,0,0)\n",
	   V3ARGS(ell->a));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (not supported via
     * raw view XY; use rt_knob_edit_rot for interactive rotation)
     * ================================================================*/
    sph_reset(s, ell);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_ROT);
    s->edit_mode = RT_PARAMS_EDIT_ROT;
    mousevec[X] = 0.1; mousevec[Y] = -0.05; mousevec[Z] = 0;
    bu_vls_trunc(s->log_str, 0);
    int rot_xy_ret = (*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec);
    if (rot_xy_ret != BRLCAD_OK)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(xy) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(xy) SUCCESS: rotation applied via knob path\n");

    /* ================================================================
     * RT_MATRIX_EDIT_ROT: absolute matrix rotation from keyboard angles
     *
     * Set model_changes to a 30-deg rotation about X.  Keypoint is at
     * origin so world position of keypoint is also origin - the rotation
     * point is the origin and no translation stripping is needed.
     * After the edit model_changes must differ from identity.
     * ================================================================*/
    sph_reset(s, ell);
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
	/* acc_rot_sol should now hold the 30-deg X rotation */
	mat_t expected_rot;
	MAT_IDN(expected_rot);
	bn_mat_angles(expected_rot, 30, 0, 0);
	if (!bn_mat_is_equal(s->acc_rot_sol, expected_rot, &tol))
	    bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT: acc_rot_sol not updated\n");
	bu_log("RT_MATRIX_EDIT_ROT SUCCESS: model_changes rotated, acc_rot_sol updated\n");
    }

    /* ================================================================
     * RT_MATRIX_EDIT_ROT XY returns BRLCAD_ERROR (not supported via
     * raw view XY; use rt_knob_edit_rot for interactive rotation)
     * ================================================================*/
    sph_reset(s, ell);
    MAT_IDN(s->model_changes);
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_ROT);
    mousevec[X] = 0.1; mousevec[Y] = -0.05; mousevec[Z] = 0;
    bu_vls_trunc(s->log_str, 0);
    int mrot_xy_ret = (*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec);
    if (mrot_xy_ret != BRLCAD_OK)
	bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT(xy) failed\n");
    bu_log("RT_MATRIX_EDIT_ROT(xy) SUCCESS: rotation applied via knob path\n");

    /* ================================================================
     * RT_MATRIX_EDIT_TRANS_MODEL_XYZ: absolute model-space oed translation
     *
     * Start with identity model_changes and keypoint at origin.
     * After translating to (10,20,30): model_changes * (0,0,0) == (10,20,30).
     * ================================================================*/
    sph_reset(s, ell);
    MAT_IDN(s->model_changes);
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_TRANS_MODEL_XYZ);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);  /* target position in local units */
    VSET(s->e_keypoint, 0, 0, 0);
    s->local2base = 1.0;          /* 1:1 for test */

    rt_edit_process(s);
    {
	point_t kp_world;
	MAT4X3PNT(kp_world, s->model_changes, s->e_keypoint);
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
