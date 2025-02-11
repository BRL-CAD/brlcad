/*                         E D T G C . C
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
/** @file primitives/edtgc.c
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

#include "../edit_private.h"

#define ECMD_TGC_MV_H		2005
#define ECMD_TGC_MV_HH		2006
#define ECMD_TGC_MV_H_CD	2081 /* move end of tgc, while scaling CD */
#define ECMD_TGC_MV_H_V_AB	2082 /* move vertex end of tgc, while scaling AB */
#define ECMD_TGC_ROT_AB		2008
#define ECMD_TGC_ROT_H		2007
#define ECMD_TGC_SCALE_A	2029
#define ECMD_TGC_SCALE_AB	2033
#define ECMD_TGC_SCALE_ABCD	2035
#define ECMD_TGC_SCALE_B	2030
#define ECMD_TGC_SCALE_C	2031
#define ECMD_TGC_SCALE_CD	2034
#define ECMD_TGC_SCALE_D	2032
#define ECMD_TGC_SCALE_H	2027
#define ECMD_TGC_SCALE_H_CD	2111
#define ECMD_TGC_SCALE_H_V	2028
#define ECMD_TGC_SCALE_H_V_AB	2112

static void
tgc_ed(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    // Most of the commands are scale, so set those flags by default.  That
    // will handle most of the flag resetting as well, so we just need to zero
    // and set specific flags of interest in the other cases.
    rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_SCALE);

    s->edit_flag = arg;

    switch(arg) {
	case ECMD_TGC_MV_H:
	case ECMD_TGC_MV_HH:
	case ECMD_TGC_MV_H_CD:
	case ECMD_TGC_MV_H_V_AB:
	    s->solid_edit_scale = 0;
	    s->solid_edit_translate = 1;
	    break;
	case ECMD_TGC_ROT_AB:
	case ECMD_TGC_ROT_H:
	    s->solid_edit_scale = 0;
	    s->solid_edit_rotate = 1;
	    break;

    };

    // If we need to update edit axes positions, do so.
    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}

struct rt_solid_edit_menu_item tgc_menu[] = {
    { "TGC MENU", NULL, 0 },
    { "Set H",	tgc_ed, ECMD_TGC_SCALE_H },
    { "Set H (move V)", tgc_ed, ECMD_TGC_SCALE_H_V },
    { "Set H (adj C,D)",	tgc_ed, ECMD_TGC_SCALE_H_CD },
    { "Set H (move V, adj A,B)", tgc_ed, ECMD_TGC_SCALE_H_V_AB },
    { "Set A",	tgc_ed, ECMD_TGC_SCALE_A },
    { "Set B",	tgc_ed, ECMD_TGC_SCALE_B },
    { "Set C",	tgc_ed, ECMD_TGC_SCALE_C },
    { "Set D",	tgc_ed, ECMD_TGC_SCALE_D },
    { "Set A,B",	tgc_ed, ECMD_TGC_SCALE_AB },
    { "Set C,D",	tgc_ed, ECMD_TGC_SCALE_CD },
    { "Set A,B,C,D", tgc_ed, ECMD_TGC_SCALE_ABCD },
    { "Rotate H",	tgc_ed, ECMD_TGC_ROT_H },
    { "Rotate AxB",	tgc_ed, ECMD_TGC_ROT_AB },
    { "Move End H(rt)", tgc_ed, ECMD_TGC_MV_H },
    { "Move End H", tgc_ed, ECMD_TGC_MV_HH },
    { "", NULL, 0 }
};

struct rt_solid_edit_menu_item *
rt_solid_edit_tgc_menu_item(const struct bn_tol *UNUSED(tol))
{
    return tgc_menu;
}

