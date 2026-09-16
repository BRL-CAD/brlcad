/*                         E L L . C P P
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
/** @file ell.cpp
 *
 * Test editing of ELL primitive parameters.
 */

#include "common.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"


struct directory *
make_ell(struct rt_wdb *wdbp)
{
    const char *objname = "ell";
    struct rt_ell_internal *ell;
    BU_ALLOC(ell, struct rt_ell_internal);
    ell->magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell->v, 10, 5, 20);
    VSET(ell->a, 4, 0, 0);
    VSET(ell->b, 0, 3, 0);
    VSET(ell->c, 0, 0, 2);

    wdb_export(wdbp, objname, (void *)ell, ID_ELL, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
bu_exit(1, "ERROR: Unable to create ell object: %s\n", objname);

    return dp;
}

int
ell_diff(const char *cmd, struct rt_ell_internal *ctrl, struct rt_ell_internal *e)
{
    int ret = 0;
    if (!ctrl || !e)
return 1;

    if (!VNEAR_EQUAL(ctrl->v, e->v, VUNITIZE_TOL)) {
bu_log("ERROR(%s) unexpected change - ell parameter v %0.17f,%0.17f,%0.17f -> %0.17f,%0.17f,%0.17f\n", cmd, V3ARGS(ctrl->v), V3ARGS(e->v));
ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->a, e->a, VUNITIZE_TOL)) {
bu_log("ERROR(%s) unexpected change - ell parameter a %0.17f,%0.17f,%0.17f -> %0.17f,%0.17f,%0.17f\n", cmd, V3ARGS(ctrl->a), V3ARGS(e->a));
ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->b, e->b, VUNITIZE_TOL)) {
bu_log("ERROR(%s) unexpected change - ell parameter b %0.17f,%0.17f,%0.17f -> %0.17f,%0.17f,%0.17f\n", cmd, V3ARGS(ctrl->b), V3ARGS(e->b));
ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->c, e->c, VUNITIZE_TOL)) {
bu_log("ERROR(%s) unexpected change - ell parameter c %0.17f,%0.17f,%0.17f -> %0.17f,%0.17f,%0.17f\n", cmd, V3ARGS(ctrl->c), V3ARGS(e->c));
ret = 1;
    }

    return ret;
}

/* Reset the working ell to original values and clear state */
static void
ell_reset(struct rt_edit *s, struct rt_ell_internal *edit_ell,
  const struct rt_ell_internal *orig_ell,
  struct rt_ell_internal *cmp_ell)
{
    VMOVE(edit_ell->v, orig_ell->v);
    VMOVE(edit_ell->a, orig_ell->a);
    VMOVE(edit_ell->b, orig_ell->b);
    VMOVE(edit_ell->c, orig_ell->c);
    VMOVE(cmp_ell->v, orig_ell->v);
    VMOVE(cmp_ell->a, orig_ell->a);
    VMOVE(cmp_ell->b, orig_ell->b);
    VMOVE(cmp_ell->c, orig_ell->c);
    MAT_IDN(s->acc_rot_sol);
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

    struct directory *dp = make_ell(wdbp);

    /* We'll refer to the original for reference and comparison */
    struct rt_db_internal intern;
    rt_db_get_internal(&intern, dp, dbip, NULL);
    struct rt_ell_internal *orig_ell = (struct rt_ell_internal *)intern.idb_ptr;
    struct rt_db_internal cmpintern;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL);
    struct rt_ell_internal *cmp_ell = (struct rt_ell_internal *)cmpintern.idb_ptr;

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    /* Set up a view matching the tor test for comparability */
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

    /* Set up rt_edit container */
    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;

    /* Direct access to the working ellipsoid */
    struct rt_ell_internal *edit_ell = (struct rt_ell_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_ELL_SCALE_A
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ELL_SCALE_A);

    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSCALE(cmp_ell->a, orig_ell->a, s->es_scale);

    rt_edit_process(s);

    if (ell_diff("ECMD_ELL_SCALE_A", cmp_ell, edit_ell))
bu_exit(1, "ERROR: ECMD_ELL_SCALE_A failed scaling ell parameter a\n");

    bu_log("ECMD_ELL_SCALE_A SUCCESS: original a value %g,%g,%g modified via es_scale to %g,%g,%g\n", V3ARGS(orig_ell->a), V3ARGS(edit_ell->a));

    /* Restore via e_inpara: set a to magnitude 4 (original) */
    s->e_inpara = 1;
    s->e_para[0] = 4;
    VMOVE(cmp_ell->a, orig_ell->a);

    rt_edit_process(s);

    if (ell_diff("ECMD_ELL_SCALE_A", cmp_ell, edit_ell))
