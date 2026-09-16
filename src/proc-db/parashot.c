/*                      P A R A S H O T . C
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
/** @file proc-db/parashot.c
 *
 * Generate gravity-affected, non-linear ("parabolic") fragment flyout
 * shotlines and emit them as BRL-CAD pipe primitives in a .g file so
 * the trajectories can be viewed/analyzed directly in BRL-CAD tools
 * (mged, archer, rt).
 *
 * Given a launch origin P0, an initial velocity V0 and a constant
 * gravity vector g, constant-acceleration projectile motion has the
 * closed-form solution
 *
 *     P(t) = P0 + V0*t + 0.5*g*t^2
 *
 * which is exact (no numerical integration error) for constant g.  The
 * trajectory is sampled at N steps (CLI parameter -n) from t=0 to a
 * stop time t_end (ground impact, or an explicit -t max time), and the
 * sampled points are strung into a "shotline.pipe" pipe primitive.  A
 * gravity-free "baseline.pipe" (straight line P0 + V0*t) is emitted for
 * an at-a-glance curved-vs-straight contrast.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/str.h"
#include "raytrace.h"
#include "wdb.h"


static const char *progname = "parashot";


static void
Usage(void)
{
    fprintf(stderr, "Usage: %s [options] output.g\n", progname);
    fprintf(stderr, "  Generate a gravity-affected (parabolic) shotline as BRL-CAD pipe(s).\n");
    fprintf(stderr, "  -p x,y,z   launch origin (default 0,0,0)\n");
    fprintf(stderr, "  -v x,y,z   initial velocity vector, m/s (default 300,0,150)\n");
    fprintf(stderr, "  -g x,y,z   gravity vector, m/s^2 (default 0,0,-9.80665)\n");
    fprintf(stderr, "  -n N       number of trajectory steps (default 64)\n");
    fprintf(stderr, "  -t tmax    maximum flight time, s (overrides ground impact)\n");
    fprintf(stderr, "  -z ground  ground plane z; flight stops at impact (default 0)\n");
    fprintf(stderr, "  -d od      pipe outer diameter (default 0.5)\n");
    fprintf(stderr, "  -h -?      this help\n");
}


/*
 * Parse a "x,y,z" (comma and/or whitespace separated) triple into v.
 * Returns 0 on success, -1 on parse failure.
 */
static int
parse_vect(const char *arg, vect_t v)
{
    double x, y, z;

    if (arg == NULL)
	return -1;

    if (sscanf(arg, "%lf , %lf , %lf", &x, &y, &z) != 3 &&
	sscanf(arg, "%lf %lf %lf", &x, &y, &z) != 3)
	return -1;

    VSET(v, x, y, z);
    return 0;
}


