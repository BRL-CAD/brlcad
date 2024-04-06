
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
 * Calculate exterior surface areas and volume.
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
 * and last hit points to estimate exterior surface area and inner segments
 * to estimate volume.
 *
 * = Examples =
 *
 * # Calculate area within 0.1% convergence (after 3+ iterations):
 * rtsurf file.g object
 * 
 * # Calculate area and volume within 0.1% convergence (after 3+ iterations):
 * rtsurf -v file.g object
 *
 * # Calculate area using precisely 1M samples (1 iteration only):
 * rtsurf -n 1000000 file.g object
 *
 * # Sets of 1M samples to within 0.001% convergence (3+ iters):
 * rtsurf -t 0.001 -n 1000000
 *
 * # Sets of 1k, 0.1% convergence, saving geometry script to file:
 * rtsurf -n 1000 -o file.g object > file.mged
 * mged -c file.g source file.mged
 *
 * # Areas per exterior material encountered, output saved to file:
 * rtsurf -m density.txt file.g object
 *
 * # Print summary of areas per region and per group to file:
 * rtsurf -r -g file.g object 2> rtsurf.log
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
 * Write utility manual page
 *
 * Integrate in MGED/Archer
 *
 * Write command manual page
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
#include "bu/vls.h"
#include "bu/getopt.h"
#include "bu/assert.h"
#include "bu/parallel.h"
#include "raytrace.h"
#include "analyze.h"

#include "./rtsurf_hits.h"


struct options
{
    double radius;    /** calculated bounding radius */
    ssize_t samples;  /** segment count, -1 to iterate to convergence */
    double threshold; /** percentage change to stop at, 0 to do 1-shot */
    char *materials;  /** print out areas per region, path to file */
    int makeGeometry; /** whether to write out geometry script to stdout */
    int printRegions; /** whether to print the full list of regions */
    int printGroups;  /** whether to print the full list of regions */
    int volume;       /** whether to calculate the volume of the objects */
};


struct ray {
    point_t r_pt;
    vect_t r_dir;
};


struct region_callback_data {
    double samples;
    double radius;
};


struct material_callback_data {
    struct analyze_densities *densities;
    double samples;
    double radius;
};


static int
hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    double radius = ap->a_user;
    int makeGeometry = ap->a_flag;

    /* make our hit spheres big enough to see */
    double hitrad = ((radius / 256.0) > 1.0) ? radius / 256.0 : 1.0;

    static size_t cnt = 0;

    /* register the first and last hit */
    void *context = ap->a_uptr;

    /* keep track of all shots, hit or miss */
    rtsurf_register_line(context);

    struct soltab *stp;
    struct hit *hitp;
    point_t pt;

    /* in hit point */
    struct partition *pp=PartHeadp->pt_forw;
    rtsurf_register_hit(context, pp->pt_regionp->reg_name, pp->pt_regionp->reg_gmater); // in-hit
    hitp = pp->pt_inhit;
    VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
    stp = pp->pt_inseg->seg_stp;

    /* in hit normal */
    vect_t inormal;
    RT_HIT_NORMAL(inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip);

    //rt_pr_hit("  In", hitp);
    //VPRINT("  Ipoint", pt);
    //VPRINT("  Inormal", inormal);

    /* print the entry hit point */
    if (makeGeometry) {
	printf("in hit.%zu.%zu.sph sph %lf %lf %lf %lf\nZ\n", (size_t)ap->a_dist, cnt++, pt[0], pt[1], pt[2], hitrad);
    }

    /* out hit point */
    struct partition *pprev=PartHeadp->pt_back;
    rtsurf_register_hit(context, pprev->pt_regionp->reg_name, pprev->pt_regionp->reg_gmater); // out-hit

    hitp = pprev->pt_outhit;
    VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
    stp = pprev->pt_outseg->seg_stp;

    /* out hit normal */
    vect_t onormal;
    RT_HIT_NORMAL(onormal, hitp, stp, &(ap->a_ray), pprev->pt_outflip);

    /* print the exit hit point */
    if (makeGeometry) {
	printf("in hit.%zu.%zu.sph sph %lf %lf %lf %lf\nZ\n", (size_t)ap->a_dist, cnt++, pt[0], pt[1], pt[2], hitrad);
    }

    /* print the exit hit point info */
    //rt_pr_hit("  Out", hitp);
    //VPRINT("  Opoint", pt);
    //VPRINT("  Onormal", onormal);

    return 1;
}


