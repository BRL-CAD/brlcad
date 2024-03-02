/*                         R T S U R F . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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
/** @file rt/rtsurf.c
 *
 * Calculate exterior surface areas.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/assert.h"
#include "raytrace.h"


struct options
{
    size_t samples;
    int print;
};


static int
hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    double radius = ap->a_user;
    int print = ap->a_flag;

    /* make our hit spheres big enough to see */
    double hitrad = ((radius / 1000.0) > 1.0) ? radius / 1000.0 : 1.0;

    static size_t cnt = 0;

    for (struct partition *pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {

	/* print the name of the region we hit as well as the name of
	 * the primitives encountered on entry and exit.
	 */
#if 0
	bu_log("\n--- Hit region %s (in %s, out %s)\n",
	       pp->pt_regionp->reg_name,
	       pp->pt_inseg->seg_stp->st_name,
	       pp->pt_outseg->seg_stp->st_name);
#endif

	/* in hit point */
	point_t pt;
	struct hit *hitp;
	hitp = pp->pt_inhit;
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);

	struct soltab *stp;
	stp = pp->pt_inseg->seg_stp;

	/* in hit normal */
	vect_t inormal;
	RT_HIT_NORMAL(inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip);

	//rt_pr_hit("  In", hitp);
	//VPRINT("  Ipoint", pt);
	//VPRINT("  Inormal", inormal);

	/* print the entry hit point */
	if (hitp->hit_dist < radius) {
	    double angle_cos = VDOT(ap->a_ray.r_dir, inormal);

	    if (fabs(angle_cos) > 1e-5) {
		ap->a_dist = M_PI * pow(hitp->hit_dist * angle_cos, 2);
	    }

	    if (print) {
		printf("in hit%zu.sph sph %lf %lf %lf %lf\nZ\n", cnt++, pt[0], pt[1], pt[2], hitrad);
	    }
	} else {
	    if (print) {
		printf("in past%zu.sph sph %lf %lf %lf %lf\nZ\n", cnt++, pt[0], pt[1], pt[2], hitrad * 2.0);
	    }
	}

	/* out hit point */
	hitp = pp->pt_outhit;
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
	stp = pp->pt_outseg->seg_stp;

	/* out hit normal */
	vect_t onormal;
	RT_HIT_NORMAL(onormal, hitp, stp, &(ap->a_ray), pp->pt_outflip);

	//rt_pr_hit("  In", hitp);
	//VPRINT("  Ipoint", pt);
	//VPRINT("  Inormal", inormal);

	/* print the exit hit point */
	if (hitp->hit_dist < radius) {
	    double angle_cos = VDOT(ap->a_ray.r_dir, inormal);

	    if (fabs(angle_cos) > 1e-5) {
		ap->a_dist = M_PI * pow(hitp->hit_dist * angle_cos, 2);
	    }

	    if (print) {
		printf("in hit%zu.sph sph %lf %lf %lf %lf\nZ\n", cnt++, pt[0], pt[1], pt[2], hitrad);
	    }
	} else {
	    if (print) {
		printf("in past%zu.sph sph %lf %lf %lf %lf\nZ\n", cnt++, pt[0], pt[1], pt[2], hitrad * 2.0);
	    }
	}

	/* print the exit hit point info */
	//rt_pr_hit("  Out", hitp);
	//VPRINT("  Opoint", pt);
	//VPRINT("  Onormal", onormal);
    }

    return 1;
}


static int
miss(struct application *UNUSED(ap))
{
    //bu_log("missed\n");
    return 0;
}


static void
initialize_resources(size_t cnt, struct resource *resp, struct rt_i *rtip)
{
    if (!resp)
	return;

    /* Initialize all the per-CPU memory resources.  Number of
     * processors can change at runtime, so initialize all.
     */
    memset(resp, 0, sizeof(struct resource) * cnt);

    int i;
    for (i = 0; i < MAX_PSW; i++) {
	rt_init_resource(&resp[i], i, rtip);
    }
}


static void
init_random(void)
{
    srand((unsigned int)time(NULL));
}


