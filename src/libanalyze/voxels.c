/*                    V O X E L S . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2014 United States Government as represented by
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
 * Function to get the corresponding region entry to a region name.
 */
HIDDEN struct voxelRegion *
getRegionByName(struct voxelRegion *head, const char *regionName) {
    struct voxelRegion *ret = NULL;

    BU_ASSERT(regionName != NULL);

    if (head->regionName == NULL) { /* the first region on this voxel */
	head->regionName = bu_strdup(regionName);
	ret = head;
    }
    else {
	while (head->nextRegion != NULL) {
	    if (bu_strcmp(head->regionName, regionName) == 0) {
		ret = head;
		break;
	    }

	    head = head->nextRegion;
	}

	if (ret == NULL) { /* not found until here */
	    if (bu_strcmp(head->regionName ,regionName) == 0) /* is it the last one on the list? */
		ret = head;
	    else {
		BU_ALLOC(ret, struct voxelRegion);
		head->nextRegion = ret;
		ret->regionName  = bu_strdup(regionName);
	    }
	}
    }

    return ret;
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
hit_voxelize(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    struct partition *pp            = PartHeadp->pt_forw;
    struct rayInfo   *voxelHits     = (struct rayInfo*) ap->a_uptr;
    fastf_t           sizeVoxel     = voxelHits->sizeVoxel;
    fastf_t          *fillDistances = voxelHits->fillDistances;

    while (pp != PartHeadp) {
	/**
	 * hitInp, hitOutp are hit structures to save distances where
	 * ray entered and exited the present partition.  hitDistIn,
	 * hitDistOut are the respective distances from the origin of
	 * ray.  voxelNumIn, voxelNumOut are the voxel numbers where
	 * ray entered and exited the present partition.
	 */

	struct hit *hitInp      = pp->pt_inhit;
	struct hit *hitOutp     = pp->pt_outhit;
	fastf_t     hitDistIn   = hitInp->hit_dist - 1.;
	fastf_t     hitDistOut  = hitOutp->hit_dist - 1.;
	int         voxelNumIn  = (int)(hitDistIn / sizeVoxel);
	int         voxelNumOut = (int)(hitDistOut / sizeVoxel);

	if (EQUAL((hitDistOut / sizeVoxel), floor(hitDistOut / sizeVoxel)))
	    voxelNumOut = FMAX(voxelNumIn, voxelNumOut - 1);

	/**
	 * If voxel entered and voxel exited are same then nothing can
	 * be evaluated till we see the next partition too. If not,
	 * evaluate entry voxel. Also, all the intermediate voxels are
	 * in.
	 */
	if (voxelNumIn == voxelNumOut) {
	    fillDistances[voxelNumIn]                                                                     += hitDistOut - hitDistIn;
	    getRegionByName(voxelHits->regionList + voxelNumIn, pp->pt_regionp->reg_name)->regionDistance += hitDistOut - hitDistIn;
	} else {
	    int j;

	    fillDistances[voxelNumIn]                                                                     += (voxelNumIn + 1) * sizeVoxel - hitDistIn;
	    getRegionByName(voxelHits->regionList + voxelNumIn, pp->pt_regionp->reg_name)->regionDistance += (voxelNumIn + 1) * sizeVoxel - hitDistIn;

	    for (j = voxelNumIn + 1; j < voxelNumOut; ++j) {
		fillDistances[j]                                                                     += sizeVoxel;
		getRegionByName(voxelHits->regionList + j, pp->pt_regionp->reg_name)->regionDistance += sizeVoxel;
	    }

	    fillDistances[voxelNumOut]                                                                     += hitDistOut - (voxelNumOut * sizeVoxel);
	    getRegionByName(voxelHits->regionList + voxelNumOut, pp->pt_regionp->reg_name)->regionDistance += hitDistOut - (voxelNumOut * sizeVoxel);
	}

	pp = pp->pt_forw;
    }

    return 0;

}


/**
 * voxelize function takes raytrace instance and user parameters as inputs
 */
void
voxelize(struct rt_i *rtip, fastf_t sizeVoxel[3], int levelOfDetail, void (*create_boxes)(void *callBackData, int x, int y, int z, const char *regionName, fastf_t percentageFill), void *callBackData)
{
    struct rayInfo voxelHits;
    int            numVoxel[3];
    int            yMin;
    int            zMin;
	int            i, j, k, rayNum;
    fastf_t        *voxelArray;
    fastf_t        rayTraceDistance;
    fastf_t        effectiveDistance;

    /* get bounding box values etc. */
    rt_prep_parallel(rtip, 1);

    /* calculate number of voxels in each dimension */
    numVoxel[0] = (int)(((rtip->mdl_max)[0] - (rtip->mdl_min)[0])/sizeVoxel[0]) + 1;
    numVoxel[1] = (int)(((rtip->mdl_max)[1] - (rtip->mdl_min)[1])/sizeVoxel[1]) + 1;
    numVoxel[2] = (int)(((rtip->mdl_max)[2] - (rtip->mdl_min)[2])/sizeVoxel[2]) + 1;

    if (EQUAL(numVoxel[0] - 1, (((rtip->mdl_max)[0] - (rtip->mdl_min)[0])/sizeVoxel[0])))
	numVoxel[0] -=1;

    if (EQUAL(numVoxel[1] - 1, (((rtip->mdl_max)[1] - (rtip->mdl_min)[1])/sizeVoxel[1])))
	numVoxel[1] -=1;

    if (EQUAL(numVoxel[2] - 1, (((rtip->mdl_max)[2] - (rtip->mdl_min)[2])/sizeVoxel[2])))
	numVoxel[2] -=1;

    voxelHits.sizeVoxel = sizeVoxel[0];

    /* voxelArray stores the distance in path of ray inside a voxel which is filled
     * initialize with 0s */
    voxelArray = (fastf_t *)bu_calloc(numVoxel[0], sizeof(fastf_t), "voxelize:voxelArray");

    /* regionList holds the names of voxels inside the voxels
     * initialize with NULLs */
    voxelHits.regionList = (struct voxelRegion *)bu_calloc(numVoxel[0], sizeof(struct voxelRegion), "voxelize:regionList");

    /* minimum value of bounding box in Y and Z directions */
    yMin = (int)((rtip->mdl_min)[1]);
    zMin = (int)((rtip->mdl_min)[2]);

    BU_ASSERT_LONG(levelOfDetail, >, 0);
    /* 1.0 / (levelOfDetail + 1) and effectiveDistance have to be used multiple times in the following loops */
    rayTraceDistance  = 1. / levelOfDetail;
    effectiveDistance = levelOfDetail * levelOfDetail * sizeVoxel[0];

    /* start shooting */
    for (i = 0; i < numVoxel[2]; ++i) {
	for (j = 0; j < numVoxel[1]; ++j) {
	    struct application ap;

	    RT_APPLICATION_INIT(&ap);
	    ap.a_rt_i   = rtip;
	    ap.a_onehit = 0;
	    VSET(ap.a_ray.r_dir, 1., 0., 0.);

	    ap.a_hit  = hit_voxelize;
	    ap.a_miss = NULL;
	    ap.a_uptr = &voxelHits;

	    voxelHits.fillDistances = voxelArray;

	    for (rayNum = 0; rayNum < levelOfDetail; ++rayNum) {
		for (k = 0; k < levelOfDetail; ++k) {

		    /* ray is hit through evenly spaced points of the unit sized voxels */
		    VSET(ap.a_ray.r_pt, (rtip->mdl_min)[0] - 1.,
			                yMin + (j + (k + 0.5) * rayTraceDistance) * sizeVoxel[1],
			                zMin + (i + (rayNum + 0.5) * rayTraceDistance) * sizeVoxel[2]);
		    rt_shootray(&ap);
		}
	    }

	    /* output results via a call-back supplied by user*/
	    for (k = 0; k < numVoxel[0]; ++k) {
		if (voxelHits.regionList[k].regionName == NULL)
		    /* an air voxel */
		    create_boxes(callBackData, k, j, i, NULL, 0.);
		else {
		    struct voxelRegion *tmp = voxelHits.regionList + k;
		    struct voxelRegion *old = tmp->nextRegion;

		    create_boxes(callBackData, k, j, i, tmp->regionName, tmp->regionDistance / effectiveDistance);

		    if (tmp->regionName != 0)
			bu_free(tmp->regionName, "voxelize:voxelRegion:regionName");

		    while (old != NULL) {
			tmp = old;
			create_boxes(callBackData, k, j, i, tmp->regionName, tmp->regionDistance / effectiveDistance);
			old = tmp->nextRegion;

			/* free the space allocated for new regions */
			if (tmp->regionName != 0)
			    bu_free(tmp->regionName, "voxelize:voxelRegion:regionName");

			BU_FREE(tmp, struct voxelRegion);
		    }
		}

		voxelArray[k] = 0.;
		voxelHits.regionList[k].regionName     = NULL;
		voxelHits.regionList[k].nextRegion     = NULL;
		voxelHits.regionList[k].regionDistance = 0.;
	    }
	}
    }

    bu_free(voxelArray, "voxelize:voxelArray");
    bu_free(voxelHits.regionList, "voxelize:regionList");
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