bu_exit(1, "ERROR: ECMD_ELL_SCALE_A failed restoring ell parameter a\n");

    bu_log("ECMD_ELL_SCALE_A SUCCESS: a value restored via e_para to %g,%g,%g\n", V3ARGS(edit_ell->a));

    /* ================================================================
     * ECMD_ELL_SCALE_B
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ELL_SCALE_B);

    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSCALE(cmp_ell->b, orig_ell->b, s->es_scale);

    rt_edit_process(s);

    if (ell_diff("ECMD_ELL_SCALE_B", cmp_ell, edit_ell))
bu_exit(1, "ERROR: ECMD_ELL_SCALE_B failed scaling ell parameter b\n");

    bu_log("ECMD_ELL_SCALE_B SUCCESS: original b value %g,%g,%g modified via es_scale to %g,%g,%g\n", V3ARGS(orig_ell->b), V3ARGS(edit_ell->b));

    s->e_inpara = 1;
    s->e_para[0] = 3;
    VMOVE(cmp_ell->b, orig_ell->b);

    rt_edit_process(s);

    if (ell_diff("ECMD_ELL_SCALE_B", cmp_ell, edit_ell))
bu_exit(1, "ERROR: ECMD_ELL_SCALE_B failed restoring ell parameter b\n");

    bu_log("ECMD_ELL_SCALE_B SUCCESS: b value restored via e_para to %g,%g,%g\n", V3ARGS(edit_ell->b));

    /* ================================================================
     * ECMD_ELL_SCALE_C
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ELL_SCALE_C);

    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSCALE(cmp_ell->c, orig_ell->c, s->es_scale);

    rt_edit_process(s);

    if (ell_diff("ECMD_ELL_SCALE_C", cmp_ell, edit_ell))
bu_exit(1, "ERROR: ECMD_ELL_SCALE_C failed scaling ell parameter c\n");

    bu_log("ECMD_ELL_SCALE_C SUCCESS: original c value %g,%g,%g modified via es_scale to %g,%g,%g\n", V3ARGS(orig_ell->c), V3ARGS(edit_ell->c));

    s->e_inpara = 1;
    s->e_para[0] = 2;
    VMOVE(cmp_ell->c, orig_ell->c);

    rt_edit_process(s);

    if (ell_diff("ECMD_ELL_SCALE_C", cmp_ell, edit_ell))
bu_exit(1, "ERROR: ECMD_ELL_SCALE_C failed restoring ell parameter c\n");

    bu_log("ECMD_ELL_SCALE_C SUCCESS: c value restored via e_para to %g,%g,%g\n", V3ARGS(edit_ell->c));

    /* ================================================================
     * ECMD_ELL_SCALE_ABC
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ELL_SCALE_ABC);

    s->e_inpara = 0;
    s->es_scale = 2.0;

    {
/* a*2 = (8,0,0), ma=8; b *= (8/3); c *= (8/2) */
vect_t a_new, b_new, c_new;
VSCALE(a_new, orig_ell->a, s->es_scale);
fastf_t ma = MAGNITUDE(a_new);
VSCALE(b_new, orig_ell->b, ma / MAGNITUDE(orig_ell->b));
VSCALE(c_new, orig_ell->c, ma / MAGNITUDE(orig_ell->c));
VMOVE(cmp_ell->a, a_new);
VMOVE(cmp_ell->b, b_new);
VMOVE(cmp_ell->c, c_new);
    }

    rt_edit_process(s);

    if (ell_diff("ECMD_ELL_SCALE_ABC", cmp_ell, edit_ell))
bu_exit(1, "ERROR: ECMD_ELL_SCALE_ABC failed scaling ell parameters a,b,c\n");

    bu_log("ECMD_ELL_SCALE_ABC SUCCESS: a=%g,%g,%g b=%g,%g,%g c=%g,%g,%g\n",
   V3ARGS(edit_ell->a), V3ARGS(edit_ell->b), V3ARGS(edit_ell->c));

    /* e_inpara: set a to magnitude 4 → all scale to 4 */
    s->e_inpara = 1;
    s->e_para[0] = 4;
    VSET(cmp_ell->a, 4, 0, 0);
    VSET(cmp_ell->b, 0, 4, 0);
    VSET(cmp_ell->c, 0, 0, 4);

    rt_edit_process(s);

    if (ell_diff("ECMD_ELL_SCALE_ABC", cmp_ell, edit_ell))
