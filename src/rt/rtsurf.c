/*                         R T S U R F . C
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
 * The general method involves generating a set of random points on
 * the bounding sphere, and sampling through those points via ray
 * tracing to determine if we encounter the geometry.  Hit information
 * can then be used to calculate surface area or volume.
 *
 * This only works if there's an even and adequate distribution of the
 * sample space, which means we can use pseudo-random number
 * generation (blue noise would probably be better) or even sampling
 * patterns on the bounding surface.  It also means we can do
 * monte-carlo sampling iteratively and refine towards a desired
 * convergence threshold.
 *
 * With this method, we could use hit points to estimate surface area
 * or solid segments to estimate volume.  Here, we're using the first
 * and last hit points to estimate exterior surface area.
 *
 * = Examples =
 *
 * # Calculate area within 0.1% convergence (after 3+ iterations):
 * rtsurf file.g object
 *
 * # Calculate area using precisely 1M samples (1 iteration only):
 * rtsurf -n 1000000 file.g object
 *
 * # Sets of 1M samples to within 0.001% convergence (3+ iters):
 * rtsurf -t 0.001 -n 1000000
 *
 * # Sets of 1k, 0.1% convergence, saving geometry script to file:
 * rtsurf -n 1000 -p file.g object > file.mged
 * mged -c file.g source file.mged
 *
 * = Citations =
 *
 * Came up with the idea for this approach many years ago, but
 * apparently not alone in that regard.  Found two other papers very
 * closely relating to the method we're using:
 *
 * Li, Xueqing & Wang, Wenping & Martin, Ralph & Bowyer, Adrian.
 * (2003).  Using low-discrepancy sequences and the Crofton formula to
 * compute surface areas of geometric models. Computer-Aided Design.
 * 35.  771-782.  10.1016/S0010-4485(02)00100-8.
 *
 * Liu, Yu-Shen & Yi, Jing & Zhang, Hu & Zheng, Guo-Qin & Paul,
 * Jean-Claude.  (2010).  Surface area estimation of digitized 3D
 * objects using quasi-Monte Carlo methods.  Pattern Recognition.  43.
 * 3900-3909. 10.1016/j.patcog.2010.06.002.
 *
 * = TODO =
 *
 * Improve script generation
 *
 * Track hits per region
 *
 * Track hits per material
 *
 * Shoot in parallel
 *
 * Write utility manual page
 *
 * Integrate in MGED/Archer
 *
 * Write command manual page
 *
 * Calculate volume
 *
 * Calculate exterior mesh (shrinkwrap)
 *
 * Integrate into libanalyze
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
#include "bu/vls.h"
#include "raytrace.h"


struct options
{
    ssize_t samples;  /** number of segments, -1 to iterate to convergence */
    double threshold; /** percentage change to stop at, 0 to do 1-shot */
    int print;        /** whether to log output or not */
};


struct ray {
    point_t r_pt;
    vect_t r_dir;
};


static int
hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    double radius = ap->a_user;
    int print = ap->a_flag;

    /* make our hit spheres big enough to see */
    double hitrad = ((radius / 256.0) > 1.0) ? radius / 256.0 : 1.0;

    static size_t cnt = 0;

    struct partition *pp=PartHeadp->pt_forw;
    struct partition *pprev=PartHeadp->pt_back;

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
    if (print) {
	printf("in hit.%zu.%zu.sph sph %lf %lf %lf %lf\nZ\n", (size_t)ap->a_dist, cnt++, pt[0], pt[1], pt[2], hitrad);
    }

    /* out hit point */
    hitp = pprev->pt_outhit;
    VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
    stp = pprev->pt_outseg->seg_stp;

    /* out hit normal */
    vect_t onormal;
    RT_HIT_NORMAL(onormal, hitp, stp, &(ap->a_ray), pprev->pt_outflip);

    /* print the exit hit point */
    if (print) {
	printf("in hit.%zu.%zu.sph sph %lf %lf %lf %lf\nZ\n", (size_t)ap->a_dist, cnt++, pt[0], pt[1], pt[2], hitrad);
    }

    /* print the exit hit point info */
    //rt_pr_hit("  Out", hitp);
    //VPRINT("  Opoint", pt);
    //VPRINT("  Onormal", onormal);

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
rays_from_points_to_center(struct ray *rays, size_t count, const point_t pnts[], const point_t center)
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
rays_through_point_pairs(struct ray *rays, size_t count, point_t pnts[])
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


