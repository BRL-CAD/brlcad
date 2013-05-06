/*                     G - V O X E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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

/**
 * Function to print values to File
 */
HIDDEN void
printToFile(genptr_t callBackData, int x, int y, int z, const char *a, fastf_t fill) {
    fastf_t *threshold = (fastf_t *)callBackData;
    FILE *fp;

    if (a != NULL) {
	fp = fopen("voxels1.txt","a");

	if (fp != NULL) {
	    fprintf(fp, "%f\t(%4d,%4d,%4d)\t%s\t%f\n", *threshold, x, y, z, a, fill);
	    fclose(fp);
	}
    }
    /* else this voxel is air */
}


/*
 *			M A I N
 */
int
main(int argc, char **argv) {

static struct rt_i *rtip;
fastf_t sizeVoxel[3], threshold;
int levelOfDetail;
genptr_t callBackData;

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


    /* user parameters are being given values directly here*/
    sizeVoxel[0] = 1.0;
    sizeVoxel[1] = 1.0;
    sizeVoxel[2] = 1.0;

    threshold = 0.5;
    levelOfDetail = 4;

    callBackData = (void *)(& threshold);

    /* voxelize function is called here with rtip(ray trace instance), userParameters and printToFile/printToScreen options */
    voxelize(rtip, sizeVoxel, levelOfDetail, printToFile, callBackData);

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
