/*                      L O F T W I N G . C
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
/** @file proc-db/loftwing.c
 *
 * Build a lofted airfoil wing, or a twisted fan/propeller, as ARS
 * (waterline) primitives.
 *
 * A NACA 4-digit airfoil is generated as a CLOSED outline of P points.
 * The unit-chord outline is then transformed for each of S spanwise
 * stations by chord taper, geometric twist (washout rotated about the
 * quarter-chord), leading-edge sweep, and dihedral, and placed at its
 * spanwise station.  Each station becomes one ARS "curve" (ring).  The
 * root ring is duplicated to form a flat closing cap, and the tip ring
 * is collapsed to a single repeated point (a pole) so the ARS is a
 * closed, valid solid that caps cleanly at root and tip.
 *
 * In "wing" mode the single lofted ARS is unioned into a top-level
 * group named "all".  In "prop" (blade) mode a single strongly twisted
 * blade ARS is built once and then instanced N times -- evenly rotated
 * about the hub axis -- around a central cylindrical hub (mk_rcc) to
 * form a complete propeller, all unioned into the "all" group.
 *
 * Since the values in the database are stored in millimeters, all the
 * coordinates produced here are in millimeters.
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


/* Quarter-chord is the conventional pitch/twist axis for an airfoil. */
#define QUARTER_CHORD 0.25

/* Reasonable, attractive defaults so that running with only an output
 * path yields a complete tapered, swept, twisted wing.
 */
#define DEF_NACA       2412     /* NACA 4-digit code, e.g. 2412 */
#define DEF_STATIONS   20       /* spanwise stations (interior rings) */
#define DEF_POINTS     80       /* points around the closed airfoil */
#define DEF_SPAN       1200.0   /* total span, mm (root to tip) */
#define DEF_ROOT_CHORD 350.0    /* chord length at the root, mm */
#define DEF_TIP_CHORD  180.0    /* chord length at the tip, mm */
#define DEF_TWIST      6.0      /* washout: degrees, tip pitched down */
#define DEF_SWEEP      18.0     /* leading-edge sweep angle, degrees */
#define DEF_DIHEDRAL   5.0      /* dihedral angle, degrees */
#define DEF_BLADES     3        /* number of propeller blades */
#define DEF_SEED       1        /* deterministic; reserved for variety */


/* mode of operation */
#define MODE_WING  0
#define MODE_BLADE 1


/*
 * Evaluate the NACA 4-digit mean-camber line and thickness at a chord
 * fraction x in [0, 1] for a unit chord.  Returns the camber yc and the
 * camber-line slope (dyc/dx); thickness is returned via *yt.
 *
 * The code is decoded as: m = first digit / 100 (max camber),
 * p = second digit / 10 (location of max camber), tt = last two digits
 * / 100 (max thickness fraction).
 */
static void
naca_eval(double x, double m, double p, double tt, double *yc, double *dyc, double *yt)
{
    /* Standard NACA 4-digit half-thickness distribution.  The final
     * coefficient is adjusted to -0.1036 (instead of the open-trailing-
     * edge -0.1015) so the upper and lower surfaces meet at a sharp,
     * closed trailing edge -- giving the classic airfoil silhouette.
     */
    *yt = 5.0 * tt * (0.2969 * sqrt(x)
		      - 0.1260 * x
		      - 0.3516 * x * x
		      + 0.2843 * x * x * x
		      - 0.1036 * x * x * x * x);

    if (p <= 0.0 || m <= 0.0) {
	/* symmetric airfoil: no camber */
	*yc = 0.0;
	*dyc = 0.0;
	return;
    }

    if (x < p) {
	double k = m / (p * p);
	*yc = k * (2.0 * p * x - x * x);
	*dyc = k * (2.0 * p - 2.0 * x);
    } else {
	double k = m / ((1.0 - p) * (1.0 - p));
	*yc = k * ((1.0 - 2.0 * p) + 2.0 * p * x - x * x);
	*dyc = k * (2.0 * p - 2.0 * x);
    }
}


