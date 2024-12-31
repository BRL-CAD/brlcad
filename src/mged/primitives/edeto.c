/*                         E D E T O . C
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
/** @file mged/primitives/edeto.c
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
#include "./edeto.h"

static void
eto_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    if (arg == MENU_ETO_ROT_C)
	es_edflag = ECMD_ETO_ROT_C;
    else
	es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item eto_menu[] = {
    { "ELL-TORUS MENU", NULL, 0 },
    { "Set r", eto_ed, MENU_ETO_R },
    { "Set D", eto_ed, MENU_ETO_RD },
    { "Set C", eto_ed, MENU_ETO_SCALE_C },
    { "Rotate C", eto_ed, MENU_ETO_ROT_C },
    { "", NULL, 0 }
};

struct menu_item *
mged_eto_menu_item(const struct bn_tol *UNUSED(tol))
{
    return eto_menu;
}

/* scale radius 1 (r) of ETO */
void
menu_eto_r(struct mged_state *s)
{
    struct rt_eto_internal *eto =
	(struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;
    fastf_t ch, cv, dh, newrad;
    vect_t Nu;

    RT_ETO_CK_MAGIC(eto);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	newrad = es_para[0];
    } else {
	newrad = eto->eto_r * s->edit_state.es_scale;
    }
    if (newrad < SMALL) newrad = 4*SMALL;
    VMOVE(Nu, eto->eto_N);
    VUNITIZE(Nu);
    /* get horiz and vert components of C and Rd */
    cv = VDOT(eto->eto_C, Nu);
    ch = sqrt(VDOT(eto->eto_C, eto->eto_C) - cv * cv);
    /* angle between C and Nu */
    dh = eto->eto_rd * cv / MAGNITUDE(eto->eto_C);
    /* make sure revolved ellipse doesn't overlap itself */
    if (ch <= newrad && dh <= newrad)
	eto->eto_r = newrad;
}

/* scale Rd, ellipse semi-minor axis length, of ETO */
void
menu_eto_rd(struct mged_state *s)
{
    struct rt_eto_internal *eto =
	(struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;
    fastf_t dh, newrad, work;
    vect_t Nu;

    RT_ETO_CK_MAGIC(eto);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	newrad = es_para[0];
    } else {
	newrad = eto->eto_rd * s->edit_state.es_scale;
    }
    if (newrad < SMALL) newrad = 4*SMALL;
    work = MAGNITUDE(eto->eto_C);
    if (newrad <= work) {
	VMOVE(Nu, eto->eto_N);
	VUNITIZE(Nu);
	dh = newrad * VDOT(eto->eto_C, Nu) / work;
	/* make sure revolved ellipse doesn't overlap itself */
	if (dh <= eto->eto_r)
	    eto->eto_rd = newrad;
    }
}

/* scale vector C */
void
menu_eto_scale_c(struct mged_state *s)
{
    struct rt_eto_internal *eto =
	(struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;
    fastf_t ch, cv;
    vect_t Nu, Work;

    RT_ETO_CK_MAGIC(eto);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(eto->eto_C);
    }
    if (s->edit_state.es_scale * MAGNITUDE(eto->eto_C) >= eto->eto_rd) {
	VMOVE(Nu, eto->eto_N);
	VUNITIZE(Nu);
	VSCALE(Work, eto->eto_C, s->edit_state.es_scale);
	/* get horiz and vert comps of C and Rd */
	cv = VDOT(Work, Nu);
	ch = sqrt(VDOT(Work, Work) - cv * cv);
	if (ch <= eto->eto_r)
	    VMOVE(eto->eto_C, Work);
    }
}

/* rotate ellipse semi-major axis vector */
void
ecmd_eto_rot_c(struct mged_state *s)
{
    struct rt_eto_internal *eto =
	(struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;
    mat_t mat;
    mat_t mat1;
    mat_t edit;
    
    RT_ETO_CK_MAGIC(eto);
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

	MAT4X3VEC(eto->eto_C, mat, eto->eto_C);
    } else {
	MAT4X3VEC(eto->eto_C, incr_change, eto->eto_C);
    }

    MAT_IDN(incr_change);
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
