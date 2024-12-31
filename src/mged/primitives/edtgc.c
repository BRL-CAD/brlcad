/*                         E D T G C . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2024 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/primitives/edtgc.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../mged.h"
#include "../sedit.h"
#include "../mged_dm.h"
#include "./edtgc.h"

static void
tgc_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;
    if (arg == MENU_TGC_ROT_H)
	es_edflag = ECMD_TGC_ROT_H;
    if (arg == MENU_TGC_ROT_AB)
	es_edflag = ECMD_TGC_ROT_AB;
    if (arg == MENU_TGC_MV_H)
	es_edflag = ECMD_TGC_MV_H;
    if (arg == MENU_TGC_MV_HH)
	es_edflag = ECMD_TGC_MV_HH;

    set_e_axes_pos(s, 1);
}

struct menu_item tgc_menu[] = {
    { "TGC MENU", NULL, 0 },
    { "Set H",	tgc_ed, MENU_TGC_SCALE_H },
    { "Set H (move V)", tgc_ed, MENU_TGC_SCALE_H_V },
    { "Set H (adj C,D)",	tgc_ed, MENU_TGC_SCALE_H_CD },
    { "Set H (move V, adj A,B)", tgc_ed, MENU_TGC_SCALE_H_V_AB },
    { "Set A",	tgc_ed, MENU_TGC_SCALE_A },
    { "Set B",	tgc_ed, MENU_TGC_SCALE_B },
    { "Set C",	tgc_ed, MENU_TGC_SCALE_C },
    { "Set D",	tgc_ed, MENU_TGC_SCALE_D },
    { "Set A,B",	tgc_ed, MENU_TGC_SCALE_AB },
    { "Set C,D",	tgc_ed, MENU_TGC_SCALE_CD },
    { "Set A,B,C,D", tgc_ed, MENU_TGC_SCALE_ABCD },
    { "Rotate H",	tgc_ed, MENU_TGC_ROT_H },
    { "Rotate AxB",	tgc_ed, MENU_TGC_ROT_AB },
    { "Move End H(rt)", tgc_ed, MENU_TGC_MV_H },
    { "Move End H", tgc_ed, MENU_TGC_MV_HH },
    { "", NULL, 0 }
};

void
mged_tgc_e_axes_pos(
	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol))
{
    if (es_edflag == ECMD_TGC_MV_H ||
	    es_edflag == ECMD_TGC_MV_HH) {
	struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)ip->idb_ptr;
	point_t tgc_v;
	vect_t tgc_h;

	MAT4X3PNT(tgc_v, es_mat, tgc->v);
	MAT4X3VEC(tgc_h, es_mat, tgc->h);
	VADD2(curr_e_axes_pos, tgc_h, tgc_v);
    } else {
	VMOVE(curr_e_axes_pos, es_keypoint);
    }
}

/* scale height vector */
void
menu_tgc_scale_h(struct mged_state *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->h);
    }
    VSCALE(tgc->h, tgc->h, s->edit_state.es_scale);
}

/* scale height vector (but move V) */
void
menu_tgc_scale_h_v(struct mged_state *s)
{
    point_t old_top;

    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->h);
    }
    VADD2(old_top, tgc->v, tgc->h);
    VSCALE(tgc->h, tgc->h, s->edit_state.es_scale);
    VSUB2(tgc->v, old_top, tgc->h);
}

void
menu_tgc_scale_h_cd(struct mged_state *s)
{
    vect_t vec1, vec2;
    vect_t c, d;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

    RT_TGC_CK_MAGIC(tgc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->h);
    }

    /* calculate new c */
    VSUB2(vec1, tgc->a, tgc->c);
    VSCALE(vec2, vec1, 1-s->edit_state.es_scale);
    VADD2(c, tgc->c, vec2);

    /* calculate new d */
    VSUB2(vec1, tgc->b, tgc->d);
    VSCALE(vec2, vec1, 1-s->edit_state.es_scale);
    VADD2(d, tgc->d, vec2);

    if (0 <= VDOT(tgc->c, c) &&
	    0 <= VDOT(tgc->d, d) &&
	    !ZERO(MAGNITUDE(c)) &&
	    !ZERO(MAGNITUDE(d))) {
	/* adjust c, d and h */
	VMOVE(tgc->c, c);
	VMOVE(tgc->d, d);
	VSCALE(tgc->h, tgc->h, s->edit_state.es_scale);
    }
}

