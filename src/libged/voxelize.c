/*                         V O X E L I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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

#include "bu/cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"
#include "analyze.h"
#include "wdb.h"


struct voxelizeData
{
    fastf_t threshold;
    fastf_t *bbMin;
    fastf_t sizeVoxel[3];
    struct rt_wdb *wdbp;
    char *newname;
    struct wmember content;
};

HIDDEN void
create_boxes(genptr_t callBackData, int x, int y, int z, const char *a, fastf_t fill)
{
    if (a != NULL) {
	fastf_t min[3], max[3];

	struct bu_vls *vp;
	char bufx[50], bufy[50], bufz[50];
	char *nameDestination;

	struct voxelizeData *dataValues = (struct voxelizeData *)callBackData;

	sprintf(bufx, "%d", x);
	sprintf(bufy, "%d", y);
	sprintf(bufz, "%d", z);

	if (dataValues->threshold <= fill) {
	    vp = bu_vls_vlsinit();
	    bu_vls_strcat(vp, dataValues->newname);
	    bu_vls_strcat(vp, ".x");
	    bu_vls_strcat(vp, bufx);
	    bu_vls_strcat(vp, "y");
	    bu_vls_strcat(vp, bufy);
	    bu_vls_strcat(vp, "z");
	    bu_vls_strcat(vp, bufz);
	    bu_vls_strcat(vp, ".s");

	    min[0] = (dataValues->bbMin)[0] + (x * (dataValues->sizeVoxel)[0]);
	    min[1] = (dataValues->bbMin)[1] + (y * (dataValues->sizeVoxel)[1]);
	    min[2] = (dataValues->bbMin)[2] + (z * (dataValues->sizeVoxel)[2]);
	    max[0] = (dataValues->bbMin)[0] + ( (x + 1.0) * (dataValues->sizeVoxel)[0]);
	    max[1] = (dataValues->bbMin)[1] + ( (y + 1.0) * (dataValues->sizeVoxel)[1]);
	    max[2] = (dataValues->bbMin)[2] + ( (z + 1.0) * (dataValues->sizeVoxel)[2]);

	    nameDestination = bu_vls_strgrab(vp);
	    mk_rpp(dataValues->wdbp,nameDestination, min, max);
	    mk_addmember(nameDestination, &dataValues->content.l, 0, WMOP_UNION);
	}
    }
    /* else this voxel is air */
}

int
ged_voxelize(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_i *rtip;
    static const char *usage = "[-s \"dx dy dz\"] [-d n] [-t f] new_obj old_obj [old_obj2 old_obj3 ...]";
    fastf_t sizeVoxel[3];
    int levelOfDetail;
    genptr_t callBackData;
    struct voxelizeData voxDat;
    int c;

    /* intentionally double for scan */
    double threshold;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialization */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* incorrect arguments */
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    sizeVoxel[0]  = 1.0;
    sizeVoxel[1]  = 1.0;
    sizeVoxel[2]  = 1.0;
    levelOfDetail = 1;
    threshold = 0.5;

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, (const char *)"s:d:t:")) != -1) {
	double scan[3];

	switch (c) {
	    case 's':
		if (sscanf(bu_optarg, "%lf %lf %lf",
			   &scan[0],
			   &scan[1],
			   &scan[2]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return GED_ERROR;
		} else {
		    /* convert from double to fastf_t */
		    VMOVE(sizeVoxel, scan);

		    sizeVoxel[0] = sizeVoxel[0] * gedp->ged_wdbp->dbip->dbi_local2base;
		    sizeVoxel[1] = sizeVoxel[1] * gedp->ged_wdbp->dbip->dbi_local2base;
		    sizeVoxel[2] = sizeVoxel[2] * gedp->ged_wdbp->dbip->dbi_local2base;
		}
		break;

	    case 'd':
		if (sscanf(bu_optarg, "%d", &levelOfDetail) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return GED_ERROR;
		}
		break;

	    case 't':
		if (sscanf(bu_optarg, "%lf", &threshold) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return GED_ERROR;
		}
		break;

	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	}
    }

    argc -= bu_optind;
    argv += bu_optind;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "error: missing argument(s)\n");
	return GED_ERROR;
    }

    voxDat.newname = (char *)argv[0];
    argc--;
    argv++;

    if (db_lookup(gedp->ged_wdbp->dbip, voxDat.newname, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "error: solid '%s' already exists, aborting\n", voxDat.newname);
	return GED_ERROR;
    }

    rtip = rt_new_rti(gedp->ged_wdbp->dbip);
    rtip->useair = 1;

    /* Walk trees.  Here we identify any object trees in the database
     * that the user wants included in the ray trace.
     */
    while (argc > 0) {
	if (rt_gettree(rtip,argv[0]) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "error: object '%s' does not exists, aborting\n", argv[1]);
	    return GED_ERROR;
	}

	argc--;
	argv++;
    }


    voxDat.sizeVoxel[0] = sizeVoxel[0];
    voxDat.sizeVoxel[1] = sizeVoxel[1];
    voxDat.sizeVoxel[2] = sizeVoxel[2];
    voxDat.threshold = threshold;
    voxDat.wdbp = gedp->ged_wdbp;
    voxDat.bbMin = rtip->mdl_min;
    BU_LIST_INIT(&voxDat.content.l);

    callBackData = (void*)(&voxDat);

   /* voxelize function is called here with rtip(ray trace instance), userParameter and create_boxes function */
    voxelize(rtip, sizeVoxel, levelOfDetail, create_boxes, callBackData);

    mk_comb(gedp->ged_wdbp, voxDat.newname, &voxDat.content.l, 1, "plastic", "sh=4 sp=0.5 di=0.5 re=0.1", 0, 1000, 0, 0, 100, 0, 0, 0);

    mk_freemembers(&voxDat.content.l);
    rt_free_rti(rtip);

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
