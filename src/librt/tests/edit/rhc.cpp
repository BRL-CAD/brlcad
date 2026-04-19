/*                         R H C . C P P
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
/** @file rhc.cpp
 *
 * Test editing of RHC (right hyperbolic cylinder) primitive parameters.
 *
 * Reference RHC: V=(0,0,0), H=(0,0,10), B=(0,5,0), r=3, c=2
 *
 * e_inpara convention: e_para[0] is an absolute value (magnitude);
 * es_scale is derived from e_para[0]/current_magnitude.
 *
 * Expected values are derived from MGED edsol.c (MENU_RHC_*) and
 * analytical tracing of the same code path.
 * rt_rhc_mat is alias-safe (copies to temporaries).
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
make_rhc(struct rt_wdb *wdbp)
{
    const char *objname = "rhc";
    struct rt_rhc_internal *rhc;
    BU_ALLOC(rhc, struct rt_rhc_internal);
    rhc->rhc_magic = RT_RHC_INTERNAL_MAGIC;
    VSET(rhc->rhc_V, 0, 0, 0);
    VSET(rhc->rhc_H, 0, 0, 10);
    VSET(rhc->rhc_B, 0, 5, 0);
    rhc->rhc_r = 3;
    rhc->rhc_c = 2;

    wdb_export(wdbp, objname, (void *)rhc, ID_RHC, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create rhc object: %s\n", objname);

    return dp;
}

int
rhc_diff(const char *cmd,
	 const struct rt_rhc_internal *ctrl,
	 const struct rt_rhc_internal *e)
{
    int ret = 0;
    if (!ctrl || !e) return 1;

    if (!VNEAR_EQUAL(ctrl->rhc_V, e->rhc_V, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) rhc_V ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->rhc_V), V3ARGS(e->rhc_V));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->rhc_H, e->rhc_H, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) rhc_H ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->rhc_H), V3ARGS(e->rhc_H));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->rhc_B, e->rhc_B, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) rhc_B ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->rhc_B), V3ARGS(e->rhc_B));
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->rhc_r, e->rhc_r, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) rhc_r ctrl=%0.17f got=%0.17f\n", cmd, ctrl->rhc_r, e->rhc_r);
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->rhc_c, e->rhc_c, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) rhc_c ctrl=%0.17f got=%0.17f\n", cmd, ctrl->rhc_c, e->rhc_c);
	ret = 1;
    }

    return ret;
}

static void
rhc_reset(struct rt_edit *s, struct rt_rhc_internal *edit_rhc,
	  const struct rt_rhc_internal *orig, struct rt_rhc_internal *cmp)
{
    VMOVE(edit_rhc->rhc_V, orig->rhc_V);
    VMOVE(edit_rhc->rhc_H, orig->rhc_H);
    VMOVE(edit_rhc->rhc_B, orig->rhc_B);
    edit_rhc->rhc_r = orig->rhc_r;
    edit_rhc->rhc_c = orig->rhc_c;
    VMOVE(cmp->rhc_V, orig->rhc_V);
    VMOVE(cmp->rhc_H, orig->rhc_H);
    VMOVE(cmp->rhc_B, orig->rhc_B);
    cmp->rhc_r = orig->rhc_r;
    cmp->rhc_c = orig->rhc_c;
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
    if (argc != 1) return BRLCAD_ERROR;

    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create database instance\n");
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = make_rhc(wdbp);

    struct rt_db_internal intern, cmpintern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_rhc_internal *orig_rhc = (struct rt_rhc_internal *)intern.idb_ptr;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_rhc_internal *cmp_rhc = (struct rt_rhc_internal *)cmpintern.idb_ptr;

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
    struct rt_rhc_internal *edit_rhc = (struct rt_rhc_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_RHC_B  (scale breadth vector B)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_RHC_B);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_rhc->rhc_B, 0, 10, 0);

    rt_edit_process(s);
    if (rhc_diff("ECMD_RHC_B", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: ECMD_RHC_B failed\n");
    bu_log("ECMD_RHC_B SUCCESS: B=%g,%g,%g\n", V3ARGS(edit_rhc->rhc_B));

    s->e_inpara = 1;
    s->e_para[0] = 5;   /* absolute → es_scale=5/10=0.5 → B=(0,5,0) */
    VMOVE(cmp_rhc->rhc_B, orig_rhc->rhc_B);
    rt_edit_process(s);
    if (rhc_diff("ECMD_RHC_B restore", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: ECMD_RHC_B restore failed\n");
    bu_log("ECMD_RHC_B SUCCESS: B restored to %g,%g,%g\n", V3ARGS(edit_rhc->rhc_B));

    /* ================================================================
     * ECMD_RHC_H  (scale height vector H)
     * ================================================================*/
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_RHC_H);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_rhc->rhc_H, 0, 0, 20);

    rt_edit_process(s);
    if (rhc_diff("ECMD_RHC_H", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: ECMD_RHC_H failed\n");
    bu_log("ECMD_RHC_H SUCCESS: H=%g,%g,%g\n", V3ARGS(edit_rhc->rhc_H));

    s->e_inpara = 1;
    s->e_para[0] = 10;
    VMOVE(cmp_rhc->rhc_H, orig_rhc->rhc_H);
    rt_edit_process(s);
    if (rhc_diff("ECMD_RHC_H restore", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: ECMD_RHC_H restore failed\n");
    bu_log("ECMD_RHC_H SUCCESS: H restored to %g,%g,%g\n", V3ARGS(edit_rhc->rhc_H));

    /* ================================================================
     * ECMD_RHC_R  (scale half-width r)
     * ================================================================*/
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_RHC_R);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    cmp_rhc->rhc_r = 6;

    rt_edit_process(s);
    if (rhc_diff("ECMD_RHC_R", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: ECMD_RHC_R failed\n");
    bu_log("ECMD_RHC_R SUCCESS: r=%g\n", edit_rhc->rhc_r);

    s->e_inpara = 1;
    s->e_para[0] = 3;
    cmp_rhc->rhc_r = 3;
    rt_edit_process(s);
    if (rhc_diff("ECMD_RHC_R restore", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: ECMD_RHC_R restore failed\n");
    bu_log("ECMD_RHC_R SUCCESS: r restored to %g\n", edit_rhc->rhc_r);

    /* ================================================================
     * ECMD_RHC_C  (scale asymptote distance c)
     * ================================================================*/
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_RHC_C);
    s->e_inpara = 0;
    s->es_scale = 3.0;
    cmp_rhc->rhc_c = 6;

    rt_edit_process(s);
    if (rhc_diff("ECMD_RHC_C", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: ECMD_RHC_C failed\n");
    bu_log("ECMD_RHC_C SUCCESS: c=%g\n", edit_rhc->rhc_c);

    s->e_inpara = 1;
    s->e_para[0] = 2;   /* es_scale = 2/6 = 0.333 → c=6*(1/3)=2 */
    cmp_rhc->rhc_c = 2;
    rt_edit_process(s);
    if (rhc_diff("ECMD_RHC_C restore", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: ECMD_RHC_C restore failed\n");
    bu_log("ECMD_RHC_C SUCCESS: c restored to %g\n", edit_rhc->rhc_c);

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about V=(0,0,0), scale=2)
     * ================================================================*/
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VMOVE(cmp_rhc->rhc_V, orig_rhc->rhc_V);
    VSCALE(cmp_rhc->rhc_H, orig_rhc->rhc_H, s->es_scale);  /* (0,0,20) */
    VSCALE(cmp_rhc->rhc_B, orig_rhc->rhc_B, s->es_scale);  /* (0,10,0) */
    cmp_rhc->rhc_r = orig_rhc->rhc_r * s->es_scale;        /* 6 */
    cmp_rhc->rhc_c = orig_rhc->rhc_c * s->es_scale;        /* 4 */

    rt_edit_process(s);
    if (rhc_diff("RT_PARAMS_EDIT_SCALE", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: H=%g,%g,%g B=%g,%g,%g r=%g c=%g\n",
	   V3ARGS(edit_rhc->rhc_H), V3ARGS(edit_rhc->rhc_B),
	   edit_rhc->rhc_r, edit_rhc->rhc_c);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; V moves to e_para)
     * ================================================================*/
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VMOVE(cmp_rhc->rhc_V, s->e_para);

    rt_edit_process(s);
    if (rhc_diff("RT_PARAMS_EDIT_TRANS", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: V=%g,%g,%g\n", V3ARGS(edit_rhc->rhc_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint = V, mv_context=1)
     * R = bn_mat_angles(5,5,5), mat[15]=1.
     * V stays (0,0,0).
     * H' = R*(0,0,10) = (0.87155742747658165, -0.86824088833465165, 9.92403876506104070)
     * B' = R*(0,5,0)  = (-0.43412044416732585, 4.95870915360495285, 0.47195653391733902)
     * r, c unchanged.
     * ================================================================*/
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_rhc->rhc_V);

    VMOVE(cmp_rhc->rhc_V, orig_rhc->rhc_V);
    VSET(cmp_rhc->rhc_H,  0.87155742747658165, -0.86824088833465165, 9.92403876506104070);
    VSET(cmp_rhc->rhc_B, -0.43412044416732585,  4.95870915360495285, 0.47195653391733902);
    cmp_rhc->rhc_r = orig_rhc->rhc_r;
    cmp_rhc->rhc_c = orig_rhc->rhc_c;

    rt_edit_process(s);
    if (rhc_diff("RT_PARAMS_EDIT_ROT (k)", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: V=%g,%g,%g H=%g,%g,%g B=%g,%g,%g\n",
	   V3ARGS(edit_rhc->rhc_V), V3ARGS(edit_rhc->rhc_H), V3ARGS(edit_rhc->rhc_B));

    /* ================================================================
     * ECMD_RHC_B - XY mouse-driven scale
     * B' = (0, 5.844116210937500, 0)
     * ================================================================*/
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_RHC_B);

    {
	int xpos = 1372, ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_RHC_B(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    VSET(cmp_rhc->rhc_B, 0, 5.844116210937500, 0);
    rt_edit_process(s);
    if (rhc_diff("ECMD_RHC_B (xy)", cmp_rhc, edit_rhc))
	bu_exit(1, "ERROR: ECMD_RHC_B(xy) failed\n");
    bu_log("ECMD_RHC_B(xy) SUCCESS: B=%g,%g,%g\n", V3ARGS(edit_rhc->rhc_B));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS - XY: verify V moved
     * ================================================================*/
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig_rhc->rhc_V);

    {
	int xpos = 1482, ypos = 762;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    rt_edit_process(s);
    if (VNEAR_EQUAL(edit_rhc->rhc_V, orig_rhc->rhc_V, SQRT_SMALL_FASTF))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move V\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: V=%g,%g,%g (moved from %g,%g,%g)\n",
	   V3ARGS(edit_rhc->rhc_V), V3ARGS(orig_rhc->rhc_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT - XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
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
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
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
    rhc_reset(s, edit_rhc, orig_rhc, cmp_rhc);
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
