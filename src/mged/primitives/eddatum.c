/*                         E D D A T U M . C
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
/** @file mged/primitives/eddatum.c
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
#include "./edfunctab.h"

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_datum_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_datum_internal *datum = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum);

    do {
	if (!ZERO(datum->w))
	    bu_vls_printf(p, "Plane: %.9f %.9f %.9f (pnt) %.9f %.9f %.9f (dir) %.9f (scale)\n", V3BASE2LOCAL(datum->pnt), V3BASE2LOCAL(datum->dir), datum->w);
	else if (!ZERO(MAGNITUDE(datum->dir)))
	    bu_vls_printf(p, "Line: %.9f %.9f %.9f (pnt) %.9f %.9f %.9f (dir)\n", V3BASE2LOCAL(datum->pnt), V3BASE2LOCAL(datum->dir));
	else
	    bu_vls_printf(p, "Point: %.9f %.9f %.9f\n", V3BASE2LOCAL(datum->pnt));
    } while ((datum = datum->next));
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
mged_datum_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double d = 0.0;
    double e = 0.0;
    double f = 0.0;
    double g = 0.0;
    struct rt_datum_internal *datum = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum);

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
    int first_line = 1;

    // Set up initial line
    ln = strchr(lc, tc);
    if (ln) *ln = '\0';

    // Trim off prefixes, if user left them in
    while (lc && strchr(lc, ':')) lc++;

    do {
	if (!first_line) {
	    read_params_line_incr;
	} else {
	    first_line = 0;
	}

	if (bu_strncasecmp(lc, "point", strlen("point")) == 0) {
	    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
	    VSET(datum->pnt, a, b, c);
	    VSCALE(datum->pnt, datum->pnt, local2base);
	} else if (bu_strncasecmp(lc, "line", strlen("line")) == 0) {
	    sscanf(lc, "%lf %lf %lf %lf %lf %lf", &a, &b, &c, &d, &e, &f);
	    VSET(datum->pnt, a, b, c);
	    VSET(datum->dir, d, e, f);
	    VSCALE(datum->pnt, datum->pnt, local2base);
	    VSCALE(datum->dir, datum->dir, local2base);
	} else if (bu_strncasecmp(lc, "plane", strlen("plane")) == 0) {
	    sscanf(lc, "%lf %lf %lf %lf %lf %lf %lf", &a, &b, &c, &d, &e, &f, &g);
	    VSET(datum->pnt, a, b, c);
	    VSET(datum->dir, d, e, f);
	    VSCALE(datum->pnt, datum->pnt, local2base);
	    VSCALE(datum->dir, datum->dir, local2base);
	    datum->w = g;
	}
    } while ((datum = datum->next));

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
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