static int
miss(struct application *ap)
{
    void *context = ap->a_uptr;

    /* keep track of all shots, hit or miss */
    rtsurf_register_line(context);

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
    struct rt_i *rtip = NULL;
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
    ap->a_resource = resources;

    /* shoot through. */
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
rays_through_point_pairs(struct ray *rays, size_t count, point_t pnts[], struct options* opts, double * total_hits_bs)
{
    /* generate N/2 rays from an array of N points */
    shuffle_points(pnts, count);

    for (size_t i = 0; i < count; i += 2) {
	    /* from one point through another */
	    VMOVE(rays[i / 2].r_pt, pnts[i]);
	    VSUB2(rays[i / 2].r_dir, pnts[i + 1], pnts[i]);


        /*
         * If we are estimating the volume, compute
         * the length of the segment between the two points
         * inside the bounding sphere.
         */
        if (opts->volume) {
	        fastf_t curr_dist = DIST_PNT_PNT(pnts[i], pnts[i + 1]);
            *total_hits_bs += curr_dist;
        }
    }
}

// Function to generate sample points on the bounding sphere and shoot rays through them
static struct ray*
bounding_sphere_sampling(size_t samples, double radius, point_t center, struct options* opts, double* total_hits_bs)
{
    /* get sample points */
    point_t* points = (point_t*)bu_calloc(samples, sizeof(point_t), "points");
    points_on_sphere(samples, points, radius, center);

    struct ray* rays = (struct ray*)bu_calloc(samples, sizeof(struct ray), "rays");

    /* use the sample points twice to generate our set of sample rays */
    rays_through_point_pairs(rays, samples, points, opts, total_hits_bs);
    rays_through_point_pairs(rays + (samples / 2), samples, points, opts, total_hits_bs);

    /* done with points, loaded into rays */
    bu_free(points, "points");

    return rays;
}

// Function to compute surface area using the Cauchy-Crofton formula
static double
compute_surface_area(double intersections, double lines, double radius)
{
    // surface area of sphere = 4*PI*r^2
    const double PROPORTIONALITY_CONSTANT = 4.0 * M_PI * radius * radius;

    if (lines == 0) {
        return 0.0;
    }

    /* apply Cauchy-Crofton formula.
     *
     * each line intersection represents an entry and exit hit point,
     * so we divide by 2 so it becomes a ratio of shots that hit to
     * shots that miss.
     */
    double area = PROPORTIONALITY_CONSTANT * intersections / (lines * 2.0);

    return area;
}

// Function to compute volume
static double
compute_volume(double segments_intersection, double all_segments, double radius)
{
    // volume of sphere = 4/3*PI*r^3
    const double PROPORTIONALITY_CONSTANT = (4.0 / 3.0) * M_PI * radius * radius * radius;

    if (all_segments == 0) {
        return 0.0;
    }

    /*
     * The volume is calculated taking the proportion of the segments inside the object
     * over all the segments generated with Monte Carlo inside the bounding sphere.
     */
    double volume = PROPORTIONALITY_CONSTANT * (double)segments_intersection / (double)all_segments;

    return volume;
}


struct rtsurf_shootray_data {
    struct application *ap;
    struct ray *rays;
    size_t start;
    size_t end;
    double hitrad;
    int makeGeometry;
    size_t *hitpairs;
    int is_volume;
};


static void
sample_in_parallel(int id, void *data)
{
    struct rtsurf_shootray_data *pdata = &((struct rtsurf_shootray_data *)data)[id-1];
    struct application *ap = pdata->ap;
    struct ray *rays = pdata->rays;
    double hitrad = pdata->hitrad;
    int makeGeometry = pdata->makeGeometry;
    size_t *hitpairs = pdata->hitpairs;
    int is_volume = pdata->is_volume;

    // keep track of this iteration
    for (size_t i = pdata->start; i < pdata->end; ++i) {
        /* can't struct copy because our ray is smaller than xray */
        VMOVE(ap->a_ray.r_pt, rays[i].r_pt);
        VMOVE(ap->a_ray.r_dir, rays[i].r_dir);

        if (makeGeometry) {
            bu_semaphore_acquire(BU_SEM_SYSCALL);
            printf("in pnt.%zu.%zu.sph sph %lf %lf %lf %lf\nZ\n", (size_t)ap->a_dist, i, V3ARGS(ap->a_ray.r_pt), hitrad * 1.25);
            printf("in dir.%zu.%zu.rcc rcc %lf %lf %lf %lf %lf %lf %lf\nZ\n", (size_t)ap->a_dist, i, V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir), hitrad / 1.5);
            bu_semaphore_release(BU_SEM_SYSCALL);
        }

        /* unitize before firing */
        VUNITIZE(ap->a_ray.r_dir);

        size_t hitit = 0;
        struct partition* part;

        /* Shoot the ray.
         * If we are estimating the volume, retrieve the partition list
         * containing the segments inside the volume of the object.
         */
        if (is_volume) {
            part = rt_shootray_simple(ap, ap->a_ray.r_pt, ap->a_ray.r_dir);

            // calculate the length of the segments
            if (part != NULL) {
                do {
                    size_t length = part->pt_outhit->hit_dist - part->pt_inhit->hit_dist;
                    hitit += length;

                    part = part->pt_forw;
                } while (part != NULL);
            }
        }
        else {
            hitit = rt_shootray(ap);
        }

	bu_semaphore_acquire(BU_SEM_GENERAL);
	*hitpairs += hitit;
	bu_semaphore_release(BU_SEM_GENERAL);
    }
}


