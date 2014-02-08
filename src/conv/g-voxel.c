/*                     G - V O X E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
#include "analyze.h"


struct voxelizeData
{
    fastf_t voxelSize[3];
    fastf_t threshold;
    fastf_t bbMin[3];
};


/**
 * Function to print values to File
 */
HIDDEN void
printToFile(genptr_t callBackData, int x, int y, int z, const char *a, fastf_t fill) {
    struct voxelizeData *dataValues = (struct voxelizeData *)callBackData;
    FILE *fp;

    if ((a != NULL) && (dataValues->threshold <= fill)) {
	fp = fopen("voxels1.txt","a");

	if (fp != NULL) {
	    fastf_t voxel[3];

	    voxel[0] = dataValues->bbMin[0] + (x + 0.5) * dataValues->voxelSize[0];
	    voxel[1] = dataValues->bbMin[1] + (y + 0.5) * dataValues->voxelSize[1];
	    voxel[2] = dataValues->bbMin[2] + (z + 0.5) * dataValues->voxelSize[2];

	    fprintf(fp, "(%f, %f, %f)\t%s\t%f\n", voxel[0], voxel[1], voxel[2], a, fill);
	    fclose(fp);
	}
    }
    /* else this voxel is air */
}


int
main(int argc, char **argv)
{
    static struct rt_i *rtip;
    static const char *usage = "[-s \"dx dy dz\"] [-d n] [-t f] model.g objects...\n";
    struct voxelizeData dataValues;
    int levelOfDetail;
    genptr_t callBackData;
    int c;
    int gottree = 0;

    char title[1024] = {0};

    /* Check for command-line arguments.  Make sure we have at least a
     * geometry file and one geometry object on the command line.
     */
    if (argc < 3) {
	bu_exit(1, "Usage: %s %s", argv[0], usage);
    }

    /* default user parameters */

    dataValues.voxelSize[0] = 1.0;
    dataValues.voxelSize[1] = 1.0;
    dataValues.voxelSize[2] = 1.0;

    dataValues.threshold = 0.5;

    levelOfDetail = 4;

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, (const char *)"s:d:t:")) != -1) {
	double scan[3];

	switch (c) {
	    case 's':
		if (sscanf(bu_optarg, "%lf %lf %lf",
			   &scan[0],
			   &scan[1],
			   &scan[2]) != 3) {
		    bu_exit(1, "Usage: %s %s", argv[0], usage);
		} else {
		    /* convert from double to fastf_t */
		    VMOVE(dataValues.voxelSize, scan);
		}
		break;

	    case 'd':
		if (sscanf(bu_optarg, "%d", &levelOfDetail) != 1) {
		    bu_exit(1, "Usage: %s %s", argv[0], usage);
		}
		break;

	    case 't':
		if(sscanf(bu_optarg, "%lf", &dataValues.threshold) != 1) {
		    bu_exit(1, "Usage: %s %s", argv[0], usage);
		}
		break;

	    default:
		bu_exit(1, "Usage: %s %s", argv[0], usage);
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    /* Load the specified geometry database (i.e., a ".g" file).
     * rt_dirbuild() returns an "instance" pointer which describes the
     * database to be raytraced.  It also gives you back the title
     * string if you provide a buffer.  This builds a directory of the
     * geometry (i.e., a table of contents) in the file.
     */
    rtip = rt_dirbuild(argv[0], title, sizeof(title));
    if (rtip == RTI_NULL) {
	bu_exit(2, "Building the database directory for [%s] FAILED\n", argv[0]);
    }
    argc--;
    argv++;

    /* Walk the geometry trees.  Here you identify any objects in the
     * database that you want included in the ray trace by iterating
     * of the object names that were specified on the command-line.
     */
    while (argc > 0) {
	if (rt_gettree(rtip, argv[0]) < 0)
	    bu_log("Loading the geometry for [%s] FAILED\n", argv[0]);
	else
	    gottree = 1;

	argc--;
	argv++;
    }

    if (gottree != 0) {
	VMOVE(dataValues.bbMin, rtip->mdl_min);

	callBackData = (void *)(& dataValues);

	/* voxelize function is called here with rtip(ray trace instance), userParameters and printToFile/printToScreen options */
	voxelize(rtip, dataValues.voxelSize, levelOfDetail, printToFile, callBackData);
    }

    rt_free_rti(rtip);

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
