/*                      E X T R U D E . C P P
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
/** @file extrude.cpp
 *
 * Test editing of EXTRUDE (2D extrusion) primitive parameters.
 *
 * Reference EXTRUDE: V=(0,0,0), h=(0,0,10), u_vec=(1,0,0), v_vec=(0,1,0)
 *
 * Expected values derived from MGED edsol.c (MENU_EXTR_*) and
 * analytical tracing of the same code path.
 *
 * Bug fixed: ecmd_extr_rot_h had aliasing — MAT4X3VEC(extr->h, mat, extr->h)
 * with the same pointer as input and output.  Fixed by using a temporary.
 * MGED's edsol.c has the same bug.  Fix justified: pure rotation must
 * preserve magnitude; the alias does not.
 *
 * Bug fixed: ecmd_extr_scale_h had strict e_inpara != 1 check blocking
 * the XY mouse-driven (es_scale) path.  Fixed to allow es_scale when
 * e_inpara == 0.
 *
 * rt_extrude_mat is alias-safe (copies to temporaries before MAT4X3VEC).
 *
 * Note: rt_extrude_mat applies the scale/rotation to h, u_vec, AND v_vec.
 * ECMD_EXTR_ROT_H only rotates h; u_vec and v_vec are unchanged.
 *
 * Rotation matrix R = bn_mat_angles(5, 5, 5) columns (from ell.cpp):
 *   R[:,0] = ( 0.99240387650610407,  0.09439130678413448, -0.07889757346864876)
 *   R[:,1] = (-0.08682408883346517,  0.99174183072099057,  0.09439130678346780)
 *   R[:,2] = ( 0.08715574274765817, -0.08682408883346517,  0.99240387650610407)
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


/* Create a minimal sketch so the extrude import does not print warnings */
static void
make_sketch(struct rt_wdb *wdbp, const char *skt_name)
{
    struct rt_sketch_internal *skt;
    BU_ALLOC(skt, struct rt_sketch_internal);
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(skt->V,     0, 0, 0);
    VSET(skt->u_vec, 1, 0, 0);
    VSET(skt->v_vec, 0, 1, 0);
    skt->vert_count = 0;
    skt->verts = NULL;
    skt->curve.count = 0;
    skt->curve.reverse = NULL;
    skt->curve.segment = NULL;

    /* wdb_export → rt_db_free_internal → rt_sketch_ifree frees skt;
     * do NOT free it manually. */
    wdb_export(wdbp, skt_name, (void *)skt, ID_SKETCH, 1.0);
}

struct directory *
make_extrude(struct rt_wdb *wdbp, const char *skt_name)
{
    const char *objname = "extrude";
    struct rt_extrude_internal *extr;
    BU_ALLOC(extr, struct rt_extrude_internal);
    extr->magic = RT_EXTRUDE_INTERNAL_MAGIC;
    VSET(extr->V,     0, 0, 0);
    VSET(extr->h,     0, 0, 10);
    VSET(extr->u_vec, 1, 0, 0);
    VSET(extr->v_vec, 0, 1, 0);
    extr->keypoint = 0;
    extr->sketch_name = bu_strdup(skt_name);
    extr->skt = NULL;

    /* wdb_export → wdb_put_internal → rt_db_free_internal → rt_extrude_ifree
     * frees extr->sketch_name and extr itself; do NOT free them manually. */
    wdb_export(wdbp, objname, (void *)extr, ID_EXTRUDE, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create extrude object\n");

    return dp;
}

int
extr_diff(const char *cmd,
	  const struct rt_extrude_internal *ctrl,
	  const struct rt_extrude_internal *e)
{
    int ret = 0;
    if (!ctrl || !e) return 1;

    if (!VNEAR_EQUAL(ctrl->V, e->V, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) V ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->V), V3ARGS(e->V));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->h, e->h, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) h ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->h), V3ARGS(e->h));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->u_vec, e->u_vec, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) u_vec ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->u_vec), V3ARGS(e->u_vec));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->v_vec, e->v_vec, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) v_vec ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->v_vec), V3ARGS(e->v_vec));
	ret = 1;
    }

    return ret;
}

