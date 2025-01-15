/*                    L O A D V I E W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/loadview.cpp
 *
 * The loadview command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"
extern "C" {
#include "./ged_draw.h"
}

/**
 * here we define a minimal table of commands that are supported by the
 * loadview command.  unsupported commands are those that have no bearing on
 * view restoration.
 */

int
_ged_cm_vsize(struct ged *gedp, vect_t *UNUSED(v), mat_t *UNUSED(m), const int argc, const char **argv)
{
    if (argc < 2)
	return -1;
    /* for some reason, scale is supposed to be half of size... */
    gedp->ged_gvp->gv_size = atof(argv[1]);
    gedp->ged_gvp->gv_scale = gedp->ged_gvp->gv_size * 0.5;
    gedp->ged_gvp->gv_isize = 1.0 / gedp->ged_gvp->gv_size;
    return 0;
}

int
_ged_cm_eyept(struct ged *UNUSED(gedp), vect_t *v, mat_t *UNUSED(m), const int argc, const char **argv)
{
    if (argc < 4)
	return -1;
    (*v)[X] = atof(argv[1]);
    (*v)[Y] = atof(argv[2]);
    (*v)[Z] = atof(argv[3]);

    /* Processing is deferred until view 'end' */
    return 0;
}


int
_ged_cm_lookat_pt(struct ged *gedp, vect_t *v, mat_t *m, const int argc, const char **argv)
{
    point_t pt;
    vect_t dir;

    if (argc < 4)
	return -1;
    pt[X] = atof(argv[1]);
    pt[Y] = atof(argv[2]);
    pt[Z] = atof(argv[3]);

    VSUB2(dir, pt, (*v));
    VUNITIZE(dir);

    /*
     * At the moment bn_mat_lookat() will return NAN's if the
     * direction vector is aligned with the Z axis. The following is a
     * workaround.
     */
    {
	vect_t neg_Z_axis = VINIT_ZERO;
	neg_Z_axis[Z] = -1.0;

	struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	bn_mat_fromto((*m), dir, neg_Z_axis, &wdbp->wdb_tol);
    }

    /* Final processing is deferred until view 'end', but eye_pt
     * must have been specified before here (for now)
     */
    return 0;
}


int
_ged_cm_vrot(struct ged *UNUSED(gedp), vect_t *UNUSED(v), mat_t *m, const int argc, const char **argv)
{
    int i;

    if (argc < 17)
	return -1;
    for (i = 0; i < 16; i++)
	(*m)[i] = atof(argv[i+1]);
    /* Processing is deferred until view 'end' */
    return 0;
}


int
_ged_cm_orientation(struct ged *UNUSED(gedp), vect_t *UNUSED(v), mat_t *m, const int argc, const char **argv)
{
    int i;
    quat_t quat;

    if (argc < 4)
	return -1;

    for (i = 0; i < 4; i++)
	quat[i] = atof(argv[i+1]);
    quat_quat2mat((*m), quat);

    return 0;
}


int
_ged_cm_set(struct ged *UNUSED(gedp), vect_t *UNUSED(v), mat_t *UNUSED(m), const int UNUSED(argc), const char **UNUSED(argv))
{
    return -1;
}


/**
 * any commands that are not supported or implemented may call this null
 * routine to avoid rt_do_cmd() "command not found" error reporting
 */
    int
_ged_cm_null(struct ged *UNUSED(gedp), vect_t *UNUSED(v), mat_t *UNUSED(m), const int argc, const char **argv)
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
_ged_cm_end(struct ged *gedp, vect_t *v, mat_t *m, const int argc, const char **argv)
{

    if (argc < 0 || argv == NULL)
	return 1;

    /* now we have to finish view calculations that are deferred until
     * the end command runs.
     */
    MAT_COPY(gedp->ged_gvp->gv_rotation, (*m));
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, (*v));
    bv_update(gedp->ged_gvp);

    struct bu_vls eye = BU_VLS_INIT_ZERO;
    bu_vls_printf(&eye, "%lf %lf %lf", V3ARGS((*v)));
    char *eye_argv[5] = {NULL};
    bu_argv_from_string(eye_argv+1, 3, bu_vls_addr(&eye));
    eye_argv[0] = bu_strdup("eye");
    ged_exec_eye(gedp, 4, (const char **)eye_argv);
    bu_free(eye_argv[0], "argv0");
    bu_vls_free(&eye);

    return 0;
}

