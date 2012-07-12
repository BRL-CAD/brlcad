/*                         V O X E L I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2012 United States Government as represented by
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
/** @file libged/voxelize.c
 *
 * The voxelize command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"
#include "analyze.h"

int
ged_voxelize(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_i *rtip;
    static const char *usage = "object [object ...]";
    fastf_t sizeVoxel[3], threshold;
    int levelOfDetail;
    genptr_t callBackData;


    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialization */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* incorrect arguments */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    rtip = rt_new_rti(gedp->ged_wdbp->dbip);
    rtip->useair = 1;

    /* Walk trees.  Here we identify any object trees in the database
     * that the user wants included in the ray trace.
     */
    while(argc > 1) {
	if(rt_gettree(rtip,argv[1]) < 0)
	    return GED_ERROR;
	argc--;
	argv++;
    }

    /* get bounding box values etc */
    rt_prep_parallel(rtip, 1);

    bu_vls_printf(gedp->ged_result_str, "the minimum of bounding box is%d\n", (rtip->mdl_min)[0]);

    /* user parameters are being given values directly here*/
    sizeVoxel[0] = 1.0;
    sizeVoxel[1] = 1.0;
    sizeVoxel[2] = 1.0;

    threshold = 0.5;
    levelOfDetail = 4;

    callBackData = (void *)(& threshold);

    /* voxelize function is called here with rtip(ray trace instance), userParameters and printToFile/printToScreen options */
    voxelize(rtip, sizeVoxel, levelOfDetail, printToScreen, callBackData);

    
    return GED_OK;
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
