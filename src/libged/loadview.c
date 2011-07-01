/*                         L O A D V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/loadview.c
 *
 * The loadview command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


vect_t _ged_eye_model;
mat_t _ged_viewrot;
struct ged *_ged_current_gedp;

/**
 * here we define a minimal table of commands that are supported by the
 * loadview command.  unsupported commands are those that have no bearing on
 * view restoration.
 */
struct command_tab ged_loadview_cmdtab[] = {
    {"viewsize", "size in mm", "set view size",
     _ged_cm_vsize,	2, 2},
    {"eye_pt", "xyz of eye", "set eye point",
     _ged_cm_eyept,	4, 4},
    {"lookat_pt", "x y z [yflip]", "set eye look direction, in X-Y plane",
     _ged_cm_lookat_pt,	4, 5},
    {"viewrot", "4x4 matrix", "set view direction from matrix",
     _ged_cm_vrot,	17, 17},
    {"orientation", "quaternion", "set view direction from quaturnion",
     _ged_cm_orientation,	5, 5},
    {"set", 	"", "show or set parameters",
     _ged_cm_set,		1, 999},

    /* begin unsupported commands (for view loading) */

    {"start", "frame number", "start a new frame",
     _ged_cm_null,	2, 2},
    {"clean", "", "clean articulation from previous frame",
     _ged_cm_null,	1, 1},
    {"end", 	"", "end of frame setup, begin raytrace",
     _ged_cm_null,		1, 1},

    /* not output, by default in saveview */

    {"multiview", "", "produce stock set of views",
     _ged_cm_null,	1, 1},
    {"anim", 	"path type args", "specify articulation animation",
     _ged_cm_null,	4, 999},
    {"tree", 	"treetop(s)", "specify alternate list of tree tops",
     _ged_cm_null,	1, 999},
    {"ae", "azim elev", "specify view as azim and elev, in degrees",
     _ged_cm_null,		3, 3},
    {"opt", "-flags", "set flags, like on command line",
     _ged_cm_null,		2, 999},

    /* this is a quick hack used for quietly parsing the EOF delimiter in the
     * script files.
     */
    {"EOF", "", "End of file delimiter",
     _ged_cm_null,		1, 1},

    /* XXX support for the ae command is not included, though it probably should */
    {(char *)0, (char *)0, (char *)0,
     0,		0, 0	/* END */}
};


int
ged_loadview(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    FILE *fp;
    char buffer[512] = {0};

    /* data pulled from script file */
    int perspective=-1;
#define MAX_DBNAME 2048
    char dbName[MAX_DBNAME] = {0};
    char objects[10000] = {0};
    char *editArgv[3];
    int prevPerspective;
    static const char *usage = "filename";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* make sure the file exists */
    if (!bu_file_exists(argv[1])) {
	bu_log("Error: File %s does not exist\n", argv[1]);
	return GED_ERROR;
    }

    /* open the file for reading */
    if ((fp = fopen(argv[1], "r")) == NULL) {
	perror(argv[1]);
	return GED_ERROR;
    }

    prevPerspective =  gedp->ged_gvp->gv_perspective;
    _ged_current_gedp = gedp;

    /* turn perspective mode off, by default.  A "-p" option in the
     * view script will turn it back on.
     */
    gedp->ged_gvp->gv_perspective = 0;

    /* iterate over the contents of the raytrace script */
    /* TODO: change to bu_fgets or bu_vls_fgets */
    while (!feof(fp)) {
	memset(buffer, 0, 512);
	ret = fscanf(fp, "%512s", buffer);
	if (ret != 1)
	    bu_log("Failed to read buffer\n");

	if (strncmp(buffer, "-p", 2)==0) {
	    /* we found perspective */

	    buffer[0]=' ';
	    buffer[1]=' ';
	    sscanf(buffer, "%d", &perspective);
	    /* bu_log("perspective=%d\n", perspective);*/
	    gedp->ged_gvp->gv_perspective = perspective;

	} else if (strncmp(buffer, "$*", 2)==0) {
	    /* the next read is the file name, the objects come
	     * after that
	     */

	    memset(dbName, 0, MAX_DBNAME);
	    ret = fscanf(fp, "%2048s", dbName); /* MAX_DBNAME */
	    if (ret != 1)
		bu_log("Failed to read database name\n");

	    /* if the last character is a line termination,
	     * remove it (it should always be unless the user
	     * modifies the file)
	     */
	    if (*(dbName + strlen(dbName) - 1)=='\\') {
		memset(dbName+strlen(dbName)-1, 0, 1);
	    }
	    /* bu_log("dbName=%s\n", dbName); */

	    if (!bu_same_file(gedp->ged_wdbp->dbip->dbi_filename, dbName)) {
		/* stop here if they are not the same file,
		 * otherwise, we may proceed as expected, and load
		 * the objects.
		 */
		bu_vls_printf(gedp->ged_result_str, "View script references a different database\nCannot load the view without closing the current database\n(i.e. run \"opendb %s\")\n", dbName);

		/* restore state before leaving */
		gedp->ged_gvp->gv_perspective = prevPerspective;

		return GED_ERROR;
	    }

	    /* get rid of anything that may be displayed, since we
	     * will load objects that are listed in the script next.
	     */
	    (void)ged_zap(gedp, 1, NULL);

	    /* now get the objects listed */
	    ret = fscanf(fp, "%10000s", objects);
	    if (ret != 1)
		bu_log("Failed to read object names\n");

	    /* bu_log("OBJECTS=%s\n", objects);*/
	    while ((!feof(fp)) && (strncmp(objects, "\\", 1)!=0)) {

		/* clean off the single quotes... */
		if (strncmp(objects, "'", 1)==0) {
		    objects[0]=' ';
		    memset(objects+strlen(objects)-1, ' ', 1);
		    sscanf(objects, "%10000s", objects);
		}

		editArgv[0] = "draw";
		editArgv[1] = objects;
		editArgv[2] = (char *)NULL;
		if (ged_draw(gedp, 2, (const char **)editArgv) != GED_OK) {
		    bu_vls_printf(gedp->ged_result_str, "Unable to load object: %s\n", objects);
		}

		/* bu_log("objects=%s\n", objects);*/
		ret = fscanf(fp, "%10000s", objects);
		if (ret != 1)
		    bu_log("Failed to read object names\n");
	    }

	    /* end iteration over reading in listed objects */
	} else if (strncmp(buffer, "<<EOF", 5)==0) {
	    char *cmdBuffer = NULL;
	    /* we are almost done .. read in the view commands */

	    while ((cmdBuffer = rt_read_cmd(fp)) != NULL) {
		/* even unsupported commands should return successfully as
		 * they should be calling ged_cm_null()
		 */
		if (rt_do_cmd((struct rt_i *)0, cmdBuffer, ged_loadview_cmdtab) < 0) {
		    bu_vls_printf(gedp->ged_result_str, "command failed: %s\n", cmdBuffer);
		}
		bu_free((genptr_t)cmdBuffer, "loadview cmdBuffer");
	    }
	    /* end iteration over rt commands */

	}
	/* end check for non-view values (dbname, etc) */

    }
    /* end iteration over file until eof */
    fclose(fp);

    /* now we have to finish the eye point calculations that usually get
     * postponed until the end command runs.  Since we are at the "end"
     * of a commands section, we may finish the computations.
     */
    /* First step:  put eye at view center (view 0, 0, 0) */
    MAT_COPY(gedp->ged_gvp->gv_rotation, _ged_viewrot);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, _ged_eye_model);
    ged_view_update(gedp->ged_gvp);

    return GED_OK;
}


