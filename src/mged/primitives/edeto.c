/*                         E D E T O . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2025 United States Government as represented by
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
#include "./mged_functab.h"

#define ECMD_ETO_ROT_C		21016

#define MENU_ETO_R		21057
#define MENU_ETO_RD		21058
#define MENU_ETO_SCALE_C	21059
#define MENU_ETO_ROT_C		21060

static void
eto_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->s_edit->edit_menu = arg;
    mged_set_edflag(s, PSCALE);
    if (arg == MENU_ETO_ROT_C) {
	s->s_edit->edit_flag = ECMD_ETO_ROT_C;
	s->s_edit->solid_edit_rotate = 1;
	s->s_edit->solid_edit_translate = 0;
	s->s_edit->solid_edit_scale = 0;
	s->s_edit->solid_edit_pick = 0;
    }

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

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_eto_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_eto_internal *eto = (struct rt_eto_internal *)ip->idb_ptr;
    RT_ETO_CK_MAGIC(eto);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(eto->eto_V));
    bu_vls_printf(p, "Normal: %.9f %.9f %.9f\n", V3BASE2LOCAL(eto->eto_N));
    bu_vls_printf(p, "Semi-major axis: %.9f %.9f %.9f\n", V3BASE2LOCAL(eto->eto_C));
    bu_vls_printf(p, "Semi-minor length: %.9f\n", eto->eto_rd * base2local);
    bu_vls_printf(p, "Radius of rotation: %.9f\n", eto->eto_r * base2local);
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
mged_eto_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_eto_internal *eto = (struct rt_eto_internal *)ip->idb_ptr;
    RT_ETO_CK_MAGIC(eto);

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
    VSET(eto->eto_V, a, b, c);
    VSCALE(eto->eto_V, eto->eto_V, local2base);

    // Set up Normal line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(eto->eto_N, a, b, c);
    VUNITIZE(eto->eto_N);

    // Set up Semi-major axis line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(eto->eto_C, a, b, c);
    VSCALE(eto->eto_C, eto->eto_C, local2base);

    // Set up Semi-minor length line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    eto->eto_rd = a * local2base;

    // Set up Radius of rotation line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    eto->eto_r = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale radius 1 (r) of ETO */