bu_exit(1, "ERROR: ECMD_ELL_SCALE_ABC failed resizing ell parameters a,b,c\n");

    bu_log("ECMD_ELL_SCALE_ABC SUCCESS: a=%g,%g,%g b=%g,%g,%g c=%g,%g,%g via e_para\n",
   V3ARGS(edit_ell->a), V3ARGS(edit_ell->b), V3ARGS(edit_ell->c));

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);

    s->e_inpara = 0;
    s->es_scale = 3.0;

    /* Uniform scale about keypoint (=v): v stays, a/b/c scale by 3 */
    VMOVE(cmp_ell->v, orig_ell->v);
    VSCALE(cmp_ell->a, orig_ell->a, s->es_scale);
    VSCALE(cmp_ell->b, orig_ell->b, s->es_scale);
    VSCALE(cmp_ell->c, orig_ell->c, s->es_scale);

    rt_edit_process(s);

    if (ell_diff("RT_PARAMS_EDIT_SCALE", cmp_ell, edit_ell))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed scaling ell\n");

    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: a=%g,%g,%g b=%g,%g,%g c=%g,%g,%g\n",
   V3ARGS(edit_ell->a), V3ARGS(edit_ell->b), V3ARGS(edit_ell->c));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);

    s->e_inpara = 1;
    VSET(s->e_para, 20, 55, 40);
    VMOVE(cmp_ell->v, s->e_para);   /* v moves to e_para */
    VMOVE(cmp_ell->a, orig_ell->a);
    VMOVE(cmp_ell->b, orig_ell->b);
    VMOVE(cmp_ell->c, orig_ell->c);

    rt_edit_process(s);

    if (ell_diff("RT_PARAMS_EDIT_TRANS", cmp_ell, edit_ell))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed translating ell\n");

    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_ell->v), V3ARGS(edit_ell->v));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT — rotate about view center ('v')
     *   With gv_aet=(45,35,0) the view center maps to model origin (0,0,0),
     *   so v_new = R*(10,5,20) — same result as model-center rotation.
     *   a/b/c always rotate by R regardless of which center is used.
     * Expected values computed analytically from bn_mat_angles(5,5,5).
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);

    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'v';

    VSET(cmp_ell->v, 11.23303317584687910, 4.16614044477699519, 19.53105832935626651);
    VSET(cmp_ell->a,  3.96961550602441626, 0.37756522713653790, -0.31559029387459503);
    VSET(cmp_ell->b, -0.26047226650039551, 2.97522549216297172,  0.28317392035240341);
    VSET(cmp_ell->c,  0.17431148549531633,-0.17364817766693033,  1.98480775301220813);

    rt_edit_process(s);

    if (ell_diff("RT_PARAMS_EDIT_ROT (v)", cmp_ell, edit_ell))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(v) failed rotating ell\n");

    bu_log("RT_PARAMS_EDIT_ROT(v) SUCCESS: v=%g,%g,%g a=%g,%g,%g\n",
   V3ARGS(edit_ell->v), V3ARGS(edit_ell->a));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT — rotate about eye ('e')
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);

    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'e';

    VSET(cmp_ell->v, 11.40534719696836596, 4.16282380897874571, 19.36178344500957493);
    VSET(cmp_ell->a,  3.96961550602441626, 0.37756522713653790, -0.31559029387459503);
    VSET(cmp_ell->b, -0.26047226650039551, 2.97522549216297172,  0.28317392035240341);
    VSET(cmp_ell->c,  0.17431148549531633,-0.17364817766693033,  1.98480775301220813);

    rt_edit_process(s);

    if (ell_diff("RT_PARAMS_EDIT_ROT (e)", cmp_ell, edit_ell))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(e) failed rotating ell\n");

    bu_log("RT_PARAMS_EDIT_ROT(e) SUCCESS: v=%g,%g,%g a=%g,%g,%g\n",
   V3ARGS(edit_ell->v), V3ARGS(edit_ell->a));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT — rotate about model center ('m')
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);

    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'm';

    VSET(cmp_ell->v, 11.23303317584687910, 4.16614044477699519, 19.53105832935626651);
    VSET(cmp_ell->a,  3.96961550602441626, 0.37756522713653790, -0.31559029387459503);
    VSET(cmp_ell->b, -0.26047226650039551, 2.97522549216297172,  0.28317392035240341);
    VSET(cmp_ell->c,  0.17431148549531633,-0.17364817766693033,  1.98480775301220813);

    rt_edit_process(s);

    if (ell_diff("RT_PARAMS_EDIT_ROT (m)", cmp_ell, edit_ell))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(m) failed rotating ell\n");

    bu_log("RT_PARAMS_EDIT_ROT(m) SUCCESS: v=%g,%g,%g a=%g,%g,%g\n",
   V3ARGS(edit_ell->v), V3ARGS(edit_ell->a));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT — rotate about keypoint ('k')
     *   Keypoint = v = (10,5,20), so v stays put.
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);

    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_ell->v);

    VMOVE(cmp_ell->v, orig_ell->v);   /* v stays */
    VSET(cmp_ell->a,  3.96961550602441626, 0.37756522713653790, -0.31559029387459503);
    VSET(cmp_ell->b, -0.26047226650039551, 2.97522549216297172,  0.28317392035240341);
    VSET(cmp_ell->c,  0.17431148549531633,-0.17364817766693033,  1.98480775301220813);

    rt_edit_process(s);

    if (ell_diff("RT_PARAMS_EDIT_ROT (k)", cmp_ell, edit_ell))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed rotating ell\n");

    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: v stays %g,%g,%g  a=%g,%g,%g\n",
   V3ARGS(edit_ell->v), V3ARGS(edit_ell->a));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT — rotate about keypoint, mv_context=0
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    MAT_IDN(s->acc_rot_sol);
    s->mv_context = 0;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);

    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_ell->v);

    VMOVE(cmp_ell->v, orig_ell->v);   /* v stays (rotation is about keypoint=v) */
    VSET(cmp_ell->a,  3.96961550602441626, 0.37756522713653790, -0.31559029387459503);
    VSET(cmp_ell->b, -0.26047226650039551, 2.97522549216297172,  0.28317392035240341);
    VSET(cmp_ell->c,  0.17431148549531633,-0.17364817766693033,  1.98480775301220813);

    rt_edit_process(s);

    if (ell_diff("RT_PARAMS_EDIT_ROT (k) mv_context=0", cmp_ell, edit_ell))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) mv_context=0 failed rotating ell\n");

    bu_log("RT_PARAMS_EDIT_ROT(k) mv_context=0 SUCCESS: a=%g,%g,%g\n", V3ARGS(edit_ell->a));
    s->mv_context = 1;

    /* ================================================================
     * ECMD_ELL_SCALE_A - XY mouse-driven scale
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_ELL_SCALE_A);

    {
int xpos = 1372;
int ypos = 1383;
mousevec[X] = xpos * INV_BV;
mousevec[Y] = ypos * INV_BV;
mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
bu_exit(1, "ERROR: ECMD_ELL_SCALE_A (xy) failed ft_edit_xy call: %s\n", bu_vls_cstr(s->log_str));

    /* es_scale = 1.0 + 0.25 * |ypos * INV_BV| = 1 + 0.25*0.67529... = 1.16882...
     * a_new = a * es_scale = (4*1.16882..., 0, 0) */
    VMOVE(cmp_ell->v, orig_ell->v);
    VMOVE(cmp_ell->b, orig_ell->b);
    VMOVE(cmp_ell->c, orig_ell->c);
    VSET(cmp_ell->a, 4.675292968750000, 0, 0);

    rt_edit_process(s);

    if (ell_diff("ECMD_ELL_SCALE_A (xy)", cmp_ell, edit_ell))
