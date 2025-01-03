/*                         E D H Y P . C
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
/** @file mged/primitives/edhyp.c
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
#include "./edhyp.h"

static void
hyp_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    switch (arg) {
	case MENU_HYP_ROT_H:
	    es_edflag = ECMD_HYP_ROT_H;
	    break;
	default:
	    es_edflag = PSCALE;
	    break;
    }
    set_e_axes_pos(s, 1);
    return;
}
struct menu_item  hyp_menu[] = {
    { "HYP MENU", NULL, 0 },
    { "Set H", hyp_ed, MENU_HYP_H },
    { "Set A", hyp_ed, MENU_HYP_SCALE_A },
    { "Set B", hyp_ed, MENU_HYP_SCALE_B },
    { "Set c", hyp_ed, MENU_HYP_C },
    { "Rotate H", hyp_ed, MENU_HYP_ROT_H },
    { "", NULL, 0 }
};

struct menu_item *
mged_hyp_menu_item(const struct bn_tol *UNUSED(tol))
{
    return hyp_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_hyp_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_hyp_internal *hyp = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(hyp->hyp_Vi));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(hyp->hyp_Hi));
    bu_vls_printf(p, "Semi-major axis: %.9f %.9f %.9f\n", V3BASE2LOCAL(hyp->hyp_A));
    bu_vls_printf(p, "Semi-minor length: %.9f\n", hyp->hyp_b * base2local);
    bu_vls_printf(p, "Ratio of Neck to Base: %.9f\n", hyp->hyp_bnr);
}

#define read_params_line_incr \
    lc = (ln) ? (ln + lcj) : NULL; \
    if (!lc) { \
	bu_free(wc, "wc"); \
	return BRLCAD_ERROR; \
    } \
    ln = strchr(lc, tc); \
    if (ln) *ln = '\0'; \
    while (lc && strchr(lc, ':')) lc++

int
mged_hyp_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_hyp_internal *hyp = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp);

    if (!fc)
	return BRLCAD_ERROR;

    // We're getting the file contents as a string, so we need to split it up
    // to process lines. See https://stackoverflow.com/a/17983619

    // Figure out if we need to deal with Windows line endings
    const char *crpos = strchr(fc, '\r');
    int crlf = (crpos && crpos[1] == '\n') ? 1 : 0;
    char tc = (crlf) ? '\r' : '\n';
    // If we're CRLF jump ahead another character.
    int lcj = (crlf) ? 2 : 1;

    char *ln = NULL;
    char *wc = bu_strdup(fc);
    char *lc = wc;

    // Set up initial line (Vertex)
    ln = strchr(lc, tc);
    if (ln) *ln = '\0';

    // Trim off prefixes, if user left them in
    while (lc && strchr(lc, ':')) lc++;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(hyp->hyp_Vi, a, b, c);
    VSCALE(hyp->hyp_Vi, hyp->hyp_Vi, local2base);

    // Set up Height line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(hyp->hyp_Hi, a, b, c);
    VSCALE(hyp->hyp_Hi, hyp->hyp_Hi, local2base);

    // Set up Semi-major axis line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(hyp->hyp_A, a, b, c);
    VSCALE(hyp->hyp_A, hyp->hyp_A, local2base);

    // Set up Semi-minor length line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    hyp->hyp_b = a * local2base;

    // Set up Ratio of Neck to Base line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    hyp->hyp_bnr = a;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale height of HYP */
void
menu_hyp_h(struct mged_state *s)
{
    struct rt_hyp_internal *hyp =
	(struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

    RT_HYP_CK_MAGIC(hyp);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0];
    }
    VSCALE(hyp->hyp_Hi, hyp->hyp_Hi, s->edit_state.es_scale);
}

/* scale A vector of HYP */
void
menu_hyp_scale_a(struct mged_state *s)
{
    struct rt_hyp_internal *hyp =
	(struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

    RT_HYP_CK_MAGIC(hyp);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0];
    }
    VSCALE(hyp->hyp_A, hyp->hyp_A, s->edit_state.es_scale);
}

/* scale B vector of HYP */
void
menu_hyp_scale_b(struct mged_state *s)
{
    struct rt_hyp_internal *hyp =
	(struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

    RT_HYP_CK_MAGIC(hyp);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0];
    }
    hyp->hyp_b = hyp->hyp_b * s->edit_state.es_scale;
}

/* scale Neck to Base ratio of HYP */
void
menu_hyp_c(struct mged_state *s)
{
    struct rt_hyp_internal *hyp =
	(struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

    RT_HYP_CK_MAGIC(hyp);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0];
    }
    if (hyp->hyp_bnr * s->edit_state.es_scale <= 1.0) {
	hyp->hyp_bnr = hyp->hyp_bnr * s->edit_state.es_scale;
    }
}

/* rotate hyperboloid height vector */
void
ecmd_hyp_rot_h(struct mged_state *s)
{
    struct rt_hyp_internal *hyp =
	(struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

    mat_t mat;
    mat_t mat1;
    mat_t edit;

    RT_HYP_CK_MAGIC(hyp);
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

	MAT4X3VEC(hyp->hyp_Hi, mat, hyp->hyp_Hi);
    } else {
	MAT4X3VEC(hyp->hyp_Hi, incr_change, hyp->hyp_Hi);
    }

    MAT_IDN(incr_change);
}

int
mged_hyp_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case ECMD_HYP_ROT_H:
	    ecmd_hyp_rot_h(s);
	    break;
    }
    return 0;
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
