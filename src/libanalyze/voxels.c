/*                    V O X E L S . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2012 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "vmath.h"		/* vector math macros */
#include "raytrace.h"		/* librt interface definitions */

#include "analyze.h"
#include "bu.h"

/**
 * Function to assign a new region to a voxel.
 */
HIDDEN void
setRegionName(struct bu_vls **vp, const char **nameSource, const char **nameDestination) {
    size_t newlen;

    *vp = bu_vls_vlsinit();
    newlen = strlen(*nameSource);
    bu_vls_setlen(*vp,newlen);
    (*vp)->vls_str = (char *)(*nameSource);

    *nameDestination = bu_vls_strgrab(*vp);
}


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
hit_voxelize(struct application *ap, struct partition *PartHeadp, struct seg*UNUSED(segs))
{
    struct partition *pp;
    struct rayInfo *voxelHits;
    struct hit *hitOutp, *hitInp;
    struct voxelRegion *tmp, *newRegion;
    struct bu_vls *vp;

    int voxelNumIn, voxelNumOut, j = 0, regionSaved;
    fastf_t hitDistIn, hitDistOut, sizeVoxel[3], *fillDistances;


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

	regionSaved = 0;

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

	voxelNumIn =  (int)(hitDistIn / sizeVoxel[0]);
	voxelNumOut = (int)(hitDistOut / sizeVoxel[0]);

	if (EQUAL((hitDistOut / sizeVoxel[0]), floor(hitDistOut / sizeVoxel[0]))) {
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

	    if(voxelHits->regionList[voxelNumIn].regionName == NULL) {

		setRegionName(&vp, &(pp->pt_regionp->reg_name), &(voxelHits->regionList[voxelNumIn].regionName));
		voxelHits->regionList[voxelNumIn].regionDistance += hitDistOut - hitDistIn;

	    } else {

		tmp = voxelHits->regionList + voxelNumIn;

		while(tmp->nextRegion != NULL) {

		    if(!bu_strcmp(pp->pt_regionp->reg_name, tmp->regionName)){

			regionSaved = 1;
			tmp->regionDistance += hitDistOut - hitDistIn;

		    }
		    tmp = tmp->nextRegion;
		}

		if(!bu_strcmp(tmp->regionName,pp->pt_regionp->reg_name)){
		    regionSaved = 1;
		    tmp->regionDistance += hitDistOut - hitDistIn;
		}
		if(regionSaved!=1) {
		    newRegion = bu_calloc(1, sizeof(struct voxelRegion), "newRegion");

		    tmp->nextRegion = newRegion;

		    setRegionName(&vp, &(pp->pt_regionp->reg_name), &(newRegion->regionName));

		    newRegion->nextRegion = NULL;
		    newRegion->regionDistance += hitDistOut - hitDistIn;
		}
	    }


	} else {

	    fillDistances[voxelNumIn] += (voxelNumIn + 1) * sizeVoxel[0] - hitDistIn;
	    if(voxelHits->regionList[voxelNumIn].regionName == NULL) {

		setRegionName(&vp, &(pp->pt_regionp->reg_name), &(voxelHits->regionList[voxelNumIn].regionName));
		voxelHits->regionList[voxelNumIn].regionDistance += (voxelNumIn + 1) * sizeVoxel[0] - hitDistIn;

	    }  else {
		tmp = voxelHits->regionList + voxelNumIn;
		while(tmp->nextRegion != NULL) {

		    if(!bu_strcmp(pp->pt_regionp->reg_name,tmp->regionName)) {
			regionSaved = 1;
			tmp->regionDistance += (voxelNumIn + 1) * sizeVoxel[0] - hitDistIn;
		    }
		    tmp = tmp->nextRegion;
		}
		if(!bu_strcmp(tmp->regionName, pp->pt_regionp->reg_name)) {
		    regionSaved = 1;
		    tmp->regionDistance += (voxelNumIn + 1) * sizeVoxel[0] - hitDistIn;
		}
		if(regionSaved!=1){
		    newRegion = bu_calloc(1, sizeof(struct voxelRegion), "newRegion");
		    tmp->nextRegion = newRegion;

		    setRegionName(&vp, &(pp->pt_regionp->reg_name), &(newRegion->regionName));

		    newRegion->nextRegion = NULL;
		    newRegion->regionDistance += (voxelNumIn + 1) * sizeVoxel[0] - hitDistIn;
		}
	    }

	    for (j = voxelNumIn + 1; j<voxelNumOut; j++) {

		fillDistances[j] += sizeVoxel[0];
		setRegionName(&vp, &(pp->pt_regionp->reg_name), &(voxelHits->regionList[j].regionName));
		voxelHits->regionList[j].regionDistance += sizeVoxel[0];
	    }

	    fillDistances[voxelNumOut] += hitDistOut - (voxelNumOut * sizeVoxel[0]);
	    setRegionName(&vp, &(pp->pt_regionp->reg_name), &(voxelHits->regionList[voxelNumOut].regionName));
	    voxelHits->regionList[voxelNumOut].regionDistance += hitDistOut - (voxelNumOut * sizeVoxel[0]);

	}

	pp = pp->pt_forw;
    }

    return 0;

}


