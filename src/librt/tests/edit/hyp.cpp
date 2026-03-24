/*                         H Y P . C P P
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
/** @file hyp.cpp
 *
 * Test editing of HYP (hyperboloid of one sheet) primitive parameters.
 *
 * Reference HYP: Vi=(0,0,0), Hi=(0,0,10), A=(4,0,0), b=3, bnr=0.5
 *
 * Note on e_inpara convention for HYP:
 *   For ECMD_HYP_H/SCALE_A/SCALE_B/C, e_para[0] is used as es_scale
 *   directly (multiplies the current parameter), NOT as an absolute value.
 *   This differs from EPA/EHY where e_para[0] sets an absolute length.
 *
 * Expected values are derived from the MGED editing code in
 * brlcad/src/mged/edsol.c (search for MENU_HYP_*), confirmed by
 * analytical tracing of the same code path.
 *
 * Bug fixed: MAT4X3VEC aliasing in ecmd_hyp_rot_h.
 * MGED has the identical bug.  Fix justified: pure rotation must
 * preserve |Hi|; the aliased macro does not.
 *
 * rt_hyp_mat note: hyp_bnr (neck/base ratio) is not included in
 * rt_hyp_mat, so it is unchanged by RT_PARAMS_EDIT_SCALE and
 * RT_PARAMS_EDIT_ROT.
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
make_hyp(struct rt_wdb *wdbp)
{
    const char *objname = "hyp";
    struct rt_hyp_internal *hyp;
    BU_ALLOC(hyp, struct rt_hyp_internal);
    hyp->hyp_magic = RT_HYP_INTERNAL_MAGIC;
    VSET(hyp->hyp_Vi, 0, 0, 0);
    VSET(hyp->hyp_Hi, 0, 0, 10);
    VSET(hyp->hyp_A,  4, 0, 0);
    hyp->hyp_b   = 3;
    hyp->hyp_bnr = 0.5;

    wdb_export(wdbp, objname, (void *)hyp, ID_HYP, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create hyp object: %s\n", objname);

    return dp;
}

int
hyp_diff(const char *cmd,
	 const struct rt_hyp_internal *ctrl,
	 const struct rt_hyp_internal *e)
{
    int ret = 0;
    if (!ctrl || !e)
	return 1;

    if (!VNEAR_EQUAL(ctrl->hyp_Vi, e->hyp_Vi, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) hyp_Vi ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->hyp_Vi), V3ARGS(e->hyp_Vi));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->hyp_Hi, e->hyp_Hi, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) hyp_Hi ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->hyp_Hi), V3ARGS(e->hyp_Hi));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->hyp_A, e->hyp_A, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) hyp_A ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->hyp_A), V3ARGS(e->hyp_A));
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->hyp_b, e->hyp_b, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) hyp_b ctrl=%0.17f got=%0.17f\n", cmd, ctrl->hyp_b, e->hyp_b);
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->hyp_bnr, e->hyp_bnr, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) hyp_bnr ctrl=%0.17f got=%0.17f\n", cmd, ctrl->hyp_bnr, e->hyp_bnr);
	ret = 1;
    }

    return ret;
}

static void
hyp_reset(struct rt_edit *s, struct rt_hyp_internal *edit_hyp,
	  const struct rt_hyp_internal *orig, struct rt_hyp_internal *cmp)
{
    VMOVE(edit_hyp->hyp_Vi, orig->hyp_Vi);
    VMOVE(edit_hyp->hyp_Hi, orig->hyp_Hi);
    VMOVE(edit_hyp->hyp_A,  orig->hyp_A);
    edit_hyp->hyp_b   = orig->hyp_b;
    edit_hyp->hyp_bnr = orig->hyp_bnr;
    VMOVE(cmp->hyp_Vi, orig->hyp_Vi);
    VMOVE(cmp->hyp_Hi, orig->hyp_Hi);
    VMOVE(cmp->hyp_A,  orig->hyp_A);
    cmp->hyp_b   = orig->hyp_b;
    cmp->hyp_bnr = orig->hyp_bnr;
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

    struct directory *dp = make_hyp(wdbp);

    /* Grab copies for reference and comparison */
    struct rt_db_internal intern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_hyp_internal *orig_hyp = (struct rt_hyp_internal *)intern.idb_ptr;
    struct rt_db_internal cmpintern;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_hyp_internal *cmp_hyp = (struct rt_hyp_internal *)cmpintern.idb_ptr;

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

    struct rt_hyp_internal *edit_hyp = (struct rt_hyp_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_HYP_H  (scale Hi; note: e_para[0] is es_scale, not |Hi|)
     * MGED: es_scale = e_para[0] (scale factor); Hi' = Hi * es_scale
     * es_scale=2: Hi'=(0,0,20)
     * Restore: e_para[0]=0.5, es_scale=0.5, Hi'=(0,0,10)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_HYP_H);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_hyp->hyp_Hi, 0, 0, 20);

    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_H", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_H failed\n");
    bu_log("ECMD_HYP_H SUCCESS: Hi=%g,%g,%g\n", V3ARGS(edit_hyp->hyp_Hi));

    /* Restore via e_inpara (e_para[0] = scale factor 0.5, Hi goes from 20→10) */
    s->e_inpara = 1;
    s->e_para[0] = 0.5;
    VMOVE(cmp_hyp->hyp_Hi, orig_hyp->hyp_Hi);
    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_H restore", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_H restore failed\n");
    bu_log("ECMD_HYP_H SUCCESS: Hi restored to %g,%g,%g\n", V3ARGS(edit_hyp->hyp_Hi));

    /* ================================================================
     * ECMD_HYP_SCALE_A  (scale semi-major axis A)
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_HYP_SCALE_A);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_hyp->hyp_A, 8, 0, 0);

    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_SCALE_A", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_SCALE_A failed\n");
    bu_log("ECMD_HYP_SCALE_A SUCCESS: A=%g,%g,%g\n", V3ARGS(edit_hyp->hyp_A));

    s->e_inpara = 1;
    s->e_para[0] = 0.5;
    VMOVE(cmp_hyp->hyp_A, orig_hyp->hyp_A);
    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_SCALE_A restore", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_SCALE_A restore failed\n");
    bu_log("ECMD_HYP_SCALE_A SUCCESS: A restored to %g,%g,%g\n", V3ARGS(edit_hyp->hyp_A));

    /* ================================================================
     * ECMD_HYP_SCALE_B  (scale semi-minor length b)
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_HYP_SCALE_B);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    cmp_hyp->hyp_b = 6;

    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_SCALE_B", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_SCALE_B failed\n");
    bu_log("ECMD_HYP_SCALE_B SUCCESS: b=%g\n", edit_hyp->hyp_b);

    s->e_inpara = 1;
    s->e_para[0] = 0.5;
    cmp_hyp->hyp_b = 3;
    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_SCALE_B restore", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_SCALE_B restore failed\n");
    bu_log("ECMD_HYP_SCALE_B SUCCESS: b restored to %g\n", edit_hyp->hyp_b);

    /* ================================================================
     * ECMD_HYP_C  (scale bnr; allowed only if bnr*s <= 1.0)
     * Initial bnr=0.5; es_scale=1.5: bnr'=0.5*1.5=0.75 <= 1.0 ✓
     * Restore: e_para[0] = 1/1.5 * 1/(0.75/0.5) = e_para[0] to get bnr=0.5
     *   es_scale = 1/1.5 ≈ 0.6667; bnr'=0.75*(1/1.5)=0.5 ✓
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_HYP_C);
    s->e_inpara = 0;
    s->es_scale = 1.5;
    cmp_hyp->hyp_bnr = 0.75;

    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_C", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_C failed\n");
    bu_log("ECMD_HYP_C SUCCESS: bnr=%g\n", edit_hyp->hyp_bnr);

    /* Restore: scale by 1/1.5 to go from 0.75 back to 0.5 */
    s->e_inpara = 1;
    s->e_para[0] = 1.0 / 1.5;
    cmp_hyp->hyp_bnr = 0.5;
    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_C restore", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_C restore failed\n");
    bu_log("ECMD_HYP_C SUCCESS: bnr restored to %g\n", edit_hyp->hyp_bnr);

    /* ================================================================
     * ECMD_HYP_ROT_H  (rotate Hi vector; MAT4X3VEC aliasing bug fixed)
     *
     * R = bn_mat_angles(5,5,5), mat[15]=1.
     * Hi = (0,0,10): Hi' = R*(0,0,10) = 10*R[:,2]
     * R[:,2] = (0.087155742747658, -0.086824088833465, 0.992403877)
     * Hi' = (0.87155742747658165, -0.86824088833465165, 9.92403876506104070)
     * Vi, A, b, bnr unchanged.
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_HYP_ROT_H);
    s->e_inpara = 3;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_hyp->hyp_Vi);

    VMOVE(cmp_hyp->hyp_Vi, orig_hyp->hyp_Vi);   /* Vi unchanged */
    VSET(cmp_hyp->hyp_Hi, 0.87155742747658165, -0.86824088833465165, 9.92403876506104070);
    VMOVE(cmp_hyp->hyp_A,  orig_hyp->hyp_A);    /* A unchanged */
    cmp_hyp->hyp_b   = orig_hyp->hyp_b;
    cmp_hyp->hyp_bnr = orig_hyp->hyp_bnr;

    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_ROT_H", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_ROT_H failed\n");
    bu_log("ECMD_HYP_ROT_H SUCCESS: Hi=%g,%g,%g (|Hi|=%g, should be %g)\n",
	   V3ARGS(edit_hyp->hyp_Hi),
	   MAGNITUDE(edit_hyp->hyp_Hi), MAGNITUDE(orig_hyp->hyp_Hi));

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about Vi=(0,0,0))
     *
     * rt_hyp_mat: Vi stays, Hi scales, A scales, b scales.
     * hyp_bnr NOT in rt_hyp_mat → stays unchanged.
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VMOVE(cmp_hyp->hyp_Vi, orig_hyp->hyp_Vi);               /* Vi stays */
    VSCALE(cmp_hyp->hyp_Hi, orig_hyp->hyp_Hi, s->es_scale); /* (0,0,20) */
    VSCALE(cmp_hyp->hyp_A,  orig_hyp->hyp_A,  s->es_scale); /* (8,0,0) */
    cmp_hyp->hyp_b   = orig_hyp->hyp_b   * s->es_scale;     /* 6 */
    cmp_hyp->hyp_bnr = orig_hyp->hyp_bnr;                   /* 0.5 (unchanged) */

    rt_edit_process(s);
    if (hyp_diff("RT_PARAMS_EDIT_SCALE", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: Hi=%g,%g,%g A=%g,%g,%g b=%g bnr=%g\n",
	   V3ARGS(edit_hyp->hyp_Hi), V3ARGS(edit_hyp->hyp_A),
	   edit_hyp->hyp_b, edit_hyp->hyp_bnr);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; Vi moves to e_para)
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VMOVE(cmp_hyp->hyp_Vi, s->e_para);   /* Vi moves to e_para */
    /* Hi, A, b, bnr unchanged */

    rt_edit_process(s);
    if (hyp_diff("RT_PARAMS_EDIT_TRANS", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: Vi=%g,%g,%g\n", V3ARGS(edit_hyp->hyp_Vi));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint = Vi, mv_context=1)
     *
     * R = bn_mat_angles(5,5,5), mat[15]=1.
     * Vi stays (rotation about Vi=(0,0,0)).
     * Hi' = R * Hi = R * (0,0,10) (same as ECMD_HYP_ROT_H)
     * A' = R * A = R * (4,0,0) = 4*R[:,0]
     * b unchanged (b/mat[15] = b/1)
     * bnr unchanged (not in rt_hyp_mat)
     * R[:,0] = (0.99240387650610407, 0.09439130678413448, -0.07889757346864876)
     * A' = (3.96961550602441626, 0.37756522713653790, -0.31559029387459503)
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_hyp->hyp_Vi);

    VMOVE(cmp_hyp->hyp_Vi, orig_hyp->hyp_Vi);   /* Vi stays */
    VSET(cmp_hyp->hyp_Hi,  0.87155742747658165, -0.86824088833465165, 9.92403876506104070);
    VSET(cmp_hyp->hyp_A,   3.96961550602441626,  0.37756522713653790, -0.31559029387459503);
    cmp_hyp->hyp_b   = orig_hyp->hyp_b;
    cmp_hyp->hyp_bnr = orig_hyp->hyp_bnr;   /* unchanged */

    rt_edit_process(s);
    if (hyp_diff("RT_PARAMS_EDIT_ROT (k)", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: Vi=%g,%g,%g Hi=%g,%g,%g A=%g,%g,%g\n",
	   V3ARGS(edit_hyp->hyp_Vi), V3ARGS(edit_hyp->hyp_Hi),
	   V3ARGS(edit_hyp->hyp_A));

    /* ================================================================
     * ECMD_HYP_H - XY mouse-driven scale
     * ypos=1383: es_scale = 1 + 0.25*(1383/2048) = 1.168823242...
     * Hi_new = Hi * 1.168823242 = (0,0,11.688232421875)
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_HYP_H);

    {
	int xpos = 1372;
	int ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_HYP_H(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    VSET(cmp_hyp->hyp_Hi, 0, 0, 11.688232421875000);

    rt_edit_process(s);
    if (hyp_diff("ECMD_HYP_H (xy)", cmp_hyp, edit_hyp))
	bu_exit(1, "ERROR: ECMD_HYP_H(xy) failed\n");
    bu_log("ECMD_HYP_H(xy) SUCCESS: Hi=%g,%g,%g\n", V3ARGS(edit_hyp->hyp_Hi));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS - XY mouse-driven translation
     * Verify Vi moved from its original position.
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig_hyp->hyp_Vi);

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
    if (VNEAR_EQUAL(edit_hyp->hyp_Vi, orig_hyp->hyp_Vi, SQRT_SMALL_FASTF))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move Vi\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: Vi=%g,%g,%g (moved from %g,%g,%g)\n",
	   V3ARGS(edit_hyp->hyp_Vi), V3ARGS(orig_hyp->hyp_Vi));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT - XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
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
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
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
    hyp_reset(s, edit_hyp, orig_hyp, cmp_hyp);
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