/*
 * Generate the closed NACA airfoil outline for a unit chord, storing
 * npts (x, z) coordinate pairs into outx[]/outz[].  The outline starts
 * at the trailing edge, runs forward along the UPPER surface to the
 * leading edge, then back along the LOWER surface to the trailing edge.
 * The final point is forced exactly equal to the first so the ring
 * closes in 3D (required by the ARS primitive).
 *
 * Cosine spacing is used so the points cluster near the rounded leading
 * edge and the sharp trailing edge, which keeps the nose smooth.  The
 * leading-edge point (x=0) is shared by the two surfaces and emitted
 * exactly once.
 *
 * The airfoil lies in the local X-Z plane with the chord along +X.
 */
static void
naca_outline(int naca, size_t npts, double *outx, double *outz)
{
    double m, p, tt;
    double yc, dyc, yt, theta;
    double x, beta;
    size_t nupper, nlower, i, idx;

    /* Decode the 4-digit code into camber/position/thickness. */
    m  = ((naca / 1000) % 10) / 100.0;
    p  = ((naca / 100) % 10) / 10.0;
    tt = (naca % 100) / 100.0;

    /* The closed ring uses npts slots: the last slot duplicates the
     * first (trailing edge) to close the loop, leaving npts-1 distinct
     * vertices.  Split those between the upper and lower surfaces; the
     * leading-edge vertex (index nupper) is shared.
     *
     *   slot 0            : trailing edge, upper side
     *   slots 1..nupper-1 : upper surface, marching toward the LE
     *   slot nupper       : leading edge (x = 0), shared
     *   slots nupper+1..  : lower surface, marching back to the TE
     *   slot npts-1       : trailing edge again (== slot 0)
     */
    nupper = (npts - 1) / 2;        /* upper run, TE..(just before LE) */
    nlower = (npts - 1) - nupper;   /* lower run, (after LE)..TE */

    /* Upper surface: trailing edge (x=1) forward to leading edge (x=0). */
    for (i = 0; i < nupper; i++) {
	beta = 0.5 * M_PI * (double)i / (double)nupper;  /* 0 .. <pi/2 */
	x = cos(beta) * cos(beta);                       /* 1 .. >0 (cosine) */
	naca_eval(x, m, p, tt, &yc, &dyc, &yt);
	theta = atan(dyc);
	outx[i] = x - yt * sin(theta);
	outz[i] = yc + yt * cos(theta);
    }

    /* Shared leading-edge vertex at x = 0 (rounded nose tip). */
    naca_eval(0.0, m, p, tt, &yc, &dyc, &yt);
    outx[nupper] = 0.0;
    outz[nupper] = yc;

    /* Lower surface: leading edge (x=0) back to trailing edge (x=1). */
    for (i = 1; i <= nlower; i++) {
	idx = nupper + i;
	beta = 0.5 * M_PI * (double)i / (double)nlower;  /* >0 .. pi/2 */
	x = 1.0 - cos(beta) * cos(beta);                 /* >0 .. 1 (cosine) */
	naca_eval(x, m, p, tt, &yc, &dyc, &yt);
	theta = atan(dyc);
	outx[idx] = x + yt * sin(theta);
	outz[idx] = yc - yt * cos(theta);
    }

    /* Force the ring closed: last point coincides exactly with the
     * first (both are the trailing edge).
     */
    outx[npts - 1] = outx[0];
    outz[npts - 1] = outz[0];
}


/*
 * Build a lofted airfoil ARS from a unit-chord outline.  The named
 * solid is created in db_fp.  The loft tapers the chord linearly, sweeps
 * and dihedrals the leading edge, and applies a linearly increasing
 * geometric twist (washout) about the quarter-chord.  The root ring is
 * duplicated to form a flat closing cap and the tip ring collapses to a
 * pole so the resulting ARS is a closed, watertight solid.
 *
 * Returns 0 on success, nonzero on failure.
 */
