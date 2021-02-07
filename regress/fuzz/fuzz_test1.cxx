/*                  F U Z Z _ T E S T 1 . C X X
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file fuzz_test1.cxx
 *
 * Brief description
 *
 */

#include "common.h"

#include <stdint.h>
#include <stddef.h>

#include "raytrace.h"
#include <unistd.h>


int
fhit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    /* iterating over partitions, this will keep track of the current
     * partition we're working on.
     */
    struct partition *pp;

    /* will serve as a pointer for the entry and exit hitpoints */
    struct hit *hitp;

    /* will serve as a pointer to the solid primitive we hit */
    struct soltab *stp;

    /* will contain surface curvature information at the entry */
    struct curvature cur = RT_CURVATURE_INIT_ZERO;

    /* will contain our hit point coordinate */
    point_t pt;
   

    /* will contain normal vector where ray enters geometry */
    vect_t inormal;

    /* will contain normal vector where ray exits geometry */
    vect_t onormal;

    /* iterate over each partition until we get back to the head.
     * each partition corresponds to a specific homogeneous region of
     * material.
     */
    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {

	/* entry hit point, so we type less */
	hitp = pp->pt_inhit;

	/* construct the actual (entry) hit-point from the ray and the
	 * distance to the intersection point (i.e., the 't' value).
	 */
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);

	/* primitive we encountered on entry */
	stp = pp->pt_inseg->seg_stp;

	/* compute the normal vector at the entry point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip);

	/* This next macro fills in the curvature information which
	 * consists on a principle direction vector, and the inverse
	 * radii of curvature along that direction and perpendicular
	 * to it.  Positive curvature bends toward the outward
	 * pointing normal.
	 */
	RT_CURVATURE(&cur, hitp, pp->pt_inflip, stp);

	/* exit point, so we type less */
	hitp = pp->pt_outhit;

	/* construct the actual (exit) hit-point from the ray and the
	 * distance to the intersection point (i.e., the 't' value).
	 */
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);

	/* primitive we exited from */
	stp = pp->pt_outseg->seg_stp;

	/* compute the normal vector at the exit point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(onormal, hitp, stp, &(ap->a_ray), pp->pt_outflip);
    }

    /* A more complicated application would probably fill in a new
     * local application structure and describe, for example, a
     * reflected or refracted ray, and then call rt_shootray() for
     * those rays.
     */

    /* Hit routine callbacks generally return 1 on hit or 0 on miss.
     * This value is returned by rt_shootray().
     */
    return 1;
}


int
miss(struct application *UNUSED(ap))
{
    return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const int8_t *data, size_t size) {
    if(data == NULL){}
    if(size == 0){}
    struct application ap;
    static struct rt_i *rtip = NULL;
    struct resource res = RT_RESOURCE_INIT_ZERO;
    char title[1024] = {0};
    char cwd[PATH_MAX];
   if (getcwd(cwd, sizeof(cwd)) != NULL) {
       printf("Current working dir: %s\n", cwd);
   } else {
       perror("getcwd() error");
       return 1;
   }
    const char *file = "../../share/db/moss.g";
    const char *objs = "all.g";

    rt_init_resource(&res, 1, rtip);

    rtip = rt_dirbuild(file, title, sizeof(title));
    if (rtip == RTI_NULL) {
	bu_exit(2, "Building the database directory for [%s] FAILED\n", file);
    }
    rt_clean(rtip);

    return 0;

    if (title[0]) {
	bu_log("Title:\n%s\n", title);
    }
    if (rt_gettree(rtip, objs) < 0)
	bu_exit(2, "Loading the geometry for [%s] FAILED\n", objs);

    rt_prep_parallel(rtip, 1);
    RT_APPLICATION_INIT(&ap);
    ap.a_resource = &res;
    ap.a_rt_i = rtip;
    ap.a_onehit = 0;
    ap.a_hit = fhit;
    ap.a_miss = miss;

    VSET(ap.a_ray.r_pt, 0.0, 0.0, 10000.0);
    VSET(ap.a_ray.r_dir, 0.0, 0.0, -1.0);

    rt_shootray(&ap);

    rt_clean_resource_complete(rtip, &res);
    rt_clean(rtip);

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
