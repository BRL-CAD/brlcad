/*                         E D T O R . C
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
/** @file mged/primitives/edtor.c
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

#define MENU_TOR_R1		1021
#define MENU_TOR_R2		1022

static void
tor_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->s_edit->edit_menu = arg;
    mged_set_edflag(s, PSCALE);

    set_e_axes_pos(s, 1);
}


struct menu_item tor_menu[] = {
    { "TORUS MENU", NULL, 0 },
    { "Set Radius 1", tor_ed, MENU_TOR_R1 },
    { "Set Radius 2", tor_ed, MENU_TOR_R2 },
    { "", NULL, 0 }
};


struct menu_item *
mged_tor_menu_item(const struct bn_tol *UNUSED(tol))
{
    return tor_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_tor_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_tor_internal *tor = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tor);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(tor->v));
    bu_vls_printf(p, "Normal: %.9f %.9f %.9f\n", V3BASE2LOCAL(tor->h));
    bu_vls_printf(p, "radius_1: %.9f\n", tor->r_a*base2local);
    bu_vls_printf(p, "radius_2: %.9f\n", tor->r_h*base2local);
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
mged_tor_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_tor_internal *tor = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tor);

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
    VSET(tor->v, a, b, c);
    VSCALE(tor->v, tor->v, local2base);

    // Set up Normal line
    read_params_line_incr;

    // Read the numbers
    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(tor->h, a, b, c);
    VUNITIZE(tor->h);

    // Set up radius_1 line
    read_params_line_incr;

    // Read the numbers
    sscanf(lc, "%lf", &a);
    tor->r_a = a * local2base;

    // Set up radius_2 line
    read_params_line_incr;

    // Read the numbers
    sscanf(lc, "%lf", &a);
    tor->r_h = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale radius 1 of TOR */
void
menu_tor_r1(struct mged_state *s)
{
    struct rt_tor_internal *tor =
	(struct rt_tor_internal *)s->s_edit->es_int.idb_ptr;
    fastf_t newrad;
    RT_TOR_CK_MAGIC(tor);
    if (s->s_edit->e_inpara) {
	/* take s->s_edit->e_mat[15] (path scaling) into account */
	s->s_edit->e_para[0] *= s->s_edit->e_mat[15];
	newrad = s->s_edit->e_para[0];
    } else {
	newrad = tor->r_a * s->s_edit->es_scale;
    }
    if (newrad < SMALL) newrad = 4*SMALL;
    if (tor->r_h <= newrad)
	tor->r_a = newrad;
}

/* scale radius 2 of TOR */
void
menu_tor_r2(struct mged_state *s)
{
    struct rt_tor_internal *tor =
	(struct rt_tor_internal *)s->s_edit->es_int.idb_ptr;
    fastf_t newrad;
    RT_TOR_CK_MAGIC(tor);
    if (s->s_edit->e_inpara) {
	/* take s->s_edit->e_mat[15] (path scaling) into account */
	s->s_edit->e_para[0] *= s->s_edit->e_mat[15];
	newrad = s->s_edit->e_para[0];
    } else {
	newrad = tor->r_h * s->s_edit->es_scale;
    }
    if (newrad < SMALL) newrad = 4*SMALL;
    if (newrad <= tor->r_a)
	tor->r_h = newrad;
}

static int
mged_tor_pscale(struct mged_state *s, int mode)
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
	case MENU_TOR_R1:
	    menu_tor_r1(s);
	    break;
	case MENU_TOR_R2:
	    menu_tor_r2(s);
	    break;
    };

    return 0;
}

int
mged_tor_edit(struct mged_state *s, int edflag)
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
	    return mged_tor_pscale(s, s->s_edit->edit_menu);
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