static int
build_loft(struct rt_wdb *db_fp, const char *name,
	   const double *base_x, const double *base_z, size_t ppc,
	   int stations, double span,
	   double root_chord, double tip_chord,
	   double twist_deg, double sweep_deg, double dihedral_deg)
{
    fastf_t **curves;
    size_t ncurves;             /* stations + root cap + tip pole */
    size_t c, k;
    int ret;

    /* Total curves: a duplicate root ring (flat cap), the interior
     * station rings, and a collapsed tip pole.
     */
    ncurves = (size_t)stations + 2;

    curves = (fastf_t **)bu_calloc(ncurves, sizeof(fastf_t *), "ars curves");
    for (c = 0; c < ncurves; c++) {
	curves[c] = (fastf_t *)bu_calloc(ppc * ELEMENTS_PER_VECT,
					 sizeof(fastf_t), "ars curve");
    }

    for (c = 0; c < ncurves; c++) {
	double frac;            /* 0 at root .. 1 at tip */
	double chord;           /* local chord length */
	double yspan;           /* spanwise position (Y) */
	double xsweep;          /* leading-edge offset from sweep */
	double zdihedral;       /* vertical offset from dihedral */
	double twist;           /* local twist angle, radians */
	double ct, st;          /* cos/sin of twist */
	int is_tip;

	is_tip = (c == ncurves - 1);

	/* Curve 0 duplicates the root ring (frac 0) as a flat cap;
	 * curves 1..stations are the interior airfoil rings spanning
	 * frac (0,1]; the last curve is the collapsed tip pole.
	 */
	if (c == 0)
	    frac = 0.0;
	else
	    frac = (double)c / (double)(ncurves - 1);

	/* Linear taper of the chord from root to tip. */
	chord = root_chord + (tip_chord - root_chord) * frac;

	/* Spanwise station, leading-edge sweep, and dihedral rise. */
	yspan = span * frac;
	xsweep = yspan * tan(sweep_deg * DEG2RAD);
	zdihedral = yspan * tan(dihedral_deg * DEG2RAD);

	/* Geometric washout: linearly increasing nose-down twist toward
	 * the tip, applied about the quarter-chord point.
	 */
	twist = -(twist_deg * DEG2RAD) * frac;
	ct = cos(twist);
	st = sin(twist);

	if (is_tip) {
	    /* Collapse the ring to a single point at the quarter-chord
	     * so this curve forms a closing pole at the wing tip.
	     */
	    point_t pole;
	    VSET(pole,
		 xsweep + QUARTER_CHORD * chord,
		 yspan,
		 zdihedral);
	    for (k = 0; k < ppc; k++)
		VMOVE(&curves[c][k * ELEMENTS_PER_VECT], pole);
	    continue;
	}

	/* Lay out a full airfoil ring for this station. */
	for (k = 0; k < ppc; k++) {
	    double lx, lz;      /* local airfoil coords, unit chord */
	    double sx, sz;      /* scaled by chord, about quarter-chord */
	    double rx, rz;      /* after twist rotation */
	    point_t pt;

	    lx = base_x[k];
	    lz = base_z[k];

	    /* Scale to chord and shift so rotation is about quarter-chord. */
	    sx = (lx - QUARTER_CHORD) * chord;
	    sz = lz * chord;

	    /* Rotate about the spanwise (Y) axis for twist. */
	    rx = sx * ct - sz * st;
	    rz = sx * st + sz * ct;

	    /* Translate into world position: chordwise X (with sweep and
	     * quarter-chord origin restored), spanwise Y, vertical Z
	     * (with dihedral).
	     */
	    VSET(pt,
		 xsweep + QUARTER_CHORD * chord + rx,
		 yspan,
		 zdihedral + rz);
	    VMOVE(&curves[c][k * ELEMENTS_PER_VECT], pt);
	}
    }

    /* mk_ars takes ownership of the curves array and its rows. */
    ret = mk_ars(db_fp, name, ncurves, ppc, curves);
    return ret;
}