int
_ged_cm_vsize(int argc, char **argv)
{
    if (argc < 2)
	return -1;
    /* for some reason, scale is supposed to be half of size... */
    _ged_current_gedp->ged_gvp->gv_size = atof(argv[1]);
    _ged_current_gedp->ged_gvp->gv_scale = _ged_current_gedp->ged_gvp->gv_size * 0.5;
    _ged_current_gedp->ged_gvp->gv_isize = 1.0 / _ged_current_gedp->ged_gvp->gv_size;
    return 0;
}


int
_ged_cm_eyept(int argc, char **argv)
{
    if (argc < 4)
	return -1;
    _ged_eye_model[X] = atof(argv[1]);
    _ged_eye_model[Y] = atof(argv[2]);
    _ged_eye_model[Z] = atof(argv[3]);
    /* Processing is deferred until ged_cm_end() */
    return 0;
}


int
_ged_cm_lookat_pt(int argc, char **argv)
{
    point_t pt;
    vect_t dir;

    if (argc < 4)
	return -1;
    pt[X] = atof(argv[1]);
    pt[Y] = atof(argv[2]);
    pt[Z] = atof(argv[3]);

    VSUB2(dir, pt, _ged_eye_model);
    VUNITIZE(dir);

#if 1
    /*
      At the moment bn_mat_lookat will return NAN's if the direction vector
      is aligned with the Z axis. The following is a temporary workaround.
    */
    {
	vect_t neg_Z_axis;

	VSET(neg_Z_axis, 0.0, 0.0, -1.0);
	bn_mat_fromto(_ged_viewrot, dir, neg_Z_axis);
    }
#else
    bn_mat_lookat(_ged_viewrot, dir, yflip);
#endif

    /* Final processing is deferred until ged_cm_end(), but eye_pt
     * must have been specified before here (for now)
     */
    return 0;
}


int
_ged_cm_vrot(int argc, char **argv)
{
    int i;

    if (argc < 17)
	return -1;
    for (i=0; i<16; i++)
	_ged_viewrot[i] = atof(argv[i+1]);
    /* Processing is deferred until ged_cm_end() */
    return 0;
}


int
_ged_cm_orientation(int argc, char **argv)
{
    int i;
    quat_t quat;

    if (argc < 4)
	return -1;

    for (i=0; i<4; i++)
	quat[i] = atof(argv[i+1]);
    quat_quat2mat(_ged_viewrot, quat);

    return 0;
}


int
_ged_cm_set(int UNUSED(argc), char **UNUSED(argv))
{
    return -1;
}


/**
 * any commands that are not supported or implemented may call this null
 * routine to avoid rt_do_cmd() "command not found" error reporting
 */
int
_ged_cm_null(int argc, char **argv)
{
    if (argc < 0 || argv == NULL)
	return 1;

    return 0;
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