static void
do_samples_in_parallel(struct application *ap, size_t samples, struct ray *rays, double radius, struct options *opts, size_t *hitpairs)
{
    double hitrad = ((radius / 256.0) > 1.0) ? radius / 256.0 : 1.0;
    int makeGeometry = opts->makeGeometry;

    size_t ncpus = bu_avail_cpus();
    struct rtsurf_shootray_data *pdata = (struct rtsurf_shootray_data *)bu_calloc(ncpus, sizeof(struct rtsurf_shootray_data), "pdata");

    size_t samples_per_cpu = samples / ncpus;

    for (size_t i = 0; i < ncpus; ++i) {
	/* give each one their own copy of the app */
	struct application *a = (struct application *)bu_calloc(1, sizeof(struct application), "app");
	*a = *ap; /* struct copy */
	a->a_resource = &(a->a_resource[i]); // index into the array
        pdata[i].ap = a;
        pdata[i].rays = rays;
        pdata[i].start = i * samples_per_cpu;
        pdata[i].end = (i == ncpus - 1) ? samples : (i + 1) * samples_per_cpu; // handle last chunk special (i.e., to samples)
        pdata[i].hitrad = hitrad;
        pdata[i].makeGeometry = makeGeometry;
        pdata[i].hitpairs = hitpairs;
        pdata[i].is_volume = opts->volume;
    }

    // Execute in parallel
    bu_parallel(sample_in_parallel, ncpus, (void *)pdata);

    // Cleanup
    for (size_t i = 0; i < ncpus; ++i) {
	bu_free(pdata[i].ap, "app");
    }
    bu_free(pdata, "pdata");
}