int
main(int argc, char **argv)
{
    point_t p0 = VINIT_ZERO;
    vect_t v0;
    vect_t grav;
    int nsteps = 64;
    double tmax = -1.0;		/* <0 => not user-specified */
    double ground_z = 0.0;
    double od = 0.5;
    const char *outfile;

    struct rt_wdb *fp;
    struct bu_list head;
    point_t pt;
    double t, dt, t_end;
    double disc;
    int i;
    int optc;

    bu_setprogname(argv[0]);
    progname = argv[0];

    /* defaults */
    VSET(v0, 300.0, 0.0, 150.0);
    VSET(grav, 0.0, 0.0, -9.80665);

    while ((optc = bu_getopt(argc, argv, "p:v:g:n:t:z:d:h?")) != -1) {
	switch (optc) {
	    case 'p':
		if (parse_vect(bu_optarg, p0) < 0) {
		    fprintf(stderr, "%s: bad -p origin '%s'\n", progname, bu_optarg);
		    return 1;
		}
		break;
	    case 'v':
		if (parse_vect(bu_optarg, v0) < 0) {
		    fprintf(stderr, "%s: bad -v velocity '%s'\n", progname, bu_optarg);
		    return 1;
		}
		break;
	    case 'g':
		if (parse_vect(bu_optarg, grav) < 0) {
		    fprintf(stderr, "%s: bad -g gravity '%s'\n", progname, bu_optarg);
		    return 1;
		}
		break;
	    case 'n':
		nsteps = atoi(bu_optarg);
		break;
	    case 't':
		tmax = atof(bu_optarg);
		break;
	    case 'z':
		ground_z = atof(bu_optarg);
		break;
	    case 'd':
		od = atof(bu_optarg);
		break;
	    default:
		Usage();
		return 1;
	}
    }

    if ((argc - bu_optind) != 1) {
	Usage();
	return 1;
    }
    outfile = argv[bu_optind];

    if (nsteps < 1) {
	fprintf(stderr, "%s: step count (-n) must be >= 1\n", progname);
	return 1;
    }
    if (od <= 0.0) {
	fprintf(stderr, "%s: pipe outer diameter (-d) must be > 0\n", progname);
	return 1;
    }

    /*
     * Determine the flight end time t_end.
     *
     * If -t was given, use it verbatim.  Otherwise solve for ground
     * impact: the z-component of P(t) = ground_z, i.e.
     *
     *     0.5*g.z*t^2 + v0.z*t + (p0.z - ground_z) = 0
     *
     * Take the first strictly-positive root.  If gravity has no
     * z-component (or no positive root exists) fall back to a nominal
     * flight time so we still emit a visible trajectory.
     */
    if (tmax > 0.0) {
	t_end = tmax;
    } else {
	double a = 0.5 * grav[Z];
	double b = v0[Z];
	double c = p0[Z] - ground_z;
	t_end = -1.0;

	if (!NEAR_ZERO(a, SMALL_FASTF)) {
	    disc = b * b - 4.0 * a * c;
	    if (disc >= 0.0) {
		double sq = sqrt(disc);
		double r1 = (-b + sq) / (2.0 * a);
		double r2 = (-b - sq) / (2.0 * a);
		/* pick smallest strictly-positive root */
		if (r1 > SMALL_FASTF && (t_end < 0.0 || r1 < t_end))
		    t_end = r1;
		if (r2 > SMALL_FASTF && (t_end < 0.0 || r2 < t_end))
		    t_end = r2;
	    }
	} else if (!NEAR_ZERO(b, SMALL_FASTF)) {
	    /* linear: v0.z*t + c = 0 */
	    double r = -c / b;
	    if (r > SMALL_FASTF)
		t_end = r;
	}

	if (t_end <= 0.0) {
	    /* no impact solution; use a nominal flight time */
	    t_end = 10.0;
	    bu_log("%s: no ground impact found; using nominal t_end=%g s\n",
		   progname, t_end);
	}
    }

    dt = t_end / (double)nsteps;

    fp = wdb_fopen(outfile);
    if (fp == NULL) {
	fprintf(stderr, "%s: cannot open output file '%s'\n", progname, outfile);
	perror(progname);
	return 1;
    }

    mk_conversion("meters");
    mk_id(fp, "Parabolic Shotline");

    bu_log("%s: origin=(%g,%g,%g) v0=(%g,%g,%g) g=(%g,%g,%g)\n",
	   progname, V3ARGS(p0), V3ARGS(v0), V3ARGS(grav));
    bu_log("%s: steps=%d t_end=%g s dt=%g s ground_z=%g od=%g\n",
	   progname, nsteps, t_end, dt, ground_z, od);

    /*
     * Curved shotline: full gravity-affected trajectory.
     * P(t) = P0 + V0*t + 0.5*g*t^2  ->  VJOIN2(pt, P0, t, V0, 0.5*t*t, g)
     */
    mk_pipe_init(&head);
    for (i = 0; i <= nsteps; i++) {
	t = dt * (double)i;
	VJOIN2(pt, p0, t, v0, 0.5 * t * t, grav);
	mk_add_pipe_pnt(&head, pt, od, 0.0, od * 2.0);
    }
    if (mk_pipe(fp, "shotline.pipe", &head) < 0)
	fprintf(stderr, "%s: mk_pipe(shotline.pipe) failed\n", progname);
    mk_pipe_free(&head);

    /*
     * Straight baseline (no gravity): P(t) = P0 + V0*t.  Same launch
     * and same sampling so the curvature introduced by gravity is
     * directly visible against this reference line.
     */
    mk_pipe_init(&head);
    for (i = 0; i <= nsteps; i++) {
	t = dt * (double)i;
	VJOIN1(pt, p0, t, v0);
	mk_add_pipe_pnt(&head, pt, od, 0.0, od * 2.0);
    }
    if (mk_pipe(fp, "baseline.pipe", &head) < 0)
	fprintf(stderr, "%s: mk_pipe(baseline.pipe) failed\n", progname);
    mk_pipe_free(&head);

    db_close(fp->dbip);

    bu_log("%s: wrote %s (shotline.pipe, baseline.pipe)\n", progname, outfile);

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