struct ged_command_tab ged_loadview_cmdtab[] = {
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
ged_do_cmd(struct ged *gedp, vect_t *v, mat_t *m, const char *ilp, const struct ged_command_tab *tp)
{
    int nwords;        /* number of words seen */
    char *cmd_args[MAXPATHLEN+1]; /* array of ptrs to args */
    char *lp;
    int retval;

    if (ilp[0] == '{') {
	int tcl_argc;
	const char **tcl_argv;
	if(bu_argv_from_tcl_list(ilp, &tcl_argc, &tcl_argv) || tcl_argc != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_do_cmd:  invalid input %s\n", ilp);
	    return BRLCAD_ERROR; /* Looked like a tcl list, but apparently not */
	} else {
	    lp = bu_strdup(tcl_argv[0]);
	}
    } else {
	lp = bu_strdup(ilp);
    }

    nwords = bu_argv_from_string(cmd_args, MAXPATHLEN, lp);
    if (nwords <= 0)
	return BRLCAD_OK;       /* No command to process */


    for (; tp->ct_cmd != (char *)0; tp++) {
	if (cmd_args[0][0] != tp->ct_cmd[0] ||
		/* length of "n" is not significant, just needs to be big
		 * enough
		 */
		bu_strncmp(cmd_args[0], tp->ct_cmd, MAXPATHLEN) != 0)
	    continue;
	if ((nwords >= tp->ct_min)
		&& ((tp->ct_max < 0) || (nwords <= tp->ct_max)))
	{
	    retval = tp->ct_func(gedp, v, m, nwords, (const char **)cmd_args);
	    bu_free(lp, "ged_do_cmd lp");
	    return retval;
	}
	bu_vls_printf(gedp->ged_result_str, "ged_do_cmd Usage: %s %s\n\t%s\n",
		tp->ct_cmd, tp->ct_parms, tp->ct_comment);
	bu_free(lp, "ged_do_cmd lp");
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gedp->ged_result_str, "ged_do_cmd(%s):  command not found\n", cmd_args[0]);
    bu_free(lp, "ged_do_cmd lp");
    return BRLCAD_ERROR;
}

int
ged_loadview_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int failed = 0;
    FILE *fp;
    vect_t *ged_eye_model = &gedp->i->i->ged_eye_model;
    mat_t *ged_viewrot = &gedp->i->i->ged_viewrot;

#define BUFFER_SIZE 511
    char buffer[BUFFER_SIZE+1] = {0};

    /* data pulled from script file */
    int perspective=-1;

#define DBNAME_SIZE 2047
    char name[DBNAME_SIZE+1] = {0};
    char *dbName = name;

#define OBJECTS_SIZE 9999
    char objects[OBJECTS_SIZE+1] = {0};
    const char *editArgv[3];

    static const char *usage = "filename";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
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

    /* turn perspective mode off, by default.  A "-p" option in the
     * view script will turn it back on.
     */
    gedp->ged_gvp->gv_perspective = 0;

    /* iterate over the contents of the raytrace script */
    /* TODO: change to bu_fgets or bu_vls_fgets */
    while (!feof(fp)) {
	memset(buffer, 0, sizeof(buffer));
	ret = fscanf(fp, "%" CPP_XSTR(BUFFER_SIZE) "s", buffer);
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

	    ged_exec_perspective(gedp, 2, (const char **)perspective_argv);

	} else if (bu_strncmp(buffer, "$*", 2) == 0) {
	    /* the next read is the file name, the objects come
	     * after that
	     */
	    ret = fscanf(fp, "%" CPP_XSTR(DBNAME_SIZE) "s", dbName);
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
	    const char *Z_cmd[1] = {"Z"};
	    (void)ged_exec_Z(gedp, 1, Z_cmd);

	    /* now get the objects listed */
	    ret = fscanf(fp, "%" CPP_XSTR(OBJECTS_SIZE) "s", objects);
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
		    sscanf(objects, "%" CPP_XSTR(OBJECTS_SIZE) "s", objects);
		}

		editArgv[0] = "draw";
		editArgv[1] = objects;
		editArgv[2] = (char *)NULL;
		if (ged_exec_draw(gedp, 2, (const char **)editArgv) != BRLCAD_OK) {
		    bu_vls_printf(gedp->ged_result_str, "Unable to load object: %s\n", objects);
		}

		/* bu_log("objects=%s\n", objects);*/
		ret = fscanf(fp, "%" CPP_XSTR(OBJECTS_SIZE) "s", objects);
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
		if (ged_do_cmd(gedp, ged_eye_model, ged_viewrot, cmdBuffer, ged_loadview_cmdtab) < 0) {
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


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
