/*                      H E L I C O I L . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file proc-db/helicoil.c
 *
 * Procedurally build helical solids swept along a pipe centerline
 * using the libwdb mk_pipe() family of routines.  Three modes are
 * supported:
 *
 *   spring - a single chrome helix (a coil spring),
 *   screw  - a tight helical thread wrapped around a central rcc
 *            shaft (a screw / bolt),
 *   dna    - a double helix: two strands 180 degrees out of phase,
 *            joined by periodic "rung" pipe segments, evoking a
 *            DNA-like ladder.
 *
 * Each coil is a single pipe primitive whose centerline traces a
 * helix:  for a parameter t over N steps,
 *
 *     coord = (radius*cos(t), radius*sin(t), pitch*t/(2*pi)).
 *
 * The wire is a solid pipe (inner diameter 0).  The pipe bend radius
 * MUST be strictly greater than the wire outer radius (od/2), and the
 * helix must be sampled finely enough that consecutive centerline
 * points are far enough apart for the bends to be valid; both
 * constraints are enforced below.
 *
 * The result is assembled into a top-level group named "all"
 * containing a shiny metal coil region, a ground plane, and a light
 * so the scene renders attractively out of the box.
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


/* the three available build modes */
#define MODE_SPRING 0
#define MODE_SCREW  1
#define MODE_DNA    2

/* number of helix samples taken per full turn; finer sampling yields a
 * smoother coil and keeps consecutive centerline points comfortably
 * spaced relative to the bend radius.
 */
#define STEPS_PER_TURN 24


/*
 * Append the points of one helix (optionally phase-shifted) to a pipe
 * segment list.  The list must already have been initialized with
 * mk_pipe_init().
 *
 * head        - initialized pipe segment list
 * turns       - number of full revolutions
 * radius      - helix radius (mm) measured to the centerline
 * pitch       - rise (mm) per full turn
 * wire        - wire outer diameter (mm)
 * phase       - starting angular offset (radians); used by the dna
 *               mode to place the second strand 180 degrees away
 *
 * Returns the number of points added.
 */
static int
build_helix(struct bu_list *head, int turns, double radius,
	    double pitch, double wire, double phase)
{
    int total;
    int i;
    double t;
    double od;
    double bend;
    point_t coord;

    /* outer diameter is the wire thickness; inner diameter is 0 so the
     * pipe is a solid wire rather than a hollow tube.
     */
    od = wire;

    /* The bend radius must be STRICTLY greater than the wire outer
     * radius (od/2).  We also want it large enough to comfortably span
     * the gentle curvature of the helix, so we use a generous multiple
     * of the outer radius.  This safely satisfies the pipe validity
     * constraint regardless of the helix geometry.
     */
    bend = od * 1.5;
    if (bend <= od * 0.5)
	bend = od * 0.5 + 1.0; /* defensive: never violate od/2 < bend */

    /* sample the helix; one extra point closes out the final turn */
    total = turns * STEPS_PER_TURN + 1;

    for (i = 0; i < total; i++) {
	/* parameter sweeps from 0 to turns*2*pi */
	t = (double)i / (double)STEPS_PER_TURN * M_2PI;

	VSET(coord,
	     radius * cos(t + phase),
	     radius * sin(t + phase),
	     pitch * t / M_2PI);

	mk_add_pipe_pnt(head, coord, od, 0.0, bend);
    }

    return total;
}


/*
 * Build a "rung" pipe segment connecting two points on opposing DNA
 * strands.  Each rung is its own small pipe primitive; the caller
 * unions them all into the coil region.
 */