static void
extr_reset(struct rt_edit *s,
	   struct rt_extrude_internal *edit_extr,
	   const struct rt_extrude_internal *orig,
	   struct rt_extrude_internal *cmp)
{
    VMOVE(edit_extr->V,     orig->V);
    VMOVE(edit_extr->h,     orig->h);
    VMOVE(edit_extr->u_vec, orig->u_vec);
    VMOVE(edit_extr->v_vec, orig->v_vec);

    VMOVE(cmp->V,     orig->V);
    VMOVE(cmp->h,     orig->h);
    VMOVE(cmp->u_vec, orig->u_vec);
    VMOVE(cmp->v_vec, orig->v_vec);

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

    const char *skt_name = "test_sketch";
    make_sketch(wdbp, skt_name);

    struct directory *dp = make_extrude(wdbp, skt_name);

    struct rt_db_internal intern, cmpintern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_extrude_internal *orig = (struct rt_extrude_internal *)intern.idb_ptr;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_extrude_internal *cmp = (struct rt_extrude_internal *)cmpintern.idb_ptr;

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

    struct rt_extrude_internal *edit_extr =
	(struct rt_extrude_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_EXTR_SCALE_H  (scale height vector; e_para[0] = abs |h|)
     *
     * es_scale path: es_scale=2 → h=(0,0,20)
     * e_inpara path: e_para[0]=10 → es_scale=10/20=0.5 → h=(0,0,10) restored
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EXTR_SCALE_H);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp->h, 0, 0, 20);

    rt_edit_process(s);
    if (extr_diff("ECMD_EXTR_SCALE_H", cmp, edit_extr))
	bu_exit(1, "ERROR: ECMD_EXTR_SCALE_H failed\n");
    bu_log("ECMD_EXTR_SCALE_H SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_extr->h));

    s->e_inpara = 1;
    s->e_para[0] = 10.0;   /* absolute target |h| = 10 */
    VSET(cmp->h, 0, 0, 10);
    rt_edit_process(s);
    if (extr_diff("ECMD_EXTR_SCALE_H restore", cmp, edit_extr))
	bu_exit(1, "ERROR: ECMD_EXTR_SCALE_H restore failed\n");
    bu_log("ECMD_EXTR_SCALE_H SUCCESS: h restored to %g,%g,%g\n",
	   V3ARGS(edit_extr->h));

    /* ================================================================
     * ECMD_EXTR_MOV_H  (move h endpoint; mv_context=1)
     *
     * e_inpara=3, e_para=(3,4,0):
     *   e_invmat = IDN → work = (3,4,0)
     *   h = work - V = (3,4,0) - (0,0,0) = (3,4,0)
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EXTR_MOV_H);
    s->e_inpara = 3;
    VSET(s->e_para, 3, 4, 0);
    VSET(cmp->h, 3, 4, 0);

    rt_edit_process(s);
    if (extr_diff("ECMD_EXTR_MOV_H", cmp, edit_extr))
	bu_exit(1, "ERROR: ECMD_EXTR_MOV_H failed\n");
    bu_log("ECMD_EXTR_MOV_H SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_extr->h));

    /* ================================================================
     * ECMD_EXTR_ROT_H  (rotate h; e_inpara=3, e_para=(5,5,5))
     *
     * Bug fixed: aliasing MAT4X3VEC(extr->h, mat, extr->h).
     * With e_mat=IDN, e_invmat=IDN, keypoint=V=(0,0,0), mv_context=1:
     *   R = bn_mat_angles(5,5,5)
     *   mat = bn_mat_xform_about_pnt(R, (0,0,0)) = R
     *   h' = R * (0,0,10) = 10 * R[:,2]
     *      = (0.87155742747658175, -0.86824088833465165, 9.92403876506104070)
     * Only h rotates; u_vec and v_vec stay unchanged.
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EXTR_ROT_H);
    s->e_inpara = 3;
    VSET(s->e_para, 5, 5, 5);
    s->mv_context = 1;

    VMOVE(cmp->V, orig->V);
    VSET(cmp->h,     0.87155742747658175, -0.86824088833465165, 9.92403876506104070);
    VMOVE(cmp->u_vec, orig->u_vec);   /* unchanged */
    VMOVE(cmp->v_vec, orig->v_vec);   /* unchanged */

    rt_edit_process(s);
    if (extr_diff("ECMD_EXTR_ROT_H", cmp, edit_extr))
	bu_exit(1, "ERROR: ECMD_EXTR_ROT_H failed\n");
    bu_log("ECMD_EXTR_ROT_H SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_extr->h));

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about V=(0,0,0), scale=2)
     *
     * rt_extrude_mat: V stays (keypoint), h/u_vec/v_vec scale by 2.
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VMOVE(cmp->V, orig->V);
    VSCALE(cmp->h,     orig->h,     2.0);  /* (0,0,20) */
    VSCALE(cmp->u_vec, orig->u_vec, 2.0);  /* (2,0,0) */
    VSCALE(cmp->v_vec, orig->v_vec, 2.0);  /* (0,2,0) */

    rt_edit_process(s);
    if (extr_diff("RT_PARAMS_EDIT_SCALE", cmp, edit_extr))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: h=%g,%g,%g u=%g,%g,%g v=%g,%g,%g\n",
	   V3ARGS(edit_extr->h), V3ARGS(edit_extr->u_vec),
	   V3ARGS(edit_extr->v_vec));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; V moves to e_para)
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VMOVE(cmp->V, s->e_para);

    rt_edit_process(s);
    if (extr_diff("RT_PARAMS_EDIT_TRANS", cmp, edit_extr))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: V=%g,%g,%g\n", V3ARGS(edit_extr->V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint V=(0,0,0), mv_context=1)
     *
     * R = bn_mat_angles(5,5,5), mat[15]=1.
     * V stays (0,0,0).
     * h'     = R * (0,0,10)  = (0.87155742747658175, -0.86824088833465165, 9.92403876506104070)
     * u_vec' = R * (1,0,0)   = (0.99240387650610407,  0.09439130678413448, -0.07889757346864876)
     * v_vec' = R * (0,1,0)   = (-0.08682408883346517, 0.99174183072099057,  0.09439130678346780)
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig->V);

    VMOVE(cmp->V, orig->V);
    VSET(cmp->h,      0.87155742747658175, -0.86824088833465165,  9.92403876506104070);
    VSET(cmp->u_vec,  0.99240387650610407,  0.09439130678413448, -0.07889757346864876);
    VSET(cmp->v_vec, -0.08682408883346517,  0.99174183072099057,  0.09439130678346780);

    rt_edit_process(s);
    if (extr_diff("RT_PARAMS_EDIT_ROT (k)", cmp, edit_extr))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: V=%g,%g,%g h=%g,%g,%g\n",
	   V3ARGS(edit_extr->V), V3ARGS(edit_extr->h));

    /* ================================================================
     * ECMD_EXTR_SCALE_H XY mouse-driven scale (requires bug fix)
     *
     * edit_sscale_xy sets es_scale; then ecmd_extr_scale_h uses it.
     * ypos=1383: es_scale = 1 + 0.25*(1383/2048) ≈ 1.168823242...
     * h' = (0,0,10) * 1.168823242... = (0, 0, 11.68823242...)
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EXTR_SCALE_H);

    {
	int xpos = 1372, ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_EXTR_SCALE_H(xy) ft_edit_xy failed: %s\n",
		bu_vls_cstr(s->log_str));

    VSET(cmp->h, 0, 0, 10.0 * (1.0 + 0.25 * (1383.0 / 2048.0)));

    rt_edit_process(s);
    if (extr_diff("ECMD_EXTR_SCALE_H (xy)", cmp, edit_extr))
	bu_exit(1, "ERROR: ECMD_EXTR_SCALE_H(xy) failed\n");
    bu_log("ECMD_EXTR_SCALE_H(xy) SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_extr->h));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS XY (verify V moved)
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig->V);

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
    if (VNEAR_EQUAL(edit_extr->V, orig->V, SQRT_SMALL_FASTF))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move V\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: V=%g,%g,%g (moved from %g,%g,%g)\n",
	   V3ARGS(edit_extr->V), V3ARGS(orig->V));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
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
    extr_reset(s, edit_extr, orig, cmp);
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
    extr_reset(s, edit_extr, orig, cmp);
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
     * ECMD_EXTR_SCALE_A  (scale A / u_vec reference vector)
     *
     * es_scale path: es_scale=3 → u_vec=(3,0,0)
     * e_inpara path: e_para[0]=1 → es_scale=1/3=0.333 → u_vec=(1,0,0) restored
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EXTR_SCALE_A);
    s->e_inpara = 0;
    s->es_scale = 3.0;
    VSET(cmp->u_vec, 3, 0, 0);

    rt_edit_process(s);
    if (extr_diff("ECMD_EXTR_SCALE_A", cmp, edit_extr))
	bu_exit(1, "ERROR: ECMD_EXTR_SCALE_A failed\n");
    bu_log("ECMD_EXTR_SCALE_A SUCCESS: u_vec=%g,%g,%g\n", V3ARGS(edit_extr->u_vec));

    s->e_inpara = 1;
    s->e_para[0] = 1.0;   /* absolute target |u_vec| = 1 */
    VSET(cmp->u_vec, 1, 0, 0);
    rt_edit_process(s);
    if (extr_diff("ECMD_EXTR_SCALE_A restore", cmp, edit_extr))
	bu_exit(1, "ERROR: ECMD_EXTR_SCALE_A restore failed\n");
    bu_log("ECMD_EXTR_SCALE_A SUCCESS: u_vec restored to %g,%g,%g\n",
	   V3ARGS(edit_extr->u_vec));

    /* ================================================================
     * ECMD_EXTR_SCALE_B  (scale B / v_vec reference vector)
     *
     * es_scale=4 → v_vec=(0,4,0)
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EXTR_SCALE_B);
    s->e_inpara = 0;
    s->es_scale = 4.0;
    VSET(cmp->v_vec, 0, 4, 0);

    rt_edit_process(s);
    if (extr_diff("ECMD_EXTR_SCALE_B", cmp, edit_extr))
	bu_exit(1, "ERROR: ECMD_EXTR_SCALE_B failed\n");
    bu_log("ECMD_EXTR_SCALE_B SUCCESS: v_vec=%g,%g,%g\n", V3ARGS(edit_extr->v_vec));

    /* ================================================================
     * ECMD_EXTR_ROT_A  (rotate A / u_vec by 5,5,5 degrees)
     *
     * Uses the same bn_mat_angles(5,5,5) matrix as ECMD_EXTR_ROT_H.
     * Expected result: R * (1,0,0) where R = bn_mat_angles(5,5,5).
     *   col 0 of R = ( 0.992404, 0.094391, -0.078898) (see extrude.cpp header)
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EXTR_ROT_A);
    s->e_inpara = 3;
    VSET(s->e_para, 5.0, 5.0, 5.0);
    s->mv_context = 0;

    rt_edit_process(s);
    if (VNEAR_EQUAL(edit_extr->u_vec, orig->u_vec, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_EXTR_ROT_A: u_vec did not change\n");
    bu_log("ECMD_EXTR_ROT_A SUCCESS: u_vec=%g,%g,%g\n", V3ARGS(edit_extr->u_vec));

    /* ================================================================
     * ECMD_EXTR_ROT_B  (rotate B / v_vec by 5,5,5 degrees)
     *
     * Expected result: R * (0,1,0) where R = bn_mat_angles(5,5,5).
     *   col 1 of R = (-0.086824, 0.991742,  0.094391)
     * ================================================================*/
    extr_reset(s, edit_extr, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EXTR_ROT_B);
    s->e_inpara = 3;
    VSET(s->e_para, 5.0, 5.0, 5.0);
    s->mv_context = 0;

    rt_edit_process(s);
    if (VNEAR_EQUAL(edit_extr->v_vec, orig->v_vec, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_EXTR_ROT_B: v_vec did not change\n");
    bu_log("ECMD_EXTR_ROT_B SUCCESS: v_vec=%g,%g,%g\n", V3ARGS(edit_extr->v_vec));

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
