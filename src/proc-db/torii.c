/*                         T O R I I . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file proc-db/torii.c
 *
 * Generate an interwoven sheet of torii -- a chain-mail weave.
 *
 * The rings are laid out on a square grid as a checkerboard of two
 * orientations, each tilted by +/- the same angle about the row axis.
 * Because adjacent rings are never coplanar, every neighboring pair
 * interlocks like a chain link (one ring threads through the other's
 * hole) WITHOUT the solid tubes intersecting -- so the resulting mesh is
 * a single, flexible, fully 3-D-printable piece of mail.  The ring tube
 * thickness is modulated across the sheet in a smooth radial pattern (and
 * the rings are colored by that thickness) so the weave shows a varying
 * pattern of sizes rather than a uniform field.
 *
 * All coordinates are millimeters.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bn.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"


/* clamp a double into [lo,hi] */
static double
clampd(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}


/* linear interpolation of an 8-bit color channel */
static unsigned char
lerp8(double a, double b, double t)
{
    double v = a + (b - a) * t;
    if (v < 0.0) v = 0.0;
    if (v > 255.0) v = 255.0;
    return (unsigned char)(v + 0.5);
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    struct wmember all_hd;

    const char *fileName = "torii.g";

    /* tunable parameters -- all optional, every one has a default that
     * produces a clean, non-self-intersecting chain-mail weave.
     */
    int rows = 9;		/* rings along Y */
    int cols = 12;		/* rings along X */
    double major = 60.0;	/* torus major radius (center of tube)  */
    double minor = 5.0;		/* base tube (minor) radius             */
    double tilt = 52.0;		/* ring lean from the sheet, in degrees */
    double pitch = 92.0;	/* grid spacing between ring centers     */
    double vary = 0.45;		/* tube-thickness variation amplitude    */

    /* These defaults were tuned (steep tilt, generous pitch, slim wire)
     * so that every interlocking ring -- including the thickened ones in
     * the center -- clears its neighbors with zero solid overlap, i.e. the
     * whole sheet is a single, articulating, 3-D-printable piece of mail.
     */

    double theta, ny, nz;
    double minr, maxr;
    int i, j;
    unsigned long serial = 0;

    bu_setprogname(av[0]);

    if (ac < 2) {
	bu_exit(1, "Usage: %s output.g [--rows n] [--cols n] [--major mm] [--minor mm]\n"
		   "                 [--tilt deg] [--pitch mm] [--vary frac]\n", av[0]);
    }

    fileName = av[1];

    /* tiny "--flag value" parser */
    for (i = 2; i + 1 < ac; i += 2) {
	if (BU_STR_EQUAL(av[i], "--rows")) rows = atoi(av[i + 1]);
	else if (BU_STR_EQUAL(av[i], "--cols")) cols = atoi(av[i + 1]);
	else if (BU_STR_EQUAL(av[i], "--major")) major = atof(av[i + 1]);
	else if (BU_STR_EQUAL(av[i], "--minor")) minor = atof(av[i + 1]);
	else if (BU_STR_EQUAL(av[i], "--tilt")) tilt = atof(av[i + 1]);
	else if (BU_STR_EQUAL(av[i], "--pitch")) pitch = atof(av[i + 1]);
	else if (BU_STR_EQUAL(av[i], "--vary")) vary = atof(av[i + 1]);
	else bu_log("Warning: ignoring unknown option '%s'\n", av[i]);
    }

    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    if (major < 1.0) major = 1.0;
    if (minor < 0.1) minor = 0.1;
    vary = clampd(vary, 0.0, 0.9);

    theta = tilt * DEG2RAD;
    ny = sin(theta);
    nz = cos(theta);

    /* tube radius ranges from minor (edges) to minor*(1+vary) (center) */
    minr = minor;
    maxr = minor * (1.0 + vary);

    bu_log("torii: %dx%d chain-mail weave, major=%g minor=%g..%g tilt=%g pitch=%g\n",
	   cols, rows, major, minr, maxr, tilt, pitch);

    if ((db_fp = wdb_fopen(fileName)) == NULL) {
	perror(fileName);
	return 2;
    }
    mk_id_units(db_fp, "Interwoven Chain-Mail Torii", "mm");

    BU_LIST_INIT(&all_hd.l);

    /* Lay out the checkerboard of rings.  Rings with (i+j) even tilt one
     * way; (i+j) odd tilt the other.  Each ring threads its four edge
     * neighbors of the opposite tilt.
     */
    for (j = 0; j < rows; j++) {
	for (i = 0; i < cols; i++) {
	    point_t center;
	    vect_t normal;
	    double r, dx, dy, dn;
	    unsigned char rgb[3];
	    char sname[64], rname[64];
	    int parity = (i + j) & 1;

	    VSET(center, i * pitch, j * pitch, 0.0);

	    /* +/- tilt about the X (row) axis */
	    if (parity)
		VSET(normal, 0.0, -ny, nz);
	    else
		VSET(normal, 0.0, ny, nz);

	    /* smooth radial thickness variation: thick at the center of
	     * the sheet, tapering to the base radius at the edges */
	    dx = (i - (cols - 1) / 2.0) / ((cols) / 2.0);
	    dy = (j - (rows - 1) / 2.0) / ((rows) / 2.0);
	    dn = sqrt(dx * dx + dy * dy);
	    dn = clampd(dn, 0.0, 1.0);
	    r = minr + (maxr - minr) * cos(dn * M_PI_2);  /* 1 at center, 0 at corner */

	    snprintf(sname, sizeof(sname), "ring_%lu.s", serial);
	    snprintf(rname, sizeof(rname), "ring_%lu.r", serial);

	    if (mk_tor(db_fp, sname, center, normal, major, r) != 0) {
		bu_log("Unable to write torus \"%s\"\n", sname);
		return 3;
	    }

	    /* color by thickness: thin -> cool steel blue, thick -> warm
	     * copper, so the size pattern reads at a glance */
	    {
		double t = (maxr > minr) ? (r - minr) / (maxr - minr) : 0.0;
		rgb[0] = lerp8(70.0, 225.0, t);
		rgb[1] = lerp8(120.0, 120.0, t);
		rgb[2] = lerp8(205.0, 45.0, t);
	    }

	    mk_region1(db_fp, rname, sname, "plastic", "sh=12 sp=0.5 di=0.5", rgb);
	    (void)mk_addmember(rname, &all_hd.l, NULL, WMOP_UNION);
	    serial++;
	}
    }

    if (mk_lcomb(db_fp, "all", &all_hd, 0, NULL, NULL, NULL, 0) != 0) {
	bu_log("Unable to write the \"all\" group\n");
	return 4;
    }

    bu_log("torii: wrote %lu interlocking rings into group \"all\"\n", serial);

    db_close(db_fp->dbip);
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
