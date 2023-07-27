/*                          N I R T . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2023 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/nirt.c
 *
 * Routines to interface to nirt.
 *
 * This code was imported from the RTUIF and modified to work as part
 * of the drawable geometry object.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bresource.h"

#include "bn.h"
#include "bu/app.h"
#include "bu/cmd.h"
#include "bu/file.h"
#include "bu/snooze.h"
#include "bu/time.h"
#include "bu/process.h"
#include "vmath.h"

#include "../qray.h"
#include "../ged_private.h"

/**
 * Invoke nirt with the current view & stuff
 *
 * NOTE: invoked through mged, nirt is currently set up to fire THREE
 *       rays in order to get partition and overlap information.
 */
int
ged_nirt_core(struct ged *gedp, int argc, const char *argv[])
{
    char **vp = NULL;
    FILE *fp_in = NULL;
    FILE *fp_out = NULL;
    FILE *fp_err = NULL;
    struct bu_process *p = NULL;
    char **av = NULL;
    int ret;
    int retcode;
    int use_input_orig = 0;
    vect_t center_model;
    vect_t dir;
    vect_t cml;
    double scan[4]; /* holds sscanf values */
    int i;
    int j;
    char line[RT_MAXLINE] = {0};
    struct bu_vls p_vls = BU_VLS_INIT_ZERO;
    struct bv_vlblock *vbp = NULL;
    struct qray_dataList *ndlp = NULL;
    struct qray_dataList HeadQRayData;
    char **gd_rt_cmd = NULL;
    int gd_rt_cmd_len = 0;
    int skip_drawn = 0;
    int64_t start = 0;

    const char *nirt = NULL;
    char nirtcmd[MAXPATHLEN] = {0};
    size_t args;

    /* for bu_fgets space trimming */
    struct bu_vls v = BU_VLS_INIT_ZERO;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    nirt = bu_dir(nirtcmd, MAXPATHLEN, BU_DIR_BIN, argv[0], NULL);
    if (!nirt) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: Unable to find 'nirt' plugin.\n");
	return BRLCAD_ERROR;
    }

    /* look for options which solely print information to user */
    for (i = 0; i < argc; i++) {
        if (BU_STR_EQUAL(argv[i], "-h") || 
            BU_STR_EQUAL(argv[i], "-?") ||
            BU_STR_EQUAL(argv[i], "--help") || 
            BU_STR_EQUAL(argv[i], "-L")) {

            // load av with request
            av = (char **)bu_calloc(4, sizeof(char *), "av");
            av[0] = (char*)nirt;
            av[1] = (char*)argv[i];
            av[2] = "-X";
            av[3] = "ged";

            bu_process_exec(&p, nirt, 4, (const char **)av, 0, 1);
            bu_free(av, "av");

            // open pipes
            fp_in = bu_process_open(p, BU_PROCESS_STDIN);

            /* use fp_out to read back the result */
            fp_out = bu_process_open(p, BU_PROCESS_STDOUT);

            /* use fp_err to read any error messages */
            fp_err = bu_process_open(p, BU_PROCESS_STDERR);

            /* send quit command to nirt */
            fprintf(fp_in, "q\n");
            bu_process_close(p, BU_PROCESS_STDIN);

            goto print;
        }
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    args = argc + 20 + 2 + ged_who_argc(gedp);
    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gd_rt_cmd[0];
    *vp++ = &nirtcmd[0];

    /* swipe x, y, z off the end if present */
    if (argc > 3) {
	if (sscanf(argv[argc-3], "%lf", &scan[X]) == 1 &&
	    sscanf(argv[argc-2], "%lf", &scan[Y]) == 1 &&
	    sscanf(argv[argc-1], "%lf", &scan[Z]) == 1) {
	    use_input_orig = 1;
	    argc -= 3;
	    VSCALE(center_model, scan, gedp->dbip->dbi_local2base);
	}
    }

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    /* Calculate point from which to fire ray */
    if (!use_input_orig) {
	VSET(center_model, -gedp->ged_gvp->gv_center[MDX],
	     -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);
    }

    VSCALE(cml, center_model, gedp->dbip->dbi_base2local);
    VMOVEN(dir, gedp->ged_gvp->gv_rotation + 8, 3);
    VSCALE(dir, dir, -1.0);

    bu_vls_printf(&p_vls, "xyz %lf %lf %lf;",
		  cml[X], cml[Y], cml[Z]);
    bu_vls_printf(&p_vls, "dir %lf %lf %lf; s",
		  dir[X], dir[Y], dir[Z]);

    /* include nirt script string */
    if (bu_vls_strlen(&gedp->ged_gdp->gd_qray_script)) {
	*vp++ = "-e";
	*vp++ = bu_vls_addr(&gedp->ged_gdp->gd_qray_script);
    }

    if (DG_QRAY_TEXT(gedp->ged_gdp)) {
	/* setup default print formatting
	 * NOTE: user specified -f will come later in succession and trump this
	 */
	*vp++ = "-f";
	*vp++ = "default-ged";
    }
    /* first ray: start, direction, and 's' command -> used for printing
     * printing is formatted with either nirt default or user specified '-f fmt'
     */
    *vp++ = "-e";
    *vp++ = bu_vls_addr(&p_vls);

    if (DG_QRAY_GRAPHICS(gedp->ged_gdp)) {
	/* wipe formatting */
	*vp++ = "-e";
	*vp++ = DG_QRAY_FORMAT_NULL;

	/* set up partition reporting */
	*vp++ = "-e";
	*vp++ = DG_QRAY_FORMAT_P;

	*vp++ = "-e";
	*vp++ = "fmt r \"MGED-PARTITION-REPORT\\n\"";

	/* second ray: start, direction, and 's' command -> used for partion report */
	*vp++ = "-e";
	*vp++ = bu_vls_addr(&p_vls);

	/* set up overlap formatting */
	*vp++ = "-e";
	*vp++ = DG_QRAY_FORMAT_O;

	*vp++ = "-e";
	*vp++ = "fmt r \"MGED-OVERLAP-REPORT\\n\"";

	/* third ray: start, direction, and 's' command -> used for overlap report */
	*vp++ = "-e";
	*vp++ = bu_vls_addr(&p_vls);
    }

    *vp++ = gedp->dbip->dbi_filename;
    /* load user args */
    for (i = 1; i < argc; i++)
	*vp++ = (char *)argv[i];

    /* check if user reqested objects other than what is drawn */
    if (argc > 1 && argv[argc-1][0] != '-') {	/* ignore obvious trailing flag */
	if (db_lookup(gedp->dbip, argv[argc-1], 0) != RT_DIR_NULL)
	    skip_drawn = 1;
    }

    gd_rt_cmd_len = vp - gd_rt_cmd;
    if (!skip_drawn) {
        /* Note - ged_who_argv sets the last vp to (char *)0 */
        gd_rt_cmd_len += ged_who_argv(gedp, vp, (const char **)&gd_rt_cmd[args]);
    }

    if (gedp->ged_gdp->gd_qray_cmd_echo) {
	/* Print out the command we are about to run */
	vp = &gd_rt_cmd[0];

	while (*vp)
	    bu_vls_printf(gedp->ged_result_str, "%s ", *vp++);

	bu_vls_printf(gedp->ged_result_str, "\n");
    }

    if (use_input_orig) {
	bu_vls_printf(gedp->ged_result_str, "\nFiring from (%lf, %lf, %lf)...\n",
		      center_model[X], center_model[Y], center_model[Z]);
    } else {
	bu_vls_printf(gedp->ged_result_str, "\nFiring from view center...\n");
    }

    j = 1;
    /* increment gd_rt_cmd_len +1 for null termination
    * +2 to indicate we are running within libged with "-X ged"
    */
    av = (char **)bu_calloc(gd_rt_cmd_len + 3, sizeof(char *), "av");
    av[0] = gd_rt_cmd[0];
    av[j++] = "-X";
    av[j++] = "ged";
    for (i = 1; i < gd_rt_cmd_len; i++) {
	/* skip commands */
	if (BU_STR_EQUAL(gd_rt_cmd[i], "-e")) {
	    i++;	// skip script too
	} else {
	    av[j] = gd_rt_cmd[i];
	    j++;
	}
    }

    bu_process_exec(&p, gd_rt_cmd[0], j, (const char **)av, 0, 1);
    bu_free(av, "av");


    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	for (int pi = 0; pi < gd_rt_cmd_len; pi++) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", gd_rt_cmd[pi]);
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
	bu_free(gd_rt_cmd, "free gd_rt_cmd");
	return BRLCAD_ERROR;
    }

    fp_in = bu_process_open(p, BU_PROCESS_STDIN);

    /* send commands down the pipe */
    for (i = 1; i < gd_rt_cmd_len - 2; i++) {
	if (gd_rt_cmd[i] != NULL && BU_STR_EQUAL(gd_rt_cmd[i], "-e")) {
	    fprintf(fp_in, "%s\n", gd_rt_cmd[++i]);
	}
    }

    /* use fp_out to read back the result */
    fp_out = bu_process_open(p, BU_PROCESS_STDOUT);

    /* use fp_err to read any error messages */
    fp_err = bu_process_open(p, BU_PROCESS_STDERR);

    /* send quit command to nirt */
    fprintf(fp_in, "q\n");
    bu_process_close(p, BU_PROCESS_STDIN);

    bu_vls_free(&p_vls);   /* use to form "partition" part of nirt command above */

