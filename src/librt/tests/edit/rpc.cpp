/*                         R P C . C P P
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
/** @file rpc.cpp
 *
 * Test editing of RPC (right parabolic cylinder) primitive parameters.
 *
 * Reference RPC: V=(0,0,0), H=(0,0,10), B=(0,5,0), r=3
 *
 * e_inpara convention: e_para[0] is an absolute value (magnitude);
 * es_scale is derived from e_para[0]/current_magnitude.
 *
 * Expected values are derived from MGED edsol.c (MENU_RPC_*) and
 * analytical tracing of the same code path.
 * rt_rpc_mat is alias-safe (copies to temporaries).
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
make_rpc(struct rt_wdb *wdbp)
{
    const char *objname = "rpc";
    struct rt_rpc_internal *rpc;
    BU_ALLOC(rpc, struct rt_rpc_internal);
    rpc->rpc_magic = RT_RPC_INTERNAL_MAGIC;
    VSET(rpc->rpc_V, 0, 0, 0);
    VSET(rpc->rpc_H, 0, 0, 10);
    VSET(rpc->rpc_B, 0, 5, 0);
    rpc->rpc_r = 3;

    wdb_export(wdbp, objname, (void *)rpc, ID_RPC, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
bu_exit(1, "ERROR: Unable to create rpc object: %s\n", objname);

    return dp;
}

int
rpc_diff(const char *cmd,
 const struct rt_rpc_internal *ctrl,
 const struct rt_rpc_internal *e)
{
    int ret = 0;
    if (!ctrl || !e) return 1;

    if (!VNEAR_EQUAL(ctrl->rpc_V, e->rpc_V, VUNITIZE_TOL)) {
bu_log("ERROR(%s) rpc_V ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
       cmd, V3ARGS(ctrl->rpc_V), V3ARGS(e->rpc_V));
ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->rpc_H, e->rpc_H, VUNITIZE_TOL)) {
bu_log("ERROR(%s) rpc_H ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
       cmd, V3ARGS(ctrl->rpc_H), V3ARGS(e->rpc_H));
ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->rpc_B, e->rpc_B, VUNITIZE_TOL)) {
bu_log("ERROR(%s) rpc_B ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
       cmd, V3ARGS(ctrl->rpc_B), V3ARGS(e->rpc_B));
ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->rpc_r, e->rpc_r, VUNITIZE_TOL)) {
bu_log("ERROR(%s) rpc_r ctrl=%0.17f got=%0.17f\n", cmd, ctrl->rpc_r, e->rpc_r);
ret = 1;
    }

    return ret;
}

static void
rpc_reset(struct rt_edit *s, struct rt_rpc_internal *edit_rpc,
  const struct rt_rpc_internal *orig, struct rt_rpc_internal *cmp)
{
    VMOVE(edit_rpc->rpc_V, orig->rpc_V);
    VMOVE(edit_rpc->rpc_H, orig->rpc_H);
    VMOVE(edit_rpc->rpc_B, orig->rpc_B);
    edit_rpc->rpc_r = orig->rpc_r;
    VMOVE(cmp->rpc_V, orig->rpc_V);
    VMOVE(cmp->rpc_H, orig->rpc_H);
    VMOVE(cmp->rpc_B, orig->rpc_B);
    cmp->rpc_r = orig->rpc_r;
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
    struct directory *dp = make_rpc(wdbp);

    struct rt_db_internal intern, cmpintern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_rpc_internal *orig_rpc = (struct rt_rpc_internal *)intern.idb_ptr;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_rpc_internal *cmp_rpc = (struct rt_rpc_internal *)cmpintern.idb_ptr;

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
    struct rt_rpc_internal *edit_rpc = (struct rt_rpc_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_RPC_B  (scale breadth vector B)
     * es_scale = e_para[0]/|B|; B' = B * es_scale
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_RPC_B);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_rpc->rpc_B, 0, 10, 0);

    rt_edit_process(s);
    if (rpc_diff("ECMD_RPC_B", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: ECMD_RPC_B failed\n");
    bu_log("ECMD_RPC_B SUCCESS: B=%g,%g,%g\n", V3ARGS(edit_rpc->rpc_B));

    s->e_inpara = 1;
    s->e_para[0] = 5;   /* absolute value → es_scale = 5/10 = 0.5 → B=(0,5,0) */
    VMOVE(cmp_rpc->rpc_B, orig_rpc->rpc_B);
    rt_edit_process(s);
    if (rpc_diff("ECMD_RPC_B restore", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: ECMD_RPC_B restore failed\n");
    bu_log("ECMD_RPC_B SUCCESS: B restored to %g,%g,%g\n", V3ARGS(edit_rpc->rpc_B));

    /* ================================================================
     * ECMD_RPC_H  (scale height vector H)
     * ================================================================*/
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_RPC_H);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_rpc->rpc_H, 0, 0, 20);

    rt_edit_process(s);
    if (rpc_diff("ECMD_RPC_H", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: ECMD_RPC_H failed\n");
    bu_log("ECMD_RPC_H SUCCESS: H=%g,%g,%g\n", V3ARGS(edit_rpc->rpc_H));

    s->e_inpara = 1;
    s->e_para[0] = 10;
    VMOVE(cmp_rpc->rpc_H, orig_rpc->rpc_H);
    rt_edit_process(s);
    if (rpc_diff("ECMD_RPC_H restore", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: ECMD_RPC_H restore failed\n");
    bu_log("ECMD_RPC_H SUCCESS: H restored to %g,%g,%g\n", V3ARGS(edit_rpc->rpc_H));

    /* ================================================================
     * ECMD_RPC_R  (scale half-width r)
     * ================================================================*/
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_RPC_R);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    cmp_rpc->rpc_r = 6;

    rt_edit_process(s);
    if (rpc_diff("ECMD_RPC_R", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: ECMD_RPC_R failed\n");
    bu_log("ECMD_RPC_R SUCCESS: r=%g\n", edit_rpc->rpc_r);

    s->e_inpara = 1;
    s->e_para[0] = 3;
    cmp_rpc->rpc_r = 3;
    rt_edit_process(s);
    if (rpc_diff("ECMD_RPC_R restore", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: ECMD_RPC_R restore failed\n");
    bu_log("ECMD_RPC_R SUCCESS: r restored to %g\n", edit_rpc->rpc_r);

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about V=(0,0,0), scale=2)
     * ================================================================*/
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VMOVE(cmp_rpc->rpc_V, orig_rpc->rpc_V);
    VSCALE(cmp_rpc->rpc_H, orig_rpc->rpc_H, s->es_scale);  /* (0,0,20) */
    VSCALE(cmp_rpc->rpc_B, orig_rpc->rpc_B, s->es_scale);  /* (0,10,0) */
    cmp_rpc->rpc_r = orig_rpc->rpc_r * s->es_scale;        /* 6 */

    rt_edit_process(s);
    if (rpc_diff("RT_PARAMS_EDIT_SCALE", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: H=%g,%g,%g B=%g,%g,%g r=%g\n",
   V3ARGS(edit_rpc->rpc_H), V3ARGS(edit_rpc->rpc_B), edit_rpc->rpc_r);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; V moves to e_para)
     * ================================================================*/
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VMOVE(cmp_rpc->rpc_V, s->e_para);

    rt_edit_process(s);
    if (rpc_diff("RT_PARAMS_EDIT_TRANS", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: V=%g,%g,%g\n", V3ARGS(edit_rpc->rpc_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint = V, mv_context=1)
     * R = bn_mat_angles(5,5,5), mat[15]=1.
     * V stays (0,0,0).
     * H' = R*(0,0,10) = 10*R[:,2]
     * B' = R*(0,5,0)  = 5*R[:,1]
     * R[:,1] = b'/3 where b=(0,3,0) → b_rot from ELL test
     * B' = 5*(-0.086824088833465170, 0.99174183072099057, 0.094391306783467803)
     *    = (-0.43412044416732585, 4.95870915360495285, 0.47195653391733902)
     * r unchanged.
     * ================================================================*/
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_rpc->rpc_V);

    VMOVE(cmp_rpc->rpc_V, orig_rpc->rpc_V);   /* stays (0,0,0) */
    VSET(cmp_rpc->rpc_H,  0.87155742747658165, -0.86824088833465165, 9.92403876506104070);
    VSET(cmp_rpc->rpc_B, -0.43412044416732585,  4.95870915360495285, 0.47195653391733902);
    cmp_rpc->rpc_r = orig_rpc->rpc_r;

    rt_edit_process(s);
    if (rpc_diff("RT_PARAMS_EDIT_ROT (k)", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: V=%g,%g,%g H=%g,%g,%g B=%g,%g,%g\n",
   V3ARGS(edit_rpc->rpc_V), V3ARGS(edit_rpc->rpc_H), V3ARGS(edit_rpc->rpc_B));

    /* ================================================================
     * ECMD_RPC_B - XY mouse-driven scale
     * es_scale = 1 + 0.25*(1383/2048) = 1.168823242...
     * B' = (0, 5*1.168823242, 0) = (0, 5.844116210937500, 0)
     * ================================================================*/
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_RPC_B);

    {
int xpos = 1372, ypos = 1383;
mousevec[X] = xpos * INV_BV;
mousevec[Y] = ypos * INV_BV;
mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
bu_exit(1, "ERROR: ECMD_RPC_B(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    VSET(cmp_rpc->rpc_B, 0, 5.844116210937500, 0);
    rt_edit_process(s);
    if (rpc_diff("ECMD_RPC_B (xy)", cmp_rpc, edit_rpc))
bu_exit(1, "ERROR: ECMD_RPC_B(xy) failed\n");
    bu_log("ECMD_RPC_B(xy) SUCCESS: B=%g,%g,%g\n", V3ARGS(edit_rpc->rpc_B));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS - XY: verify V moved
     * ================================================================*/
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig_rpc->rpc_V);

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
    if (VNEAR_EQUAL(edit_rpc->rpc_V, orig_rpc->rpc_V, SQRT_SMALL_FASTF))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move V\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: V=%g,%g,%g (moved from %g,%g,%g)\n",
   V3ARGS(edit_rpc->rpc_V), V3ARGS(orig_rpc->rpc_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT - XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
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
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
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
    rpc_reset(s, edit_rpc, orig_rpc, cmp_rpc);
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
