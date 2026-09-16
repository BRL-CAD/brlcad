/*                      T E R R A G E N . C
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
/** @file proc-db/terragen.c
 *
 * Procedural fractal terrain generator.
 *
 * This teaching demo synthesizes a (2^n + 1) x (2^n + 1) height field
 * using the classic "diamond-square" midpoint-displacement algorithm,
 * then layers a couple of fractional-Brownian-motion (bn_noise_fbm)
 * octaves on top for fine detail.  The field is normalized to the
 * unsigned 16-bit range, written to a big-endian (network byte order)
 * sidecar ".dsp" file next to the output geometry database, and wrapped
 * in a BRL-CAD displacement-map (DSP) primitive via mk_dsp().
 *
 * Around the terrain the program builds a small but complete scene: a
 * reflective blue water slab at a chosen sea level, an inverted sky
 * dome, and a sun modeled as a BRL-CAD "light" region.  Everything is
 * unioned under a single top-level group named "all" so the scene can
 * be rendered by naming one object.
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


/* A simple, self-contained linear congruential generator so that runs
 * are reproducible from a --seed parameter and we never call time().
 * (Numerical Recipes / glibc-style constants.)
 */
static unsigned long lcg_state = 1UL;

static void
lcg_seed(unsigned long s)
{
    /* avoid a zero state */
    lcg_state = s ? s : 1UL;
}

/* return a pseudo-random double in [0.0, 1.0) */
static double
lcg_rand(void)
{
    lcg_state = (1103515245UL * lcg_state + 12345UL) & 0x7fffffffUL;
    return (double)lcg_state / (double)0x80000000UL;
}