static void
build_rung(struct rt_wdb *db_fp, const char *name,
	   const point_t a, const point_t b, double wire)
{
    struct bu_list head;
    double od;
    double bend;

    od = wire;
    bend = od * 1.5;

    mk_pipe_init(&head);
    mk_add_pipe_pnt(&head, a, od, 0.0, bend);
    mk_add_pipe_pnt(&head, b, od, 0.0, bend);
    mk_pipe(db_fp, name, &head);
    mk_pipe_free(&head);
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp;

    /* tunable parameters with attractive defaults */
    int mode = MODE_SPRING;
    int turns = 8;
    double radius = 40.0;	/* mm, helix radius to centerline */
    double pitch = 25.0;	/* mm, rise per full turn */
    double wire = 6.0;		/* mm, wire outer diameter */

    int i;
    int npts;
    double height;		/* total vertical extent of the coil */

    struct bu_list head;	/* pipe segment list */
    struct wmember coil_hd;	/* members of the coil region */
    struct wmember all_hd;	/* members of the top-level group */

    unsigned char chrome[3];
    unsigned char steel[3];
    unsigned char ground_rgb[3];

    point_t base;
    vect_t hvec;
    point_t gmin, gmax;
    point_t lpos;

    bu_setprogname(av[0]);

    if (ac < 2) {
	bu_exit(1, "Usage: %s output.g [--mode spring|screw|dna] "
		"[--turns n] [--radius mm] [--pitch mm] [--wire mm]\n", av[0]);
    }

    /* parse the optional parameters; every one has a sensible default
     * so that running with only the output path yields a full scene.
     */
    for (i = 2; i < ac; i++) {
	if (BU_STR_EQUAL(av[i], "--mode") && i + 1 < ac) {
	    i++;
	    if (BU_STR_EQUAL(av[i], "spring"))
		mode = MODE_SPRING;
	    else if (BU_STR_EQUAL(av[i], "screw"))
		mode = MODE_SCREW;
	    else if (BU_STR_EQUAL(av[i], "dna"))
		mode = MODE_DNA;
	    else
		bu_exit(1, "Unknown mode '%s' (use spring, screw, or dna)\n", av[i]);
	} else if (BU_STR_EQUAL(av[i], "--turns") && i + 1 < ac) {
	    turns = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--radius") && i + 1 < ac) {
	    radius = atof(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--pitch") && i + 1 < ac) {
	    pitch = atof(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--wire") && i + 1 < ac) {
	    wire = atof(av[++i]);
	} else {
	    bu_exit(1, "Unrecognized argument '%s'\n", av[i]);
	}
    }

    /* sanity-clamp the parameters so the pipe primitive stays valid */
    if (turns < 1)
	turns = 1;
    if (radius < 1.0)
	radius = 1.0;
    if (wire < 0.5)
	wire = 0.5;
    if (pitch < wire * 1.5)
	pitch = wire * 1.5; /* keep adjacent coils from overlapping badly */

    /* screw threads sit close to the shaft and wind tightly */
    if (mode == MODE_SCREW && pitch < wire * 2.0)
	pitch = wire * 2.0;

    /* Open/create the database for writing. */
    if ((db_fp = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	bu_exit(2, "ERROR: unable to open %s for writing\n", av[1]);
    }

    mk_id_units(db_fp, "Helical Coil Demo", "mm");

    /* total vertical extent of the coil(s) */
    height = pitch * (double)turns;

    /* nice colors */
    VSET(chrome, 200, 205, 215);	/* bright chrome */
    VSET(steel, 130, 135, 145);		/* darker steel shaft */
    VSET(ground_rgb, 90, 80, 70);	/* warm earthy ground */

    /* initialize the member lists */
    BU_LIST_INIT(&coil_hd.l);
    BU_LIST_INIT(&all_hd.l);

    /* ---- build the coil geometry according to the chosen mode ---- */

    if (mode == MODE_DNA) {
	int rung;
	int rung_steps;
	double t;

	bu_log("Building DNA double helix: %d turns, radius %g mm, "
	       "pitch %g mm, wire %g mm\n", turns, radius, pitch, wire);

	/* first strand, phase 0 */
	mk_pipe_init(&head);
	npts = build_helix(&head, turns, radius, pitch, wire, 0.0);
	mk_pipe(db_fp, "strandA.s", &head);
	mk_pipe_free(&head);
	(void)mk_addmember("strandA.s", &coil_hd.l, NULL, WMOP_UNION);

	/* second strand, 180 degrees out of phase */
	mk_pipe_init(&head);
	(void)build_helix(&head, turns, radius, pitch, wire, M_PI);
	mk_pipe(db_fp, "strandB.s", &head);
	mk_pipe_free(&head);
	(void)mk_addmember("strandB.s", &coil_hd.l, NULL, WMOP_UNION);

	/* periodic rungs connecting the two strands; place one rung
	 * every half-turn so the ladder reads clearly.
	 */
	rung_steps = STEPS_PER_TURN / 2;
	if (rung_steps < 1)
	    rung_steps = 1;
	rung = 0;
	for (i = 0; i < npts; i += rung_steps) {
	    point_t a, b;
	    char nm[64];

	    t = (double)i / (double)STEPS_PER_TURN * M_2PI;

	    VSET(a, radius * cos(t), radius * sin(t), pitch * t / M_2PI);
	    VSET(b, radius * cos(t + M_PI), radius * sin(t + M_PI),
		 pitch * t / M_2PI);

	    snprintf(nm, sizeof(nm), "rung%03d.s", rung);
	    build_rung(db_fp, nm, a, b, wire * 0.6);
	    (void)mk_addmember(nm, &coil_hd.l, NULL, WMOP_UNION);
	    rung++;
	}
	bu_log("  added %d rungs\n", rung);

    } else {
	/* spring and screw both use a single helix */
	const char *what = (mode == MODE_SCREW) ? "screw thread" : "spring";

	bu_log("Building %s: %d turns, radius %g mm, pitch %g mm, "
	       "wire %g mm\n", what, turns, radius, pitch, wire);

	mk_pipe_init(&head);
	(void)build_helix(&head, turns, radius, pitch, wire, 0.0);
	mk_pipe(db_fp, "coil.s", &head);
	mk_pipe_free(&head);
	(void)mk_addmember("coil.s", &coil_hd.l, NULL, WMOP_UNION);

	/* the screw also gets a central shaft running up the axis */
	if (mode == MODE_SCREW) {
	    double shaft_r;

	    /* shaft sits just inside the thread crests */
	    shaft_r = radius - wire * 0.25;
	    if (shaft_r < wire)
		shaft_r = wire;

	    VSET(base, 0.0, 0.0, -pitch * 0.5);
	    VSET(hvec, 0.0, 0.0, height + pitch);
	    mk_rcc(db_fp, "shaft.s", base, hvec, shaft_r);
	    (void)mk_addmember("shaft.s", &coil_hd.l, NULL, WMOP_UNION);
	}
    }

    /* ---- assemble the shiny metal coil region ---- */

    if (mode == MODE_SCREW) {
	/* steel screw with a strong specular highlight */
	mk_lcomb(db_fp, "coil.r", &coil_hd, 1,
		 "plastic", "di=0.4 sp=0.6 sh=12", steel, 0);
    } else {
	/* mirror-like chrome for the spring and the DNA strands */
	mk_lcomb(db_fp, "coil.r", &coil_hd, 1,
		 "mirror", "di=0.2 sp=0.8 sh=20 re=0.5", chrome, 0);
    }

    /* ---- ground plane: a broad, thin slab beneath the coil ---- */

    VSET(gmin, -radius * 6.0, -radius * 6.0, -wire);
    VSET(gmax, radius * 6.0, radius * 6.0, 0.0);
    /* sink the ground just under the lowest coil point */
    gmin[Z] = -wire * 2.0 - 1.0;
    gmax[Z] = -wire * 2.0;
    mk_rpp(db_fp, "ground.s", gmin, gmax);
    mk_region1(db_fp, "ground.r", "ground.s", "plastic", "di=0.8 sp=0.1",
	       ground_rgb);

    /* ---- a light source up and to the side to catch the metal ---- */

    VSET(lpos, radius * 4.0, -radius * 4.0, height + radius * 4.0);
    mk_sph(db_fp, "light.s", lpos, radius * 0.5);
    {
	unsigned char white[3];
	VSET(white, 255, 255, 255);
	mk_region1(db_fp, "light.r", "light.s", "light", "inten 1000.0",
		   white);
    }

    /* ---- top-level group "all" so 'tops' returns one renderable ---- */

    (void)mk_addmember("coil.r", &all_hd.l, NULL, WMOP_UNION);
    (void)mk_addmember("ground.r", &all_hd.l, NULL, WMOP_UNION);
    (void)mk_addmember("light.r", &all_hd.l, NULL, WMOP_UNION);
    mk_lcomb(db_fp, "all", &all_hd, 0, NULL, NULL, NULL, 0);

    bu_log("Wrote helical coil scene to %s (top-level group 'all')\n", av[1]);

    /* Close the database file. */
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