static size_t
do_one_iteration(struct application *ap, size_t samples, point_t center, double radius, struct options *opts, double * total_hits_bs)
{
    int makeGeometry = opts->makeGeometry;
    struct ray* rays = bounding_sphere_sampling(samples, radius, center, opts, total_hits_bs);

    // FIXME: for uniquely naming our hit spheres, but makes this not
    // threadsafe or isolated.
    static size_t total_hits = 0;

    // DO IT.
    size_t hits = 0;
    do_samples_in_parallel(ap, samples, rays, radius, opts, &hits);
    total_hits += hits;

    /* group them all for performance */
    if (makeGeometry) {
	struct bu_vls pntvp = BU_VLS_INIT_ZERO;
	struct bu_vls dirvp = BU_VLS_INIT_ZERO;
	struct bu_vls hitvp = BU_VLS_INIT_ZERO;
	bu_vls_printf(&pntvp, "g pnts.%zu", (size_t)ap->a_dist);
	bu_vls_printf(&dirvp, "g dirs.%zu", (size_t)ap->a_dist);
	bu_vls_printf(&hitvp, "g hits.%zu", (size_t)ap->a_dist);
	for (size_t i = 0; i < samples; ++i) {
	    bu_vls_printf(&pntvp, " pnt.%zu.%zu.sph", (size_t)ap->a_dist, i);
	    bu_vls_printf(&dirvp, " dir.%zu.%zu.rcc", (size_t)ap->a_dist, i);
	}
	for (size_t i = (total_hits-hits)*2; i < total_hits*2; i++) {
	    bu_vls_printf(&hitvp, " hit.%zu.%zu.sph", (size_t)ap->a_dist, i);
	}
	printf("%s\nZ\n", bu_vls_cstr(&pntvp));
	printf("%s\nZ\n", bu_vls_cstr(&dirvp));
	printf("%s\nZ\n", bu_vls_cstr(&hitvp));
	bu_vls_free(&pntvp);
	bu_vls_free(&dirvp);
	bu_vls_free(&hitvp);
    }

    /* sanity */
    if (opts->volume == 0) {
        if (hits > samples)
            bu_log("WARNING: ray hits (%zu) > samples (%zu)\n", hits, samples);
    }

    /* done with our rays */
    bu_free(rays, "rays");

    if (opts->volume) {
        return hits;
    }
    else {
        return hits * 2; /* return # in + out hits */
    }
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
    double total_hits = 0;          // if opts->volume is on, total_hits is the sum of lengths of each segment inside the inner object
    double total_hits_bs = 0;   // if opts->volume is on, total_line_volume is the sum of lengths of each segment inside the bounding sphere 


    /* do exact count requested or start at 1000 and iterate */
    size_t curr_samples = (opts->samples > 0) ? (size_t)opts->samples : 1000;

    /* set to mm so working units match */
    if (opts->makeGeometry)
	printf("units mm\n");

    bu_log("Radius: %g\n", radius);
    //VPRINT("Center:", center);

    if (opts->makeGeometry) {
	printf("in center.sph sph %lf %lf %lf %lf\nZ\n", V3ARGS(center), 5.0);
	printf("in bounding.sph sph %lf %lf %lf %lf\nZ\n", V3ARGS(center), radius);
    }

    /* run the loop assessment */
    do {
        if (opts->samples > 0) {
            // do what we're told
            curr_samples = opts->samples;
        }
        else {
            // increase samples every iteration by uneven factor to avoid sampling patterns
            curr_samples *= pow(1.5, iteration);
        }

        /* we keep track of total hits and total lines so we don't
         * ignore previous work in the estimate calculation.  the hit
         * count includes both the in-hit and the out-hit separately.
         */
        ap->a_dist = (fastf_t)iteration;
        size_t hits = do_one_iteration(ap, curr_samples, center, radius, opts, &total_hits_bs);
        if (opts->volume == 0) {
            total_samples += curr_samples;
        }
        total_hits += hits;

        if (opts->volume) {
            curr_estimate = compute_volume(total_hits, total_hits_bs, radius);
            bu_log("Cauchy-Crofton Volume Estimate: (%zu hits length segments / %zu total length segments) = %g mm^3\n", (size_t)total_hits, (size_t)total_hits_bs, curr_estimate);
        }
        else {
            curr_estimate = compute_surface_area(total_hits, (double)total_samples, radius);
            bu_log("Cauchy-Crofton Surface Area Estimate: (%zu hits / %zu lines) = %g mm^2\n", (size_t)total_hits, (size_t)total_samples, curr_estimate);
        }

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
    if (opts->makeGeometry) {
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
	bu_vls_printf(&pntvp, "r pnts.r u pnts");
	bu_vls_printf(&dirvp, "r dirs.r u dirs");
	bu_vls_printf(&hitvp, "r hits.r u hits");
	bu_vls_free(&pntvp);
	bu_vls_free(&dirvp);
	bu_vls_free(&hitvp);
    }

    return curr_estimate;
}


static void
regions_callback(const char *name, size_t hits, size_t lines, void* data)
{
    struct region_callback_data *rdata = (struct region_callback_data *)data;
    BU_ASSERT(rdata);

    double area = compute_surface_area((double)hits, (double)lines, rdata->radius);

    bu_log("\t%s\t(%zu hits / %zu lines) = %.1lf mm^2\n", name, hits, lines, area);
}


static void
materials_callback(int id, size_t hits, size_t lines, void* data)
{
    struct material_callback_data *mdata = (struct material_callback_data *)data;
    BU_ASSERT(mdata);

    double area = compute_surface_area((double)hits, (double)lines, mdata->radius);

    if (mdata->densities) {
        const char* name = analyze_densities_name(mdata->densities, id);
        if (name) {
            bu_log("\t%s\t(%zu hits / %zu lines) = %.1lf mm^2\n", name, hits, lines, area);
        }
        else {
            bu_log("\tMaterial %d\t(%zu hits / %zu lines) = %.1lf mm^2\n", id, hits, lines, area);
        }
    }
    else {
        bu_log("\tMaterial %d\t(%zu hits / %zu lines) = %.1lf mm^2\n", id, hits, lines, area);
    }
}


static double
estimate_geometry(const char *db, const char *obj[], struct options *opts)
{
    BU_ASSERT(db && obj && opts);

    struct application ap;
    initialize(&ap, db, obj);

    double radius = ap.a_rt_i->rti_radius;

    ap.a_user = radius; // stores bounding radius
    ap.a_flag = opts->makeGeometry; // stores whether to print
    ap.a_dist = (fastf_t)0.0; // stores iteration count

    void *context = rtsurf_context_create();
    ap.a_uptr = context;

    point_t center;
    VADD2SCALE(center, ap.a_rt_i->mdl_max, ap.a_rt_i->mdl_min, 0.5);

    /* iterate until we converge on a solution */
    double area = do_iterations(&ap, center, radius, opts);
    
    /* Options only for surface estimate */
    if (opts->volume == 0) {
        if (opts->printRegions) {
	        /* print out all regions */
            bu_log("Area Estimate By Region:\n");
	        struct region_callback_data rdata = {opts->samples, radius};
	        rtsurf_iterate_regions(context, &regions_callback, &rdata);
        }

        if (opts->printGroups) {
	        /* print out all combs above regions */
            bu_log("Area Estimate By Combination:\n");
	        struct region_callback_data rdata = {opts->samples, radius};
	        rtsurf_iterate_groups(context, &regions_callback, &rdata);
        }

        /* print out areas per-region material */
        if (opts->materials) {
	        struct analyze_densities *densities = NULL;
	        struct bu_mapped_file *dfile = NULL;

            bu_log("Area Estimate By Material:\n");

	        dfile = bu_open_mapped_file(opts->materials, "densities file");
	        if (!dfile || !dfile->buf) {
	            bu_log("WARNING: could not open density file [%s]\n", opts->materials);
	        } else {
	            (void)analyze_densities_create(&densities);
	            (void)analyze_densities_load(densities, (const char *)dfile->buf, NULL, NULL);
	            bu_close_mapped_file(dfile);
	        }

	        struct material_callback_data mdata = {densities, opts->samples, radius};
	        rtsurf_iterate_materials(context, &materials_callback, &mdata);
	        analyze_densities_destroy(densities);
        }
    }

    /* release our raytracing instance and counters */
    rtsurf_context_destroy(context);
    rt_free_rti(ap.a_rt_i);
    ap.a_rt_i = NULL;

    return area;
}


static void
get_options(int argc, char *argv[], struct options *opts)
{
    static const char *usage = "Usage: %s [-g] [-r] [-v] [-n #samples] [-t %%threshold] [-m density.txt] [-o] model.g objects...\n";

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
    while ((c = bu_getopt(argc, (char * const *)argv, "vgrcn:t:m:oh?")) != -1) {
	if (bu_optopt == '?')
	    c = 'h';

	switch (c) {
	    case 'o':
		if (opts)
		    opts->makeGeometry = 1;
		break;
	    case 'g':
		if (opts)
		    opts->printGroups = 1;
		break;
	    case 'r':
		if (opts)
		    opts->printRegions = 1;
		break;
	    case 't':
		if (opts)
		    opts->threshold = (double)strtod(bu_optarg, NULL);
		break;
	    case 'm':
		if (opts)
		    opts->materials = bu_optarg;
		break;
	    case 'n':
		if (opts)
		    opts->samples = (size_t)strtol(bu_optarg, NULL, 10);
		break;
        case 'v':
        if (opts)
            opts->volume = 1;
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

    if (opts->materials && !bu_file_exists(opts->materials, NULL)) {
	bu_exit(EXIT_FAILURE, "ERROR: material file [%s] not found\n", opts->materials);
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
    opts.threshold = 0.0;
    opts.materials = NULL;
    opts.makeGeometry = 0;
    opts.printRegions = 0;
    opts.printGroups = 0;
    opts.radius = 0.0;
    opts.volume = 0;

    char *db = NULL;
    char **obj = NULL;

    bu_setprogname(argv[0]);

    get_options(argc, argv, &opts);
    db = argv[bu_optind];
    obj = argv + bu_optind + 1;

    bu_log(" db is %s\n", db);
    bu_log(" obj[0] is %s\n", obj[0]);

    /* Save the option of calculating volume for later */
    int volume = opts.volume;
    opts.volume = 0;

    /* Calculate surface area */
    double estimate = estimate_geometry(db, (const char **)obj, &opts);
    bu_log("Estimated exterior surface area: %.1lf\n", estimate);

    /* Calculate volume */
    if (volume) {
        opts.volume = 1;

        double estimate = estimate_geometry(db, (const char**)obj, &opts);
        bu_log("Estimated volume: %.1lf\n", estimate);
    }


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
