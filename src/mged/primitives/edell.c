/*                         E D E L L . C
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
/** @file mged/primitives/edell.c
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

#define MENU_ELL_SCALE_A	3039
#define MENU_ELL_SCALE_B	3040
#define MENU_ELL_SCALE_C	3041
#define MENU_ELL_SCALE_ABC	3042

static void
ell_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    s->edit_state.edit_menu = arg;
    mged_set_edflag(s, PSCALE);

    set_e_axes_pos(s, 1);
}
struct menu_item ell_menu[] = {
    { "ELLIPSOID MENU", NULL, 0 },
    { "Set A", ell_ed, MENU_ELL_SCALE_A },
    { "Set B", ell_ed, MENU_ELL_SCALE_B },
    { "Set C", ell_ed, MENU_ELL_SCALE_C },
    { "Set A,B,C", ell_ed, MENU_ELL_SCALE_ABC },
    { "", NULL, 0 }
};

struct menu_item *
mged_ell_menu_item(const struct bn_tol *UNUSED(tol))
{
    return ell_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_ell_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_ell_internal *ell = (struct rt_ell_internal *)ip->idb_ptr;
    RT_ELL_CK_MAGIC(ell);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(ell->v));
    bu_vls_printf(p, "A: %.9f %.9f %.9f\n", V3BASE2LOCAL(ell->a));
    bu_vls_printf(p, "B: %.9f %.9f %.9f\n", V3BASE2LOCAL(ell->b));
    bu_vls_printf(p, "C: %.9f %.9f %.9f\n", V3BASE2LOCAL(ell->c));
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
mged_ell_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_ell_internal *ell = (struct rt_ell_internal *)ip->idb_ptr;
    RT_ELL_CK_MAGIC(ell);

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
    VSET(ell->v, a, b, c);
    VSCALE(ell->v, ell->v, local2base);

    // Set up A line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(ell->a, a, b, c);
    VSCALE(ell->a, ell->a, local2base);

    // Set up B line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(ell->b, a, b, c);
    VSCALE(ell->b, ell->b, local2base);

    // Set up C line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(ell->c, a, b, c);
    VSCALE(ell->c, ell->c, local2base);

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale vector A */
void
menu_ell_scale_a(struct mged_state *s)
{
    struct rt_ell_internal *ell =
	(struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;
    RT_ELL_CK_MAGIC(ell);
    if (s->edit_state.e_inpara) {
	/* take s->edit_state.e_mat[15] (path scaling) into account */
	s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
	    MAGNITUDE(ell->a);
    }
    VSCALE(ell->a, ell->a, s->edit_state.es_scale);
}

/* scale vector B */
void
menu_ell_scale_b(struct mged_state *s)
{
    struct rt_ell_internal *ell =
	(struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;
    RT_ELL_CK_MAGIC(ell);
    if (s->edit_state.e_inpara) {
	/* take s->edit_state.e_mat[15] (path scaling) into account */
	s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
	    MAGNITUDE(ell->b);
    }
    VSCALE(ell->b, ell->b, s->edit_state.es_scale);
}

/* scale vector C */
void
menu_ell_scale_c(struct mged_state *s)
{
    struct rt_ell_internal *ell =
	(struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;
    RT_ELL_CK_MAGIC(ell);
    if (s->edit_state.e_inpara) {
	/* take s->edit_state.e_mat[15] (path scaling) into account */
	s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
	    MAGNITUDE(ell->c);
    }
    VSCALE(ell->c, ell->c, s->edit_state.es_scale);
}

/* set A, B, and C length the same */
void
menu_ell_scale_abc(struct mged_state *s)
{
    fastf_t ma, mb;
    struct rt_ell_internal *ell =
	(struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;
    RT_ELL_CK_MAGIC(ell);
    if (s->edit_state.e_inpara) {
	/* take s->edit_state.e_mat[15] (path scaling) into account */
	s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
	    MAGNITUDE(ell->a);
    }
    VSCALE(ell->a, ell->a, s->edit_state.es_scale);
    ma = MAGNITUDE(ell->a);
    mb = MAGNITUDE(ell->b);
    VSCALE(ell->b, ell->b, ma/mb);
    mb = MAGNITUDE(ell->c);
    VSCALE(ell->c, ell->c, ma/mb);
}

static int
mged_ell_pscale(struct mged_state *s, int mode)
{
    if (s->edit_state.e_inpara > 1) {
	Tcl_AppendResult(s->interp, "ERROR: only one argument needed\n", (char *)NULL);
	s->edit_state.e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->edit_state.e_para[0] <= 0.0) {
	Tcl_AppendResult(s->interp, "ERROR: SCALE FACTOR <= 0\n", (char *)NULL);
	s->edit_state.e_inpara = 0;
	return TCL_ERROR;
    }

    /* must convert to base units */
    s->edit_state.e_para[0] *= s->dbip->dbi_local2base;
    s->edit_state.e_para[1] *= s->dbip->dbi_local2base;
    s->edit_state.e_para[2] *= s->dbip->dbi_local2base;

    switch (mode) {
	case MENU_ELL_SCALE_A:
	    menu_ell_scale_a(s);
	    break;
	case MENU_ELL_SCALE_B:
	    menu_ell_scale_b(s);
	    break;
	case MENU_ELL_SCALE_C:
	    menu_ell_scale_c(s);
	    break;
	case MENU_ELL_SCALE_ABC:
	    menu_ell_scale_abc(s);
	    break;
    };

    return 0;
}

int
mged_ell_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    return mged_generic_sscale(s, &s->edit_state.es_int);
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->edit_state.es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->edit_state.es_int);
	    break;
	case PSCALE:
	    return mged_ell_pscale(s, s->edit_state.edit_menu);
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