static void
usage(const char *prog)
{
    bu_log("Usage: %s output.g [options]\n", prog);
    bu_log("  Builds a lofted NACA-airfoil wing, or a twisted propeller, as ARS.\n");
    bu_log("Options (all optional; sensible defaults applied):\n");
    bu_log("  --mode wing|prop    wing (default) or twisted propeller\n");
    bu_log("  --naca NNNN         NACA 4-digit code (default %d)\n", DEF_NACA);
    bu_log("  --stations N        spanwise stations (default %d)\n", DEF_STATIONS);
    bu_log("  --points N          points around airfoil (default %d)\n", DEF_POINTS);
    bu_log("  --span MM           total span/blade length (default %g)\n", DEF_SPAN);
    bu_log("  --root-chord MM     chord at root (default %g)\n", DEF_ROOT_CHORD);
    bu_log("  --tip-chord MM      chord at tip (default %g)\n", DEF_TIP_CHORD);
    bu_log("  --twist DEG         washout, tip pitched down (default %g)\n", DEF_TWIST);
    bu_log("  --sweep DEG         leading-edge sweep (default %g)\n", DEF_SWEEP);
    bu_log("  --dihedral DEG      dihedral angle (default %g)\n", DEF_DIHEDRAL);
    bu_log("  --blades N          propeller blade count, prop mode (default %d)\n", DEF_BLADES);
    bu_log("  --seed N            random seed (default %d)\n", DEF_SEED);
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    struct wmember wm_hd;
    unsigned char rgb[3];

    /* tunable parameters with defaults */
    int mode = MODE_WING;
    int naca = DEF_NACA;
    int stations = DEF_STATIONS;
    int points = DEF_POINTS;
    double span = DEF_SPAN;
    double root_chord = DEF_ROOT_CHORD;
    double tip_chord = DEF_TIP_CHORD;
    double twist_deg = DEF_TWIST;
    double sweep_deg = DEF_SWEEP;
    double dihedral_deg = DEF_DIHEDRAL;
    int blades = DEF_BLADES;
    int seed = DEF_SEED;

    /* Track which floating-point tunables the caller explicitly set, so
     * blade mode can substitute its own defaults only for the untouched
     * ones.  (Comparing the doubles against their defaults would be both
     * unreliable and a -Wfloat-equal violation.)
     */
    int set_span = 0;
    int set_root_chord = 0;
    int set_tip_chord = 0;
    int set_twist = 0;
    int set_sweep = 0;
    int set_dihedral = 0;

    /* working storage */
    double *base_x = NULL;      /* unit-chord airfoil X coords */
    double *base_z = NULL;      /* unit-chord airfoil Z coords */
    size_t ppc;                 /* points per curve */
    int i;

    bu_setprogname(av[0]);

    if (ac < 2) {
	usage(av[0]);
	bu_exit(1, "Usage: %s output.g [params]\n", av[0]);
    }

    /* Parse the optional arguments.  av[1] is always the output path. */
    for (i = 2; i < ac; i++) {
	if (BU_STR_EQUAL(av[i], "--mode") && i + 1 < ac) {
	    i++;
	    if (BU_STR_EQUAL(av[i], "blade") || BU_STR_EQUAL(av[i], "prop"))
		mode = MODE_BLADE;
	    else
		mode = MODE_WING;
	} else if (BU_STR_EQUAL(av[i], "--naca") && i + 1 < ac) {
	    naca = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--stations") && i + 1 < ac) {
	    stations = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--points") && i + 1 < ac) {
	    points = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--span") && i + 1 < ac) {
	    span = atof(av[++i]);
	    set_span = 1;
	} else if (BU_STR_EQUAL(av[i], "--root-chord") && i + 1 < ac) {
	    root_chord = atof(av[++i]);
	    set_root_chord = 1;
	} else if (BU_STR_EQUAL(av[i], "--tip-chord") && i + 1 < ac) {
	    tip_chord = atof(av[++i]);
	    set_tip_chord = 1;
	} else if (BU_STR_EQUAL(av[i], "--twist") && i + 1 < ac) {
	    twist_deg = atof(av[++i]);
	    set_twist = 1;
	} else if (BU_STR_EQUAL(av[i], "--sweep") && i + 1 < ac) {
	    sweep_deg = atof(av[++i]);
	    set_sweep = 1;
	} else if (BU_STR_EQUAL(av[i], "--dihedral") && i + 1 < ac) {
	    dihedral_deg = atof(av[++i]);
	    set_dihedral = 1;
	} else if (BU_STR_EQUAL(av[i], "--blades") && i + 1 < ac) {
	    blades = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--seed") && i + 1 < ac) {
	    seed = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "-h") || BU_STR_EQUAL(av[i], "--help")) {
	    usage(av[0]);
	    return 0;
	} else {
	    bu_log("Warning: ignoring unrecognized argument '%s'\n", av[i]);
	}
    }

    /* seed is reserved for future variety; reference it so it is not an
     * unused parameter and so deterministic runs stay reproducible.
     */
    bn_randmt_seed((unsigned long)seed);

    /* A fan/propeller blade is a much more aggressively twisted, more
     * sharply tapered wing with a more symmetric airfoil.  Adjust the
     * defaults for visual punch when the caller did not override them.
     */
    if (mode == MODE_BLADE) {
	if (!set_twist)
	    twist_deg = 38.0;       /* strong root-to-tip washout */
	if (!set_sweep)
	    sweep_deg = 6.0;
	if (!set_dihedral)
	    dihedral_deg = 0.0;
	if (NEAR_EQUAL(naca, DEF_NACA, SMALL_FASTF))
	    naca = 4412;            /* fatter, draggier propeller section */
	if (!set_root_chord)
	    root_chord = 140.0;
	if (!set_tip_chord)
	    tip_chord = 90.0;
	if (!set_span)
	    span = 700.0;           /* blade length from hub to tip */
    }

    /* Sanity-clamp counts so the ARS is always valid. */
    if (points < 8)
	points = 8;
    if (stations < 2)
	stations = 2;
    if (blades < 1)
	blades = 1;

    /* The closed airfoil ring has 'points' entries (last == first). */
    ppc = (size_t)points;

    bu_log("loftwing: building a %s\n", (mode == MODE_BLADE) ? "propeller" : "wing");
    bu_log("  NACA %04d, %d stations, %d points/airfoil\n", naca, stations, points);
    bu_log("  span=%g  root=%g  tip=%g (mm)\n", span, root_chord, tip_chord);
    bu_log("  twist=%g  sweep=%g  dihedral=%g (deg)\n",
	   twist_deg, sweep_deg, dihedral_deg);
    if (mode == MODE_BLADE)
	bu_log("  blades=%d\n", blades);

    /* Open/Create the database file for writing. */
    if ((db_fp = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	return 2;
    }

    mk_id_units(db_fp, (mode == MODE_BLADE) ? "Twisted Propeller" : "Lofted Airfoil Wing", "mm");

    /* Generate the canonical unit-chord airfoil outline once; every
     * station is a scaled/rotated/translated copy of it.
     */
    base_x = (double *)bu_calloc(ppc, sizeof(double), "base_x");
    base_z = (double *)bu_calloc(ppc, sizeof(double), "base_z");
    naca_outline(naca, ppc, base_x, base_z);

    if (mode == MODE_WING) {
	/* Build the single lofted wing solid. */
	if (build_loft(db_fp, "wing.s", base_x, base_z, ppc,
		       stations, span, root_chord, tip_chord,
		       twist_deg, sweep_deg, dihedral_deg) != 0) {
	    bu_exit(1, "loftwing: mk_ars failed building the wing\n");
	}

	bu_free(base_x, "base_x");
	bu_free(base_z, "base_z");

	/* Wing region with a shiny brushed-aluminum plastic shader. */
	BU_LIST_INIT(&wm_hd.l);
	(void)mk_addmember("wing.s", &wm_hd.l, NULL, WMOP_UNION);

	VSET(rgb, 170, 175, 185);   /* brushed-aluminum gray */
	mk_lcomb(db_fp, "wing.r", &wm_hd, 1,
		 "plastic", "di=0.5 sp=0.7 sh=24", rgb, 0);

	/* Top-level group named "all". */
	BU_LIST_INIT(&wm_hd.l);
	(void)mk_addmember("wing.r", &wm_hd.l, NULL, WMOP_UNION);
	mk_lcomb(db_fp, "all", &wm_hd, 0, NULL, NULL, NULL, 0);

	bu_log("loftwing: wrote %s (top-level group 'all')\n", av[1]);
    } else {
	/* Propeller: build ONE strongly twisted blade ARS, then instance
	 * it N times -- evenly rotated about the hub spin axis (Z) -- and
	 * union it with a central cylindrical hub.
	 *
	 * The blade ARS is built in the wing frame: chord along X, span
	 * along +Y, thickness along Z.  To make the blade radiate from
	 * the hub (which spins about Z) we first rotate it so the span
	 * runs along +X (radially outward) standing up in the disc, then
	 * we rotate each instance about Z.
	 */
	point_t hub_base, hub_apex;
	double hub_radius;
	double hub_half;
	int b;

	if (build_loft(db_fp, "blade.s", base_x, base_z, ppc,
		       stations, span, root_chord, tip_chord,
		       twist_deg, sweep_deg, dihedral_deg) != 0) {
	    bu_exit(1, "loftwing: mk_ars failed building the blade\n");
	}

	bu_free(base_x, "base_x");
	bu_free(base_z, "base_z");

	/* Central hub: a short cylinder spinning about Z, sized so the
	 * blade roots are comfortably buried in it.
	 */
	hub_radius = root_chord * 0.9;
	hub_half = root_chord * 0.45;
	VSET(hub_base, 0.0, 0.0, -hub_half);
	VSET(hub_apex, 0.0, 0.0, 2.0 * hub_half);
	if (mk_rcc(db_fp, "hub.s", hub_base, hub_apex, hub_radius) != 0) {
	    bu_exit(1, "loftwing: mk_rcc failed building the hub\n");
	}

	/* Hub region: dark plastic. */
	BU_LIST_INIT(&wm_hd.l);
	(void)mk_addmember("hub.s", &wm_hd.l, NULL, WMOP_UNION);
	VSET(rgb, 60, 60, 65);
	mk_lcomb(db_fp, "hub.r", &wm_hd, 1,
		 "plastic", "di=0.6 sp=0.3 sh=10", rgb, 0);

	/* Blade region: brushed-aluminum plastic, shared by all instances. */
	BU_LIST_INIT(&wm_hd.l);
	(void)mk_addmember("blade.s", &wm_hd.l, NULL, WMOP_UNION);
	VSET(rgb, 170, 175, 185);
	mk_lcomb(db_fp, "blade.r", &wm_hd, 1,
		 "plastic", "di=0.5 sp=0.7 sh=24", rgb, 0);

	/* Top-level propeller group: the hub plus N rotated blade
	 * instances.  Each blade is first stood up so its span points
	 * radially outward (rotate -90 deg about X maps +Y span to +Z,
	 * then we lay the disc in the X-Y plane by rotating about Y),
	 * then spun to its angular position about Z.
	 */
	BU_LIST_INIT(&wm_hd.l);
	(void)mk_addmember("hub.r", &wm_hd.l, NULL, WMOP_UNION);

	for (b = 0; b < blades; b++) {
	    mat_t orient;       /* span +Y -> radial +X, in the disc plane */
	    mat_t spin;         /* angular position about the Z spin axis */
	    mat_t place;        /* combined placement */
	    double deg;
	    struct wmember *wm;

	    /* Map the blade's spanwise +Y axis onto the radial +X axis so
	     * the blade sticks straight out from the hub in the rotor
	     * disc; the thickness (Z) stays along Z.  A -90 deg rotation
	     * about Z takes +Y to +X.
	     */
	    bn_mat_angles(orient, 0.0, 0.0, -90.0);

	    /* Spin this instance to its station around the hub. */
	    deg = 360.0 * (double)b / (double)blades;
	    bn_mat_angles(spin, 0.0, 0.0, deg);

	    /* place = spin * orient */
	    bn_mat_mul(place, spin, orient);

	    wm = mk_addmember("blade.r", &wm_hd.l, place, WMOP_UNION);
	    if (wm == NULL)
		bu_exit(1, "loftwing: failed to instance blade %d\n", b);
	}

	mk_lcomb(db_fp, "all", &wm_hd, 0, NULL, NULL, NULL, 0);

	bu_log("loftwing: wrote %s (%d-blade propeller, top-level group 'all')\n",
	       av[1], blades);
    }

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
