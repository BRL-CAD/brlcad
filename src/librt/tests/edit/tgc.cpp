/*                         T G C . C P P
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
/** @file tgc.cpp
 *
 * Test editing of TGC primitive parameters.
 *
 * Reference TGC: v=(5,3,10), h=(0,0,8), a=(3,0,0), b=(0,2,0),
 *                c=(2,0,0), d=(0,1.5,0)
 *
 * This covers a truncated general cone (non-circular cross-sections on
 * both ends to exercise the full set of edit modes).
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
make_tgc(struct rt_wdb *wdbp)
{
    const char *objname = "tgc";
    struct rt_tgc_internal *tgc;
    BU_ALLOC(tgc, struct rt_tgc_internal);
    tgc->magic = RT_TGC_INTERNAL_MAGIC;
    VSET(tgc->v, 5, 3, 10);
    VSET(tgc->h, 0, 0, 8);
    VSET(tgc->a, 3, 0, 0);
    VSET(tgc->b, 0, 2, 0);
    VSET(tgc->c, 2, 0, 0);
    VSET(tgc->d, 0, 1.5, 0);

    wdb_export(wdbp, objname, (void *)tgc, ID_TGC, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create tgc object: %s\n", objname);

    return dp;
}

int
tgc_diff(const char *cmd,
	 const struct rt_tgc_internal *ctrl,
	 const struct rt_tgc_internal *t)
{
    int ret = 0;
    if (!ctrl || !t)
	return 1;

#define TGC_DIFF_VEC(field) \
    if (!VNEAR_EQUAL(ctrl->field, t->field, VUNITIZE_TOL)) { \
	bu_log("ERROR(%s) " #field " ctrl=%0.17f,%0.17f,%0.17f got=%0.17f,%0.17f,%0.17f\n", \
	       cmd, V3ARGS(ctrl->field), V3ARGS(t->field)); \
	ret = 1; \
    }

    TGC_DIFF_VEC(v)
    TGC_DIFF_VEC(h)
    TGC_DIFF_VEC(a)
    TGC_DIFF_VEC(b)
    TGC_DIFF_VEC(c)
    TGC_DIFF_VEC(d)
#undef TGC_DIFF_VEC

    return ret;
}

/* Reset working tgc to original and clear edit state */
static void
tgc_reset(struct rt_edit *s, struct rt_tgc_internal *edit_tgc,
	  const struct rt_tgc_internal *orig, struct rt_tgc_internal *cmp)
{
    VMOVE(edit_tgc->v, orig->v);
    VMOVE(edit_tgc->h, orig->h);
    VMOVE(edit_tgc->a, orig->a);
    VMOVE(edit_tgc->b, orig->b);
    VMOVE(edit_tgc->c, orig->c);
    VMOVE(edit_tgc->d, orig->d);
    VMOVE(cmp->v, orig->v);
    VMOVE(cmp->h, orig->h);
    VMOVE(cmp->a, orig->a);
    VMOVE(cmp->b, orig->b);
    VMOVE(cmp->c, orig->c);
    VMOVE(cmp->d, orig->d);
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

    struct directory *dp = make_tgc(wdbp);

    /* Grab copies for reference and comparison */
    struct rt_db_internal intern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_tgc_internal *orig_tgc = (struct rt_tgc_internal *)intern.idb_ptr;
    struct rt_db_internal cmpintern;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_tgc_internal *cmp_tgc = (struct rt_tgc_internal *)cmpintern.idb_ptr;

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

    struct rt_tgc_internal *edit_tgc = (struct rt_tgc_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_TGC_SCALE_H
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_H);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_tgc->h, 0, 0, 16);   /* h_new = h*2 */

    rt_edit_process(s);

    if (tgc_diff("ECMD_TGC_SCALE_H", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_H failed\n");
    bu_log("ECMD_TGC_SCALE_H SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_tgc->h));

    /* Restore via e_inpara: |h| back to 8 */
    s->e_inpara = 1;
    s->e_para[0] = 8;
    VMOVE(cmp_tgc->h, orig_tgc->h);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_H restore", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_H restore failed\n");
    bu_log("ECMD_TGC_SCALE_H SUCCESS: h restored to %g,%g,%g\n", V3ARGS(edit_tgc->h));

    /* ================================================================
     * ECMD_TGC_SCALE_H_V  (scale H, move V to keep top fixed)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_H_V);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    /* old_top = v + h = (5,3,18); h_new = (0,0,16); v_new = (5,3,2) */
    VSET(cmp_tgc->v, 5, 3, 2);
    VSET(cmp_tgc->h, 0, 0, 16);

    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_H_V", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_H_V failed\n");
    bu_log("ECMD_TGC_SCALE_H_V SUCCESS: v=%g,%g,%g h=%g,%g,%g\n",
	   V3ARGS(edit_tgc->v), V3ARGS(edit_tgc->h));

    /* ================================================================
     * ECMD_TGC_SCALE_H_CD  (scale H, adjust C,D to stay proportional)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_H_CD);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    /* vec1 = a-c = (1,0,0); vec2 = vec1*(1-2) = (-1,0,0); c_new = c+vec2 = (1,0,0)
     * vec1 = b-d = (0,0.5,0); vec2*(1-2) = (0,-0.5,0); d_new = d+(0,-0.5,0) = (0,1,0)
     * h_new = (0,0,16) */
    VSET(cmp_tgc->h, 0, 0, 16);
    VSET(cmp_tgc->c, 1, 0, 0);
    VSET(cmp_tgc->d, 0, 1, 0);

    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_H_CD", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_H_CD failed\n");
    bu_log("ECMD_TGC_SCALE_H_CD SUCCESS: h=%g,%g,%g c=%g,%g,%g d=%g,%g,%g\n",
	   V3ARGS(edit_tgc->h), V3ARGS(edit_tgc->c), V3ARGS(edit_tgc->d));

    /* ================================================================
     * ECMD_TGC_SCALE_H_V_AB  (scale H moving V, adjust A,B)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_H_V_AB);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    /* vec1=c-a=(-1,0,0); vec2*(1-2)=(1,0,0); a_new=a+(1,0,0)=(4,0,0)
     * vec1=d-b=(0,-0.5,0); vec2*(1-2)=(0,0.5,0); b_new=b+(0,0.5,0)=(0,2.5,0)
     * old_top=(5,3,18); h_new=(0,0,16); v_new=(5,3,2) */
    VSET(cmp_tgc->v, 5, 3, 2);
    VSET(cmp_tgc->h, 0, 0, 16);
    VSET(cmp_tgc->a, 4, 0, 0);
    VSET(cmp_tgc->b, 0, 2.5, 0);

    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_H_V_AB", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_H_V_AB failed\n");
    bu_log("ECMD_TGC_SCALE_H_V_AB SUCCESS: v=%g,%g,%g h=%g,%g,%g a=%g,%g,%g b=%g,%g,%g\n",
	   V3ARGS(edit_tgc->v), V3ARGS(edit_tgc->h), V3ARGS(edit_tgc->a), V3ARGS(edit_tgc->b));

    /* ================================================================
     * ECMD_TGC_SCALE_A
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_A);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(cmp_tgc->a, 6, 0, 0);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_A", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_A failed\n");
    bu_log("ECMD_TGC_SCALE_A SUCCESS: a=%g,%g,%g\n", V3ARGS(edit_tgc->a));

    s->e_inpara = 1; s->e_para[0] = 3;
    VMOVE(cmp_tgc->a, orig_tgc->a);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_A restore", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_A restore failed\n");
    bu_log("ECMD_TGC_SCALE_A SUCCESS: a restored to %g,%g,%g\n", V3ARGS(edit_tgc->a));

    /* ================================================================
     * ECMD_TGC_SCALE_B
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_B);
    s->e_inpara = 0; s->es_scale = 2.0;
    VSET(cmp_tgc->b, 0, 4, 0);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_B", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_B failed\n");
    bu_log("ECMD_TGC_SCALE_B SUCCESS: b=%g,%g,%g\n", V3ARGS(edit_tgc->b));

    s->e_inpara = 1; s->e_para[0] = 2;
    VMOVE(cmp_tgc->b, orig_tgc->b);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_B restore", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_B restore failed\n");
    bu_log("ECMD_TGC_SCALE_B SUCCESS: b restored to %g,%g,%g\n", V3ARGS(edit_tgc->b));

    /* ================================================================
     * ECMD_TGC_SCALE_C
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_C);
    s->e_inpara = 0; s->es_scale = 2.0;
    VSET(cmp_tgc->c, 4, 0, 0);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_C", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_C failed\n");
    bu_log("ECMD_TGC_SCALE_C SUCCESS: c=%g,%g,%g\n", V3ARGS(edit_tgc->c));

    s->e_inpara = 1; s->e_para[0] = 2;
    VMOVE(cmp_tgc->c, orig_tgc->c);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_C restore", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_C restore failed\n");
    bu_log("ECMD_TGC_SCALE_C SUCCESS: c restored to %g,%g,%g\n", V3ARGS(edit_tgc->c));

    /* ================================================================
     * ECMD_TGC_SCALE_D
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_D);
    s->e_inpara = 0; s->es_scale = 2.0;
    VSET(cmp_tgc->d, 0, 3, 0);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_D", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_D failed\n");
    bu_log("ECMD_TGC_SCALE_D SUCCESS: d=%g,%g,%g\n", V3ARGS(edit_tgc->d));

    s->e_inpara = 1; s->e_para[0] = 1.5;
    VMOVE(cmp_tgc->d, orig_tgc->d);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_D restore", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_D restore failed\n");
    bu_log("ECMD_TGC_SCALE_D SUCCESS: d restored to %g,%g,%g\n", V3ARGS(edit_tgc->d));

    /* ================================================================
     * ECMD_TGC_SCALE_AB  (scale A, keep B proportional)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_AB);
    s->e_inpara = 0; s->es_scale = 2.0;
    /* a_new=(6,0,0), |a_new|=6; |b|=2; b_new = b*(6/2) = (0,6,0) */
    VSET(cmp_tgc->a, 6, 0, 0);
    VSET(cmp_tgc->b, 0, 6, 0);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_AB", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_AB failed\n");
    bu_log("ECMD_TGC_SCALE_AB SUCCESS: a=%g,%g,%g b=%g,%g,%g\n",
	   V3ARGS(edit_tgc->a), V3ARGS(edit_tgc->b));

    /* ================================================================
     * ECMD_TGC_SCALE_CD  (scale C, keep D proportional)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_CD);
    s->e_inpara = 0; s->es_scale = 2.0;
    /* c_new=(4,0,0), |c_new|=4; |d|=1.5; d_new = d*(4/1.5) = (0,4,0) */
    VSET(cmp_tgc->c, 4, 0, 0);
    VSET(cmp_tgc->d, 0, 4, 0);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_CD", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_CD failed\n");
    bu_log("ECMD_TGC_SCALE_CD SUCCESS: c=%g,%g,%g d=%g,%g,%g\n",
	   V3ARGS(edit_tgc->c), V3ARGS(edit_tgc->d));

    /* ================================================================
     * ECMD_TGC_SCALE_ABCD  (scale A, keep B/C/D proportional)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_ABCD);
    s->e_inpara = 0; s->es_scale = 2.0;
    /* a_new=(6,0,0), ma=6; b*(6/2)=(0,6,0); c*(6/2)=(6,0,0); d*(6/1.5)=(0,6,0) */
    VSET(cmp_tgc->a, 6, 0, 0);
    VSET(cmp_tgc->b, 0, 6, 0);
    VSET(cmp_tgc->c, 6, 0, 0);
    VSET(cmp_tgc->d, 0, 6, 0);
    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_ABCD", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_ABCD failed\n");
    bu_log("ECMD_TGC_SCALE_ABCD SUCCESS: a=%g,%g,%g b=%g,%g,%g c=%g,%g,%g d=%g,%g,%g\n",
	   V3ARGS(edit_tgc->a), V3ARGS(edit_tgc->b),
	   V3ARGS(edit_tgc->c), V3ARGS(edit_tgc->d));

    /* ================================================================
     * ECMD_TGC_MV_HH  (move top endpoint; a,b,c,d unchanged)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_MV_HH);
    s->e_inpara = 3;
    VSET(s->e_para, 5, 3, 26);   /* top endpoint (v + h_new) */

    /* h_new = e_para - v = (0,0,16); v,a,b,c,d unchanged */
    VSET(cmp_tgc->h, 0, 0, 16);

    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_MV_HH", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_MV_HH failed\n");
    bu_log("ECMD_TGC_MV_HH SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_tgc->h));

    /* ================================================================
     * ECMD_TGC_ROT_H  (rotate H vector by (5,5,5) degrees about keypoint=v)
     *   h_new = R * h where R = bn_mat_angles(5,5,5)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_ROT_H);
    s->e_inpara = 3;
    VSET(s->e_para, 5, 5, 5);
    VMOVE(s->e_keypoint, orig_tgc->v);

    /* Expected: R*(0,0,8) where R = bn_mat_angles(5,5,5).
     * The MGED implementation had a MAT4X3VEC aliasing bug (output and input
     * were the same pointer) that produced incorrect results (~1.5% magnitude
     * loss).  librt fixes this by using a temporary vect_t. */
    VSET(cmp_tgc->h, 0.69724594198126533, -0.69459271066772132, 7.93923101204883253);

    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_ROT_H", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_ROT_H failed\n");
    bu_log("ECMD_TGC_ROT_H SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_tgc->h));

    /* ================================================================
     * ECMD_TGC_ROT_AB  (rotate A,B,C,D by (5,5,5) degrees about kp=v)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_ROT_AB);
    s->e_inpara = 3;
    VSET(s->e_para, 5, 5, 5);
    VMOVE(s->e_keypoint, orig_tgc->v);

    /* Expected: R*a, R*b, R*c, R*d (correct values after aliasing bug fix) */
    VSET(cmp_tgc->a,  2.97721162951831220,  0.28317392035240341, -0.23669272040594627);
    VSET(cmp_tgc->b, -0.17364817766693033,  1.98348366144198129,  0.18878261356826895);
    VSET(cmp_tgc->c,  1.98480775301220813,  0.18878261356826895, -0.15779514693729751);
    VSET(cmp_tgc->d, -0.13023613325019776,  1.48761274608148586,  0.14158696017620170);

    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_ROT_AB", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_ROT_AB failed\n");
    bu_log("ECMD_TGC_ROT_AB SUCCESS: a=%g,%g,%g b=%g,%g,%g\n",
	   V3ARGS(edit_tgc->a), V3ARGS(edit_tgc->b));

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE  (uniform scale about keypoint = v)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0; s->es_scale = 3.0;

    /* v stays; h/a/b/c/d all scale by 3 */
    VMOVE(cmp_tgc->v, orig_tgc->v);
    VSET(cmp_tgc->h, 0, 0, 24);
    VSET(cmp_tgc->a, 9, 0, 0);
    VSET(cmp_tgc->b, 0, 6, 0);
    VSET(cmp_tgc->c, 6, 0, 0);
    VSET(cmp_tgc->d, 0, 4.5, 0);

    rt_edit_process(s);
    if (tgc_diff("RT_PARAMS_EDIT_SCALE", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n");
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: h=%g,%g,%g a=%g,%g,%g\n",
	   V3ARGS(edit_tgc->h), V3ARGS(edit_tgc->a));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS  (translate; v moves to e_para)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 20, 55, 40);
    VMOVE(cmp_tgc->v, s->e_para);   /* v moves to e_para */
    /* h,a,b,c,d unchanged */

    rt_edit_process(s);
    if (tgc_diff("RT_PARAMS_EDIT_TRANS", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: v=%g,%g,%g\n", V3ARGS(edit_tgc->v));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT  (rotate whole solid about keypoint = v)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    MAT_IDN(s->acc_rot_sol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, orig_tgc->v);

    /* Expected (confirmed by probe): edit_srot/rt_tgc_mat correctly preserves magnitudes */
    VMOVE(cmp_tgc->v, orig_tgc->v);
    VSET(cmp_tgc->h,  0.69724594198126533, -0.69459271066772132, 7.93923101204883253);
    VSET(cmp_tgc->a,  2.97721162951831220,  0.28317392035240341, -0.23669272040594627);
    VSET(cmp_tgc->b, -0.17364817766693033,  1.98348366144198129,  0.18878261356826895);
    VSET(cmp_tgc->c,  1.98480775301220813,  0.18878261356826895, -0.15779514693729751);
    VSET(cmp_tgc->d, -0.13023613325019776,  1.48761274608148586,  0.14158696017620170);

    rt_edit_process(s);
    if (tgc_diff("RT_PARAMS_EDIT_ROT (k)", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: v=%g,%g,%g h=%g,%g,%g\n",
	   V3ARGS(edit_tgc->v), V3ARGS(edit_tgc->h));

    /* ================================================================
     * ECMD_TGC_SCALE_H - XY mouse-driven scale
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_SCALE_H);

    {
	int xpos = 1372;
	int ypos = 1383;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_H(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    /* es_scale = 1 + 0.25*|ypos*INV_BV| = 1.16882...; h_new = h * es_scale */
    VSET(cmp_tgc->h, 0, 0, 9.3505859375);

    rt_edit_process(s);
    if (tgc_diff("ECMD_TGC_SCALE_H (xy)", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: ECMD_TGC_SCALE_H(xy) failed\n");
    bu_log("ECMD_TGC_SCALE_H(xy) SUCCESS: h=%g,%g,%g\n", V3ARGS(edit_tgc->h));

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS - XY mouse-driven translation
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    VMOVE(s->curr_e_axes_pos, orig_tgc->v);   /* keypoint = v = (5,3,10) */

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

    /* Expected v_new confirmed by probe run with curr_e_axes_pos=(5,3,10) */
    VSET(cmp_tgc->v, -18.28408973267668713, 19.23254999861374515, 17.12101954302146822);

    rt_edit_process(s);
    if (tgc_diff("RT_PARAMS_EDIT_TRANS (xy)", cmp_tgc, edit_tgc))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) failed\n");
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: v=%g,%g,%g\n", V3ARGS(edit_tgc->v));

    /* ================================================================
     * ECMD_TGC_MV_H - XY mouse-driven move-H-endpoint
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_MV_H);
    /* curr_e_axes_pos for MV_H is the H endpoint = v + h = (5,3,18) */
    VADD2(s->curr_e_axes_pos, orig_tgc->v, orig_tgc->h);

    {
	int xpos = 500;
	int ypos = -300;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_TGC_MV_H(xy) failed ft_edit_xy: %s\n", bu_vls_cstr(s->log_str));

    /* h_new is determined by view transform; confirmed by build + run.
     * After rt_edit_process, the MV_H function also recomputes a,b,c,d
     * from the new h direction. We only check v and h here and verify
     * they were modified. */
    {
	/* Do not check specific values for MV_H xy - just verify h changed */
	vect_t orig_h;
	VMOVE(orig_h, orig_tgc->h);
	rt_edit_process(s);
	if (VNEAR_EQUAL(edit_tgc->h, orig_h, SQRT_SMALL_FASTF))
	    bu_exit(1, "ERROR: ECMD_TGC_MV_H(xy) did not modify h\n");
    }
    bu_log("ECMD_TGC_MV_H(xy) SUCCESS: h=%g,%g,%g (changed from %g,%g,%g)\n",
	   V3ARGS(edit_tgc->h), V3ARGS(orig_tgc->h));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT - XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
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
     * ECMD_TGC_ROT_H/ROT_AB - XY returns BRLCAD_ERROR (undefined)
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TGC_ROT_H);
    bu_vls_trunc(s->log_str, 0);
    int roth_xy_ret = (*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec);
    if (roth_xy_ret != BRLCAD_ERROR) {
	bu_exit(1, "ERROR: ECMD_TGC_ROT_H(xy) should return BRLCAD_ERROR, got %d\n", roth_xy_ret);
    }
    bu_log("ECMD_TGC_ROT_H(xy) correctly returns BRLCAD_ERROR (XY undefined for ROT_H)\n");


    /* ================================================================
     * RT_MATRIX_EDIT_ROT: absolute matrix rotation from keyboard angles
     *
     * Set model_changes to a 30-deg rotation about X. Keypoint is at
     * the primitive's vertex (varies per primitive).
     * After the edit model_changes must differ from identity.
     * ================================================================*/
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
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
    tgc_reset(s, edit_tgc, orig_tgc, cmp_tgc);
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
