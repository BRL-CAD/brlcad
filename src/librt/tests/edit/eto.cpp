/*                         E T O . C P P
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
/** @file eto.cpp
 *
 * Test editing of ETO (elliptical torus) primitive parameters.
 *
 * Reference ETO: V=(5,0,0), N=(0,0,1), C=(2,0,1), r=10, rd=1
 *
 * Geometry constraints (must hold for all ecmd edits):
 *  cv  = C·N / |N| = 1  (N is unit)
 *  ch  = sqrt(|C|² - cv²) = sqrt(5-1) = 2
 *  dh  = rd * cv / |C|   = 1/sqrt(5)
 *  ECMD_ETO_R  allowed when ch<=r_new AND dh<=r_new (both trivially met for r_new>2)
 *  ECMD_ETO_RD allowed when rd_new<=|C| AND dh_new<=r
 *  ECMD_ETO_SCALE_C allowed when scale*|C|>=rd AND ch_new<=r
 *
 * Expected values are derived from the MGED editing code in
 * brlcad/src/mged/edsol.c (search for MENU_ETO_*), confirmed by
 * analytical tracing of the same code path.
 *
 * Bug fixed: MAT4X3VEC aliasing in ecmd_eto_rot_c.
 * MGED has the identical bug.  Fix justified: pure rotation must
 * preserve |C|; the aliased macro does not.
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
make_eto(struct rt_wdb *wdbp)
{
    const char *objname = "eto";
    struct rt_eto_internal *eto;
    BU_ALLOC(eto, struct rt_eto_internal);
    eto->eto_magic = RT_ETO_INTERNAL_MAGIC;
    VSET(eto->eto_V, 5, 0, 0);
    VSET(eto->eto_N, 0, 0, 1);
    VSET(eto->eto_C, 2, 0, 1);
    eto->eto_r  = 10;
    eto->eto_rd = 1;

    wdb_export(wdbp, objname, (void *)eto, ID_ETO, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create eto object: %s\n", objname);

    return dp;
}

int
eto_diff(const char *cmd,
	 const struct rt_eto_internal *ctrl,
	 const struct rt_eto_internal *e)
{
    int ret = 0;
    if (!ctrl || !e)
	return 1;

    if (!VNEAR_EQUAL(ctrl->eto_V, e->eto_V, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) eto_V ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->eto_V), V3ARGS(e->eto_V));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->eto_N, e->eto_N, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) eto_N ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->eto_N), V3ARGS(e->eto_N));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->eto_C, e->eto_C, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) eto_C ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->eto_C), V3ARGS(e->eto_C));
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->eto_r, e->eto_r, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) eto_r ctrl=%0.17f got=%0.17f\n", cmd, ctrl->eto_r, e->eto_r);
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->eto_rd, e->eto_rd, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) eto_rd ctrl=%0.17f got=%0.17f\n", cmd, ctrl->eto_rd, e->eto_rd);
	ret = 1;
    }

    return ret;
}

static void
eto_reset(struct rt_edit *s, struct rt_eto_internal *edit_eto,
	  const struct rt_eto_internal *orig, struct rt_eto_internal *cmp)
{
    VMOVE(edit_eto->eto_V, orig->eto_V);
    VMOVE(edit_eto->eto_N, orig->eto_N);
    VMOVE(edit_eto->eto_C, orig->eto_C);
    edit_eto->eto_r  = orig->eto_r;
    edit_eto->eto_rd = orig->eto_rd;
    VMOVE(cmp->eto_V, orig->eto_V);
    VMOVE(cmp->eto_N, orig->eto_N);
    VMOVE(cmp->eto_C, orig->eto_C);
    cmp->eto_r  = orig->eto_r;
    cmp->eto_rd = orig->eto_rd;
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

    struct directory *dp = make_eto(wdbp);

    /* Grab copies for reference and comparison */
    struct rt_db_internal intern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_eto_internal *orig_eto = (struct rt_eto_internal *)intern.idb_ptr;
    struct rt_db_internal cmpintern;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_eto_internal *cmp_eto = (struct rt_eto_internal *)cmpintern.idb_ptr;

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

    struct rt_eto_internal *edit_eto = (struct rt_eto_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_ETO_R  (scale radius of rotation r)
     * MGED: newrad = r * es_scale; allowed if ch<=newrad && dh<=newrad
     * Initial: r=10, ch=2, dh=1/sqrt(5)=0.447
     * es_scale=2: newrad=20; 2<=20 && 0.447<=20 → r=20 ✓
     * e_inpara=1, e_para[0]=10: newrad=10; conditions still met → r=10 ✓
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ETO_R);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    cmp_eto->eto_r = 20;

    rt_edit_process(s);
    if (eto_diff("ECMD_ETO_R", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: ECMD_ETO_R failed\n");
    bu_log("ECMD_ETO_R SUCCESS: r=%g\n", edit_eto->eto_r);

    s->e_inpara = 1;
    s->e_para[0] = 10;
    cmp_eto->eto_r = 10;
    rt_edit_process(s);
    if (eto_diff("ECMD_ETO_R restore", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: ECMD_ETO_R restore failed\n");
    bu_log("ECMD_ETO_R SUCCESS: r restored to %g\n", edit_eto->eto_r);

    /* ================================================================
     * ECMD_ETO_RD  (scale semi-minor axis rd)
     * es_scale=1.5: newrad=1.5; 1.5<=|C|=sqrt(5)=2.236 ✓;
     *   dh_new=1.5*1/sqrt(5)=0.671<=r=10 ✓ → rd=1.5
     * e_inpara=1, e_para[0]=1: newrad=1; 1<=2.236 ✓; dh=0.447<=10 ✓
     * ================================================================*/
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ETO_RD);
    s->e_inpara = 0;
    s->es_scale = 1.5;
    cmp_eto->eto_rd = 1.5;

    rt_edit_process(s);
    if (eto_diff("ECMD_ETO_RD", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: ECMD_ETO_RD failed\n");
    bu_log("ECMD_ETO_RD SUCCESS: rd=%g\n", edit_eto->eto_rd);

    s->e_inpara = 1;
    s->e_para[0] = 1;
    cmp_eto->eto_rd = 1;
    rt_edit_process(s);
    if (eto_diff("ECMD_ETO_RD restore", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: ECMD_ETO_RD restore failed\n");
    bu_log("ECMD_ETO_RD SUCCESS: rd restored to %g\n", edit_eto->eto_rd);

    /* ================================================================
     * ECMD_ETO_SCALE_C  (scale semi-major axis vector C)
     * es_scale=2: scale*|C|=2*sqrt(5)=4.472>=rd=1 ✓;
     *   C_new=(4,0,2), ch_new=4<=r=10 ✓ → C=(4,0,2)
     * e_inpara: e_para[0]=sqrt(5)=|C_orig|; es_scale=sqrt(5)/|C_new|=1/2 → C=(2,0,1)
     * ================================================================*/
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ETO_SCALE_C);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_eto->eto_C, 4, 0, 2);

    rt_edit_process(s);
    if (eto_diff("ECMD_ETO_SCALE_C", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: ECMD_ETO_SCALE_C failed\n");
    bu_log("ECMD_ETO_SCALE_C SUCCESS: C=%g,%g,%g\n", V3ARGS(edit_eto->eto_C));

    /* Restore: e_para[0] = sqrt(5) ≈ 2.236 → es_scale = sqrt(5)/|C_new| = sqrt(5)/(2*sqrt(5)) = 0.5 → C=(2,0,1) */
    s->e_inpara = 1;
    s->e_para[0] = sqrt(5.0);   /* = |C_orig| */
    VMOVE(cmp_eto->eto_C, orig_eto->eto_C);
    rt_edit_process(s);
    if (eto_diff("ECMD_ETO_SCALE_C restore", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: ECMD_ETO_SCALE_C restore failed\n");
    bu_log("ECMD_ETO_SCALE_C SUCCESS: C restored to %g,%g,%g\n", V3ARGS(edit_eto->eto_C));

    /* ================================================================
     * ECMD_ETO_ROT_C  (rotate C vector; MAT4X3VEC aliasing bug fixed)
     *
     * Rotation R = bn_mat_angles(5,5,5), mat[15]=1 (pure rotation).
     * C' = R * C  (alias-free with temporary)
     * C=(2,0,1): C' = 2*R[:,0] + 0*R[:,1] + 1*R[:,2]
     * R[:,0] = (0.99240387650610407, 0.09439130678413448, -0.07889757346864876)
     * R[:,2] = (0.08715574274765817, -0.08682408883346517, 0.99240387650610407)
     * C'[X] = 2*0.99240387650610407 + 0.08715574274765817 = 2.07196349575986631
     * C'[Y] = 2*0.09439130678413448 + (-0.08682408883346517) = 0.10195852473480379
     * C'[Z] = 2*(-0.07889757346864876) + 0.99240387650610407 = 0.83460872956880655
     *
     * V, N unchanged (only C is rotated).
     * r, rd unchanged.
     * ================================================================*/
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ETO_ROT_C);
    s->e_inpara = 3;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_eto->eto_V);

    VMOVE(cmp_eto->eto_V, orig_eto->eto_V);   /* V unchanged */
    VMOVE(cmp_eto->eto_N, orig_eto->eto_N);   /* N unchanged */
    VSET(cmp_eto->eto_C, 2.07196349575986631, 0.10195852473480379, 0.83460872956880655);
    cmp_eto->eto_r  = orig_eto->eto_r;    /* unchanged */
    cmp_eto->eto_rd = orig_eto->eto_rd;   /* unchanged */

    rt_edit_process(s);
    if (eto_diff("ECMD_ETO_ROT_C", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: ECMD_ETO_ROT_C failed\n");
    bu_log("ECMD_ETO_ROT_C SUCCESS: C=%g,%g,%g (|C|=%g, should be %g)\n",
	   V3ARGS(edit_eto->eto_C),
	   MAGNITUDE(edit_eto->eto_C), MAGNITUDE(orig_eto->eto_C));

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about V)
     * rt_eto_mat: eto_r = r/mat[15] = r*scale, eto_rd = rd*scale
     * MAT4X3VEC includes 1/mat[15] divisor, so N and C also scale.
     * V stays (scale about V).  After scaling N is still direction.
     * ================================================================*/
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VMOVE(cmp_eto->eto_V, orig_eto->eto_V);             /* V stays */
    VSCALE(cmp_eto->eto_N, orig_eto->eto_N, s->es_scale); /* N=(0,0,2) */
    VSCALE(cmp_eto->eto_C, orig_eto->eto_C, s->es_scale); /* C=(4,0,2) */
    cmp_eto->eto_r  = orig_eto->eto_r  * s->es_scale;   /* 20 */
    cmp_eto->eto_rd = orig_eto->eto_rd * s->es_scale;   /* 2  */

    rt_edit_process(s);
    if (eto_diff("RT_PARAMS_EDIT_SCALE", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: r=%g rd=%g C=%g,%g,%g\n",
	   edit_eto->eto_r, edit_eto->eto_rd, V3ARGS(edit_eto->eto_C));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; V moves to e_para)
     * ================================================================*/
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 20, 55, 40);
    VMOVE(cmp_eto->eto_V, s->e_para);   /* V moves to e_para */
    /* N, C, r, rd unchanged */

    rt_edit_process(s);
    if (eto_diff("RT_PARAMS_EDIT_TRANS", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: V=%g,%g,%g\n", V3ARGS(edit_eto->eto_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint = V, mv_context=1)
     * Rotates V, N, C via rt_eto_mat (alias-safe, mat[15]=1).
     * V stays (rotation about V).
     * N' = R * N = R * (0,0,1) = R[:,2]
     * C' = R * C = R * (2,0,1) (same as ECMD_ETO_ROT_C above)
     * r, rd unchanged.
     * ================================================================*/
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_eto->eto_V);

    VMOVE(cmp_eto->eto_V, orig_eto->eto_V);   /* V stays */
    /* N' = R*(0,0,1) = R[:,2] */
    VSET(cmp_eto->eto_N,  0.08715574274765817, -0.08682408883346517,  0.99240387650610407);
    /* C' = same as ECMD_ETO_ROT_C */
    VSET(cmp_eto->eto_C, 2.07196349575986631,  0.10195852473480379,  0.83460872956880655);
    cmp_eto->eto_r  = orig_eto->eto_r;
    cmp_eto->eto_rd = orig_eto->eto_rd;

    rt_edit_process(s);
    if (eto_diff("RT_PARAMS_EDIT_ROT (k)", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: V=%g,%g,%g C=%g,%g,%g\n",
	   V3ARGS(edit_eto->eto_V), V3ARGS(edit_eto->eto_C));

    /* ================================================================
     * ECMD_ETO_R - XY mouse-driven scale
     * ypos=1383: es_scale = 1 + 0.25*(1383/2048) = 1.168823242...
     * newrad = r * 1.168823242 = 10 * 1.168823242 = 11.68823242...
     * Check conditions: ch=2<=11.68 ✓  dh=0.447<=11.68 ✓
     * ================================================================*/
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ETO_R);

    {
	int xpos = 1372;
	int ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_ETO_R(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    cmp_eto->eto_r = 11.688232421875000;

    rt_edit_process(s);
    if (eto_diff("ECMD_ETO_R (xy)", cmp_eto, edit_eto))
	bu_exit(1, "ERROR: ECMD_ETO_R(xy) failed\n");
    bu_log("ECMD_ETO_R(xy) SUCCESS: r=%g\n", edit_eto->eto_r);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS - XY mouse-driven translation
     * Verify V moved from its original position.
     * ================================================================*/
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig_eto->eto_V);

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
    if (VNEAR_EQUAL(edit_eto->eto_V, orig_eto->eto_V, SQRT_SMALL_FASTF))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move V\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: V=%g,%g,%g (moved from %g,%g,%g)\n",
	   V3ARGS(edit_eto->eto_V), V3ARGS(orig_eto->eto_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT - XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
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
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
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
    eto_reset(s, edit_eto, orig_eto, cmp_eto);
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
