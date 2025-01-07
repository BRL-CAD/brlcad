/*                         E D P A R T . C
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
/** @file mged/primitives/edpart.c
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
#include "./edpart.h"

static void
part_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    mged_set_edflag(s, PSCALE);

    set_e_axes_pos(s, 1);
}
struct menu_item part_menu[] = {
    { "Particle MENU", NULL, 0 },
    { "Set H", part_ed, MENU_PART_H },
    { "Set v", part_ed, MENU_PART_v },
    { "Set h", part_ed, MENU_PART_h },
    { "", NULL, 0 }
};

struct menu_item *
mged_part_menu_item(const struct bn_tol *UNUSED(tol))
{
    return part_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_part_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_part_internal *part = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(part);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(part->part_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(part->part_H));
    bu_vls_printf(p, "v radius: %.9f\n", part->part_vrad * base2local);
    bu_vls_printf(p, "h radius: %.9f\n", part->part_hrad * base2local);
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
mged_part_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_part_internal *part = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(part);

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

    // Read the numbers
    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(part->part_V, a, b, c);
    VSCALE(part->part_V, part->part_V, local2base);

    // Set up Height line
    read_params_line_incr;

    // Read the numbers
    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(part->part_H, a, b, c);
    VSCALE(part->part_H, part->part_H, local2base);

    // Set up v radius line
    read_params_line_incr;

    // Read the numbers
    sscanf(lc, "%lf", &a);
    part->part_vrad = a * local2base;

    // Set up h radius line
    read_params_line_incr;

    // Read the numbers
    sscanf(lc, "%lf", &a);
    part->part_hrad = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale vector H */
void
menu_part_h(struct mged_state *s)
{
    struct rt_part_internal *part =
	(struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

    RT_PART_CK_MAGIC(part);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(part->part_H);
    }
    VSCALE(part->part_H, part->part_H, s->edit_state.es_scale);
}

/* scale v end radius */
void
menu_part_v(struct mged_state *s)
{
    struct rt_part_internal *part =
	(struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

    RT_PART_CK_MAGIC(part);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / part->part_vrad;
    }
    part->part_vrad *= s->edit_state.es_scale;
}

/* scale h end radius */
void
menu_part_h_end_r(struct mged_state *s)
{
    struct rt_part_internal *part =
	(struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

    RT_PART_CK_MAGIC(part);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / part->part_hrad;
    }
    part->part_hrad *= s->edit_state.es_scale;
}

static int
mged_part_pscale(struct mged_state *s, int mode)
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

    /* must convert to base units */
    es_para[0] *= s->dbip->dbi_local2base;
    es_para[1] *= s->dbip->dbi_local2base;
    es_para[2] *= s->dbip->dbi_local2base;

    switch (mode) {
	case MENU_PART_H:
	    menu_part_h(s);
	    break;
	case MENU_PART_v:
	    menu_part_v(s);
	    break;
	case MENU_PART_h:
	    menu_part_h_end_r(s);
	    break;
    };

    return 0;
}

int
mged_part_edit(struct mged_state *s, int edflag)
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
	    return mged_part_pscale(s, es_menu);
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