print:
    /* ensure nirt has started and has something to read from - with 5 second timeout */
    start = bu_gettime();
    while (!bu_process_pending(fileno(fp_err)) && !bu_process_pending(fileno(fp_out))) {
	if ((bu_gettime() - start) > BU_SEC2USEC(5))
	    break;
    }

    /* check if nirt wrote anything to error on load */
    if (bu_process_pending(fileno(fp_err))) {
	while (bu_fgets(line, RT_MAXLINE, fp_err) != (char *)NULL) {
	    bu_vls_strcpy(&v, line);
	    bu_vls_trimspace(&v);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&v));
	}
    }

    if (DG_QRAY_TEXT(gedp->ged_gdp)) {
	while (bu_fgets(line, RT_MAXLINE, fp_out) != (char *)NULL) {
	    if (bu_strncmp(line, "MGED-PARTITION-REPORT", 21) == 0)
		break;
	    bu_vls_strcpy(&v, line);
	    bu_vls_trimspace(&v);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&v));
	}
    }

    if (!skip_drawn && DG_QRAY_GRAPHICS(gedp->ged_gdp)) {
	BU_LIST_INIT(&HeadQRayData.l);

	/* handle partitions */
	while (bu_fgets(line, RT_MAXLINE, fp_out) != (char *)NULL) {
	    if (bu_strncmp(line, "MGED-OVERLAP-REPORT", 19) == 0)
		break;
	    bu_vls_strcpy(&v, line);
	    bu_vls_trimspace(&v);

	    if (line[0] == '\n' || line[0] == '\r') {
		bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&v));
		break;
	    }

	    BU_ALLOC(ndlp, struct qray_dataList);
	    BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	    ret = sscanf(bu_vls_addr(&v), "%le %le %le %le", &scan[0], &scan[1], &scan[2], &scan[3]);
	    ndlp->x_in = scan[0];
	    ndlp->y_in = scan[1];
	    ndlp->z_in = scan[2];
	    ndlp->los = scan[3];
	    if (ret != 4) {
		bu_log("WARNING: Unexpected nirt line [%s]\nExpecting four partition numbers.\n", bu_vls_addr(&v));
		break;
	    }
	}

	vbp = bv_vlblock_init(&RTG.rtg_vlfree, 32);
	qray_data_to_vlist(gedp, vbp, &HeadQRayData, dir, 0);
	bu_list_free(&HeadQRayData.l);

	const char *nview = getenv("GED_TEST_NEW_CMD_FORMS");
	if (BU_STR_EQUAL(nview, "1")) {
	    struct bview *view = gedp->ged_gvp;
	    bv_vlblock_obj(vbp, view, "nirt");
	} else {
	    _ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_addr(&gedp->ged_gdp->gd_qray_basename), 0);
	}

	bv_vlblock_free(vbp);

	/* handle overlaps */
	while (bu_fgets(line, RT_MAXLINE, fp_out) != (char *)NULL) {
	    bu_vls_strcpy(&v, line);
	    bu_vls_trimspace(&v);

	    if (line[0] == '\n' || line[0] == '\r') {
		bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&v));
		break;
	    }

	    BU_ALLOC(ndlp, struct qray_dataList);
	    BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	    ret = sscanf(bu_vls_addr(&v), "%le %le %le %le", &scan[0], &scan[1], &scan[2], &scan[3]);
	    ndlp->x_in = scan[0];
	    ndlp->y_in = scan[1];
	    ndlp->z_in = scan[2];
	    ndlp->los = scan[3];
	    if (ret != 4) {
		bu_log("WARNING: Unexpected nirt line [%s]\nExpecting four overlap numbers.\n", bu_vls_addr(&v));
		break;
	    }
	}

	vbp = bv_vlblock_init(&RTG.rtg_vlfree, 32);
	qray_data_to_vlist(gedp, vbp, &HeadQRayData, dir, 1);
	bu_list_free(&HeadQRayData.l);
	_ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_addr(&gedp->ged_gdp->gd_qray_basename), 0);
	bv_vlblock_free(vbp);
    }

    /* check if nirt wrote any errors from shots */
    if (bu_process_pending(fileno(fp_err))) {
	while (bu_fgets(line, RT_MAXLINE, fp_err) != (char *)NULL) {
	    bu_vls_strcpy(&v, line);
	    bu_vls_trimspace(&v);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&v));
	}
    }

    bu_vls_free(&v);

    bu_process_close(p, BU_PROCESS_STDOUT);
    bu_process_close(p, BU_PROCESS_STDERR);

    retcode = bu_process_wait(NULL, p, 0);

    if (retcode != 0) {
	_ged_wait_status(gedp->ged_result_str, retcode);
    }

    dl_set_wflag(gedp->ged_gdp->gd_headDisplay, DOWN);

    bu_free(gd_rt_cmd, "free gd_rt_cmd");
    gd_rt_cmd = NULL;

    return BRLCAD_OK;
}


