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
#include "raytrace.h"


static int
hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    struct partition *pp;
    struct hit *hitp;
    struct soltab *stp;
    point_t pt;
    vect_t inormal;
    vect_t onormal;

    static size_t cnt = 0;

    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {

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
	hitp = pp->pt_inhit;
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
	stp = pp->pt_inseg->seg_stp;

	/* in hit normal */
	RT_HIT_NORMAL(inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip);

	/* print the entry hit point info */
	//rt_pr_hit("  In", hitp);
	//VPRINT(   "  Ipoint", pt);
	//VPRINT(   "  Inormal", inormal);
	printf("in hit%zu.sph sph %lf %lf %lf %lf\n", cnt++, pt[0], pt[1], pt[2], 1.0);

	/* out hit point */
	hitp = pp->pt_outhit;
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
	stp = pp->pt_outseg->seg_stp;

	/* out hit normal */
	RT_HIT_NORMAL(onormal, hitp, stp, &(ap->a_ray), pp->pt_outflip);

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

    while (*obj && *obj[0] != '\0')  {
	if (rt_gettree(rtip, obj[0]) < 0)
	    bu_log("Loading the geometry for [%s] FAILED\n", obj[0]);
	obj++;
    }

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

    return;
}


static void
init_random(void)
{
    srand((unsigned int)time(NULL));
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


static double
estimate_surface_area(struct application *ap, size_t samples)
{
    double radius = ap->a_rt_i->rti_radius;
    point_t center = VINIT_ZERO;

    VADD2SCALE(center, ap->a_rt_i->mdl_max, ap->a_rt_i->mdl_min, 0.5);

    //printf("Radius: %lf\n", radius);
    //VPRINT("Center:", center);
    printf("in center.sph sph %lf %lf %lf %lf\n", V3ARGS(center), 2.0);
    printf("in bounding.sph sph %lf %lf %lf %lf\n", V3ARGS(center), radius);

    for (size_t i = 0; i < samples; ++i) {
	point_t point;
        random_point_on_sphere(radius, point);
	VADD2(point, point, center);

        //printf("Point %zu: (%f, %f, %f)\n", i+1, point[0], point[1], point[2]);

	VMOVE(ap->a_ray.r_pt, point);
	VSUB2(ap->a_ray.r_dir, center, point);

	//VPRINT("Pnt", ap->a_ray.r_pt);
	//VPRINT("Dir", ap->a_ray.r_dir);
	printf("in pnt%zu.sph sph %lf %lf %lf %lf\n", i, V3ARGS(point), 1.0);
	printf("in dir%zu.sph rcc %lf %lf %lf %lf %lf %lf %lf\n", i, V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir), 0.5);

	/* unitize before firing */
	VUNITIZE(ap->a_ray.r_dir);

	/* Shoot the ray. */
	(void)rt_shootray(ap);
    }

    double estimate = 0.0;

    return estimate;
}


int
main(int argc, char **argv)
{
    int samples = 100;

    bu_setprogname(argv[0]);
    init_random();

    /* Make sure we have at least a geometry file and one geometry
     * object on the command line.
     */
    if (argc < 3) {
	bu_exit(1, "Usage: %s model.g objects...\n", argv[0]);
    }

    struct application ap;
    initialize(&ap, argv[1], (const char **)argv+2);

    double estimate = estimate_surface_area(&ap, samples);
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
