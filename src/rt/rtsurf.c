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
#include "raytrace.h"


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
	       pp->pt_outseg->seg_stp->st_name );
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

	/* print the entry hit point info */
	//rt_pr_hit("  In", hitp);
	//VPRINT(   "  Ipoint", pt);
	//VPRINT(   "  Inormal", inormal);
	if (hitp->hit_dist < radius) {
	    double angle_cos = VDOT(ap->a_ray.r_dir, inormal);

	    if (fabs(angle_cos) > 1e-5) {
		ap->a_dist = M_PI * pow(hitp->hit_dist * angle_cos, 2);
	    }

	    if (print) {
		printf("in hit%zu.sph sph %lf %lf %lf %lf\n", cnt++, pt[0], pt[1], pt[2], hitrad);
	    }
	} else {
	    if (print) {
		printf("in past%zu.sph sph %lf %lf %lf %lf\n", cnt++, pt[0], pt[1], pt[2], hitrad * 2.0);
	    }
	}

#if 0
	/* out hit point */
	hitp = pp->pt_outhit;
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
	stp = pp->pt_outseg->seg_stp;

	/* out hit normal */
	vect_t onormal;
	RT_HIT_NORMAL(onormal, hitp, stp, &(ap->a_ray), pp->pt_outflip);
#endif

	/* print the exit hit point info */
	//rt_pr_hit("  Out", hitp);
	//VPRINT(   "  Opoint", pt);
	//VPRINT(   "  Onormal", onormal);
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

    rt_debug = 0;

    rtip = rt_dirbuild(db, title, sizeof(title));
    if (rtip == RTI_NULL) {
	bu_exit(2, "Building the database directory for [%s] FAILED\n", db);
    }
    if (title[0]) {
	bu_log("Title:\n%s\n", title);
    }

    size_t loaded = 0;
    while (*obj && *obj[0] != '\0')  {
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
    ap->a_resource = &resources[0];

    /* shoot through? */
    ap->a_onehit = 1;

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


static double
estimate_surface_area(const char *db, const char *obj[], size_t samples, int print)
{
    struct application ap;
    initialize(&ap, db, obj);

    double radius = ap.a_rt_i->rti_radius;
    point_t center = VINIT_ZERO;
    ap.a_user = radius;
    ap.a_flag = print;

    VADD2SCALE(center, ap.a_rt_i->mdl_max, ap.a_rt_i->mdl_min, 0.5);

    /* set to mm so working units match */
    if (print)
	printf("units mm\n");

    //printf("Radius: %lf\n", radius);
    //VPRINT("Center:", center);
    if (print) {
	printf("in center.sph sph %lf %lf %lf %lf\n", V3ARGS(center), 5.0);
	printf("in bounding.sph sph %lf %lf %lf %lf\n", V3ARGS(center), radius);
    }

    /* get sample points */
    point_t *points = (point_t *)bu_calloc(samples, sizeof(point_t), "points");
    points_on_sphere(samples, points, radius, center);

    double total_weighted_area = 0.0;

    for (size_t i = 0; i < samples; ++i) {
	VMOVE(ap.a_ray.r_pt, points[i]);
	VSUB2(ap.a_ray.r_dir, center, points[i]); // point back at origin

	if (print) {
	    printf("in pnt%zu.sph sph %lf %lf %lf %lf\n", i, V3ARGS(points[i]), 1.0);
	    printf("in dir%zu.rcc rcc %lf %lf %lf %lf %lf %lf %lf\n", i, V3ARGS(ap.a_ray.r_pt), V3ARGS(ap.a_ray.r_dir), 0.5);
	    printf("erase pnt%zu.sph dir%zu.rcc\n", i, i);
	}

	/* unitize before firing */
	VUNITIZE(ap.a_ray.r_dir);

	//VPRINT("Pnt", ap.a_ray.r_pt);
	//VPRINT("Dir", ap.a_ray.r_dir);

	/* Shoot the ray. */
	(void)rt_shootray(&ap);

	total_weighted_area += ap.a_dist;
    }

    /* release our raytracing instance and points */
    bu_free(points, "points");
    rt_free_rti(ap.a_rt_i);
    ap.a_rt_i = NULL;

    double estimate = total_weighted_area / (samples * M_PI * radius * radius);

    return estimate;
}


static void
get_options(int argc, char *argv[], size_t *samples, int *print)
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
	    if (print)
		*print=1;
	    break;
	case 'n':
	    if (samples) {
		*samples = (size_t)atoi(bu_optarg);
	    }
	    break;
	case '?':
	case 'h':
	    /* asking for help */
	    bu_exit(EXIT_SUCCESS, usage, argv0);
	default:
	    bu_exit(EXIT_FAILURE, "ERROR: unknown option -%c\n", *bu_optarg);
	}
    }

    bu_log("Samples: %zu\n", *samples);
    bu_log("optind is %d\n", bu_optind);
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
    size_t samples = 100;
    int print = 0;

    char *db = NULL;
    char **obj = NULL;

    bu_setprogname(argv[0]);

    get_options(argc, argv, &samples, &print);
    db = argv[bu_optind];
    obj = argv + bu_optind + 1;

    bu_log(" db is %s\n", db);
    bu_log(" obj[0] is %s\n", obj[0]);

    double estimate = estimate_surface_area(db, (const char **)obj, samples, print);
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