static void
initialize(struct application *ap, const char *db, const char *obj[])
{
    static struct rt_i *rtip = NULL;
    struct resource *resources = NULL;

    char title[4096] = {'\0'};

    BU_ASSERT(ap && db);

    rt_debug = 0;

    rtip = rt_dirbuild(db, title, sizeof(title));
    if (rtip == RTI_NULL) {
	bu_exit(2, "Building the database directory for [%s] FAILED\n", db);
    }
    if (title[0]) {
	bu_log("Title:\n%s\n", title);
    }

    size_t loaded = 0;
    while (*obj && *obj[0] != '\0') {
	if (rt_gettree(rtip, obj[0]) < 0) {
	    bu_log("Loading the geometry for [%s] FAILED\n", obj[0]);
	} else {
	    loaded++;
	}
	obj++;
    }
    if (loaded == 0)
	bu_exit(3, "No geometry loaded from [%s]\n", db);

    rt_prep_parallel(rtip, 1);

    resources = (struct resource *)bu_calloc(MAX_PSW, sizeof(struct resource), "resources");
    initialize_resources(1, resources, rtip);

    RT_APPLICATION_INIT(ap);
    ap->a_rt_i = rtip;
    ap->a_hit = hit;
    ap->a_miss = miss;
    ap->a_overlap = NULL;
    ap->a_multioverlap = NULL;
    ap->a_logoverlap = rt_silent_logoverlap;
    ap->a_resource = &resources[0];

    /* shoot through? */
    ap->a_onehit = 0;

    init_random();

    return;
}


static double
random_double(void) {
    return rand() / (RAND_MAX + 1.0);
}


static void
random_point_on_sphere(double radius, point_t point) {
    double theta = 2 * M_PI * random_double();
    double phi = acos(2 * random_double() - 1);
    point[0] = radius * sin(phi) * cos(theta);
    point[1] = radius * sin(phi) * sin(theta);
    point[2] = radius * cos(phi);
}


static void
points_on_sphere(size_t count, point_t pnts[], double radius, point_t center)
{
    for (size_t i = 0; i < count; ++i) {
	point_t point;

        //printf("Point %zu: (%f, %f, %f)\n", i+1, point[0], point[1], point[2]);

        random_point_on_sphere(radius, point);
	VADD2(pnts[i], point, center);
    }
}


#if 0
/* This is the former method that shot rays through the bounding
 * sphere center.  this attenuated areas based on the solid angle and
 * surface normal, but was egregiously oversampling surface area near
 * the center and undersampling near the radius.  Still may need this
 * method though to construct a boundary mesh representation.
 */
static void
rays_from_points_to_center(struct xray *rays, size_t count, const point_t pnts[], const point_t center)
{
    for (size_t i = 0; i < count; ++i) {
	VMOVE(rays[i].r_pt, pnts[i]);
	VSUB2(rays[i].r_dir, center, pnts[i]); // point back at origin
    }
}
#endif


static void
shuffle_points(point_t *points, size_t n)
{
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);

	point_t temp;
	VMOVE(temp, points[i]);
	VMOVE(points[i], points[j]);
	VMOVE(points[j], temp);
    }
}


static void
rays_through_point_pairs(struct xray *rays, size_t count, point_t pnts[])
{
    /* generate N/2 rays from an array of N points */
    shuffle_points(pnts, count);

    for (size_t i = 0; i < count; i += 2) {
	/* from one point through another */
	VMOVE(rays[i / 2].r_pt, pnts[i]);
	VSUB2(rays[i / 2].r_dir, pnts[i + 1], pnts[i]);
    }
}


// Function to compute surface area using the Cauchy-Crofton formula
static double
compute_surface_area(int intersections, int lines, double radius)
{
    const double PROPORTIONALITY_CONSTANT = 4.0 * M_PI * radius * radius;

    if (lines == 0) {
        return 0.0;
    }

    // Apply the Cauchy-Crofton formula
    double area = PROPORTIONALITY_CONSTANT * (double)intersections / ((double)lines * 2.0);

    return area;
}


static double
do_one_iteration(struct application *ap, size_t samples, point_t center, double radius, struct options *opts)
{
    int print = opts->print;

    /* get sample points */
    point_t *points = (point_t *)bu_calloc(samples, sizeof(point_t), "points");
    points_on_sphere(samples, points, radius, center);

    struct xray *rays = (struct xray *)bu_calloc(samples, sizeof(struct xray), "rays");

    /* use the sample points twice to generate our set of sample rays */
    rays_through_point_pairs(rays, samples, points);
    rays_through_point_pairs(rays+(samples/2), samples, points);

    size_t hits = 0;
    for (size_t i = 0; i < samples; ++i) {
	ap->a_ray = rays[i]; /* struct copy */

	if (print) {
	    printf("in pnt%zu.sph sph %lf %lf %lf %lf\nZ\n", i, V3ARGS(points[i]), 1.0);
	    printf("in dir%zu.rcc rcc %lf %lf %lf %lf %lf %lf %lf\nZ\n", i, V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir), 0.5);
	}

	/* unitize before firing */
	VUNITIZE(ap->a_ray.r_dir);

	/* Shoot the ray. */
	hits += rt_shootray(ap);
    }

    /* sanity */
    if (hits > samples)
	bu_log("WARNING: hits (%zu) > samples (%zu)\n", hits, samples);

    /* release points for this iteration */
    bu_free(points, "points");
    bu_free(rays, "rays");

    double area = compute_surface_area(hits * 2, (double)samples, radius);
    bu_log("Cauchy-Crofton Area: hits (%zu) / lines (%zu) = %lf\n", hits * 2, (size_t)((double)samples), area);

    return area;
}