bu_exit(1, "ERROR: ECMD_ELL_SCALE_A(xy) failed scaling ell a param\n");

    bu_log("ECMD_ELL_SCALE_A(xy) SUCCESS: original a value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_ell->a), V3ARGS(edit_ell->a));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS - XY mouse-driven translation
     *   Uses same v=(10,5,20) as the tor test, same view, same
     *   mousevec → same expected result.
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig_ell->v);

    {
int xpos = 1482;
int ypos = 762;
mousevec[X] = xpos * INV_BV;
mousevec[Y] = ypos * INV_BV;
mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS (xy) failed ft_edit_xy call: %s\n", bu_vls_cstr(s->log_str));

    VSET(cmp_ell->v, -12.61323935991339340, 24.90340037137704243, 22.73653941175349047);
    VMOVE(cmp_ell->a, orig_ell->a);
    VMOVE(cmp_ell->b, orig_ell->b);
    VMOVE(cmp_ell->c, orig_ell->c);

    rt_edit_process(s);

    if (ell_diff("RT_PARAMS_EDIT_TRANS (xy)", cmp_ell, edit_ell))
bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) failed translating ell\n");

    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: v modified to %g,%g,%g\n", V3ARGS(edit_ell->v));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT - XY mouse-driven rotation
     *   The XY rotation code path in edit_generic_xy (and rt_edit_ell_edit_xy)
     *   currently returns BRLCAD_ERROR with a TODO message — this is
     *   expected behavior until the XY rotation is implemented.
     *   Test that the call returns an error.
     * ================================================================*/
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);

    {
int xpos = 100;
int ypos = -50;
mousevec[X] = xpos * INV_BV;
mousevec[Y] = ypos * INV_BV;
mousevec[Z] = 0;
    }

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
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
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
    ell_reset(s, edit_ell, orig_ell, cmp_ell);
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