static size_t
do_one_iteration(struct application *ap, size_t samples, point_t center, double radius, struct options *opts)
{
    double hitrad = ((radius / 256.0) > 1.0) ? radius / 256.0 : 1.0;
    int print = opts->print;

    /* get sample points */
    point_t *points = (point_t *)bu_calloc(samples, sizeof(point_t), "points");
    points_on_sphere(samples, points, radius, center);

    struct ray *rays = (struct ray *)bu_calloc(samples, sizeof(struct ray), "rays");

    /* use the sample points twice to generate our set of sample rays */
    rays_through_point_pairs(rays, samples, points);
    rays_through_point_pairs(rays+(samples/2), samples, points);

    /* done with points, loaded into rays */
    bu_free(points, "points");

    size_t hits = 0;
    for (size_t i = 0; i < samples; ++i) {
	/* can't struct copy because our ray is smaller than xray */
	VMOVE(ap->a_ray.r_pt, rays[i].r_pt);
	VMOVE(ap->a_ray.r_dir, rays[i].r_dir);

	if (print) {
	    printf("in pnt.%zu.%zu.sph sph %lf %lf %lf %lf\nZ\n", (size_t)ap->a_dist, i, V3ARGS(ap->a_ray.r_pt), hitrad * 1.25);
	    printf("in dir.%zu.%zu.rcc rcc %lf %lf %lf %lf %lf %lf %lf\nZ\n", (size_t)ap->a_dist, i, V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir), hitrad / 1.5);
	}

	/* unitize before firing */
	VUNITIZE(ap->a_ray.r_dir);

	/* Shoot the ray. */
	size_t hitit = rt_shootray(ap);
	hits += hitit;

	if (hitit) {
	    printf("in pnt.%zu.%zu.sph sph %lf %lf %lf %lf\nZ\n", (size_t)ap->a_dist, i, V3ARGS(ap->a_ray.r_pt), hitrad * 1.25);
	}
    }

    /* group them all for performance */
    if (print) {
	struct bu_vls pntvp = BU_VLS_INIT_ZERO;
	struct bu_vls dirvp = BU_VLS_INIT_ZERO;
	struct bu_vls hitvp = BU_VLS_INIT_ZERO;
	bu_vls_printf(&pntvp, "g pnts.%zu", (size_t)ap->a_dist);
	bu_vls_printf(&dirvp, "g dirs.%zu", (size_t)ap->a_dist);
	bu_vls_printf(&hitvp, "g hits.%zu", (size_t)ap->a_dist);
	for (size_t i = 0; i < samples; ++i) {
	    bu_vls_printf(&pntvp, " pnt.%zu.%zu", (size_t)ap->a_dist, i);
	    bu_vls_printf(&dirvp, " dir.%zu.%zu", (size_t)ap->a_dist, i);
	}
	for (size_t i = 0; i < hits*2; i++) {
	    bu_vls_printf(&hitvp, " hit.%zu.%zu", (size_t)ap->a_dist, i);
	}
	printf("%s\nZ\n", bu_vls_cstr(&pntvp));
	printf("%s\nZ\n", bu_vls_cstr(&dirvp));
	printf("%s\nZ\n", bu_vls_cstr(&hitvp));
	bu_vls_free(&pntvp);
	bu_vls_free(&dirvp);
	bu_vls_free(&hitvp);
    }

    /* sanity */
    if (hits > samples)
	bu_log("WARNING: hits (%zu) > samples (%zu)\n", hits, samples);

    /* done with our rays */
    bu_free(rays, "rays");

    return hits * 2; /* in and out */
}


