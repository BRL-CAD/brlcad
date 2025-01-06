/*                         E D E P A . C
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
/** @file mged/primitives/edepa.c
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
#include "./edepa.h"

static void
epa_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item epa_menu[] = {
    { "EPA MENU", NULL, 0 },
    { "Set H", epa_ed, MENU_EPA_H },
    { "Set A", epa_ed, MENU_EPA_R1 },
    { "Set B", epa_ed, MENU_EPA_R2 },
    { "", NULL, 0 }
};

struct menu_item *
mged_epa_menu_item(const struct bn_tol *UNUSED(tol))
{
    return epa_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_epa_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_epa_internal *epa = (struct rt_epa_internal *)ip->idb_ptr;
    RT_EPA_CK_MAGIC(epa);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(epa->epa_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(epa->epa_H));
    bu_vls_printf(p, "Semi-major axis: %.9f %.9f %.9f\n", V3ARGS(epa->epa_Au));
    bu_vls_printf(p, "Semi-major length: %.9f\n", epa->epa_r1 * base2local);
    bu_vls_printf(p, "Semi-minor length: %.9f\n", epa->epa_r2 * base2local);
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
mged_epa_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_epa_internal *epa = (struct rt_epa_internal *)ip->idb_ptr;
    RT_EPA_CK_MAGIC(epa);

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
    VSET(epa->epa_V, a, b, c);
    VSCALE(epa->epa_V, epa->epa_V, local2base);

    // Set up Height line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(epa->epa_H, a, b, c);
    VSCALE(epa->epa_H, epa->epa_H, local2base);

    // Set up Semi-major axis line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(epa->epa_Au, a, b, c);
    VUNITIZE(epa->epa_Au);

    // Set up Semi-major length line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    epa->epa_r1 = a * local2base;

    // Set up Semi-minor length line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    epa->epa_r2 = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale height vector H */
void
menu_epa_h(struct mged_state *s)
{
    struct rt_epa_internal *epa =
	(struct rt_epa_internal *)s->edit_state.es_int.idb_ptr;

    RT_EPA_CK_MAGIC(epa);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(epa->epa_H);
    }
    VSCALE(epa->epa_H, epa->epa_H, s->edit_state.es_scale);
}

/* scale semimajor axis of EPA */
void
menu_epa_r1(struct mged_state *s)
{
    struct rt_epa_internal *epa =
	(struct rt_epa_internal *)s->edit_state.es_int.idb_ptr;

    RT_EPA_CK_MAGIC(epa);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / epa->epa_r1;
    }
    if (epa->epa_r1 * s->edit_state.es_scale >= epa->epa_r2)
	epa->epa_r1 *= s->edit_state.es_scale;
    else
	bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
}

/* scale semiminor axis of EPA */
void
menu_epa_r2(struct mged_state *s)
{
    struct rt_epa_internal *epa =
	(struct rt_epa_internal *)s->edit_state.es_int.idb_ptr;

    RT_EPA_CK_MAGIC(epa);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / epa->epa_r2;
    }
    if (epa->epa_r2 * s->edit_state.es_scale <= epa->epa_r1)
	epa->epa_r2 *= s->edit_state.es_scale;
    else
	bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
}

static int
mged_epa_pscale(struct mged_state *s, int mode)
{
    if (inpara > 1) {
	Tcl_AppendResult(s->interp, "ERROR: only one argument needed\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    if (es_para[0] <= 0.0) {
	Tcl_AppendResult(s->interp, "ERROR: SCALE FACTOR <= 0\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    switch (mode) {
	case MENU_EPA_H:
	    menu_epa_h(s);
	    break;
	case MENU_EPA_R1:
	    menu_epa_r1(s);
	    break;
	case MENU_EPA_R2:
	    menu_epa_r2(s);
	    break;
    };

    return 0;
}

int
mged_epa_edit(struct mged_state *s, int edflag)
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
	    return mged_epa_pscale(s, es_menu);
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
