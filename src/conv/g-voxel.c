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
 * This structure stores the information about voxels provided by a single raytrace.
 */

struct rayInfo {
    fastf_t sizeVoxel[3];
    fastf_t *fillDistances;
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
    int voxelNumIn, voxelNumOut, j = 0;
    fastf_t hitDistIn, hitDistOut, sizeVoxel[3], *fillDistances;

    struct partition *pp;
    struct rayInfo *voxelHits;
    struct hit *hitOutp, *hitInp;

    voxelHits = (struct rayInfo*) ap->a_uptr;

    /**
     * length of voxels in the 3 directions is stored in sizeVoxel[],
     */
    sizeVoxel[0] = voxelHits->sizeVoxel[0];
    sizeVoxel[1] = voxelHits->sizeVoxel[1];
    sizeVoxel[2] = voxelHits->sizeVoxel[2];

    pp = PartHeadp->pt_forw;
    fillDistances = voxelHits->fillDistances;

    while (pp != PartHeadp) {

	/**
	 * hitInp, hitOutp are hit structures to save distances where
	 * ray entered and exited the present partition.  hitDistIn,
	 * hitDistOut are the respective distances from the origin of
	 * ray.  voxelNumIn, voxelNumOut are the voxel numbers where
	 * ray entered and exited the present partition.
	 */
	hitInp = pp->pt_inhit;
	hitOutp = pp->pt_outhit;

	hitDistIn = hitInp->hit_dist - 1.0;
	hitDistOut = hitOutp->hit_dist - 1.0;

	voxelNumIn = ((int) hitDistIn / sizeVoxel[0]);
	voxelNumOut = ((int) hitDistOut / sizeVoxel[0]);

	if(ZERO((hitDistOut / sizeVoxel[0]) - floor(hitDistOut / sizeVoxel[0]))) {
	    voxelNumOut -= 1;
	}

	/**
	 * If voxel entered and voxel exited are same then nothing can
	 * be evaluated till we see the next partition too. If not,
	 * evaluate entry voxel. Also, all the intermediate voxels are
	 * in.
	 */
	if (voxelNumIn == voxelNumOut) {

	    fillDistances[voxelNumIn] +=  hitDistOut - hitDistIn;

	} else {

	    fillDistances[voxelNumIn] += (voxelNumIn + 1) * sizeVoxel[0] - hitDistIn;

	    for(j = voxelNumIn + 1; j<voxelNumOut; j++) {
		fillDistances[j] += sizeVoxel[0];
	    }

	    fillDistances[voxelNumOut] += hitDistOut - (voxelNumOut * sizeVoxel[0]);
	}

	pp = pp->pt_forw;
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
    int i, j, k, numVoxel[3], yMin, zMin, raysPerVoxel = 4, rayNum;
    fastf_t sizeVoxel[3], threshold = 0.5, *voxelArray, rayTraceDistance;

    FILE *fp;

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
     * partitioning, computes bounding volumes, etc.
     */
    rt_prep_parallel(rtip, 1);

    /* assume voxels are sizeVoxel[0], sizeVoxel[1], sizeVoxel[2] size in each dimension */
    sizeVoxel[0] = 1.0;
    sizeVoxel[1] = 1.0;
    sizeVoxel[2] = 1.0;

    numVoxel[0] = (int)(((rtip->mdl_max)[0] - (rtip->mdl_min)[0])/sizeVoxel[0]);
    numVoxel[1] = (int)(((rtip->mdl_max)[1] - (rtip->mdl_min)[1])/sizeVoxel[1]);
    numVoxel[2] = (int)(((rtip->mdl_max)[2] - (rtip->mdl_min)[2])/sizeVoxel[2]);

    voxelHits.sizeVoxel[0] = sizeVoxel[0];
    voxelHits.sizeVoxel[1] = sizeVoxel[1];
    voxelHits.sizeVoxel[2] = sizeVoxel[2];

    /* voxelArray stores the distance in path of ray inside a voxel which is filled*/
    voxelArray = bu_calloc(numVoxel[0], sizeof(fastf_t), "voxelArray");

    for(k = 0; k < numVoxel[0]; k++) {
	voxelArray[k] = 0.0;
    }

    /* minimum value of bounding box in Y and Z directions */
    yMin = (int)((rtip->mdl_min)[1]);
    zMin = (int)((rtip->mdl_min)[2]);

    /* 1.0 / (raysPerVoxel + 1) has to be used multiple times in the following loops */
    rayTraceDistance = 1.0 / (raysPerVoxel + 1);

    fp = fopen("voxels.txt","w");

    fprintf(fp, "voxel specs :\n\tDimensions of a voxel are : (%f,%f,%f)\n\tNumber of voxels in x,y,z direction is %d,%d,%d respectively\n\n", sizeVoxel[0], sizeVoxel[1], sizeVoxel[2], numVoxel[0], numVoxel[1], numVoxel[2]);


    /* start shooting */
    for (i = 0; i < numVoxel[2]; i++) {
	for (j = 0; j < numVoxel[1]; j++) {
	    RT_APPLICATION_INIT(&ap);
	    ap.a_rt_i = rtip;
	    ap.a_onehit = 0;
	    VSET(ap.a_ray.r_dir, 1.0, 0.0, 0.0);

	    ap.a_hit = hit;
	    ap.a_miss = miss;
	    ap.a_uptr = &voxelHits;

	    voxelHits.fillDistances = voxelArray;

	    for(rayNum = 1; rayNum <= raysPerVoxel; rayNum++) {
		for(k = 1; k <= raysPerVoxel; k++) {

		    /* ray is hit through evenly spaced points of the unit sized voxels */
		    VSET(ap.a_ray.r_pt, (rtip->mdl_min)[0] - 1.0, yMin + (j + k * rayTraceDistance) * sizeVoxel[1], zMin + (i + rayNum * rayTraceDistance) * sizeVoxel[2]);
		    rt_shootray(&ap);
		}
	   }

	    for(k = 0; k < numVoxel[0]; k++) {
		if(voxelArray[k] >= threshold){
		    fprintf(fp, "1 ");
		} else {
		    fprintf(fp, "0 ");
		}
		voxelArray[k] = 0.0;
	    }
	    fprintf(fp, "\n");
	}
	fprintf(fp, "\n");
    }

    fclose(fp);
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