static double
do_iterations(struct application *ap, size_t samples, point_t center, double radius, struct options *opts)
{
    double prev2_estimate = 0.0;
    double prev1_estimate = 0.0;
    double curr_estimate = 0.0;
    double curr_percent = 100.0; // initialized high
    double prev_percent = 100.0; // initialized high
    double threshold = 0.1; // convergence threshold (0.1% change)
    size_t iteration = 0;
    size_t curr_samples = samples;

    while (curr_percent > threshold && prev_percent > threshold) {

	// increase samples every iteration by just over 2x
	curr_samples = samples * pow(2.01, iteration);

	curr_estimate = do_one_iteration(ap, curr_samples, center, radius, opts);

	// check convergence after 3 iterations
	if (iteration > 1) {
	    curr_percent = fabs((curr_estimate - prev1_estimate) / prev1_estimate) * 100.0;
	    prev_percent = fabs((prev1_estimate - prev2_estimate) / prev2_estimate) * 100.0;

	    // see if we're done by checking last two estimates
	    if (curr_percent <= threshold && prev_percent <= threshold) {
		break; // yep, converged.
	    }
	    // nope, not converged.
	}

	prev2_estimate = prev1_estimate;
	prev1_estimate = curr_estimate;
	iteration++;
    }

    return curr_estimate;
}


static double
estimate_surface_area(const char *db, const char *obj[], struct options *opts)
{
    BU_ASSERT(db && obj && opts);

    struct application ap;
    initialize(&ap, db, obj);

    double radius = ap.a_rt_i->rti_radius;
    size_t samples = opts->samples;
    int print = opts->print;

    ap.a_user = radius;
    ap.a_flag = opts->print;

    point_t center;
    VADD2SCALE(center, ap.a_rt_i->mdl_max, ap.a_rt_i->mdl_min, 0.5);

    /* set to mm so working units match */
    if (print)
	printf("units mm\n");

    bu_log("Radius: %lf\n", radius);
    //VPRINT("Center:", center);

    if (print) {
	printf("in center.sph sph %lf %lf %lf %lf\nZ\n", V3ARGS(center), 5.0);
	printf("in bounding.sph sph %lf %lf %lf %lf\nZ\n", V3ARGS(center), radius);
    }

    /* iterate until we converge on a solution */
    double area = do_iterations(&ap, samples, center, radius, opts);

    /* release our raytracing instance */
    rt_free_rti(ap.a_rt_i);
    ap.a_rt_i = NULL;

    return area;
}


static void
get_options(int argc, char *argv[], struct options *opts)
{
    static const char *usage = "Usage: %s [-p] [-n #samples] model.g objects...\n";

    const char *argv0 = argv[0];
    const char *db = NULL;
    const char **obj = NULL;

    /* Make sure we have at least a geometry file and one geometry
     * object on the command line, number of samples optional.
     */
    if (argc < 3) {
	bu_exit(1, usage, argv0);
    }

    bu_optind = 1;

    int c;
    while ((c = bu_getopt(argc, (char * const *)argv, "pn:h?")) != -1) {
	if (bu_optopt == '?')
	    c = 'h';

	switch (c) {
	    case 'p':
		if (opts)
		    opts->print = 1;
		break;
	    case 'n':
		if (opts)
		    opts->samples = (size_t)atoi(bu_optarg);
		break;
	    case '?':
	    case 'h':
		/* asking for help */
		bu_exit(EXIT_SUCCESS, usage, argv0);
	    default:
		bu_exit(EXIT_FAILURE, "ERROR: unknown option -%c\n", *bu_optarg);
	}
    }

    if (opts->samples < 1) {
	opts->samples = 1;
    }

    bu_log("Samples: %zu\n", opts->samples);
    argv += bu_optind;

    db = argv[0];
    obj = (const char **)(argv+1);

    /* final sanity checks */
    if (!db || !bu_file_exists(db, NULL)) {
	bu_exit(EXIT_FAILURE, "ERROR: database %s not found\n", (db)?db:"[]");
    }
    if (!obj) {
	bu_exit(EXIT_FAILURE, "ERROR: object(s) not specified\n");
    }
}


int
main(int argc, char *argv[])
{
    struct options opts;
    opts.samples = 100;
    opts.print = 0;

    char *db = NULL;
    char **obj = NULL;

    bu_setprogname(argv[0]);

    get_options(argc, argv, &opts);
    db = argv[bu_optind];
    obj = argv + bu_optind + 1;

    bu_log(" db is %s\n", db);
    bu_log(" obj[0] is %s\n", obj[0]);

    double estimate = estimate_surface_area(db, (const char **)obj, &opts);

    bu_log("Estimated exterior surface area: %lf\n", estimate);

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