static double
do_iterations(struct application *ap, point_t center, double radius, struct options *opts)
{
    double prev2_estimate = -2.0; // must be negative to start
    double prev1_estimate = -1.0; // must be negative to start
    double curr_estimate = 0.0;
    double curr_percent = 100.0; // initialized high
    double prev_percent = 100.0; // initialized high
    double threshold = opts->threshold; // convergence threshold (% change)
    size_t iteration = 0;

    size_t total_samples = 0;
    size_t total_hits = 0;

    /* do exact count requested or start at 1000 and iterate */
    size_t curr_samples = (opts->samples > 0) ? (size_t)opts->samples : 1000;

    /* set to mm so working units match */
    if (opts->print)
	printf("units mm\n");

    bu_log("Radius: %g\n", radius);
    //VPRINT("Center:", center);

    if (opts->print) {
	printf("in center.sph sph %lf %lf %lf %lf\nZ\n", V3ARGS(center), 5.0);
	printf("in bounding.sph sph %lf %lf %lf %lf\nZ\n", V3ARGS(center), radius);
    }

    /* run the loop assessment */
    do {
	if (opts->samples > 0) {
	    // do what we're told
	    curr_samples = opts->samples;
	} else {
	    // increase samples every iteration by uneven factor to avoid sampling patterns
	    curr_samples *= pow(1.5, iteration);
	}

	// we keep track of total hits and total lines so we don't
	// ignore previous work in the estimate calculation.
	ap->a_dist = (fastf_t)iteration;
	size_t hits = do_one_iteration(ap, curr_samples, center, radius, opts);
	total_samples += curr_samples;
	total_hits += hits;

	curr_estimate = compute_surface_area(total_hits, (double)total_samples, radius);
	bu_log("Cauchy-Crofton Area: hits (%zu) / lines (%zu) = %g\n", total_hits, (size_t)((double)total_samples), curr_estimate);

	// threshold-based exit checks for convergence after 3 iterations
	curr_percent = fabs((curr_estimate - prev1_estimate) / prev1_estimate) * 100.0;
	prev_percent = fabs((prev1_estimate - prev2_estimate) / prev2_estimate) * 100.0;

	// bu_log("cur%%=%g, prev%%=%g, iter=%zu\n", curr_percent, prev_percent, iteration);

	// prep next iteration
	prev2_estimate = prev1_estimate;
	prev1_estimate = curr_estimate;
	iteration++;

    } while (threshold > 0 && (curr_percent > threshold || prev_percent > threshold));

    /* if we're printing, group all iterations too */
    if (opts->print) {
	struct bu_vls pntvp = BU_VLS_INIT_ZERO;
	struct bu_vls dirvp = BU_VLS_INIT_ZERO;
	struct bu_vls hitvp = BU_VLS_INIT_ZERO;
	bu_vls_printf(&pntvp, "g pnts");
	bu_vls_printf(&dirvp, "g dirs");
	bu_vls_printf(&hitvp, "g hits");
	for (size_t i = 0; i < iteration; ++i) {
	    bu_vls_printf(&pntvp, " pnts.%zu", i);
	    bu_vls_printf(&dirvp, " dirs.%zu", i);
	    bu_vls_printf(&hitvp, " hits.%zu", i);
	}
	printf("%s\nZ\n", bu_vls_cstr(&pntvp));
	printf("%s\nZ\n", bu_vls_cstr(&dirvp));
	printf("%s\nZ\n", bu_vls_cstr(&hitvp));
	bu_vls_free(&pntvp);
	bu_vls_free(&dirvp);
	bu_vls_free(&hitvp);
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

    ap.a_user = radius; // stores bounding radius
    ap.a_flag = opts->print; // stores whether to print
    ap.a_dist = (fastf_t)0.0; // stores iteration count

    point_t center;
    VADD2SCALE(center, ap.a_rt_i->mdl_max, ap.a_rt_i->mdl_min, 0.5);

    /* iterate until we converge on a solution */
    double area = do_iterations(&ap, center, radius, opts);

    /* release our raytracing instance */
    rt_free_rti(ap.a_rt_i);
    ap.a_rt_i = NULL;

    return area;
}


static void
get_options(int argc, char *argv[], struct options *opts)
{
    static const char *usage = "Usage: %s [-p] [-n #samples] [-t %%threshold] model.g objects...\n";

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
    while ((c = bu_getopt(argc, (char * const *)argv, "pn:t:h?")) != -1) {
	if (bu_optopt == '?')
	    c = 'h';

	switch (c) {
	    case 'p':
		if (opts)
		    opts->print = 1;
		break;
	    case 't':
		if (opts)
		    opts->threshold = (double)strtod(bu_optarg, NULL);
		break;
	    case 'n':
		if (opts)
		    opts->samples = (size_t)strtol(bu_optarg, NULL, 10);
		break;
	    case '?':
	    case 'h':
		/* asking for help */
		bu_exit(EXIT_SUCCESS, usage, argv0);
	    default:
		bu_exit(EXIT_FAILURE, "ERROR: unknown option -%c\n", *bu_optarg);
	}
    }

    if (opts->threshold < 0.0) {
	opts->threshold = 0.0;
    }

    bu_log("Samples: %zd %s\n", opts->samples, opts->samples>0?"rays":"(until converges)");
    bu_log("Threshold: %g %s\n", opts->threshold, opts->threshold>0.0?"%":"(one iteration)");

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
    opts.samples = -1;
    opts.print = 0;
    opts.threshold = 0.0;

    char *db = NULL;
    char **obj = NULL;

    bu_setprogname(argv[0]);

    get_options(argc, argv, &opts);
    db = argv[bu_optind];
    obj = argv + bu_optind + 1;

    bu_log(" db is %s\n", db);
    bu_log(" obj[0] is %s\n", obj[0]);

    double estimate = estimate_surface_area(db, (const char **)obj, &opts);

    bu_log("Estimated exterior surface area: %g\n", estimate);

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