void
menu_tgc_scale_h_v_ab(struct mged_state *s)
{
    vect_t vec1, vec2;
    vect_t a, b;
    point_t old_top;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

    RT_TGC_CK_MAGIC(tgc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->h);
    }

    /* calculate new a */
    VSUB2(vec1, tgc->c, tgc->a);
    VSCALE(vec2, vec1, 1-s->edit_state.es_scale);
    VADD2(a, tgc->a, vec2);

    /* calculate new b */
    VSUB2(vec1, tgc->d, tgc->b);
    VSCALE(vec2, vec1, 1-s->edit_state.es_scale);
    VADD2(b, tgc->b, vec2);

    if (0 <= VDOT(tgc->a, a) &&
	    0 <= VDOT(tgc->b, b) &&
	    !ZERO(MAGNITUDE(a)) &&
	    !ZERO(MAGNITUDE(b))) {
	/* adjust a, b, v and h */
	VMOVE(tgc->a, a);
	VMOVE(tgc->b, b);
	VADD2(old_top, tgc->v, tgc->h);
	VSCALE(tgc->h, tgc->h, s->edit_state.es_scale);
	VSUB2(tgc->v, old_top, tgc->h);
    }
}

/* scale vector A */
void
menu_tgc_scale_a(struct mged_state *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->a);
    }
    VSCALE(tgc->a, tgc->a, s->edit_state.es_scale);
}

/* scale vector B */
void
menu_tgc_scale_b(struct mged_state *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->b);
    }
    VSCALE(tgc->b, tgc->b, s->edit_state.es_scale);
}

/* TGC: scale ratio "c" */
void
menu_tgc_scale_c(struct mged_state *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->c);
    }
    VSCALE(tgc->c, tgc->c, s->edit_state.es_scale);
}

/* scale d for tgc */
void
menu_tgc_scale_d(struct mged_state *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->d);
    }
    VSCALE(tgc->d, tgc->d, s->edit_state.es_scale);
}

void
menu_tgc_scale_ab(struct mged_state *s)
{
    fastf_t ma, mb;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->a);
    }
    VSCALE(tgc->a, tgc->a, s->edit_state.es_scale);
    ma = MAGNITUDE(tgc->a);
    mb = MAGNITUDE(tgc->b);
    VSCALE(tgc->b, tgc->b, ma/mb);
}

/* scale C and D of tgc */
void
menu_tgc_scale_cd(struct mged_state *s)
{
    fastf_t ma, mb;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->c);
    }
    VSCALE(tgc->c, tgc->c, s->edit_state.es_scale);
    ma = MAGNITUDE(tgc->c);
    mb = MAGNITUDE(tgc->d);
    VSCALE(tgc->d, tgc->d, ma/mb);
}

/* scale A, B, C, and D of tgc */
void
menu_tgc_scale_abcd(struct mged_state *s)
{
    fastf_t ma, mb;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(tgc->a);
    }
    VSCALE(tgc->a, tgc->a, s->edit_state.es_scale);
    ma = MAGNITUDE(tgc->a);
    mb = MAGNITUDE(tgc->b);
    VSCALE(tgc->b, tgc->b, ma/mb);
    mb = MAGNITUDE(tgc->c);
    VSCALE(tgc->c, tgc->c, ma/mb);
    mb = MAGNITUDE(tgc->d);
    VSCALE(tgc->d, tgc->d, ma/mb);
}

/*
 * Move end of H of tgc, keeping plates perpendicular
 * to H vector.
 */
void
ecmd_tgc_mv_h(struct mged_state *s)
{
    float la, lb, lc, ld;	/* TGC: length of vectors */
    vect_t work;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

    RT_TGC_CK_MAGIC(tgc);
    if (inpara) {
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model coordinates */
	    MAT4X3PNT(work, es_invmat, es_para);
	    VSUB2(tgc->h, work, tgc->v);
	} else {
	    VSUB2(tgc->h, es_para, tgc->v);
	}
    }

    /* check for zero H vector */
    if (MAGNITUDE(tgc->h) <= SQRT_SMALL_FASTF) {
	Tcl_AppendResult(s->interp, "Zero H vector not allowed, resetting to +Z\n",
		(char *)NULL);
	mged_print_result(s, TCL_ERROR);
	VSET(tgc->h, 0.0, 0.0, 1.0);
	return;
    }

    /* have new height vector -- redefine rest of tgc */
    la = MAGNITUDE(tgc->a);
    lb = MAGNITUDE(tgc->b);
    lc = MAGNITUDE(tgc->c);
    ld = MAGNITUDE(tgc->d);

    /* find 2 perpendicular vectors normal to H for new A, B */
    bn_vec_perp(tgc->b, tgc->h);
    VCROSS(tgc->a, tgc->b, tgc->h);
    VUNITIZE(tgc->a);
    VUNITIZE(tgc->b);

    /* Create new C, D from unit length A, B, with previous len */
    VSCALE(tgc->c, tgc->a, lc);
    VSCALE(tgc->d, tgc->b, ld);

    /* Restore original vector lengths to A, B */
    VSCALE(tgc->a, tgc->a, la);
    VSCALE(tgc->b, tgc->b, lb);
}

