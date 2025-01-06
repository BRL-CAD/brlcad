/*                         E D E H Y . C
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
/** @file mged/primitives/edehy.c
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
#include "./edehy.h"

static void
ehy_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item ehy_menu[] = {
    { "EHY MENU", NULL, 0 },
    { "Set H", ehy_ed, MENU_EHY_H },
    { "Set A", ehy_ed, MENU_EHY_R1 },
    { "Set B", ehy_ed, MENU_EHY_R2 },
    { "Set c", ehy_ed, MENU_EHY_C },
    { "", NULL, 0 }
};

struct menu_item *
mged_ehy_menu_item(const struct bn_tol *UNUSED(tol))
{
    return ehy_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_ehy_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_ehy_internal *ehy = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(ehy);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(ehy->ehy_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(ehy->ehy_H));
    bu_vls_printf(p, "Semi-major axis: %.9f %.9f %.9f\n", V3ARGS(ehy->ehy_Au));
    bu_vls_printf(p, "Semi-major length: %.9f\n", ehy->ehy_r1 * base2local);
    bu_vls_printf(p, "Semi-minor length: %.9f\n", ehy->ehy_r2 * base2local);
    bu_vls_printf(p, "Dist to asymptotes: %.9f\n", ehy->ehy_c * base2local);
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
mged_ehy_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_ehy_internal *ehy = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(ehy);

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

    // Set up initial line
    ln = strchr(lc, tc);
    if (ln) *ln = '\0';

    // Trim off prefixes, if user left them in
    while (lc && strchr(lc, ':')) lc++;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(ehy->ehy_V, a, b, c);
    VSCALE(ehy->ehy_V, ehy->ehy_V, local2base);

    // Set up Height line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(ehy->ehy_H, a, b, c);
    VSCALE(ehy->ehy_H, ehy->ehy_H, local2base);

    // Set up Semi-major axis line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(ehy->ehy_Au, a, b, c);
    VUNITIZE(ehy->ehy_Au);

    // Set up Semi-major length line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    ehy->ehy_r1 = a * local2base;

    // Set up Semi-minor length line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    ehy->ehy_r2 = a * local2base;

    // Set up distance to asymptotes line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    ehy->ehy_c = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale height vector H */
void
menu_ehy_h(struct mged_state *s)
{
    struct rt_ehy_internal *ehy =
	(struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

    RT_EHY_CK_MAGIC(ehy);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(ehy->ehy_H);
    }
    VSCALE(ehy->ehy_H, ehy->ehy_H, s->edit_state.es_scale);
}

/* scale semimajor axis of EHY */
void
menu_ehy_r1(struct mged_state *s)
{
    struct rt_ehy_internal *ehy =
	(struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

    RT_EHY_CK_MAGIC(ehy);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / ehy->ehy_r1;
    }
    if (ehy->ehy_r1 * s->edit_state.es_scale >= ehy->ehy_r2)
	ehy->ehy_r1 *= s->edit_state.es_scale;
    else
	bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
}

/* scale semiminor axis of EHY */
void
menu_ehy_r2(struct mged_state *s)
{
    struct rt_ehy_internal *ehy =
	(struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

    RT_EHY_CK_MAGIC(ehy);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / ehy->ehy_r2;
    }
    if (ehy->ehy_r2 * s->edit_state.es_scale <= ehy->ehy_r1)
	ehy->ehy_r2 *= s->edit_state.es_scale;
    else
	bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
}

/* scale distance between apex of EHY & asymptotic cone */
void
menu_ehy_c(struct mged_state *s)
{
    struct rt_ehy_internal *ehy =
	(struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

    RT_EHY_CK_MAGIC(ehy);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / ehy->ehy_c;
    }
    ehy->ehy_c *= s->edit_state.es_scale;
}

static int
mged_ehy_pscale(struct mged_state *s, int mode)
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
	case MENU_EHY_H:
	    menu_ehy_h(s);
	    break;
	case MENU_EHY_R1:
	    menu_ehy_r1(s);
	    break;
	case MENU_EHY_R2:
	    menu_ehy_r2(s);
	    break;
	case MENU_EHY_C:
	    menu_ehy_c(s);
	    break;
    };

    return 0;
}

int
mged_ehy_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    mged_generic_sscale(s, &s->edit_state.es_int);
	    break;
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->edit_state.es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->edit_state.es_int);
	    break;
	case PSCALE:
	    mged_ehy_pscale(s, es_menu);
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