void
rt_solid_edit_tgc_e_axes_pos(
	struct rt_solid_edit *s,
	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol))
{
    if (s->edit_flag == ECMD_TGC_MV_H ||
	    s->edit_flag == ECMD_TGC_MV_HH) {
	struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)ip->idb_ptr;
	point_t tgc_v;
	vect_t tgc_h;

	MAT4X3PNT(tgc_v, s->e_mat, tgc->v);
	MAT4X3VEC(tgc_h, s->e_mat, tgc->h);
	VADD2(s->curr_e_axes_pos, tgc_h, tgc_v);
    } else {
	VMOVE(s->curr_e_axes_pos, s->e_keypoint);
    }
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
rt_solid_edit_tgc_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(tgc->v));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(tgc->h));
    bu_vls_printf(p, "A: %.9f %.9f %.9f\n", V3BASE2LOCAL(tgc->a));
    bu_vls_printf(p, "B: %.9f %.9f %.9f\n", V3BASE2LOCAL(tgc->b));
    bu_vls_printf(p, "C: %.9f %.9f %.9f\n", V3BASE2LOCAL(tgc->c));
    bu_vls_printf(p, "D: %.9f %.9f %.9f\n", V3BASE2LOCAL(tgc->d));
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
rt_solid_edit_tgc_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

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
    VSET(tgc->v, a, b, c);
    VSCALE(tgc->v, tgc->v, local2base);

    // Set up Height line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(tgc->h, a, b, c);
    VSCALE(tgc->h, tgc->h, local2base);

    // Set up A line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(tgc->a, a, b, c);
    VSCALE(tgc->a, tgc->a, local2base);

    // Set up B line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(tgc->b, a, b, c);
    VSCALE(tgc->b, tgc->b, local2base);

    // Set up C line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(tgc->c, a, b, c);
    VSCALE(tgc->c, tgc->c, local2base);

    // Set up D line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(tgc->d, a, b, c);
    VSCALE(tgc->d, tgc->d, local2base);

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale height vector */
void
ecmd_tgc_scale_h(struct rt_solid_edit *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->h);
    }
    VSCALE(tgc->h, tgc->h, s->es_scale);
}

/* scale height vector (but move V) */
void
ecmd_tgc_scale_h_v(struct rt_solid_edit *s)
{
    point_t old_top;

    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->h);
    }
    VADD2(old_top, tgc->v, tgc->h);
    VSCALE(tgc->h, tgc->h, s->es_scale);
    VSUB2(tgc->v, old_top, tgc->h);
}

void
ecmd_tgc_scale_h_cd(struct rt_solid_edit *s)
{
    vect_t vec1, vec2;
    vect_t c, d;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;

    RT_TGC_CK_MAGIC(tgc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->h);
    }

    /* calculate new c */
    VSUB2(vec1, tgc->a, tgc->c);
    VSCALE(vec2, vec1, 1-s->es_scale);
    VADD2(c, tgc->c, vec2);

    /* calculate new d */
    VSUB2(vec1, tgc->b, tgc->d);
    VSCALE(vec2, vec1, 1-s->es_scale);
    VADD2(d, tgc->d, vec2);

    if (0 <= VDOT(tgc->c, c) &&
	    0 <= VDOT(tgc->d, d) &&
	    !ZERO(MAGNITUDE(c)) &&
	    !ZERO(MAGNITUDE(d))) {
	/* adjust c, d and h */
	VMOVE(tgc->c, c);
	VMOVE(tgc->d, d);
	VSCALE(tgc->h, tgc->h, s->es_scale);
    }
}

void
ecmd_tgc_scale_h_v_ab(struct rt_solid_edit *s)
{
    vect_t vec1, vec2;
    vect_t a, b;
    point_t old_top;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;

    RT_TGC_CK_MAGIC(tgc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->h);
    }

    /* calculate new a */
    VSUB2(vec1, tgc->c, tgc->a);
    VSCALE(vec2, vec1, 1-s->es_scale);
    VADD2(a, tgc->a, vec2);

    /* calculate new b */
    VSUB2(vec1, tgc->d, tgc->b);
    VSCALE(vec2, vec1, 1-s->es_scale);
    VADD2(b, tgc->b, vec2);

    if (0 <= VDOT(tgc->a, a) &&
	    0 <= VDOT(tgc->b, b) &&
	    !ZERO(MAGNITUDE(a)) &&
	    !ZERO(MAGNITUDE(b))) {
	/* adjust a, b, v and h */
	VMOVE(tgc->a, a);
	VMOVE(tgc->b, b);
	VADD2(old_top, tgc->v, tgc->h);
	VSCALE(tgc->h, tgc->h, s->es_scale);
	VSUB2(tgc->v, old_top, tgc->h);
    }
}

