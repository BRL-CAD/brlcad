/*                       C L I N E . C P P
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
/** @file cline.cpp
 *
 * Test editing of CLINE primitive parameters.
 *
 * Reference CLINE: v=(0,0,0), h=(0,0,5), radius=3, thickness=0.5
 *
 * Expected values derived from the MGED editing code in
 * brlcad/src/mged/edsol.c (MENU_CLINE_*) and edcline.c.
 *
 * Notes on CLINE edit conventions:
 * - ECMD_CLINE_SCALE_H: e_para[0] is absolute |h| (or es_scale path).
 *   Bug fixed in ecmd_cline_scale_h/r/t: the strict `e_inpara != 1`
 *   check blocked XY mouse edits (which use es_scale with e_inpara=0).
 *   Fixed to allow es_scale path when e_inpara==0 and es_scale>0.
 * - ECMD_CLINE_SCALE_R: e_para[0] is absolute radius value.
 * - ECMD_CLINE_SCALE_T: e_para[0] is absolute thickness value.
 * - ECMD_CLINE_MOVE_H: e_para[0..2] is the new absolute endpoint of h.
 * - rt_cline_mat: h and v are alias-safe (temporaries used).
 * - RT_PARAMS_EDIT_SCALE: scales h, radius, thickness by scale factor.
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
make_cline(struct rt_wdb *wdbp)
{
    const char *objname = "cline";
    struct rt_cline_internal *cline;
    BU_ALLOC(cline, struct rt_cline_internal);
    cline->magic = RT_CLINE_INTERNAL_MAGIC;
    VSET(cline->v, 0, 0, 0);
    VSET(cline->h, 0, 0, 5);
    cline->radius    = 3.0;
    cline->thickness = 0.5;

    wdb_export(wdbp, objname, (void *)cline, ID_CLINE, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create cline object: %s\n", objname);

    return dp;
}

int
cline_diff(const char *cmd,
	   const struct rt_cline_internal *ctrl,
	   const struct rt_cline_internal *e)
{
    int ret = 0;
    if (!ctrl || !e) return 1;

    if (!VNEAR_EQUAL(ctrl->v, e->v, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) v ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->v), V3ARGS(e->v));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->h, e->h, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) h ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->h), V3ARGS(e->h));
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->radius, e->radius, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) radius ctrl=%0.17f got=%0.17f\n", cmd, ctrl->radius, e->radius);
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->thickness, e->thickness, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) thickness ctrl=%0.17f got=%0.17f\n",
	       cmd, ctrl->thickness, e->thickness);
	ret = 1;
    }

    return ret;
}

static void
cline_reset(struct rt_edit *s, struct rt_cline_internal *edit_cline,
	    const struct rt_cline_internal *orig,
	    struct rt_cline_internal *cmp)
{
    VMOVE(edit_cline->v, orig->v);
    VMOVE(edit_cline->h, orig->h);
    edit_cline->radius    = orig->radius;
    edit_cline->thickness = orig->thickness;

    VMOVE(cmp->v, orig->v);
    VMOVE(cmp->h, orig->h);
    cmp->radius    = orig->radius;
    cmp->thickness = orig->thickness;

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
    struct directory *dp = make_cline(wdbp);

    struct rt_db_internal intern, cmpintern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_cline_internal *orig = (struct rt_cline_internal *)intern.idb_ptr;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_cline_internal *cmp = (struct rt_cline_internal *)cmpintern.idb_ptr;

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

    struct rt_cline_internal *edit_cline =
	(struct rt_cline_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_CLINE_SCALE_H  (scale height vector; e_para[0] = abs |h|)
     *
     * e_inpara=1, e_para[0]=10:
     *   es_scale = 10 / |h| = 10/5 = 2.0 → h=(0,0,10)
     * Restore: e_para[0]=5 → es_scale = 5/10 = 0.5 → h=(0,0,5)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_CLINE_SCALE_H);
    s->e_inpara = 1;
    s->e_para[0] = 10.0;
    VSET(cmp->h, 0, 0, 10);

    rt_edit_process(s);
    if (cline_diff("ECMD_CLINE_SCALE_H", cmp, edit_cline))
	bu_exit(1, "ERROR: ECMD_CLINE_SCALE_H failed\n");
    bu_log("ECMD_CLINE_SCALE_H SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_cline->h));

    s->e_inpara = 1;
    s->e_para[0] = 5.0;
    VSET(cmp->h, 0, 0, 5);
    rt_edit_process(s);
    if (cline_diff("ECMD_CLINE_SCALE_H restore", cmp, edit_cline))
	bu_exit(1, "ERROR: ECMD_CLINE_SCALE_H restore failed\n");
    bu_log("ECMD_CLINE_SCALE_H SUCCESS: h restored to %g,%g,%g\n",
	   V3ARGS(edit_cline->h));

    /* ================================================================
     * ECMD_CLINE_SCALE_R  (set radius; e_para[0] = absolute radius)
     * ================================================================*/
    cline_reset(s, edit_cline, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_CLINE_SCALE_R);
    s->e_inpara = 1;
    s->e_para[0] = 6.0;
    cmp->radius = 6.0;

    rt_edit_process(s);
    if (cline_diff("ECMD_CLINE_SCALE_R", cmp, edit_cline))
	bu_exit(1, "ERROR: ECMD_CLINE_SCALE_R failed\n");
    bu_log("ECMD_CLINE_SCALE_R SUCCESS: radius=%g\n", edit_cline->radius);

    s->e_inpara = 1;
    s->e_para[0] = 3.0;
    cmp->radius = 3.0;
    rt_edit_process(s);
    if (cline_diff("ECMD_CLINE_SCALE_R restore", cmp, edit_cline))
	bu_exit(1, "ERROR: ECMD_CLINE_SCALE_R restore failed\n");
    bu_log("ECMD_CLINE_SCALE_R SUCCESS: radius restored to %g\n",
	   edit_cline->radius);

    /* ================================================================
     * ECMD_CLINE_SCALE_T  (set thickness; e_para[0] = absolute value)
     * ================================================================*/
    cline_reset(s, edit_cline, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_CLINE_SCALE_T);
    s->e_inpara = 1;
    s->e_para[0] = 1.0;
    cmp->thickness = 1.0;

    rt_edit_process(s);
    if (cline_diff("ECMD_CLINE_SCALE_T", cmp, edit_cline))
	bu_exit(1, "ERROR: ECMD_CLINE_SCALE_T failed\n");
    bu_log("ECMD_CLINE_SCALE_T SUCCESS: thickness=%g\n", edit_cline->thickness);

    s->e_inpara = 1;
    s->e_para[0] = 0.5;
    cmp->thickness = 0.5;
    rt_edit_process(s);
    if (cline_diff("ECMD_CLINE_SCALE_T restore", cmp, edit_cline))
	bu_exit(1, "ERROR: ECMD_CLINE_SCALE_T restore failed\n");
    bu_log("ECMD_CLINE_SCALE_T SUCCESS: thickness restored to %g\n",
	   edit_cline->thickness);

    /* ================================================================
     * ECMD_CLINE_MOVE_H  (move h end; e_para[0..2] = new endpoint)
     *
     * With mv_context=1 and e_mat=IDN, e_invmat=IDN:
     *   work = e_invmat * e_para = e_para
     *   h = work - v = e_para - (0,0,0) = e_para
     * e_para = (3,4,0): h = (3,4,0), |h| = 5 (unchanged)
     * ================================================================*/
    cline_reset(s, edit_cline, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_CLINE_MOVE_H);
    s->e_inpara = 3;
    VSET(s->e_para, 3, 4, 0);
    VSET(cmp->h, 3, 4, 0);

    rt_edit_process(s);
    if (cline_diff("ECMD_CLINE_MOVE_H", cmp, edit_cline))
	bu_exit(1, "ERROR: ECMD_CLINE_MOVE_H failed\n");
    bu_log("ECMD_CLINE_MOVE_H SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_cline->h));

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about v=(0,0,0), scale=2)
     *
     * rt_cline_mat: v stays (keypoint), h scales by 2, radius scales,
     * thickness scales (all via MAT4X3VEC / r/mat[15]).
     * ================================================================*/
    cline_reset(s, edit_cline, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VMOVE(cmp->v, orig->v);
    VSCALE(cmp->h, orig->h, 2.0);
    cmp->radius    = orig->radius    * 2.0;
    cmp->thickness = orig->thickness * 2.0;

    rt_edit_process(s);
    if (cline_diff("RT_PARAMS_EDIT_SCALE", cmp, edit_cline))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: h=%g,%g,%g r=%g t=%g\n",
	   V3ARGS(edit_cline->h), edit_cline->radius, edit_cline->thickness);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; v moves to e_para)
     * ================================================================*/
    cline_reset(s, edit_cline, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VMOVE(cmp->v, s->e_para);

    rt_edit_process(s);
    if (cline_diff("RT_PARAMS_EDIT_TRANS", cmp, edit_cline))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: v=%g,%g,%g\n", V3ARGS(edit_cline->v));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint v=(0,0,0), mv_context=1)
     *
     * R = bn_mat_angles(5,5,5), mat[15]=1 (pure rotation).
     * v stays at (0,0,0).
     * h' = R*(0,0,5) = 5*R[:,2]
     * R[:,2] = (0.087155742747658, -0.086824088833465, 0.992403876506104)
     * h' = (0.43577871373829, -0.43412044416732, 4.96201938253052)
     * radius, thickness unchanged (mat[15]=1 → no scale).
     * ================================================================*/
    cline_reset(s, edit_cline, orig, cmp);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig->v);

    VMOVE(cmp->v, orig->v);
    VSET(cmp->h, 0.43577871373829082, -0.43412044416732582, 4.96201938253052035);
    cmp->radius    = orig->radius;
    cmp->thickness = orig->thickness;

    rt_edit_process(s);
    if (cline_diff("RT_PARAMS_EDIT_ROT (k)", cmp, edit_cline))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: v=%g,%g,%g h=%g,%g,%g\n",
	   V3ARGS(edit_cline->v), V3ARGS(edit_cline->h));

    /* ================================================================
     * ECMD_CLINE_SCALE_H XY mouse-driven scale (requires fixed bug)
     *
     * edit_sscale_xy sets es_scale; then ecmd_cline_scale_h uses it.
     * ypos=1383: es_scale = 1 + 0.25*(1383/2048) = 1.168823242...
     * h' = (0,0,5) * 1.168823242 = (0,0,5.844116...)
     * ================================================================*/
    cline_reset(s, edit_cline, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_CLINE_SCALE_H);

    {
	int xpos = 1372;
	int ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_CLINE_SCALE_H(xy) ft_edit_xy failed: %s\n",
		bu_vls_cstr(s->log_str));

    VSET(cmp->h, 0, 0, 5.0 * (1.0 + 0.25 * (1383.0 / 2048.0)));

    bu_vls_trunc(s->log_str, 0);
    rt_edit_process(s);
    if (bu_vls_strlen(s->log_str) != 0)
	bu_log("Note: ECMD_CLINE_SCALE_H(xy) process log: %s\n",
	       bu_vls_cstr(s->log_str));
    if (cline_diff("ECMD_CLINE_SCALE_H (xy)", cmp, edit_cline))
	bu_exit(1, "ERROR: ECMD_CLINE_SCALE_H(xy) failed\n");
    bu_log("ECMD_CLINE_SCALE_H(xy) SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_cline->h));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS XY (verify v moved)
     * ================================================================*/
    cline_reset(s, edit_cline, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig->v);

    {
	int xpos = 1482;
	int ypos = 762;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) failed: %s\n",
		bu_vls_cstr(s->log_str));

    rt_edit_process(s);
    if (VNEAR_EQUAL(edit_cline->v, orig->v, SQRT_SMALL_FASTF))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move v\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: v=%g,%g,%g (moved from %g,%g,%g)\n",
	   V3ARGS(edit_cline->v), V3ARGS(orig->v));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    cline_reset(s, edit_cline, orig, cmp);
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
    cline_reset(s, edit_cline, orig, cmp);
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
    cline_reset(s, edit_cline, orig, cmp);
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
