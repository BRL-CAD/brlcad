/*                     G - V O X E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2012 United States Government as represented by
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

	/* print the name of the region we hit as well as the name of
	 * the primitives encountered on entry and exit.
	 */
/*	bu_log("\n--- Hit region %s (in %s, out %s)\n",
	       pp->pt_regionp->reg_name,
	       pp->pt_inseg->seg_stp->st_name,
	       pp->pt_outseg->seg_stp->st_name );
*/
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


int
main(int argc, char **argv)
{
    struct application	ap;
    static struct rt_i *rtip;
    char title[1024] = {0};
    int i, j, numVoxelY, numVoxelZ , yMin, zMin;

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


   /* get bounding corners of bounding box of region through which ray is shot*/ 

    printf("\nbounding box is this\n");
     	    
    VPRINT("\nmin of bounding rectangular parallelopiped --Pnt\n",rtip->mdl_min);
    VPRINT("\nmax of bounding rectangular parallelopiped --Pnt\n",rtip->mdl_max);

   
    printf("\nx value of maximum on the highest corner of parallelopiped is: %f",(rtip->mdl_max)[0]);
    printf("\ny value of maximum on the highest corner of parallelopiped is: %f",(rtip->mdl_max)[1]);
    printf("\nz value of maximum on the highest corner of parallelopiped is: %f",(rtip->mdl_max)[2]);
    printf("\nx value of minimum on the lowest corner of parallelopiped is: %f",(rtip->mdl_min)[0]);
    printf("\ny value of minimum on the lowest corner of parallelopiped is: %f",(rtip->mdl_min)[1]);
    printf("\nz value of minimum on the lowest corner of parallelopiped is: %f",(rtip->mdl_min)[2]);


    /*assume voxels are unit size in each dimension*/
    numVoxelY = (int)((rtip->mdl_max)[1] - (rtip->mdl_min)[1]) ;
    numVoxelZ = (int)(rtip->mdl_max)[2] - (rtip->mdl_min)[2] ;
    
    yMin = (int)((rtip->mdl_min)[1]);
    zMin = (int)((rtip->mdl_min)[2]);

    for(i = 0; i < numVoxelZ; i++) {
	for( j = 0; j < numVoxelY; j++) { 
    	    printf("Ray number is %d\n",numVoxelY*i + j);
	    RT_APPLICATION_INIT(&ap);
	    ap.a_rt_i = rtip;
	    ap.a_onehit = 0;
            
	    /*ray is hit through midpoint of the unit sized voxels*/
	    VSET(ap.a_ray.r_pt, (rtip->mdl_max)[0] + 1.0 , yMin + j + 0.5 , zMin + i + 0.5);
	    VSET(ap.a_ray.r_dir, -1.0, 0.0, 0.0);
	    
	    ap.a_hit = hit;
	    ap.a_miss = miss;
	    
	    (void)rt_shootray(&ap);
	}
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