int
ged_vnirt_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int status;
    fastf_t sf = 1.0 * DG_INV_GED;
    vect_t view_ray_orig;
    vect_t center_model;
    double scan[3];
    struct bu_vls x_vls = BU_VLS_INIT_ZERO;
    struct bu_vls y_vls = BU_VLS_INIT_ZERO;
    struct bu_vls z_vls = BU_VLS_INIT_ZERO;
    char **av;
    static const char *usage = "vnirt options vX vY";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /*
     * The last two arguments are expected to be x, y in view
     * coordinates.  It is also assumed that view z will be the front
     * of the viewing cube.  These coordinates are converted to x, y,
     * z in model coordinates and then converted to local units before
     * being handed to nirt. All other arguments are passed straight
     * through to nirt.
     */
    if (sscanf(argv[argc-2], "%lf", &scan[X]) != 1 ||
	sscanf(argv[argc-1], "%lf", &scan[Y]) != 1) {
	return BRLCAD_ERROR;
    }
    scan[Z] = DG_GED_MAX;
    argc -= 2;

    av = (char **)bu_calloc(1, sizeof(char *) * (argc + 4), "gd_vnirt_cmd: av");

    /* Calculate point from which to fire ray */
    VSCALE(view_ray_orig, scan, sf);
    MAT4X3PNT(center_model, gedp->ged_gvp->gv_view2model, view_ray_orig);
    VSCALE(center_model, center_model, gedp->dbip->dbi_base2local);

    bu_vls_printf(&x_vls, "%lf", center_model[X]);
    bu_vls_printf(&y_vls, "%lf", center_model[Y]);
    bu_vls_printf(&z_vls, "%lf", center_model[Z]);

    /* pass remaining arguments to ged nirt command */
    av[0] = "nirt";
    for (i = 1; i < argc; ++i)
	av[i] = (char *)argv[i];

    /* pass modified coordinates to nirt */
    av[i++] = bu_vls_addr(&x_vls);
    av[i++] = bu_vls_addr(&y_vls);
    av[i++] = bu_vls_addr(&z_vls);
    av[i] = (char *)NULL;

    status = ged_nirt_core(gedp, argc + 3, (const char **)av);

    bu_vls_free(&x_vls);
    bu_vls_free(&y_vls);
    bu_vls_free(&z_vls);
    bu_free((void *)av, "ged_vnirt_core: av");
    av = NULL;

    return status;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl nirt_cmd_impl = {"nirt", ged_nirt_core, GED_CMD_DEFAULT};
const struct ged_cmd nirt_cmd = { &nirt_cmd_impl };

struct ged_cmd_impl query_ray_cmd_impl = {"query_ray", ged_nirt_core, GED_CMD_DEFAULT};
const struct ged_cmd query_ray_cmd = { &query_ray_cmd_impl };

struct ged_cmd_impl vnirt_cmd_impl = {"vnirt", ged_vnirt_core, GED_CMD_DEFAULT};
const struct ged_cmd vnirt_cmd = { &vnirt_cmd_impl };

struct ged_cmd_impl vquery_ray_cmd_impl = {"vquery_ray", ged_vnirt_core, GED_CMD_DEFAULT};
const struct ged_cmd vquery_ray_cmd = { &vquery_ray_cmd_impl };

const struct ged_cmd *nirt_cmds[] = { &nirt_cmd, &vnirt_cmd, &query_ray_cmd, &vquery_ray_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  nirt_cmds, 4 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
