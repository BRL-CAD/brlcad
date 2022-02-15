/*                         L O A D V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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

#include "../ged_private.h"

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
    {"orientation", "quaternion", "set view direction from quaternion",
     _ged_cm_orientation,	5, 5},
    {"set", 	"", "show or set parameters",
     _ged_cm_set,		1, 999},

    /* begin unsupported commands (for view loading) */

    {"start", "frame number", "start a new frame",
     _ged_cm_null,	2, 2},
    {"clean", "", "clean articulation from previous frame",
     _ged_cm_null,	1, 1},
    {"end", 	"", "end of frame setup, begin raytrace",
     _ged_cm_end,		1, 1},

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
ged_loadview_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int failed = 0;
    FILE *fp;
    char buffer[512] = {0};

    /* data pulled from script file */
    int perspective=-1;
#define MAX_DBNAME 2048
    char name[MAX_DBNAME] = {0};
    char *dbName = name;
    char objects[10000] = {0};
    char *editArgv[3];
    static const char *usage = "filename";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    /* make sure the file exists */
    if (!bu_file_exists(argv[1], NULL)) {
	bu_log("Error: File %s does not exist\n", argv[1]);
	return BRLCAD_ERROR;
    }

    /* open the file for reading */
    fp = fopen(argv[1], "r");
    if (fp == NULL) {
	perror(argv[1]);
	return BRLCAD_ERROR;
    }

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
	if (ret != 1) {
	    bu_log("Failed to read buffer\n");
	    failed++;
	}

	if (bu_strncmp(buffer, "-p", 2) == 0) {
	    char perspective_angle[128] = {0};
	    const char *perspective_argv[2] = {"perspective", NULL};
	    perspective_argv[1] = perspective_angle;

	    /* we found perspective */

	    buffer[0] = ' ';
	    buffer[1] = ' ';
	    sscanf(buffer, "%d", &perspective);
	    /* bu_log("perspective=%d\n", perspective);*/
	    snprintf(perspective_angle, sizeof(perspective_angle), "%d", perspective);

	    ged_exec(gedp, 2, (const char **)perspective_argv);

	} else if (bu_strncmp(buffer, "$*", 2) == 0) {
	    /* the next read is the file name, the objects come
	     * after that
	     */
	    ret = fscanf(fp, "%2048s", dbName); /* MAX_DBNAME */
	    if (ret != 1) {
		bu_log("Failed to read database name\n");
		failed++;
	    }

	    /* if the last character is a line termination,
	     * remove it (it should always be unless the user
	     * modifies the file)
	     */
	    if (*(dbName + strlen(dbName) - 1)=='\\') {
		memset(dbName+strlen(dbName)-1, 0, 1);
	    }
	    /* bu_log("dbName=%s\n", dbName); */

	    /* if the name was wrapped in quotes, remove them */
	    if (dbName[0] == '\'' && *(dbName + strlen(dbName) - 1) == '\'') {
		dbName++;
		memset(dbName + strlen(dbName)-1, 0, 1);
	    }

	    if (!bu_file_same(gedp->dbip->dbi_filename, dbName)) {
		/* warn here if they are not the same file, otherwise,
		 * proceed as expected, and try to load the objects.
		 */
		bu_log("WARNING: view script seems to reference a different database\n([%s] != [%s])\n", dbName, gedp->dbip->dbi_filename);
	    }

	    /* get rid of anything that may be displayed, since we
	     * will load objects that are listed in the script next.
	     *
	     * TODO: should only zap if the objects to be displayed
	     * all exist.
	     */
	    const char *Z_cmd = "Z";
	    (void)ged_exec(gedp, 1, &Z_cmd);

	    /* now get the objects listed */
	    ret = fscanf(fp, "%10000s", objects);
	    if (ret != 1) {
		bu_log("Failed to read object names\n");
		failed++;
	    }

	    /* bu_log("OBJECTS=%s\n", objects);*/
	    while ((!feof(fp)) && (bu_strncmp(objects, "\\", 1) != 0)) {

		/* clean off the single quotes... */
		if (bu_strncmp(objects, "'", 1) == 0) {
		    objects[0]=' ';
		    memset(objects+strlen(objects)-1, ' ', 1);
		    sscanf(objects, "%10000s", objects);
		}

		editArgv[0] = "draw";
		editArgv[1] = objects;
		editArgv[2] = (char *)NULL;
		if (ged_exec(gedp, 2, (const char **)editArgv) != BRLCAD_OK) {
		    bu_vls_printf(gedp->ged_result_str, "Unable to load object: %s\n", objects);
		}

		/* bu_log("objects=%s\n", objects);*/
		ret = fscanf(fp, "%10000s", objects);
		if (ret != 1) {
		    bu_log("Failed to read object names\n");
		    failed++;
		}
	    }

	    /* end iteration over reading in listed objects */
	} else if (bu_strncmp(buffer, "<<EOF", 5) == 0) {
	    char *cmdBuffer = NULL;
	    /* we are almost done .. read in the view commands */

	    while ((cmdBuffer = rt_read_cmd(fp)) != NULL) {
		/* even unsupported commands should return successfully as
		 * they should be calling ged_cm_null()
		 */
		if (rt_do_cmd((struct rt_i *)0, cmdBuffer, ged_loadview_cmdtab) < 0) {
		    bu_vls_printf(gedp->ged_result_str, "command failed: %s\n", cmdBuffer);
		    failed++;
		}
		bu_free((void *)cmdBuffer, "loadview cmdBuffer");
	    }
	    /* end iteration over rt commands */

	}
	/* end check for non-view values (dbname, etc.) */

    }
    /* end iteration over file until eof */
    fclose(fp);

    if (failed)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


