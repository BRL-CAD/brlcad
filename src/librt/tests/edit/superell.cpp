/*                    S U P E R E L L . C P P
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
/** @file superell.cpp
 *
 * Test editing of SUPERELL (superellipsoid) primitive parameters.
 *
 * Reference SUPERELL: v=(5,5,5), a=(4,0,0), b=(0,3,0), c=(0,0,2), n=1.0, e=1.0
 *
 * Expected values are derived from the MGED editing code in
 * brlcad/src/mged/edsol.c (search for MENU_SUPERELL_*), confirmed by
 * analytical tracing of the same code path.
 *
 * Notes:
 * - SUPERELL's n and e exponents are NOT in rt_superell_mat and are
 *   therefore unchanged by RT_PARAMS_EDIT_SCALE and RT_PARAMS_EDIT_ROT.
 * - ECMD_SUPERELL_SCALE_A/B/C: es_scale = e_para[0]*e_mat[15]/|axis|
 *   (e_mat[15]=1 in the default context), so e_para[0] is an absolute
 *   magnitude for the target axis vector length.
 * - ECMD_SUPERELL_SCALE_ABC: after scaling A, B and C are rescaled to
 *   match |A| (makes all three axes the same length).
 * - rt_superell_mat is alias-safe (copies to temporaries before
 *   MAT4X3VEC calls).
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
make_superell(struct rt_wdb *wdbp)
{
    const char *objname = "superell";
    struct rt_superell_internal *superell;
    BU_ALLOC(superell, struct rt_superell_internal);
    superell->magic = RT_SUPERELL_INTERNAL_MAGIC;
    VSET(superell->v, 5, 5, 5);
    VSET(superell->a, 4, 0, 0);
    VSET(superell->b, 0, 3, 0);
    VSET(superell->c, 0, 0, 2);
    superell->n = 1.0;
    superell->e = 1.0;

    wdb_export(wdbp, objname, (void *)superell, ID_SUPERELL, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create superell object: %s\n", objname);

    return dp;
}

int
superell_diff(const char *cmd,
	      const struct rt_superell_internal *ctrl,
	      const struct rt_superell_internal *e)
{
    int ret = 0;
    if (!ctrl || !e) return 1;

    if (!VNEAR_EQUAL(ctrl->v, e->v, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) v ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->v), V3ARGS(e->v));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->a, e->a, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) a ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->a), V3ARGS(e->a));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->b, e->b, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) b ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->b), V3ARGS(e->b));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->c, e->c, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) c ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n",
	       cmd, V3ARGS(ctrl->c), V3ARGS(e->c));
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->n, e->n, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) n ctrl=%0.17f got=%0.17f\n", cmd, ctrl->n, e->n);
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->e, e->e, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) e ctrl=%0.17f got=%0.17f\n", cmd, ctrl->e, e->e);
	ret = 1;
    }

    return ret;
}

static void
superell_reset(struct rt_edit *s, struct rt_superell_internal *edit_superell,
	       const struct rt_superell_internal *orig,
	       struct rt_superell_internal *cmp)
{
    VMOVE(edit_superell->v, orig->v);
    VMOVE(edit_superell->a, orig->a);
    VMOVE(edit_superell->b, orig->b);
    VMOVE(edit_superell->c, orig->c);
    edit_superell->n = orig->n;
    edit_superell->e = orig->e;

    VMOVE(cmp->v, orig->v);
    VMOVE(cmp->a, orig->a);
    VMOVE(cmp->b, orig->b);
    VMOVE(cmp->c, orig->c);
    cmp->n = orig->n;
    cmp->e = orig->e;

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
    struct directory *dp = make_superell(wdbp);

    struct rt_db_internal intern, cmpintern;
    rt_db_get_internal(&intern, dp, dbip, NULL);
    struct rt_superell_internal *orig = (struct rt_superell_internal *)intern.idb_ptr;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL);
    struct rt_superell_internal *cmp = (struct rt_superell_internal *)cmpintern.idb_ptr;

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

    struct rt_superell_internal *edit_superell =
	(struct rt_superell_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_SUPERELL_SCALE_A
     * es_scale path: a *= 2.0 → a=(8,0,0)
     * e_inpara path: e_para[0]=4, es_scale=4/8=0.5 → a=(4,0,0) restored
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SUPERELL_SCALE_A);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp->a, 8, 0, 0);

    rt_edit_process(s);
    if (superell_diff("ECMD_SUPERELL_SCALE_A", cmp, edit_superell))
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_A failed\n");
    bu_log("ECMD_SUPERELL_SCALE_A SUCCESS: a=%g,%g,%g\n", V3ARGS(edit_superell->a));

    s->e_inpara = 1;
    s->e_para[0] = 4.0;   /* absolute target |a| = 4 → es_scale=4/8=0.5 */
    VSET(cmp->a, 4, 0, 0);
    rt_edit_process(s);
    if (superell_diff("ECMD_SUPERELL_SCALE_A restore", cmp, edit_superell))
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_A restore failed\n");
    bu_log("ECMD_SUPERELL_SCALE_A SUCCESS: a restored to %g,%g,%g\n",
	   V3ARGS(edit_superell->a));

    /* ================================================================
     * ECMD_SUPERELL_SCALE_B
     * ================================================================*/
    superell_reset(s, edit_superell, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SUPERELL_SCALE_B);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp->b, 0, 6, 0);

    rt_edit_process(s);
    if (superell_diff("ECMD_SUPERELL_SCALE_B", cmp, edit_superell))
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_B failed\n");
    bu_log("ECMD_SUPERELL_SCALE_B SUCCESS: b=%g,%g,%g\n", V3ARGS(edit_superell->b));

    s->e_inpara = 1;
    s->e_para[0] = 3.0;
    VSET(cmp->b, 0, 3, 0);
    rt_edit_process(s);
    if (superell_diff("ECMD_SUPERELL_SCALE_B restore", cmp, edit_superell))
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_B restore failed\n");
    bu_log("ECMD_SUPERELL_SCALE_B SUCCESS: b restored to %g,%g,%g\n",
	   V3ARGS(edit_superell->b));

    /* ================================================================
     * ECMD_SUPERELL_SCALE_C
     * ================================================================*/
    superell_reset(s, edit_superell, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SUPERELL_SCALE_C);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp->c, 0, 0, 4);

    rt_edit_process(s);
    if (superell_diff("ECMD_SUPERELL_SCALE_C", cmp, edit_superell))
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_C failed\n");
    bu_log("ECMD_SUPERELL_SCALE_C SUCCESS: c=%g,%g,%g\n", V3ARGS(edit_superell->c));

    s->e_inpara = 1;
    s->e_para[0] = 2.0;
    VSET(cmp->c, 0, 0, 2);
    rt_edit_process(s);
    if (superell_diff("ECMD_SUPERELL_SCALE_C restore", cmp, edit_superell))
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_C restore failed\n");
    bu_log("ECMD_SUPERELL_SCALE_C SUCCESS: c restored to %g,%g,%g\n",
	   V3ARGS(edit_superell->c));

    /* ================================================================
     * ECMD_SUPERELL_SCALE_ABC (scale A, then force B and C to same |A|)
     * es_scale=2: a=(8,0,0), b=(0,8,0), c=(0,0,8)   [all mag 8]
     * ================================================================*/
    superell_reset(s, edit_superell, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SUPERELL_SCALE_ABC);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp->a, 8, 0, 0);
    VSET(cmp->b, 0, 8, 0);
    VSET(cmp->c, 0, 0, 8);

    rt_edit_process(s);
    if (superell_diff("ECMD_SUPERELL_SCALE_ABC", cmp, edit_superell))
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_ABC failed\n");
    bu_log("ECMD_SUPERELL_SCALE_ABC SUCCESS: a=%g,%g,%g b=%g,%g,%g c=%g,%g,%g\n",
	   V3ARGS(edit_superell->a), V3ARGS(edit_superell->b),
	   V3ARGS(edit_superell->c));

    /* Restore with e_inpara=1, e_para[0]=4 → es_scale=4/8=0.5
     * a=(4,0,0), b=(0,4,0), c=(0,0,4)  (all axes same length 4, not original) */
    s->e_inpara = 1;
    s->e_para[0] = 4.0;
    VSET(cmp->a, 4, 0, 0);
    VSET(cmp->b, 0, 4, 0);
    VSET(cmp->c, 0, 0, 4);
    rt_edit_process(s);
    if (superell_diff("ECMD_SUPERELL_SCALE_ABC shrink", cmp, edit_superell))
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_ABC shrink failed\n");
    bu_log("ECMD_SUPERELL_SCALE_ABC SUCCESS (shrink to 4): a=%g,%g,%g b=%g,%g,%g c=%g,%g,%g\n",
	   V3ARGS(edit_superell->a), V3ARGS(edit_superell->b),
	   V3ARGS(edit_superell->c));

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about v=(5,5,5), scale=2)
     *
     * rt_superell_mat: v stays (keypoint), a/b/c scale by 2.
     * n and e NOT in rt_superell_mat → unchanged.
     * ================================================================*/
    superell_reset(s, edit_superell, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    VMOVE(cmp->v, orig->v);
    VSCALE(cmp->a, orig->a, 2.0);
    VSCALE(cmp->b, orig->b, 2.0);
    VSCALE(cmp->c, orig->c, 2.0);
    cmp->n = orig->n;
    cmp->e = orig->e;

    rt_edit_process(s);
    if (superell_diff("RT_PARAMS_EDIT_SCALE", cmp, edit_superell))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: a=%g,%g,%g b=%g,%g,%g c=%g,%g,%g n=%g e=%g\n",
	   V3ARGS(edit_superell->a), V3ARGS(edit_superell->b),
	   V3ARGS(edit_superell->c), edit_superell->n, edit_superell->e);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate v to e_para)
     * ================================================================*/
    superell_reset(s, edit_superell, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 20, 30, 40);
    VMOVE(cmp->v, s->e_para);

    rt_edit_process(s);
    if (superell_diff("RT_PARAMS_EDIT_TRANS", cmp, edit_superell))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: v=%g,%g,%g\n", V3ARGS(edit_superell->v));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate about keypoint v=(5,5,5), mv_context=1)
     *
     * R = bn_mat_angles(5,5,5), mat[15]=1 (pure rotation).
     * v stays at (5,5,5).
     * a' = R*(4,0,0) = 4*R[:,0]
     * b' = R*(0,3,0) = 3*R[:,1]
     * c' = R*(0,0,2) = 2*R[:,2]
     * n, e unchanged.
     *
     * R = bn_mat_angles(5,5,5) (same rotation used in ell.cpp test):
     * R[:,0] = (0.99240387650610407, 0.09439130678413448, -0.07889757346864876)
     * R[:,1] = (-0.08682408883346517, 0.99174183072099057, 0.09439130678346780)
     * R[:,2] = (0.08715574274765817, -0.08682408883346517, 0.99240387650610407)
     * ================================================================*/
    superell_reset(s, edit_superell, orig, cmp);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig->v);

    VMOVE(cmp->v, orig->v);
    VSET(cmp->a,  3.96961550602441628,  0.37756522713653792, -0.31559029387459504);
    VSET(cmp->b, -0.26047226650039551,  2.97522549216297171,  0.28317392035040341);
    VSET(cmp->c,  0.17431148549531633, -0.17364817766693033,  1.98480775301220814);
    cmp->n = orig->n;
    cmp->e = orig->e;

    rt_edit_process(s);
    if (superell_diff("RT_PARAMS_EDIT_ROT (k)", cmp, edit_superell))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: v=%g,%g,%g a=%g,%g,%g\n",
	   V3ARGS(edit_superell->v), V3ARGS(edit_superell->a));

    /* ================================================================
     * ECMD_SUPERELL_SCALE_A XY mouse-driven scale
     * ypos=1383: es_scale = 1 + 0.25*(1383/2048) = 1.168823242...
     * a' = (4,0,0) * 1.168823242 = (4.675292968..., 0, 0)
     * ================================================================*/
    superell_reset(s, edit_superell, orig, cmp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SUPERELL_SCALE_A);

    {
	int xpos = 1372;
	int ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_A(xy) failed: %s\n",
		bu_vls_cstr(s->log_str));

    VSET(cmp->a, 4.0 * (1.0 + 0.25 * (1383.0 / 2048.0)), 0, 0);

    rt_edit_process(s);
    if (superell_diff("ECMD_SUPERELL_SCALE_A (xy)", cmp, edit_superell))
	bu_exit(1, "ERROR: ECMD_SUPERELL_SCALE_A(xy) failed\n");
    bu_log("ECMD_SUPERELL_SCALE_A(xy) SUCCESS: a=%g,%g,%g\n",
	   V3ARGS(edit_superell->a));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS XY mouse-driven translation (verify V moved)
     * ================================================================*/
    superell_reset(s, edit_superell, orig, cmp);
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
    if (VNEAR_EQUAL(edit_superell->v, orig->v, SQRT_SMALL_FASTF))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move v\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: v=%g,%g,%g (moved from %g,%g,%g)\n",
	   V3ARGS(edit_superell->v), V3ARGS(orig->v));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    superell_reset(s, edit_superell, orig, cmp);
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
    superell_reset(s, edit_superell, orig, cmp);
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
    superell_reset(s, edit_superell, orig, cmp);
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
