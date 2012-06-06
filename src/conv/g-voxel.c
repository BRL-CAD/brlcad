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


struct rayInfo {
    float sizeVoxelX;
    float threshold;
    int *rayData;
};


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
    int voxelNumIn, voxelNumOut, presentVoxel=0, i, j = 0, numVoxelX, *rayData;
    struct partition *pp;
    struct rayInfo *voxelHits;
    struct hit *hitOutp, *hitInp;
    struct rt_i *rtip;
    float hitDistIn, hitDistOut, sizeVoxelX, inDistance = 0.0, threshold;


    voxelHits = (struct rayInfo*) ap->a_uptr;

    /**
     * length of voxels in the X-direction is sizeVoxelX, rtip is
     * structure for raytracing
     */
    sizeVoxelX = voxelHits->sizeVoxelX;
    rayData = voxelHits->rayData;
    threshold = voxelHits->threshold;

    rtip = ap->a_rt_i;
    pp = PartHeadp->pt_forw;

    /**
     * The following loop prints the voxels is present in path of rayi
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

	if((hitDistOut - 1.0) / sizeVoxelX - floor((hitDistOut - 1.0) / sizeVoxelX) <= 0.0) {
	    voxelNumOut -= 1;
	}


	/**
	 * If ray does not enter next partition (ie the voxel index
	 * from which ray exited) in the same voxel index, this means
	 * that last voxel of previous partition can be
	 * evaluated. Also, the intermediate voxels are NOT filled.
	 */
	if (presentVoxel != voxelNumIn) {

	    if (inDistance / sizeVoxelX >= threshold) {
		rayData[j++] += 1;

	    } else {
		rayData[j++] += 0;
	    }

	    for (i = 0; i < voxelNumIn - presentVoxel - 1; i++) {
		rayData[j++] += 0;
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
	    inDistance += (voxelNumIn + 1)*sizeVoxelX - hitDistIn + 1.0;

	    if (inDistance / sizeVoxelX >= threshold) {
		rayData[j++] += 1;
	    } else {
		rayData[j++] += 0;
	    }

	    for (i = 0; i < voxelNumOut - voxelNumIn - 1; i++) {
		rayData[j++] += 1;
	    }

	    presentVoxel = voxelNumOut;
	    inDistance = hitDistOut - 1.0 - voxelNumOut * sizeVoxelX;
	}


	pp = pp->pt_forw;
    }

    if (inDistance / sizeVoxelX >= threshold) {
	rayData[j++] += 1;
    } else {
	rayData[j++] += 0;
    }

    /**
     * voxels after the last partition are not in
     */
    numVoxelX = (int)(((rtip->mdl_max)[0] - (rtip->mdl_min)[0])/sizeVoxelX) ;
    for (i = 0; i < numVoxelX - presentVoxel - 1; i++) {
	rayData[j++] += 0;
    }

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
    return 0;
}


int
main(int argc, char **argv)
{
    struct application ap;
    static struct rt_i *rtip;
    struct rayInfo voxelHits;

    char title[1024] = {0};
    int i, j, k, numVoxelX, numVoxelY, numVoxelZ, yMin, zMin, *rayData, raysPerVoxel = 2, rayNum;
    float sizeVoxelX,  sizeVoxelY, sizeVoxelZ, threshold = 0.5, rayNumThreshold = 0.5;

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

    sizeVoxelX = 1.0;
    sizeVoxelY = 1.0;
    sizeVoxelZ = 1.0;


    /* assume voxels are sizeVoxelX, sizeVoxelY, sizeVoxelZ size in each dimension */
    numVoxelX = (int)(((rtip->mdl_max)[0] - (rtip->mdl_min)[0])/sizeVoxelX);
    numVoxelY = (int)(((rtip->mdl_max)[1] - (rtip->mdl_min)[1])/sizeVoxelY);
    numVoxelZ = (int)(((rtip->mdl_max)[2] - (rtip->mdl_min)[2])/sizeVoxelZ);

    /*rayData is pointer to an array of 1's and 0's telling whether that particular ray takes the voxel as in or out*/
    rayData = bu_calloc(numVoxelX, sizeof(int), "rayData");
    voxelHits.sizeVoxelX = sizeVoxelX;
    voxelHits.rayData = rayData;
    voxelHits.threshold = threshold;

    /* X is unused? */
    yMin = (int)((rtip->mdl_min)[1]);
    zMin = (int)((rtip->mdl_min)[2]);

    for (i = 0; i < numVoxelZ; i++) {
	for (j = 0; j < numVoxelY; j++) {
	    printf("\nRay number is %d\n", numVoxelY * i + j);
	    RT_APPLICATION_INIT(&ap);
	    ap.a_rt_i = rtip;
	    ap.a_onehit = 0;

	    for(k = 0; k < numVoxelX; k++) {
		rayData[k] = 0;
	    }

	    /* right now, rays are hit at the same point(no use, but have to see how to proceed) */
	    for(rayNum = 0; rayNum < raysPerVoxel; rayNum++) {

		/* ray is hit through midpoint of the unit sized voxels */
		VSET(ap.a_ray.r_pt, (rtip->mdl_min)[0] - 1.0, yMin + (j + 0.5) * sizeVoxelY, zMin + (i + 0.5) * sizeVoxelZ);
		VSET(ap.a_ray.r_dir, 1.0, 0.0, 0.0);

		ap.a_hit = hit;
		ap.a_miss = miss;
		ap.a_uptr = &voxelHits;

		rt_shootray(&ap);
	   }
	    printf("\nraydata in main is:\n");

	    for(k=0;k<numVoxelX;k++) {
		if(rayData[k] / raysPerVoxel >= rayNumThreshold) {
		    printf("1");
		}
		else {
		   printf("0");
		}
	    }
	    printf("\n");
	}
	printf("\n");
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
