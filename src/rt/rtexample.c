/*                     R T E X A M P L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file rt/rtexample.c
 *
 * This is a heavily commented example of a program that uses librt to
 * shoot a single ray at some geometry in a .g database.
 *
 * The primary BRL-CAD ray-tracing API consists of calls to the
 * function rt_shootray().  This function takes a single argument, the
 * address of a data structure called the "application" structure.
 * This data structure contains crucial information, such as the
 * origin and direction of the ray to be traced, what to do if the ray
 * hits geometry, or what to do if it misses everything.  While the
 * application struct is a large and somewhat complex looking, (it is
 * defined in the raytrace.h header) there are really very few items
 * in it which the application programmer must know about. These are:
 *
 *   a_rt_i	The "raytrace instance" obtained via rt_dirbuild()
 *   a_ray	The ray origin and direction to be shot
 *   a_hit	A callback function for when the ray encounters geometry
 *   a_miss	A callback function for when the ray misses everything
 *
 * Most of the work an application performs will be done in the "hit"
 * routine.  This user-supplied routine gets called deep inside the
 * raytracing library via the rt_shootray() function.  It is provided
 * with 3 parameters:
 *
 *   ap		Pointer to the application structure passed to rt_shootray()
 *   PartHeadp	List of ray "partitions" which represent geometry hit
 *   segp	List of ray "segments" that comprised partitions
 *
 * Most applications can ignore the last parameter.  The PartHeadp
 * parameter is a linked-list of "partition" structures (defined in
 * the raytrace.h header).  It is the job of the "hit" routine to
 * process these ray/object intersections to do the work of the
 * application.
 *
 * This file is part of the default compile in source distributions of
 * BRL-CAD and is usually installed or provided via binary and source
 * distributions.  To compile this example from a binary install:
 *
 * cc -I/usr/brlcad/include/brlcad -L/usr/brlcad/lib -o rtexample rtexample.c -lbu -lrt -lm
 *
 * Jump to the START HERE section below for main().
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "vmath.h"		/* vector math macros */
#include "raytrace.h"		/* librt interface definitions */


/**
 * rt_shootray() was told to call this on a hit.
 *
 * This callback routine utilizes the application structure which
 * describes the current state of the raytrace.
 *
 * This callback routine is provided a circular linked list of
 * partitions, each one describing one in and out segment of one
 * region for each region encountered.
 *
 * The 'segs' segment list is unused in this example.
 */
int
hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
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
    struct curvature cur;

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

	/* print the name of the region we hit as well as the name of
	 * the primitives encountered on entry and exit.
	 */
	bu_log("\n--- Hit region %s (in %s, out %s)\n",
	       pp->pt_regionp->reg_name,
	       pp->pt_inseg->seg_stp->st_name,
	       pp->pt_outseg->seg_stp->st_name );

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

	/* print the entry hit point info */
	rt_pr_hit("  In", hitp);
	VPRINT(   "  Ipoint", pt);
	VPRINT(   "  Inormal", inormal);

	/* This next macro fills in the curvature information which
	 * consists on a principle direction vector, and the inverse
	 * radii of curvature along that direction and perpendicular
	 * to it.  Positive curvature bends toward the outward
	 * pointing normal.
	 */
	RT_CURVATURE(&cur, hitp, pp->pt_inflip, stp);

	/* print the entry curvature information */
	VPRINT("PDir", cur.crv_pdir);
	bu_log(" c1=%g\n", cur.crv_c1);
	bu_log(" c2=%g\n", cur.crv_c2);

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

	/* print the exit hit point info */
	rt_pr_hit("  Out", hitp);
	VPRINT(   "  Opoint", pt);
	VPRINT(   "  Onormal", onormal);
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


/**
 * This is a callback routine that is invoked for every ray that
 * entirely misses hitting any geometry.  This function is invoked by
 * rt_shootray() if the ray encounters nothing.
 */
int
miss(struct application *UNUSED(ap))
{
    bu_log("missed\n");
    return 0;
}


/**
 * START HERE
 *
 * This is where it all begins.
 */
int
main(int argc, char **argv)
{
    /* Every application needs one of these.  The "application"
     * structure carries information about how the ray-casting should
     * be performed.  Defined in the raytrace.h header.
     */
    struct application	ap;

    /* The "raytrace instance" structure contains definitions for
     * librt which are specific to the particular model being
     * processed.  One copy exists for each model.  Defined in
     * the raytrace.h header and is returned by rt_dirbuild().
     */
    static struct rt_i *rtip;

    /* optional parameter to rt_dirbuild() what can be used to capture
     * a title if the geometry database has one set.
     */
    char title[1024] = {0};

    /* Check for command-line arguments.  Make sure we have at least a
     * geometry file and one geometry object on the command line.
     */
    if (argc < 3) {
	bu_exit(1, "Usage: %s model.g objects...\n", argv[0]);
    }

    /* Load the specified geometry database (i.e., a ".g" file).
     * rt_dirbuild() returns an "instance" pointer which describes the
     * database to be raytraced.  It also gives you back the title
     * string if you provide a buffer.  This builds a directory of the
     * geometry (i.e., a table of contents) in the file.
     */
    rtip = rt_dirbuild(argv[1], title, sizeof(title));
    if (rtip == RTI_NULL) {
	bu_exit(2, "Building the database directory for [%s] FAILED\n", argv[1]);
    }

    /* Display the geometry database title obtained during
     * rt_dirbuild if a title is set.
     */
    if (title[0]) {
	bu_log("Title:\n%s\n", title);
    }

    /* Walk the geometry trees.  Here you identify any objects in the
     * database that you want included in the ray trace by iterating
     * of the object names that were specified on the command-line.
     */
    while (argc > 2)  {
	if (rt_gettree(rtip, argv[2]) < 0)
	    bu_log("Loading the geometry for [%s] FAILED\n", argv[2]);
	argc--;
	argv++;
    }

    /* This next call gets the database ready for ray tracing.  This
     * causes some values to be precomputed, sets up space
     * partitioning, computes boudning volumes, etc.
     */
    rt_prep_parallel(rtip, 1);

    /* initialize all values in application structure to zero */
    RT_APPLICATION_INIT(&ap);

    /* your application uses the raytrace instance containing the
     * geometry we loaded.  this describes what we're shooting at.
     */
    ap.a_rt_i = rtip;

    /* stop at the first point of intersection or shoot all the way
     * through (defaults to 0 to shoot all the way through).
     */
    ap.a_onehit = 0;

    /* Set the ray start point and direction rt_shootray() uses these
     * two to determine what ray to fire.  In this case we simply
     * shoot down the z axis toward the origin from 10 meters away.
     *
     * It's worth nothing that librt assumes units of millimeters.
     * All geometry is stored as millimeters regardless of the units
     * set during editing.  There are libbu routines for performing
     * unit conversions if desired.
     */
    VSET(ap.a_ray.r_pt, 0.0, 0.0, 10000.0);
    VSET(ap.a_ray.r_dir, 0.0, 0.0, -1.0);

    /* Simple debug printing */
    VPRINT("Pnt", ap.a_ray.r_pt);
    VPRINT("Dir", ap.a_ray.r_dir);

    /* This is what callback to perform on a hit. */
    ap.a_hit = hit;

    /* This is what callback to perform on a miss. */
    ap.a_miss = miss;

    /* Shoot the ray. */
    (void)rt_shootray(&ap);

    /* A real application would probably set up another ray and fire
     * again or do something a lot more complex in the callbacks.
     */

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