/* scale vector A */
void
ecmd_tgc_scale_a(struct rt_solid_edit *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->a);
    }
    VSCALE(tgc->a, tgc->a, s->es_scale);
}

/* scale vector B */
void
ecmd_tgc_scale_b(struct rt_solid_edit *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->b);
    }
    VSCALE(tgc->b, tgc->b, s->es_scale);
}

/* TGC: scale ratio "c" */
void
ecmd_tgc_scale_c(struct rt_solid_edit *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->c);
    }
    VSCALE(tgc->c, tgc->c, s->es_scale);
}

/* scale d for tgc */
void
ecmd_tgc_scale_d(struct rt_solid_edit *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->d);
    }
    VSCALE(tgc->d, tgc->d, s->es_scale);
}

void
ecmd_tgc_scale_ab(struct rt_solid_edit *s)
{
    fastf_t ma, mb;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->a);
    }
    VSCALE(tgc->a, tgc->a, s->es_scale);
    ma = MAGNITUDE(tgc->a);
    mb = MAGNITUDE(tgc->b);
    VSCALE(tgc->b, tgc->b, ma/mb);
}

/* scale C and D of tgc */
void
ecmd_tgc_scale_cd(struct rt_solid_edit *s)
{
    fastf_t ma, mb;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->c);
    }
    VSCALE(tgc->c, tgc->c, s->es_scale);
    ma = MAGNITUDE(tgc->c);
    mb = MAGNITUDE(tgc->d);
    VSCALE(tgc->d, tgc->d, ma/mb);
}

