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
static int
hit(struct application *ap, struct partition *PartHeadp, struct seg*UNUSED(segs))
{
    int voxelNumIn, voxelNumOut, presentVoxel=-1, i, numVoxelX;
    struct partition *pp;
    struct hit *hitOutp, *hitInp;
    struct rt_i *rtip;
    float hitDistIn, hitDistOut, sizeVoxelX, inDistance = 0.0, threshold = 0.5;

    /**
     * length of voxels in the X-direction is sizeVoxelX, rtip is
     * structure for raytracing
     */
    sizeVoxelX = * (float*) ap->a_uptr;
    rtip = ap->a_rt_i;
    pp = PartHeadp->pt_forw;

    /**
     * The following loop prints the voxels is present in path of ray
     * (1-present)
     */
    while (pp != PartHeadp) {

	/**
	 * hitInp, hitOutp are hit structures to save distances where
	 * ray entered and exited the present partition.  hitDistIn,
	 * hitDistOut are the respective distances from the origin of
	 * ray.  voxelNumIn, voxelNumOut are the voxel numbers where
	 * ray enterd and exited the present partition.  presentVoxel
	 * is the voxel index from which the ray came out of in the
	 * last partition
	 */
	hitInp = pp->pt_inhit;
	hitOutp = pp->pt_outhit;

	hitDistIn = hitInp->hit_dist;
	hitDistOut = hitOutp->hit_dist;

	voxelNumIn = (int) (hitDistIn - 1.0) / sizeVoxelX;
	voxelNumOut = (int) (hitDistOut - 1.0) / sizeVoxelX;

	/**
	 * If ray does not enter next partition (ie the voxel index
	 * from which ray exited) in the same voxel index, this means
	 * that last voxel of previous partition can be
	 * evaluated. Also, the intermediate voxels are NOT filled.
	 */
	if (presentVoxel != voxelNumIn) {

	    if (inDistance / sizeVoxelX >= threshold) {
		printf("1");
	    } else {
		if (presentVoxel!=-1) {
		    printf("0");
		}
	    }

	    for (i = 0; i < voxelNumIn - presentVoxel - 1; i++) {
		printf("0");
	    }

	    presentVoxel = voxelNumIn;
	    inDistance = 0.0;

	}

	/**
	 * If voxel entered and voxel exited are same then nothing can
	 * be evaluated till we see the next partition too. If not,
	 * evaluate entry voxel. Also, all the intermediate voxels are
	 * in.
	 *
	 * inDistance is given the distance covered before
	 * exiting the last voxel of the present partition.
	 */

	if (voxelNumIn == voxelNumOut) {
	    inDistance += hitDistOut - hitDistIn;
	} else {
	    inDistance += (voxelNumIn + 1) * sizeVoxelX - hitDistOut;

	    if (inDistance / sizeVoxelX >= threshold) {
		printf("1");
	    } else {
		printf("0");
	    }

	    for (i = 0; i < voxelNumOut - voxelNumIn -1; i++) {
		printf("1");
	    }

	    presentVoxel = voxelNumOut;
	    inDistance = hitDistOut - voxelNumOut * sizeVoxelX;
	}


	pp = pp->pt_forw;
    }

    if (inDistance >= threshold) {
	printf("1");
    } else {
	printf("0");
    }

    /**
     * voxels after the last partition are not in
     */
    numVoxelX = (int)(((rtip->mdl_max)[0] - (rtip->mdl_min)[0])/sizeVoxelX) + 1;
    for (i = 0; i < numVoxelX - presentVoxel; i++) {
	printf("0");
    }

    printf("\n");
    return 0;

}


/**
 * This is a callback routine that is invoked for every ray that
 * entirely misses hitting any geometry.  This function is invoked by
 * rt_shootray() if the ray encounters nothing.
 */
static int
miss(struct application *UNUSED(ap))
{
    bu_log("missed\n");
    return 0;
}


int
main(int argc, char **argv)
{
    struct application ap;
    static struct rt_i *rtip;

    char title[1024] = {0};
    int i, j, numVoxelX,  numVoxelY, numVoxelZ, yMin, zMin;
    float sizeVoxelX,  sizeVoxelY, sizeVoxelZ;

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

    /* Display the geometry database title obtained during rt_dirbuild
     * if a title is set.
     */
    if (title[0]) {
	bu_log("Title:\n%s\n", title);
    }

    /* Walk the geometry trees.  Here you identify any objects in the
     * database that you want included in the ray trace by iterating
     * of the object names that were specified on the command-line.
     */
    while (argc > 2) {
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


    /* get bounding corners of bounding box of region through which ray is shot */

    printf("\nbounding box is this\n");

    VPRINT("\nmin of bounding rectangular parallelopiped --Pnt\n", rtip->mdl_min);
    VPRINT("\nmax of bounding rectangular parallelopiped --Pnt\n", rtip->mdl_max);


    /*
      printf("\nx value of maximum on the highest corner of parallelopiped is: %f", (rtip->mdl_max)[0]);
      printf("\ny value of maximum on the highest corner of parallelopiped is: %f", (rtip->mdl_max)[1]);
      printf("\nz value of maximum on the highest corner of parallelopiped is: %f", (rtip->mdl_max)[2]);
      printf("\nx value of minimum on the lowest corner of parallelopiped is: %f", (rtip->mdl_min)[0]);
      printf("\ny value of minimum on the lowest corner of parallelopiped is: %f", (rtip->mdl_min)[1]);
      printf("\nz value of minimum on the lowest corner of parallelopiped is: %f", (rtip->mdl_min)[2]);
    */


    sizeVoxelX = 1.0;
    sizeVoxelY = 1.0;
    sizeVoxelZ = 1.0;


    /* assume voxels are sizeVoxelX, sizeVoxelY, sizeVoxelZ size in each dimension */
    numVoxelX = (int)(((rtip->mdl_max)[0] - (rtip->mdl_min)[0])/sizeVoxelX) + 1;
    numVoxelY = (int)(((rtip->mdl_max)[1] - (rtip->mdl_min)[1])/sizeVoxelY) + 1;
    numVoxelZ = (int)(((rtip->mdl_max)[2] - (rtip->mdl_min)[2])/sizeVoxelZ) + 1;

    /* X is unused? */
    yMin = (int)((rtip->mdl_min)[1]);
    zMin = (int)((rtip->mdl_min)[2]);

    for (i = 0; i < numVoxelZ; i++) {
	for (j = 0; j < numVoxelY; j++) {
	    printf("Ray number is %d\n", numVoxelY * i + j);
	    RT_APPLICATION_INIT(&ap);
	    ap.a_rt_i = rtip;
	    ap.a_onehit = 0;

	    /* ray is hit through midpoint of the unit sized voxels */
	    VSET(ap.a_ray.r_pt, (rtip->mdl_min)[0] - 1.0, yMin + (j + 0.5) * sizeVoxelY, zMin + (i + 0.5) * sizeVoxelZ);
	    VSET(ap.a_ray.r_dir, 1.0, 0.0, 0.0);

	    ap.a_hit = hit;
	    ap.a_miss = miss;
	    ap.a_uptr = &sizeVoxelX;

	    rt_shootray(&ap);
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