/* Move end of H of tgc - leave ends alone */
void
ecmd_tgc_mv_hh(struct mged_state *s)
{
    vect_t work;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

    RT_TGC_CK_MAGIC(tgc);
    if (inpara) {
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model coordinates */
	    MAT4X3PNT(work, es_invmat, es_para);
	    VSUB2(tgc->h, work, tgc->v);
	} else {
	    VSUB2(tgc->h, es_para, tgc->v);
	}
    }

    /* check for zero H vector */
    if (MAGNITUDE(tgc->h) <= SQRT_SMALL_FASTF) {
	Tcl_AppendResult(s->interp, "Zero H vector not allowed, resetting to +Z\n",
		(char *)NULL);
	mged_print_result(s, TCL_ERROR);
	VSET(tgc->h, 0.0, 0.0, 1.0);
	return;
    }
}

/* rotate height vector */
void
ecmd_tgc_rot_h(struct mged_state *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

    mat_t mat;
    mat_t mat1;
    mat_t edit;

    RT_TGC_CK_MAGIC(tgc);
    if (inpara) {
	static mat_t invsolr;
	/*
	 * Keyboard parameters:  absolute x, y, z rotations,
	 * in degrees.  First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, acc_rot_sol);

	/* Build completely new rotation change */
	MAT_IDN(modelchanges);
	bn_mat_angles(modelchanges,
		es_para[0],
		es_para[1],
		es_para[2]);
	/* Borrow incr_change matrix here */
	bn_mat_mul(incr_change, modelchanges, invsolr);
	MAT_COPY(acc_rot_sol, modelchanges);

	/* Apply new rotation to solid */
	/* Clear out solid rotation */
	MAT_IDN(modelchanges);
    } else {
	/* Apply incremental changes already in incr_change */
    }

    if (mged_variables->mv_context) {
	/* calculate rotations about keypoint */
	bn_mat_xform_about_pnt(edit, incr_change, es_keypoint);

	/* We want our final matrix (mat) to xform the original solid
	 * to the position of this instance of the solid, perform the
	 * current edit operations, then xform back.
	 * mat = es_invmat * edit * es_mat
	 */
	bn_mat_mul(mat1, edit, es_mat);
	bn_mat_mul(mat, es_invmat, mat1);
	MAT4X3VEC(tgc->h, mat, tgc->h);
    } else {
	MAT4X3VEC(tgc->h, incr_change, tgc->h);
    }

    MAT_IDN(incr_change);
}

/* rotate surfaces AxB and CxD (tgc) */
void
ecmd_tgc_rot_ab(struct mged_state *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

    mat_t mat;
    mat_t mat1;
    mat_t edit;

    RT_TGC_CK_MAGIC(tgc);
    if (inpara) {
	static mat_t invsolr;
	/*
	 * Keyboard parameters:  absolute x, y, z rotations,
	 * in degrees.  First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, acc_rot_sol);

	/* Build completely new rotation change */
	MAT_IDN(modelchanges);
	bn_mat_angles(modelchanges,
		es_para[0],
		es_para[1],
		es_para[2]);
	/* Borrow incr_change matrix here */
	bn_mat_mul(incr_change, modelchanges, invsolr);
	MAT_COPY(acc_rot_sol, modelchanges);

	/* Apply new rotation to solid */
	/* Clear out solid rotation */
	MAT_IDN(modelchanges);
    } else {
	/* Apply incremental changes already in incr_change */
    }

    if (mged_variables->mv_context) {
	/* calculate rotations about keypoint */
	bn_mat_xform_about_pnt(edit, incr_change, es_keypoint);

	/* We want our final matrix (mat) to xform the original solid
	 * to the position of this instance of the solid, perform the
	 * current edit operations, then xform back.
	 * mat = es_invmat * edit * es_mat
	 */
	bn_mat_mul(mat1, edit, es_mat);
	bn_mat_mul(mat, es_invmat, mat1);
	MAT4X3VEC(tgc->a, mat, tgc->a);
	MAT4X3VEC(tgc->b, mat, tgc->b);
	MAT4X3VEC(tgc->c, mat, tgc->c);
	MAT4X3VEC(tgc->d, mat, tgc->d);
    } else {
	MAT4X3VEC(tgc->a, incr_change, tgc->a);
	MAT4X3VEC(tgc->b, incr_change, tgc->b);
	MAT4X3VEC(tgc->c, incr_change, tgc->c);
	MAT4X3VEC(tgc->d, incr_change, tgc->d);
    }
    MAT_IDN(incr_change);
}

/* Use mouse to change location of point V+H */
void
ecmd_tgc_mv_h_mousevec(struct mged_state *s, const vect_t mousevec)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);
    vect_t pos_view = VINIT_ZERO;
    vect_t tr_temp = VINIT_ZERO;
    vect_t temp = VINIT_ZERO;

    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    /* Do NOT change pos_view[Z] ! */
    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
    MAT4X3PNT(tr_temp, es_invmat, temp);
    VSUB2(tgc->h, tr_temp, tgc->v);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