/* return a pseudo-random double in [-1.0, 1.0) */
static double
lcg_signed(void)
{
    return 2.0 * lcg_rand() - 1.0;
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    struct wmember all_hd;	/* top-level "all" group  */
    struct wmember terr_hd;	/* terrain region member  */
    struct wmember sky_hd;	/* sky dome region member */

    /* tunable parameters (all optional, with good defaults) */
    int exponent = 8;		/* --size: grid is (2^exp)+1 on a side */
    unsigned long seed = 12345UL;	/* --seed */
    double relief = 25000.0;	/* --relief: peak-to-trough Z range, mm */
    double sea = 0.30;		/* --sea: sea level as 0..1 fraction   */

    int dim;			/* (2^exponent)+1, cells per side      */
    int step;			/* current diamond-square span         */
    double scale;		/* current displacement magnitude      */
    double *hf = NULL;		/* working height field, row-major     */
    unsigned short *grid = NULL;	/* normalized 16-bit output       */
    double hmin, hmax, hrange;
    int i, x, y;

    /* world extent of the terrain footprint, in mm */
    double world = 200000.0;	/* 200 m square */
    double cell;		/* mm per DSP cell along X and Y */

    char dspfile[2048];
    char *dspbase;	/* basename stored in the DSP (resolved at render) */
    char *dot;
    char *sep;
    FILE *fp;

    point_t p1, p2;
    point_t scenter, lpos;
    vect_t laim;
    mat_t dspmat;
    unsigned char rgb[3];

    bu_setprogname(av[0]);

    if (ac < 2) {
	bu_exit(1, "Usage: %s output.g [--size n] [--seed s] [--relief mm] [--sea frac]\n", av[0]);
    }

    /* Parse optional parameters.  Each is a "--flag value" pair; we scan
     * from av[2] onward so the only required argument is the .g path.
     */
    for (i = 2; i < ac - 1; i += 2) {
	if (BU_STR_EQUAL(av[i], "--size")) {
	    exponent = atoi(av[i + 1]);
	} else if (BU_STR_EQUAL(av[i], "--seed")) {
	    seed = (unsigned long)strtoul(av[i + 1], NULL, 10);
	} else if (BU_STR_EQUAL(av[i], "--relief")) {
	    relief = atof(av[i + 1]);
	} else if (BU_STR_EQUAL(av[i], "--sea")) {
	    sea = atof(av[i + 1]);
	} else {
	    bu_log("Warning: ignoring unknown option '%s'\n", av[i]);
	}
    }

    /* sanity-clamp the parameters so the program is robust */
    if (exponent < 2) exponent = 2;
    if (exponent > 11) exponent = 11;	/* 2049^2 is plenty */
    if (relief < 1.0) relief = 1.0;
    if (sea < 0.0) sea = 0.0;
    if (sea > 1.0) sea = 1.0;

    dim = (1 << exponent) + 1;
    cell = world / (double)(dim - 1);

    bu_log("terragen: %dx%d height field, seed=%lu, relief=%g mm, sea=%g\n",
	   dim, dim, seed, relief, sea);

    lcg_seed(seed);

    /* Open/create the database file for writing. */
    if ((db_fp = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	return 2;
    }

    mk_id_units(db_fp, "Fractal DSP Terrain", "mm");

    /* ---- Allocate and seed the working height field. ---------------- */
    hf = (double *)bu_calloc((size_t)dim * (size_t)dim, sizeof(double), "height field");

    /* Seed the four corners with random starting elevations. */
    hf[0 * dim + 0] = lcg_rand();
    hf[0 * dim + (dim - 1)] = lcg_rand();
    hf[(dim - 1) * dim + 0] = lcg_rand();
    hf[(dim - 1) * dim + (dim - 1)] = lcg_rand();

    /* ---- Diamond-square midpoint displacement. --------------------- *
     * On each pass we halve the step and the displacement magnitude.
     * The "diamond" step sets the center of each square from its four
     * corners; the "square" step sets the edge midpoints from their
     * (up to four) diamond neighbors.
     */
    scale = 1.0;
    for (step = dim - 1; step > 1; step /= 2) {
	int half = step / 2;

	/* diamond step: centers of squares */
	for (y = 0; y + step < dim; y += step) {
	    for (x = 0; x + step < dim; x += step) {
		double avg =
		    hf[(y) * dim + (x)] +
		    hf[(y) * dim + (x + step)] +
		    hf[(y + step) * dim + (x)] +
		    hf[(y + step) * dim + (x + step)];
		avg *= 0.25;
		hf[(y + half) * dim + (x + half)] =
		    avg + lcg_signed() * scale;
	    }
	}

	/* square step: edge midpoints.  We walk a staggered grid so each
	 * diamond-shaped neighborhood is visited; missing neighbors off
	 * the edge are simply skipped from the average.
	 */
	for (y = 0; y < dim; y += half) {
	    int xstart = ((y / half) % 2 == 0) ? half : 0;
	    for (x = xstart; x < dim; x += step) {
		double sum = 0.0;
		int cnt = 0;
		if (x - half >= 0)   { sum += hf[(y) * dim + (x - half)]; cnt++; }
		if (x + half < dim)  { sum += hf[(y) * dim + (x + half)]; cnt++; }
		if (y - half >= 0)   { sum += hf[(y - half) * dim + (x)]; cnt++; }
		if (y + half < dim)  { sum += hf[(y + half) * dim + (x)]; cnt++; }
		if (cnt > 0)
		    hf[y * dim + x] = sum / (double)cnt + lcg_signed() * scale;
	    }
	}

	scale *= 0.5;	/* roughness: each octave contributes less */
    }

    /* ---- Add a couple of fBm octaves for fine surface detail. ------ *
     * bn_noise_fbm samples a coherent procedural noise field; we sample
     * it at a frequency that maps the whole terrain across several noise
     * periods and add a small fraction of its amplitude.
     */
    for (y = 0; y < dim; y++) {
	for (x = 0; x < dim; x++) {
	    point_t np;
	    double n;
	    VSET(np,
		 (double)x / (double)dim * 6.0,
		 (double)y / (double)dim * 6.0,
		 0.5);
	    /* h_val=1.0, lacunarity=2.0, octaves=4 */
	    n = bn_noise_fbm(np, 1.0, 2.0, 4.0);
	    hf[y * dim + x] += 0.15 * n;
	}
    }

    /* ---- Normalize the field to the unsigned 16-bit range. --------- */
    hmin = hmax = hf[0];
    for (i = 0; i < dim * dim; i++) {
	if (hf[i] < hmin) hmin = hf[i];
	if (hf[i] > hmax) hmax = hf[i];
    }
    hrange = hmax - hmin;
    if (hrange < 1.0e-12) hrange = 1.0;	/* guard a flat field */

    grid = (unsigned short *)bu_calloc((size_t)dim * (size_t)dim,
				       sizeof(unsigned short), "dsp grid");
    for (i = 0; i < dim * dim; i++) {
	double t = (hf[i] - hmin) / hrange;	/* 0..1 */
	long v = (long)(t * 65535.0 + 0.5);
	if (v < 0) v = 0;
	if (v > 65535) v = 65535;
	grid[i] = (unsigned short)v;
    }

    /* ---- Derive the sidecar .dsp path from the .g path. ------------ *
     * Replace a trailing ".g" with ".dsp"; otherwise append ".dsp".
     * The file lands in the same directory as the .g so the relative
     * name stored in the DSP primitive resolves correctly.
     */
    bu_strlcpy(dspfile, av[1], sizeof(dspfile));
    dot = strrchr(dspfile, '.');
    if (dot && BU_STR_EQUAL(dot, ".g")) {
	*dot = '\0';
	bu_strlcat(dspfile, ".dsp", sizeof(dspfile));
    } else {
	bu_strlcat(dspfile, ".dsp", sizeof(dspfile));
    }

    /* ---- Write the DSP sidecar as big-endian unsigned shorts. ------ *
     * The DSP file format is row-major unsigned 16-bit network order:
     * high byte first, then low byte, for each sample.
     */
    fp = fopen(dspfile, "wb");
    if (fp == NULL) {
	perror(dspfile);
	bu_exit(3, "terragen: could not open sidecar '%s' for writing\n", dspfile);
    }
    for (i = 0; i < dim * dim; i++) {
	unsigned char hi = (unsigned char)((grid[i] >> 8) & 0xff);
	unsigned char lo = (unsigned char)(grid[i] & 0xff);
	putc(hi, fp);
	putc(lo, fp);
    }
    fclose(fp);
    bu_log("terragen: wrote DSP height data to %s\n", dspfile);

    /* ---- Build the DSP primitive. ---------------------------------- *
     * DSP local coordinates are cell-indexed in X and Y (0..dim-1) and
     * the raw stored height (0..65535) in Z.  The solid-to-model matrix
     * scales those local units into millimeters: mat[0] mm per X cell,
     * mat[5] mm per Y cell, mat[10] mm per height unit.  The relief is
     * the total world Z range divided across the full 16-bit span.
     */
    MAT_IDN(dspmat);
    dspmat[0]  = cell;			/* X: mm per cell */
    dspmat[5]  = cell;			/* Y: mm per cell */
    dspmat[10] = relief / 65535.0;	/* Z: mm per height unit */
    /* Center the footprint about the world origin in X and Y. */
    dspmat[3]  = -world * 0.5;
    dspmat[7]  = -world * 0.5;
    dspmat[11] = 0.0;

    /* The DSP/EBM data-file reference is resolved at render time against
     * a search path (cwd and the .g's directory), so store only the bare
     * basename -- an absolute path gets a "./" prefix and fails to open.
     * The sidecar already sits next to the .g, so the basename resolves.
     */
    dspbase = dspfile;
    sep = strrchr(dspfile, '/');
    if (sep) dspbase = sep + 1;
    sep = strrchr(dspbase, '\\');
    if (sep) dspbase = sep + 1;

    mk_dsp(db_fp, "terrain.s", dspbase, (size_t)dim, (size_t)dim, dspmat);

    /* Wrap the terrain solid in an earthy plastic region. */
    BU_LIST_INIT(&terr_hd.l);
    (void)mk_addmember("terrain.s", &terr_hd.l, NULL, WMOP_UNION);
    VSET(rgb, 110, 95, 70);	/* rock / soil brown */
    mk_lcomb(db_fp, "terrain.r", &terr_hd, 1,
	     "plastic", "di=0.8 sp=0.1", rgb, 0);

    /* ---- Water: a large thin slab at sea level, reflective glass. -- *
     * Sea level rides at the chosen fraction of the relief range.  The
     * slab spans well beyond the terrain footprint so the shoreline
     * reads cleanly.
     */
    {
	double sealevel = sea * relief;
	double margin = world * 0.1;	/* extend just past the shoreline */
	VSET(p1, -world * 0.5 - margin, -world * 0.5 - margin, sealevel - 50.0);
	VSET(p2,  world * 0.5 + margin,  world * 0.5 + margin, sealevel);
	mk_rpp(db_fp, "water.s", p1, p2);

	BU_LIST_INIT(&terr_hd.l);
	(void)mk_addmember("water.s", &terr_hd.l, NULL, WMOP_UNION);
	VSET(rgb, 40, 90, 170);		/* deep blue */
	mk_lcomb(db_fp, "water.r", &terr_hd, 1,
		 "glass", "ri=1.33 sp=0.9 di=0.1 tr=0.5", rgb, 0);
    }

    /* ---- Sky: a cloud environment map, not enclosing geometry. ------ *
     * Using the "envmap" shader (with the procedural "cloud" texture, as
     * sphflake/donuts do) gives a sky-blue clouded backdrop for every ray
     * that escapes the scene -- including the ambient-occlusion sample
     * rays from "set ambSamples".  A physical enclosing dome would instead
     * occlude those AO rays and darken the whole terrain, so the sky is an
     * environment map rather than a solid.  A small carrier sphere placed
     * far below the scene holds the region; its geometry is not seen.
     */
    VSET(scenter, 0.0, 0.0, -world * 4.0);
    mk_sph(db_fp, "sky.s", scenter, world * 0.01);
    BU_LIST_INIT(&sky_hd.l);
    (void)mk_addmember("sky.s", &sky_hd.l, NULL, WMOP_UNION);
    VSET(rgb, 130, 180, 240);		/* light sky blue */
    mk_lcomb(db_fp, "sky.r", &sky_hd, 1, "envmap", "cloud", rgb, 0);

    /* ---- Sun: a small sphere made into a BRL-CAD light region. ----- *
     * The "light" shader turns a region into a light source.  We place
     * it high and to one side so the terrain casts long shadows.
     */
    VSET(lpos, world * 0.5, -world * 0.5, world * 2.0);
    (void)laim;				/* aim handled implicitly by position */
    mk_sph(db_fp, "sun.s", lpos, world * 0.03);
    VSET(rgb, 255, 255, 240);		/* warm white */
    /* shadows=0: this light ignores occlusion, so the enclosing sky shell
     * cannot block it -- the terrain stays brightly lit from above. */
    mk_region1(db_fp, "sun.r", "sun.s", "light", "shadows=0 inten=3", rgb);

    /* ---- Top-level group "all" unions the complete scene. ---------- *
     * Everything -- terrain, water, the enclosing sky dome, and the sun
     * light -- goes into "all" so naming one object yields a finished,
     * fully renderable scene.  The sky dome is huge and would dominate a
     * default auto-sized view, but the ray tracer's "autoview" command
     * (run separately, against just terrain.r/water.r) frames the camera
     * on the terrain while the dome still encloses the scene.
     */
    BU_LIST_INIT(&all_hd.l);
    (void)mk_addmember("terrain.r", &all_hd.l, NULL, WMOP_UNION);
    (void)mk_addmember("water.r", &all_hd.l, NULL, WMOP_UNION);
    (void)mk_addmember("sky.r", &all_hd.l, NULL, WMOP_UNION);
    mk_lcomb(db_fp, "all", &all_hd, 0, NULL, NULL, NULL, 0);
    /* sun.r stays a separate top-level object, NOT a member of "all":
     * defining an explicit light region suppresses the ray tracer's
     * default lighting, leaving the autoview-framed scene dark.  With the
     * sky shell but no in-tree light, rt's default lighting plus the -A
     * ambient term render the terrain brightly under a sky-blue backdrop.
     */

    bu_log("terragen: wrote scene to %s (top-level group 'all')\n", av[1]);

    /* ---- Clean up. ------------------------------------------------- */
    bu_free(hf, "height field");
    bu_free(grid, "dsp grid");

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