void
menu_eto_r(struct mged_state *s)
{
    struct rt_eto_internal *eto =
	(struct rt_eto_internal *)s->s_edit->es_int.idb_ptr;
    fastf_t ch, cv, dh, newrad;
    vect_t Nu;

    RT_ETO_CK_MAGIC(eto);
    if (s->s_edit->e_inpara) {
	/* take s->s_edit->e_mat[15] (path scaling) into account */
	s->s_edit->e_para[0] *= s->s_edit->e_mat[15];
	newrad = s->s_edit->e_para[0];
    } else {
	newrad = eto->eto_r * s->s_edit->es_scale;
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
	(struct rt_eto_internal *)s->s_edit->es_int.idb_ptr;
    fastf_t dh, newrad, work;
    vect_t Nu;

    RT_ETO_CK_MAGIC(eto);
    if (s->s_edit->e_inpara) {
	/* take s->s_edit->e_mat[15] (path scaling) into account */
	s->s_edit->e_para[0] *= s->s_edit->e_mat[15];
	newrad = s->s_edit->e_para[0];
    } else {
	newrad = eto->eto_rd * s->s_edit->es_scale;
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
	(struct rt_eto_internal *)s->s_edit->es_int.idb_ptr;
    fastf_t ch, cv;
    vect_t Nu, Work;

    RT_ETO_CK_MAGIC(eto);
    if (s->s_edit->e_inpara) {
	/* take s->s_edit->e_mat[15] (path scaling) into account */
	s->s_edit->e_para[0] *= s->s_edit->e_mat[15];
	s->s_edit->es_scale = s->s_edit->e_para[0] / MAGNITUDE(eto->eto_C);
    }
    if (s->s_edit->es_scale * MAGNITUDE(eto->eto_C) >= eto->eto_rd) {
	VMOVE(Nu, eto->eto_N);
	VUNITIZE(Nu);
	VSCALE(Work, eto->eto_C, s->s_edit->es_scale);
	/* get horiz and vert comps of C and Rd */
	cv = VDOT(Work, Nu);
	ch = sqrt(VDOT(Work, Work) - cv * cv);
	if (ch <= eto->eto_r)
	    VMOVE(eto->eto_C, Work);
    }
}

/* rotate ellipse semi-major axis vector */
int
ecmd_eto_rot_c(struct mged_state *s)
{
    struct rt_eto_internal *eto =
	(struct rt_eto_internal *)s->s_edit->es_int.idb_ptr;
    mat_t mat;
    mat_t mat1;
    mat_t edit;

    RT_ETO_CK_MAGIC(eto);
    if (s->s_edit->e_inpara) {
	if (s->s_edit->e_inpara != 3) {
	    bu_vls_printf(s->s_edit->log_str, "ERROR: three arguments needed\n");
	    s->s_edit->e_inpara = 0;
	    return TCL_ERROR;
	}

	static mat_t invsolr;
	/*
	 * Keyboard parameters:  absolute x, y, z rotations,
	 * in degrees.  First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, s->s_edit->acc_rot_sol);

	/* Build completely new rotation change */
	MAT_IDN(s->s_edit->model_changes);
	bn_mat_angles(s->s_edit->model_changes,
		s->s_edit->e_para[0],
		s->s_edit->e_para[1],
		s->s_edit->e_para[2]);
	/* Borrow s->s_edit->incr_change matrix here */
	bn_mat_mul(s->s_edit->incr_change, s->s_edit->model_changes, invsolr);
	MAT_COPY(s->s_edit->acc_rot_sol, s->s_edit->model_changes);

	/* Apply new rotation to solid */
	/* Clear out solid rotation */
	MAT_IDN(s->s_edit->model_changes);
    } else {
	/* Apply incremental changes already in s->s_edit->incr_change */
    }

    if (s->s_edit->mv_context) {
	/* calculate rotations about keypoint */
	bn_mat_xform_about_pnt(edit, s->s_edit->incr_change, s->s_edit->e_keypoint);

	/* We want our final matrix (mat) to xform the original solid
	 * to the position of this instance of the solid, perform the
	 * current edit operations, then xform back.
	 * mat = s->s_edit->e_invmat * edit * s->s_edit->e_mat
	 */
	bn_mat_mul(mat1, edit, s->s_edit->e_mat);
	bn_mat_mul(mat, s->s_edit->e_invmat, mat1);

	MAT4X3VEC(eto->eto_C, mat, eto->eto_C);
    } else {
	MAT4X3VEC(eto->eto_C, s->s_edit->incr_change, eto->eto_C);
    }

    MAT_IDN(s->s_edit->incr_change);

    return 0;
}

static int
mged_eto_pscale(struct mged_state *s, int mode)
{
    if (s->s_edit->e_inpara > 1) {
	bu_vls_printf(s->s_edit->log_str, "ERROR: only one argument needed\n");
	s->s_edit->e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->s_edit->e_para[0] <= 0.0) {
	bu_vls_printf(s->s_edit->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->s_edit->e_inpara = 0;
	return TCL_ERROR;
    }

    /* must convert to base units */
    s->s_edit->e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit->e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit->e_para[2] *= s->dbip->dbi_local2base;

    switch (mode) {
	case MENU_ETO_R:
	    menu_eto_r(s);
	    break;
	case MENU_ETO_RD:
	    menu_eto_rd(s);
	    break;
	case MENU_ETO_SCALE_C:
	    menu_eto_scale_c(s);
	    break;
    };

    return 0;
}

int
mged_eto_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    return mged_generic_sscale(s, &s->s_edit->es_int);
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->s_edit->es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->s_edit->es_int);
	    break;
	case PSCALE:
	    return mged_eto_pscale(s, s->s_edit->edit_menu);
	case ECMD_ETO_ROT_C:
	    return ecmd_eto_rot_c(s);
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