int
_ged_cm_vsize(const int argc, const char **argv)
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
_ged_cm_eyept(const int argc, const char **argv)
{
    if (argc < 4)
	return -1;
    _ged_eye_model[X] = atof(argv[1]);
    _ged_eye_model[Y] = atof(argv[2]);
    _ged_eye_model[Z] = atof(argv[3]);

    /* Processing is deferred until view 'end' */
    return 0;
}


int
_ged_cm_lookat_pt(const int argc, const char **argv)
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

    /*
     * At the moment bn_mat_lookat() will return NAN's if the
     * direction vector is aligned with the Z axis. The following is a
     * workaround.
     */
    {
	vect_t neg_Z_axis = VINIT_ZERO;
	neg_Z_axis[Z] = -1.0;
	bn_mat_fromto(_ged_viewrot, dir, neg_Z_axis, &_ged_current_gedp->ged_wdbp->wdb_tol);
    }

    /* Final processing is deferred until view 'end', but eye_pt
     * must have been specified before here (for now)
     */
    return 0;
}


int
_ged_cm_vrot(const int argc, const char **argv)
{
    int i;

    if (argc < 17)
	return -1;
    for (i = 0; i < 16; i++)
	_ged_viewrot[i] = atof(argv[i+1]);
    /* Processing is deferred until view 'end' */
    return 0;
}


int
_ged_cm_orientation(const int argc, const char **argv)
{
    int i;
    quat_t quat;

    if (argc < 4)
	return -1;

    for (i = 0; i < 4; i++)
	quat[i] = atof(argv[i+1]);
    quat_quat2mat(_ged_viewrot, quat);

    return 0;
}


int
_ged_cm_set(const int UNUSED(argc), const char **UNUSED(argv))
{
    return -1;
}


/**
 * any commands that are not supported or implemented may call this null
 * routine to avoid rt_do_cmd() "command not found" error reporting
 */
int
_ged_cm_null(const int argc, const char **argv)
{
    if (argc < 0 || argv == NULL)
	return 1;

    return 0;
}


/**
 * process the 'end' of a view.  currently, requires an eye point be
 * specified beforehand.
 */
int
_ged_cm_end(const int argc, const char **argv)
{
    struct bu_vls eye = BU_VLS_INIT_ZERO;
    char *eye_argv[6] = {"eye", NULL, NULL, NULL, NULL, NULL};

    if (argc < 0 || argv == NULL)
	return 1;

    /* now we have to finish view calculations that are deferred until
     * the end command runs.
     */
    MAT_COPY(_ged_current_gedp->ged_gvp->gv_rotation, _ged_viewrot);
    MAT_DELTAS_VEC_NEG(_ged_current_gedp->ged_gvp->gv_center, _ged_eye_model);
    bv_update(_ged_current_gedp->ged_gvp);

    bu_vls_printf(&eye, "%lf %lf %lf", V3ARGS(_ged_eye_model));
    bu_argv_from_string(eye_argv+1, 4, bu_vls_addr(&eye));
    ged_exec(_ged_current_gedp, 4, (const char **)eye_argv);
    bu_vls_free(&eye);

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