/* scale A, B, C, and D of tgc */
void
ecmd_tgc_scale_abcd(struct rt_solid_edit *s)
{
    fastf_t ma, mb;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(tgc->a);
    }
    VSCALE(tgc->a, tgc->a, s->es_scale);
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
int
ecmd_tgc_mv_h(struct rt_solid_edit *s)
{
    float la, lb, lc, ld;	/* TGC: length of vectors */
    vect_t work;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_TGC_CK_MAGIC(tgc);
    if (s->e_inpara) {
	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "ERROR: three arguments needed\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;

	if (s->mv_context) {
	    /* apply s->e_invmat to convert to real model coordinates */
	    MAT4X3PNT(work, s->e_invmat, s->e_para);
	    VSUB2(tgc->h, work, tgc->v);
	} else {
	    VSUB2(tgc->h, s->e_para, tgc->v);
	}
    }

    /* check for zero H vector */
    if (MAGNITUDE(tgc->h) <= SQRT_SMALL_FASTF) {
	bu_vls_printf(s->log_str, "Zero H vector not allowed, resetting to +Z\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	VSET(tgc->h, 0.0, 0.0, 1.0);
	return BRLCAD_ERROR;
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

    return 0;
}

/* Move end of H of tgc - leave ends alone */
int
ecmd_tgc_mv_hh(struct rt_solid_edit *s)
{
    vect_t work;
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_TGC_CK_MAGIC(tgc);
    if (s->e_inpara) {
	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "ERROR: three arguments needed\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;

	if (s->mv_context) {
	    /* apply s->e_invmat to convert to real model coordinates */
	    MAT4X3PNT(work, s->e_invmat, s->e_para);
	    VSUB2(tgc->h, work, tgc->v);
	} else {
	    VSUB2(tgc->h, s->e_para, tgc->v);
	}
    }

    /* check for zero H vector */
    if (MAGNITUDE(tgc->h) <= SQRT_SMALL_FASTF) {
	bu_vls_printf(s->log_str, "Zero H vector not allowed, resetting to +Z\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	VSET(tgc->h, 0.0, 0.0, 1.0);
	return BRLCAD_ERROR;
    }

    return 0;
}

/* rotate height vector */
int
ecmd_tgc_rot_h(struct rt_solid_edit *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;

    mat_t mat;
    mat_t mat1;
    mat_t edit;

    RT_TGC_CK_MAGIC(tgc);
    if (s->e_inpara) {
	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "ERROR: three arguments needed\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	static mat_t invsolr;
	/*
	 * Keyboard parameters:  absolute x, y, z rotations,
	 * in degrees.  First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, s->acc_rot_sol);

	/* Build completely new rotation change */
	MAT_IDN(s->model_changes);
	bn_mat_angles(s->model_changes,
		s->e_para[0],
		s->e_para[1],
		s->e_para[2]);
	/* Borrow s->incr_change matrix here */
	bn_mat_mul(s->incr_change, s->model_changes, invsolr);
	MAT_COPY(s->acc_rot_sol, s->model_changes);

	/* Apply new rotation to solid */
	/* Clear out solid rotation */
	MAT_IDN(s->model_changes);
    } else {
	/* Apply incremental changes already in s->incr_change */
    }

    if (s->mv_context) {
	/* calculate rotations about keypoint */
	bn_mat_xform_about_pnt(edit, s->incr_change, s->e_keypoint);

	/* We want our final matrix (mat) to xform the original solid
	 * to the position of this instance of the solid, perform the
	 * current edit operations, then xform back.
	 * mat = s->e_invmat * edit * s->e_mat
	 */
	bn_mat_mul(mat1, edit, s->e_mat);
	bn_mat_mul(mat, s->e_invmat, mat1);
	MAT4X3VEC(tgc->h, mat, tgc->h);
    } else {
	MAT4X3VEC(tgc->h, s->incr_change, tgc->h);
    }

    MAT_IDN(s->incr_change);

    return 0;
}

/* rotate surfaces AxB and CxD (tgc) */
int
ecmd_tgc_rot_ab(struct rt_solid_edit *s)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;

    mat_t mat;
    mat_t mat1;
    mat_t edit;

    RT_TGC_CK_MAGIC(tgc);
    if (s->e_inpara) {
	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "ERROR: three arguments needed\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	static mat_t invsolr;
	/*
	 * Keyboard parameters:  absolute x, y, z rotations,
	 * in degrees.  First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, s->acc_rot_sol);

	/* Build completely new rotation change */
	MAT_IDN(s->model_changes);
	bn_mat_angles(s->model_changes,
		s->e_para[0],
		s->e_para[1],
		s->e_para[2]);
	/* Borrow s->incr_change matrix here */
	bn_mat_mul(s->incr_change, s->model_changes, invsolr);
	MAT_COPY(s->acc_rot_sol, s->model_changes);

	/* Apply new rotation to solid */
	/* Clear out solid rotation */
	MAT_IDN(s->model_changes);
    } else {
	/* Apply incremental changes already in s->incr_change */
    }

    if (s->mv_context) {
	/* calculate rotations about keypoint */
	bn_mat_xform_about_pnt(edit, s->incr_change, s->e_keypoint);

	/* We want our final matrix (mat) to xform the original solid
	 * to the position of this instance of the solid, perform the
	 * current edit operations, then xform back.
	 * mat = s->e_invmat * edit * s->e_mat
	 */
	bn_mat_mul(mat1, edit, s->e_mat);
	bn_mat_mul(mat, s->e_invmat, mat1);
	MAT4X3VEC(tgc->a, mat, tgc->a);
	MAT4X3VEC(tgc->b, mat, tgc->b);
	MAT4X3VEC(tgc->c, mat, tgc->c);
	MAT4X3VEC(tgc->d, mat, tgc->d);
    } else {
	MAT4X3VEC(tgc->a, s->incr_change, tgc->a);
	MAT4X3VEC(tgc->b, s->incr_change, tgc->b);
	MAT4X3VEC(tgc->c, s->incr_change, tgc->c);
	MAT4X3VEC(tgc->d, s->incr_change, tgc->d);
    }
    MAT_IDN(s->incr_change);

    return 0;
}

/* Use mouse to change location of point V+H */
void
ecmd_tgc_mv_h_mousevec(struct rt_solid_edit *s, const vect_t mousevec)
{
    struct rt_tgc_internal *tgc =
	(struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);
    vect_t pos_view = VINIT_ZERO;
    vect_t tr_temp = VINIT_ZERO;
    vect_t temp = VINIT_ZERO;

    MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    /* Do NOT change pos_view[Z] ! */
    MAT4X3PNT(temp, s->vp->gv_view2model, pos_view);
    MAT4X3PNT(tr_temp, s->e_invmat, temp);
    VSUB2(tgc->h, tr_temp, tgc->v);
}

static int
rt_solid_edit_tgc_pscale(struct rt_solid_edit *s)
{
    if (s->e_inpara) {
	if (s->e_inpara > 1) {
	    bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	if (s->e_para[0] <= 0.0) {
	    bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;
    }

    switch (s->edit_flag) {
	case ECMD_TGC_SCALE_H:
	    ecmd_tgc_scale_h(s);
	    break;
	case ECMD_TGC_SCALE_H_V:
	    ecmd_tgc_scale_h_v(s);
	    break;
	case ECMD_TGC_SCALE_H_CD:
	    ecmd_tgc_scale_h_cd(s);
	    break;
	case ECMD_TGC_SCALE_H_V_AB:
	    ecmd_tgc_scale_h_v_ab(s);
	    break;
	case ECMD_TGC_SCALE_A:
	    ecmd_tgc_scale_a(s);
	    break;
	case ECMD_TGC_SCALE_B:
	    ecmd_tgc_scale_b(s);
	    break;
	case ECMD_TGC_SCALE_C:
	    ecmd_tgc_scale_c(s);
	    break;
	case ECMD_TGC_SCALE_D:
	    ecmd_tgc_scale_d(s);
	    break;
	case ECMD_TGC_SCALE_AB:
	    ecmd_tgc_scale_ab(s);
	    break;
	case ECMD_TGC_SCALE_CD:
	    ecmd_tgc_scale_cd(s);
	    break;
	case ECMD_TGC_SCALE_ABCD:
	    ecmd_tgc_scale_abcd(s);
	    break;
    };

    return 0;
}

int
rt_solid_edit_tgc_edit(struct rt_solid_edit *s)
{
    int ret = 0;

    switch (s->edit_flag) {
	case RT_SOLID_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    ret = rt_solid_edit_generic_sscale(s, &s->es_int);
	    break;
	case RT_SOLID_EDIT_TRANS:
	    /* translate solid */
	    rt_solid_edit_generic_strans(s, &s->es_int);
	    break;
	case RT_SOLID_EDIT_ROT:
	    /* rot solid about vertex */
	    rt_solid_edit_generic_srot(s, &s->es_int);
	    break;
	case ECMD_TGC_MV_H:
	    ret = ecmd_tgc_mv_h(s);
	    break;
	case ECMD_TGC_MV_HH:
	    ret = ecmd_tgc_mv_hh(s);
	    break;
	case ECMD_TGC_ROT_H:
	    ret = ecmd_tgc_rot_h(s);
	    break;
	case ECMD_TGC_ROT_AB:
	    ret = ecmd_tgc_rot_ab(s);
	    break;
	default:
	    ret = rt_solid_edit_tgc_pscale(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_REPLOT_EDITING_SOLID, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, NULL);

    return ret;
}

int
rt_solid_edit_tgc_edit_xy(
	struct rt_solid_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (s->edit_flag) {
	case RT_SOLID_EDIT_SCALE:
	case ECMD_TGC_SCALE_AB:
	case ECMD_TGC_SCALE_ABCD:  
	case ECMD_TGC_SCALE_B:
	case ECMD_TGC_SCALE_C:
	case ECMD_TGC_SCALE_CD:
	case ECMD_TGC_SCALE_D:
	case ECMD_TGC_SCALE_H:
	case ECMD_TGC_SCALE_H_CD:
	case ECMD_TGC_SCALE_H_V:
	case ECMD_TGC_SCALE_H_V_AB:
	    rt_solid_edit_generic_sscale_xy(s, mousevec);
	    return 0;
	case RT_SOLID_EDIT_TRANS:
	    rt_solid_edit_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_TGC_MV_H:
	case ECMD_TGC_MV_HH:
	case ECMD_TGC_MV_H_CD:
	case ECMD_TGC_MV_H_V_AB:
	    ecmd_tgc_mv_h_mousevec(s, mousevec);
	    break;
	case ECMD_TGC_ROT_H:
	case ECMD_TGC_ROT_AB:
	    bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	default:
	    // Everything else should be a scale
	    rt_solid_edit_generic_sscale_xy(s, mousevec);
	    return 0;
    }

    rt_update_edit_absolute_tran(s, pos_view);

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