/**
 * voxelize function takes raytrace instance and user parameters as inputs
 */
void
voxelize(struct rt_i *rtip, fastf_t sizeVoxel[3], int levelOfDetail, void (*printFunction)(genptr_t callBackData, int x, int y, int z, const char *regionName, fastf_t percentageFill), genptr_t callBackData){

    struct application ap;
    struct rayInfo voxelHits;
    struct voxelRegion *tmp, *old;

    int i, j, k, numVoxel[3], yMin, zMin, rayNum;
    fastf_t *voxelArray, rayTraceDistance, effectiveDistance;

    /* get bounding box values etc */
    rt_prep_parallel(rtip, 1);


    /* calculate number oof voxels in each dimension */
    numVoxel[0] = (int)(((rtip->mdl_max)[0] - (rtip->mdl_min)[0])/sizeVoxel[0]) + 1;
    numVoxel[1] = (int)(((rtip->mdl_max)[1] - (rtip->mdl_min)[1])/sizeVoxel[1]) + 1;
    numVoxel[2] = (int)(((rtip->mdl_max)[2] - (rtip->mdl_min)[2])/sizeVoxel[2]) + 1;

    if(EQUAL(numVoxel[0] - 1, (((rtip->mdl_max)[0] - (rtip->mdl_min)[0])/sizeVoxel[0]))) {
	numVoxel[0] -=1;
    }
    if(EQUAL(numVoxel[1] - 1, (((rtip->mdl_max)[1] - (rtip->mdl_min)[1])/sizeVoxel[1]))) {
	numVoxel[1] -=1;
    }
    if(EQUAL(numVoxel[2] - 1, (((rtip->mdl_max)[2] - (rtip->mdl_min)[2])/sizeVoxel[2]))) {
	numVoxel[2] -=1;
    }

    voxelHits.sizeVoxel[0] = sizeVoxel[0];
    voxelHits.sizeVoxel[1] = sizeVoxel[1];
    voxelHits.sizeVoxel[2] = sizeVoxel[2];

    /* voxelArray stores the distance in path of ray inside a voxel which is filled*/
    voxelArray = bu_calloc(numVoxel[0], sizeof(fastf_t), "voxelArray");

    /* regionList holds the names of voxels inside the voxels*/
    voxelHits.regionList = bu_calloc(numVoxel[0], sizeof(struct voxelRegion), "regionList");

    /* initialize values of region, fillDistances etc. */
    for(k = 0; k < numVoxel[0]; k++) {
	voxelArray[k] = 0.0;
	voxelHits.regionList[k].regionName = NULL;
	voxelHits.regionList[k].nextRegion = NULL;
    }

    /* minimum value of bounding box in Y and Z directions */
    yMin = (int)((rtip->mdl_min)[1]);
    zMin = (int)((rtip->mdl_min)[2]);

    /* 1.0 / (levelOfDetail + 1) and effectiveDistance have to be used multiple times in the following loops */
    rayTraceDistance = 1.0 / (levelOfDetail + 1);
    effectiveDistance = levelOfDetail * levelOfDetail * sizeVoxel[0];

    /* start shooting */
    for (i = 0; i < numVoxel[2]; i++) {
	for (j = 0; j < numVoxel[1]; j++) {

	    RT_APPLICATION_INIT(&ap);
	    ap.a_rt_i = rtip;
	    ap.a_onehit = 0;
	    VSET(ap.a_ray.r_dir, 1.0, 0.0, 0.0);

	    ap.a_hit = hit_voxelize;
	    ap.a_miss = NULL;
	    ap.a_uptr = &voxelHits;

	    voxelHits.fillDistances = voxelArray;

	    for (rayNum = 1; rayNum <= levelOfDetail; rayNum++) {
		for (k = 1; k <= levelOfDetail; k++) {

		    /* ray is hit through evenly spaced points of the unit sized voxels */
		    VSET(ap.a_ray.r_pt, (rtip->mdl_min)[0] - 1.0, yMin + (j + k * rayTraceDistance) * sizeVoxel[1], zMin + (i + rayNum * rayTraceDistance) * sizeVoxel[2]);
		    rt_shootray(&ap);
		}
	    }

	    /*print results into file name supplied by user*/
	    for (k = 0; k < numVoxel[0]; k++) {

		    if(voxelHits.regionList[k].regionName==NULL){

			printFunction(callBackData, k, j, i, "air", 1.0);

		    } else {

			tmp = voxelHits.regionList + k;
			printFunction(callBackData, k, j, i, tmp->regionName, tmp->regionDistance / effectiveDistance);
			old = tmp->nextRegion;

			while(old != NULL) {
			    tmp = old;
			    printFunction(callBackData, k, j, i, tmp->regionName, tmp->regionDistance / effectiveDistance);
			    old = tmp->nextRegion;
			    /* free the space allocated for new regions */
			    bu_free(tmp, "");
			 }

		    }

		voxelArray[k] = 0.0;
		voxelHits.regionList[k].regionName = NULL;
		voxelHits.regionList[k].nextRegion = NULL;
		voxelHits.regionList[k].regionDistance = 0.0;
		}
	    }
	}

    bu_free(voxelArray, "");
    bu_free(voxelHits.regionList, "");
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
