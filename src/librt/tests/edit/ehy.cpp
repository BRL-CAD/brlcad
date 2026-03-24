/*                         E H Y . C P P
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
/** @file ehy.cpp
 *
 * Test editing of EHY (elliptical hyperboloid) primitive parameters.
 *
 * Reference EHY: V=(0,0,0), H=(0,0,10), Au=(1,0,0), r1=8, r2=5, c=2
 *
 * Expected values are derived from the MGED editing code in
 * brlcad/src/mged/edsol.c (search for MENU_EHY_*), confirmed by
 * analytical tracing of the same code path.
 *
 * Aliasing note: rt_ehy_mat copies all vectors to temporaries before
 * calling MAT4X3VEC — no aliasing bug, no fix needed.
 *
 * rt_params_edit_scale note: bn_mat_scale_about_pnt encodes the scale
 * factor in mat[15] = 1/scale.  Both MAT4X3VEC and MAT4X3PNT perform
 * perspective division by 1/mat[15], so ALL vector and point quantities
 * scale by the factor.  V stays only because it is the scale centre.
 * r1/r2/c scale because rt_ehy_mat uses r1/mat[15] = r1*scale.
 * This matches MGED behaviour.
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
make_ehy(struct rt_wdb *wdbp)
{
    const char *objname = "ehy";
    struct rt_ehy_internal *ehy;
    BU_ALLOC(ehy, struct rt_ehy_internal);
    ehy->ehy_magic = RT_EHY_INTERNAL_MAGIC;
    VSET(ehy->ehy_V, 0, 0, 0);
    VSET(ehy->ehy_H, 0, 0, 10);
    VSET(ehy->ehy_Au, 1, 0, 0);
    ehy->ehy_r1 = 8;
    ehy->ehy_r2 = 5;
    ehy->ehy_c  = 2;

    wdb_export(wdbp, objname, (void *)ehy, ID_EHY, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create ehy object: %s\n", objname);

    return dp;
}

int
ehy_diff(const char *cmd,
	 const struct rt_ehy_internal *ctrl,
	 const struct rt_ehy_internal *e)
{
    int ret = 0;
    if (!ctrl || !e)
	return 1;

    if (!VNEAR_EQUAL(ctrl->ehy_V, e->ehy_V, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) ehy_V ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->ehy_V), V3ARGS(e->ehy_V));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->ehy_H, e->ehy_H, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) ehy_H ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->ehy_H), V3ARGS(e->ehy_H));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->ehy_Au, e->ehy_Au, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) ehy_Au ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->ehy_Au), V3ARGS(e->ehy_Au));
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->ehy_r1, e->ehy_r1, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) ehy_r1 ctrl=%0.17f got=%0.17f\n", cmd, ctrl->ehy_r1, e->ehy_r1);
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->ehy_r2, e->ehy_r2, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) ehy_r2 ctrl=%0.17f got=%0.17f\n", cmd, ctrl->ehy_r2, e->ehy_r2);
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->ehy_c, e->ehy_c, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) ehy_c ctrl=%0.17f got=%0.17f\n", cmd, ctrl->ehy_c, e->ehy_c);
	ret = 1;
    }

    return ret;
}

static void
ehy_reset(struct rt_edit *s, struct rt_ehy_internal *edit_ehy,
	  const struct rt_ehy_internal *orig, struct rt_ehy_internal *cmp)
{
    VMOVE(edit_ehy->ehy_V,  orig->ehy_V);
    VMOVE(edit_ehy->ehy_H,  orig->ehy_H);
    VMOVE(edit_ehy->ehy_Au, orig->ehy_Au);
    edit_ehy->ehy_r1 = orig->ehy_r1;
    edit_ehy->ehy_r2 = orig->ehy_r2;
    edit_ehy->ehy_c  = orig->ehy_c;
    VMOVE(cmp->ehy_V,  orig->ehy_V);
    VMOVE(cmp->ehy_H,  orig->ehy_H);
    VMOVE(cmp->ehy_Au, orig->ehy_Au);
    cmp->ehy_r1 = orig->ehy_r1;
    cmp->ehy_r2 = orig->ehy_r2;
    cmp->ehy_c  = orig->ehy_c;
    MAT_IDN(s->acc_rot_sol);
    MAT_IDN(s->incr_change);
    s->acc_sc_sol = 1.0;
    s->e_inpara = 0;
    s->es_scale = 0.0;
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

    struct directory *dp = make_ehy(wdbp);

    /* Grab copies for reference and comparison */
    struct rt_db_internal intern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_ehy_internal *orig_ehy = (struct rt_ehy_internal *)intern.idb_ptr;
    struct rt_db_internal cmpintern;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_ehy_internal *cmp_ehy = (struct rt_ehy_internal *)cmpintern.idb_ptr;

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size = 73.3197;
    v->gv_isize = 1.0 / v->gv_size;
    v->gv_scale = 0.5 * v->gv_size;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;

    struct rt_ehy_internal *edit_ehy = (struct rt_ehy_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_EHY_H  (scale height vector H)
     * MGED: es_scale = e_para[0] / |H|; VSCALE(H, H, es_scale)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EHY_H);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_ehy->ehy_H, 0, 0, 20);   /* H_new = H * 2 */

    rt_edit_process(s);
    if (ehy_diff("ECMD_EHY_H", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: ECMD_EHY_H failed\n");
    bu_log("ECMD_EHY_H SUCCESS: H=%g,%g,%g\n", V3ARGS(edit_ehy->ehy_H));

    /* Restore via e_inpara: |H| back to 10 */
    s->e_inpara = 1;
    s->e_para[0] = 10;
    VMOVE(cmp_ehy->ehy_H, orig_ehy->ehy_H);
    rt_edit_process(s);
    if (ehy_diff("ECMD_EHY_H restore", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: ECMD_EHY_H restore failed\n");
    bu_log("ECMD_EHY_H SUCCESS: H restored to %g,%g,%g\n", V3ARGS(edit_ehy->ehy_H));

    /* ================================================================
     * ECMD_EHY_R1  (scale semi-major radius; allowed only if r1*s >= r2)
     * ================================================================*/
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EHY_R1);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    cmp_ehy->ehy_r1 = 16;  /* r1*2=16 >= r2=5 → allowed */

    rt_edit_process(s);
    if (ehy_diff("ECMD_EHY_R1", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: ECMD_EHY_R1 failed\n");
    bu_log("ECMD_EHY_R1 SUCCESS: r1=%g\n", edit_ehy->ehy_r1);

    s->e_inpara = 1;
    s->e_para[0] = 8;
    cmp_ehy->ehy_r1 = 8;
    rt_edit_process(s);
    if (ehy_diff("ECMD_EHY_R1 restore", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: ECMD_EHY_R1 restore failed\n");
    bu_log("ECMD_EHY_R1 SUCCESS: r1 restored to %g\n", edit_ehy->ehy_r1);

    /* ================================================================
     * ECMD_EHY_R2  (scale semi-minor radius; allowed only if r2*s <= r1)
     * ================================================================*/
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EHY_R2);
    s->e_inpara = 0;
    s->es_scale = 0.5;
    cmp_ehy->ehy_r2 = 2.5;  /* r2*0.5=2.5 <= r1=8 → allowed */

    rt_edit_process(s);
    if (ehy_diff("ECMD_EHY_R2", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: ECMD_EHY_R2 failed\n");
    bu_log("ECMD_EHY_R2 SUCCESS: r2=%g\n", edit_ehy->ehy_r2);

    s->e_inpara = 1;
    s->e_para[0] = 5;
    cmp_ehy->ehy_r2 = 5;
    rt_edit_process(s);
    if (ehy_diff("ECMD_EHY_R2 restore", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: ECMD_EHY_R2 restore failed\n");
    bu_log("ECMD_EHY_R2 SUCCESS: r2 restored to %g\n", edit_ehy->ehy_r2);

    /* ================================================================
     * ECMD_EHY_C  (scale distance to asymptotic cone apex)
     * MGED: es_scale = e_para[0] / c; c *= es_scale
     * ================================================================*/
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EHY_C);
    s->e_inpara = 0;
    s->es_scale = 3.0;
    cmp_ehy->ehy_c = 6;   /* c*3 = 6 */

    rt_edit_process(s);
    if (ehy_diff("ECMD_EHY_C", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: ECMD_EHY_C failed\n");
    bu_log("ECMD_EHY_C SUCCESS: c=%g\n", edit_ehy->ehy_c);

    s->e_inpara = 1;
    s->e_para[0] = 2;
    cmp_ehy->ehy_c = 2;
    rt_edit_process(s);
    if (ehy_diff("ECMD_EHY_C restore", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: ECMD_EHY_C restore failed\n");
    bu_log("ECMD_EHY_C SUCCESS: c restored to %g\n", edit_ehy->ehy_c);

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about keypoint = V)
     *
     * bn_mat_scale_about_pnt sets mat[15] = 1/scale.
     * MAT4X3VEC includes perspective division by 1/mat[15] = scale,
     * so H, Au all scale.  r1/r2/c scale via r1/mat[15] = r1*scale.
     * V stays (scale about V=(0,0,0)).
     * After scaling, Au is renormalized: (1*2,0,0)→VUNITIZE→(1,0,0).
     * ================================================================*/
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VMOVE(cmp_ehy->ehy_V, orig_ehy->ehy_V);  /* V stays */
    VSCALE(cmp_ehy->ehy_H, orig_ehy->ehy_H, s->es_scale);  /* H=(0,0,20) */
    VMOVE(cmp_ehy->ehy_Au, orig_ehy->ehy_Au);  /* (1,0,0) → normalized after scale */
    cmp_ehy->ehy_r1 = orig_ehy->ehy_r1 * s->es_scale;  /* 8*2=16 */
    cmp_ehy->ehy_r2 = orig_ehy->ehy_r2 * s->es_scale;  /* 5*2=10 */
    cmp_ehy->ehy_c  = orig_ehy->ehy_c  * s->es_scale;  /* 2*2=4  */

    rt_edit_process(s);
    if (ehy_diff("RT_PARAMS_EDIT_SCALE", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: r1=%g r2=%g c=%g H=%g,%g,%g\n",
	   edit_ehy->ehy_r1, edit_ehy->ehy_r2, edit_ehy->ehy_c,
	   V3ARGS(edit_ehy->ehy_H));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; V moves to e_para)
     * ================================================================*/
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VMOVE(cmp_ehy->ehy_V, s->e_para);   /* V moves to e_para */
    /* H, Au, r1, r2, c unchanged */

    rt_edit_process(s);
    if (ehy_diff("RT_PARAMS_EDIT_TRANS", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: V=%g,%g,%g\n", V3ARGS(edit_ehy->ehy_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint = V, mv_context=1)
     *
     * Rotation matrix R = bn_mat_angles(5,5,5).
     * V stays (rotation about V=(0,0,0)).
     * H' = R * H  (mat[15]=1 → r1/r2/c unchanged)
     * Au' = R * Au, then VUNITIZE
     *
     * R is derived from ELL test: a=(4,0,0)→a', b=(0,3,0)→b', c=(0,0,2)→c'
     * R[:,2] = c'/2 = (0.087155742747658, -0.086824088833465, 0.992403877)
     * H=(0,0,10): H' = R*(0,0,10) = 10*R[:,2]
     * R[:,0] = a'/4 → Au'
     * ================================================================*/
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_ehy->ehy_V);

    VMOVE(cmp_ehy->ehy_V, orig_ehy->ehy_V);   /* V stays at (0,0,0) */
    VSET(cmp_ehy->ehy_H,  0.87155742747658165, -0.86824088833465165,  9.92403876506104070);
    VSET(cmp_ehy->ehy_Au, 0.99240387650610407,  0.09439130678413448, -0.07889757346864876);
    cmp_ehy->ehy_r1 = orig_ehy->ehy_r1;   /* unchanged */
    cmp_ehy->ehy_r2 = orig_ehy->ehy_r2;   /* unchanged */
    cmp_ehy->ehy_c  = orig_ehy->ehy_c;    /* unchanged */

    rt_edit_process(s);
    if (ehy_diff("RT_PARAMS_EDIT_ROT (k)", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: V=%g,%g,%g H=%g,%g,%g\n",
	   V3ARGS(edit_ehy->ehy_V), V3ARGS(edit_ehy->ehy_H));

    /* ================================================================
     * ECMD_EHY_H - XY mouse-driven scale
     * ypos=1383: es_scale = 1 + 0.25*(1383/2048) = 1.168823242...
     * H_new = H * es_scale = (0,0, 10*1.168823242) = (0,0, 11.688232421875)
     * ================================================================*/
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EHY_H);

    {
	int xpos = 1372;
	int ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_EHY_H(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    VSET(cmp_ehy->ehy_H, 0, 0, 11.688232421875000);

    rt_edit_process(s);
    if (ehy_diff("ECMD_EHY_H (xy)", cmp_ehy, edit_ehy))
	bu_exit(1, "ERROR: ECMD_EHY_H(xy) failed\n");
    bu_log("ECMD_EHY_H(xy) SUCCESS: H=%g,%g,%g\n", V3ARGS(edit_ehy->ehy_H));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS - XY mouse-driven translation
     * The exact result depends on the view matrices.  We verify that V
     * moved from its original position (non-zero delta).
     * ================================================================*/
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig_ehy->ehy_V);

    {
	int xpos = 1482;
	int ypos = 762;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    rt_edit_process(s);
    if (VNEAR_EQUAL(edit_ehy->ehy_V, orig_ehy->ehy_V, SQRT_SMALL_FASTF))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move V\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: V=%g,%g,%g (moved from %g,%g,%g)\n",
	   V3ARGS(edit_ehy->ehy_V), V3ARGS(orig_ehy->ehy_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT - XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    mousevec[X] = 0.1;
    mousevec[Y] = -0.05;
    mousevec[Z] = 0;
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
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
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
    ehy_reset(s, edit_ehy, orig_ehy, cmp_ehy);
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
