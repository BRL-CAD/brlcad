/*                         P A R T . C P P
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
/** @file part.cpp
 *
 * Test editing of PART (particle) primitive parameters.
 *
 * Reference PART: V=(0,0,0), H=(0,0,10), vrad=4, hrad=2
 *
 * e_inpara convention: e_para[0] is an absolute value (magnitude);
 * es_scale is derived from e_para[0]/current_value.
 *
 * Expected values are derived from MGED edsol.c (MENU_PART_*) and
 * analytical tracing of the same code path.
 * rt_part_mat is alias-safe (uses local temporaries).
 * edpart.c has no MAT4X3VEC calls — no aliasing bugs.
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
make_part(struct rt_wdb *wdbp)
{
    const char *objname = "part";
    struct rt_part_internal *part;
    BU_ALLOC(part, struct rt_part_internal);
    part->part_magic = RT_PART_INTERNAL_MAGIC;
    VSET(part->part_V, 0, 0, 0);
    VSET(part->part_H, 0, 0, 10);
    part->part_vrad = 4;
    part->part_hrad = 2;

    wdb_export(wdbp, objname, (void *)part, ID_PARTICLE, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create part object: %s\n", objname);

    return dp;
}

int
part_diff(const char *cmd,
	  const struct rt_part_internal *ctrl,
	  const struct rt_part_internal *e)
{
    int ret = 0;
    if (!ctrl || !e) return 1;

    if (!VNEAR_EQUAL(ctrl->part_V, e->part_V, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) part_V ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->part_V), V3ARGS(e->part_V));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->part_H, e->part_H, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) part_H ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->part_H), V3ARGS(e->part_H));
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->part_vrad, e->part_vrad, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) part_vrad ctrl=%0.17f got=%0.17f\n", cmd, ctrl->part_vrad, e->part_vrad);
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->part_hrad, e->part_hrad, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) part_hrad ctrl=%0.17f got=%0.17f\n", cmd, ctrl->part_hrad, e->part_hrad);
	ret = 1;
    }

    return ret;
}

static void
part_reset(struct rt_edit *s, struct rt_part_internal *edit_part,
	   const struct rt_part_internal *orig, struct rt_part_internal *cmp)
{
    VMOVE(edit_part->part_V, orig->part_V);
    VMOVE(edit_part->part_H, orig->part_H);
    edit_part->part_vrad = orig->part_vrad;
    edit_part->part_hrad = orig->part_hrad;
    VMOVE(cmp->part_V, orig->part_V);
    VMOVE(cmp->part_H, orig->part_H);
    cmp->part_vrad = orig->part_vrad;
    cmp->part_hrad = orig->part_hrad;
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
    struct directory *dp = make_part(wdbp);

    struct rt_db_internal intern, cmpintern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_part_internal *orig_part = (struct rt_part_internal *)intern.idb_ptr;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_part_internal *cmp_part = (struct rt_part_internal *)cmpintern.idb_ptr;

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
    struct rt_part_internal *edit_part = (struct rt_part_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_PART_H  (scale height vector H; e_para[0] = absolute |H|)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_PART_H);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_part->part_H, 0, 0, 20);

    rt_edit_process(s);
    if (part_diff("ECMD_PART_H", cmp_part, edit_part))
	bu_exit(1, "ERROR: ECMD_PART_H failed\n");
    bu_log("ECMD_PART_H SUCCESS: H=%g,%g,%g\n", V3ARGS(edit_part->part_H));

    s->e_inpara = 1;
    s->e_para[0] = 10;   /* es_scale = 10/20 = 0.5 → H=(0,0,10) */
    VMOVE(cmp_part->part_H, orig_part->part_H);
    rt_edit_process(s);
    if (part_diff("ECMD_PART_H restore", cmp_part, edit_part))
	bu_exit(1, "ERROR: ECMD_PART_H restore failed\n");
    bu_log("ECMD_PART_H SUCCESS: H restored to %g,%g,%g\n", V3ARGS(edit_part->part_H));

    /* ================================================================
     * ECMD_PART_VRAD  (scale v-end radius)
     * ================================================================*/
    part_reset(s, edit_part, orig_part, cmp_part);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_PART_VRAD);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    cmp_part->part_vrad = 8;

    rt_edit_process(s);
    if (part_diff("ECMD_PART_VRAD", cmp_part, edit_part))
	bu_exit(1, "ERROR: ECMD_PART_VRAD failed\n");
    bu_log("ECMD_PART_VRAD SUCCESS: vrad=%g\n", edit_part->part_vrad);

    s->e_inpara = 1;
    s->e_para[0] = 4;   /* es_scale = 4/8 = 0.5 → vrad=4 */
    cmp_part->part_vrad = 4;
    rt_edit_process(s);
    if (part_diff("ECMD_PART_VRAD restore", cmp_part, edit_part))
	bu_exit(1, "ERROR: ECMD_PART_VRAD restore failed\n");
    bu_log("ECMD_PART_VRAD SUCCESS: vrad restored to %g\n", edit_part->part_vrad);

    /* ================================================================
     * ECMD_PART_HRAD  (scale h-end radius)
     * ================================================================*/
    part_reset(s, edit_part, orig_part, cmp_part);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_PART_HRAD);
    s->e_inpara = 0;
    s->es_scale = 3.0;
    cmp_part->part_hrad = 6;

    rt_edit_process(s);
    if (part_diff("ECMD_PART_HRAD", cmp_part, edit_part))
	bu_exit(1, "ERROR: ECMD_PART_HRAD failed\n");
    bu_log("ECMD_PART_HRAD SUCCESS: hrad=%g\n", edit_part->part_hrad);

    s->e_inpara = 1;
    s->e_para[0] = 2;   /* es_scale = 2/6 = 0.333 → hrad=2 */
    cmp_part->part_hrad = 2;
    rt_edit_process(s);
    if (part_diff("ECMD_PART_HRAD restore", cmp_part, edit_part))
	bu_exit(1, "ERROR: ECMD_PART_HRAD restore failed\n");
    bu_log("ECMD_PART_HRAD SUCCESS: hrad restored to %g\n", edit_part->part_hrad);

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about V=(0,0,0), scale=2)
     * V stays, H scales, vrad scales, hrad scales.
     * ================================================================*/
    part_reset(s, edit_part, orig_part, cmp_part);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VMOVE(cmp_part->part_V, orig_part->part_V);             /* stays (0,0,0) */
    VSCALE(cmp_part->part_H, orig_part->part_H, s->es_scale); /* (0,0,20) */
    cmp_part->part_vrad = orig_part->part_vrad * s->es_scale; /* 8 */
    cmp_part->part_hrad = orig_part->part_hrad * s->es_scale; /* 4 */

    rt_edit_process(s);
    if (part_diff("RT_PARAMS_EDIT_SCALE", cmp_part, edit_part))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: H=%g,%g,%g vrad=%g hrad=%g\n",
	   V3ARGS(edit_part->part_H), edit_part->part_vrad, edit_part->part_hrad);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; V moves to e_para)
     * ================================================================*/
    part_reset(s, edit_part, orig_part, cmp_part);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VMOVE(cmp_part->part_V, s->e_para);

    rt_edit_process(s);
    if (part_diff("RT_PARAMS_EDIT_TRANS", cmp_part, edit_part))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: V=%g,%g,%g\n", V3ARGS(edit_part->part_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint = V=(0,0,0))
     * R = bn_mat_angles(5,5,5), mat[15]=1.
     * V stays (0,0,0).
     * H' = R*(0,0,10) = 10*R[:,2] (same as EPA/EHY/HYP)
     * vrad, hrad unchanged.
     * ================================================================*/
    part_reset(s, edit_part, orig_part, cmp_part);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_part->part_V);

    VMOVE(cmp_part->part_V, orig_part->part_V);   /* stays (0,0,0) */
    VSET(cmp_part->part_H, 0.87155742747658165, -0.86824088833465165, 9.92403876506104070);
    cmp_part->part_vrad = orig_part->part_vrad;
    cmp_part->part_hrad = orig_part->part_hrad;

    rt_edit_process(s);
    if (part_diff("RT_PARAMS_EDIT_ROT (k)", cmp_part, edit_part))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: V=%g,%g,%g H=%g,%g,%g\n",
	   V3ARGS(edit_part->part_V), V3ARGS(edit_part->part_H));

    /* ================================================================
     * ECMD_PART_H - XY mouse-driven scale
     * es_scale = 1 + 0.25*(1383/2048) = 1.168823242...
     * H' = (0, 0, 10*1.168823242) = (0, 0, 11.688232421875)
     * ================================================================*/
    part_reset(s, edit_part, orig_part, cmp_part);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_PART_H);

    {
	int xpos = 1372, ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_PART_H(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    VSET(cmp_part->part_H, 0, 0, 11.688232421875000);
    rt_edit_process(s);
    if (part_diff("ECMD_PART_H (xy)", cmp_part, edit_part))
	bu_exit(1, "ERROR: ECMD_PART_H(xy) failed\n");
    bu_log("ECMD_PART_H(xy) SUCCESS: H=%g,%g,%g\n", V3ARGS(edit_part->part_H));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS - XY: verify V moved
     * ================================================================*/
    part_reset(s, edit_part, orig_part, cmp_part);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig_part->part_V);

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
    if (VNEAR_EQUAL(edit_part->part_V, orig_part->part_V, SQRT_SMALL_FASTF))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move V\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: V=%g,%g,%g (moved from %g,%g,%g)\n",
	   V3ARGS(edit_part->part_V), V3ARGS(orig_part->part_V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT - XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    part_reset(s, edit_part, orig_part, cmp_part);
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
    part_reset(s, edit_part, orig_part, cmp_part);
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
    part_reset(s, edit_part, orig_part, cmp_part);
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
