/*                          N I R T . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2025 United States Government as represented by
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

static void
dl_set_wflag(struct bu_list *hdlp, int wflag)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    /* calculate the bounding for of all solids being displayed */
    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    sp->s_old.s_wflag = wflag;
	}

	gdlp = next_gdlp;
    }
}

struct nirt_info {
    struct nirt_opt_vals *nv;
    struct bu_process *p;
    FILE *fp_in;
    FILE *fp_out;
    FILE *fp_err;
    int skip_drawn;
    vect_t dir;
};

static int
nirt_cmd_partitions(struct ged *gedp, struct nirt_info *n)
{
    // If the options indicate we don't need this, skip it
    if (np->skip_drawn || !DG_QRAY_GRAPHICS(gedp->i->ged_gdp))
	return BRLCAD_OK;

    struct bu_vls script = BU_VLS_INIT_ZERO;
    bu_vls_printf(&script, "xyz %0.17f %0.17f %0.17f;", V3ARGS(nv.center_model));
    /* Calculate the ray direction from the view, and add an -e script to
     * set it for nirt. */
    VMOVEN(np.dir, gedp->ged_gvp->gv_rotation + 8, 3);
    VSCALE(np.dir, np.dir, -1.0);
    bu_vls_printf(&script, "dir %0.17f %0.17f %0.17f;", V3ARGS(np.dir));
    /* Take the initial shot */
    bu_vls_printf(&script, "s");

    /* do partition reporting shot */
    fprintf(n->fp_in, "%s\n", DG_QRAY_FORMAT_NULL);
    fprintf(n->fp_in, "%s\n", DG_QRAY_FORMAT_P);
    fprintf(n->fp_in, "fmt r \"MGED-PARTITION-REPORT\\n\"");
    fprintf(n->fp_in, "s");

    /* ensure nirt has started and has something to read from - with 5 second timeout */
    int64_t start = bu_gettime();

    while (!bu_process_pending(fileno(np->fp_err)) && !bu_process_pending(fileno(np->fp_out))) {
	if ((bu_gettime() - start) > BU_SEC2USEC(5))
	    break;
    }

    /* See if we got output */
    struct bu_vls vbuf = BU_VLS_INIT_ZERO;
    char line[RT_MAXLINE] = {0};

    struct bv_vlblock *vbp = NULL;
    BU_LIST_INIT(&HeadQRayData.l);

    /* handle partitions */
    while (bu_fgets(line, RT_MAXLINE, np->fp_out) != (char *)NULL) {
	bu_vls_strcpy(&vbuf, line);
	bu_vls_trimspace(&vbuf);
	if (line[0] == '\n' || line[0] == '\r') {
	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&vbuf));
	    break;
	}

	struct qray_dataList *ndlp = NULL;
	BU_ALLOC(ndlp, struct qray_dataList);
	BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	double scan[4]; /* holds sscanf values */
	ret = sscanf(bu_vls_cstr(&vbuf), "%le %le %le %le", &scan[0], &scan[1], &scan[2], &scan[3]);
	ndlp->x_in = scan[0];
	ndlp->y_in = scan[1];
	ndlp->z_in = scan[2];
	ndlp->los = scan[3];
	if (ret != 4) {
	    bu_log("WARNING: Unexpected nirt line [%s]\nExpecting four partition numbers.\n", bu_vls_cstr(&vbuf));
	    break;
	}
    }

    vbp = bv_vlblock_init(vlfree, 32);
    qray_data_to_vlist(gedp, vbp, &HeadQRayData, np->dir, 0);
    bu_list_free(&HeadQRayData.l);

    if (gedp->new_cmd_forms) {
	struct bview *view = gedp->ged_gvp;
	bv_vlblock_obj(vbp, view, "nirt");
    } else {
	_ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_basename), 0);
    }

    bv_vlblock_free(vbp);

    /* check if nirt wrote any errors from shots */
    if (bu_process_pending(fileno(np->fp_err))) {
	while (bu_fgets(line, RT_MAXLINE, np->fp_err) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    bu_vls_free(&vbuf);

    return BRLCAD_OK;
}

static int
nirt_cmd_overlaps(struct ged *gedp, struct nirt_info *n)
{
    struct bu_vls script = BU_VLS_INIT_ZERO;

    bu_vls_printf(&script, "xyz %0.17f %0.17f %0.17f;", V3ARGS(nv.center_model));
    /* Calculate the ray direction from the view, and add an -e script to
     * set it for nirt. */
    VMOVEN(np.dir, gedp->ged_gvp->gv_rotation + 8, 3);
    VSCALE(np.dir, np.dir, -1.0);
    bu_vls_printf(&script, "dir %0.17f %0.17f %0.17f;", V3ARGS(np.dir));
    /* Take the initial shot */
    bu_vls_printf(&script, "s");

    /* ensure nirt has started and has something to read from - with 5 second timeout */
    int64_t start = bu_gettime();

    while (!bu_process_pending(fileno(np->fp_err)) && !bu_process_pending(fileno(np->fp_out))) {
	if ((bu_gettime() - start) > BU_SEC2USEC(5))
	    break;
    }

    /* See if we got output */
    struct bu_vls vbuf = BU_VLS_INIT_ZERO;
    char line[RT_MAXLINE] = {0};

    if (DG_QRAY_TEXT(gedp->i->ged_gdp)) {
	while (bu_fgets(line, RT_MAXLINE, np->fp_out) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    struct bv_vlblock *vbp = NULL;
    BU_LIST_INIT(&HeadQRayData.l);

    if (!np->skip_drawn && DG_QRAY_GRAPHICS(gedp->i->ged_gdp)) {
	struct bv_vlblock *vbp = NULL;
	BU_LIST_INIT(&HeadQRayData.l);

	/* handle overlaps */
	while (bu_fgets(line, RT_MAXLINE, np->fp_out) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);

	    if (line[0] == '\n' || line[0] == '\r') {
		bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&vbuf));
		break;
	    }

	    struct qray_dataList *ndlp = NULL;
	    BU_ALLOC(ndlp, struct qray_dataList);
	    BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	    double scan[4]; /* holds sscanf values */
	    ret = sscanf(bu_vls_cstr(&vbuf), "%le %le %le %le", &scan[0], &scan[1], &scan[2], &scan[3]);
	    ndlp->x_in = scan[0];
	    ndlp->y_in = scan[1];
	    ndlp->z_in = scan[2];
	    ndlp->los = scan[3];
	    if (ret != 4) {
		bu_log("WARNING: Unexpected nirt line [%s]\nExpecting four overlap numbers.\n", bu_vls_cstr(&vbuf));
		break;
	    }
	}

	vbp = bv_vlblock_init(vlfree, 32);
	qray_data_to_vlist(gedp, vbp, &HeadQRayData, np->dir, 1);
	bu_list_free(&HeadQRayData.l);
	_ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_basename), 0);
	bv_vlblock_free(vbp);
    }

    /* check if nirt wrote any errors from shots */
    if (bu_process_pending(fileno(np->fp_err))) {
	while (bu_fgets(line, RT_MAXLINE, np->fp_err) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    bu_vls_free(&vbuf);

}


static int
nirt_cmd_print(struct ged *gedp, struct nirt_info *np)
{
    int ret = BRLCAD_OK;
    struct qray_dataList HeadQRayData;
    struct bu_list *vlfree = &rt_vlfree;
    char line[RT_MAXLINE] = {0};
    /* for bu_fgets space trimming */
    struct bu_vls vbuf = BU_VLS_INIT_ZERO;

    if (!gedp || !np)
	return BRLCAD_ERROR;

    /* ensure nirt has started and has something to read from - with 5 second timeout */
    int64_t start = bu_gettime();

    while (!bu_process_pending(fileno(np->fp_err)) && !bu_process_pending(fileno(np->fp_out))) {
	if ((bu_gettime() - start) > BU_SEC2USEC(5))
	    break;
    }

    /* check if nirt wrote anything to error on load */
    if (bu_process_pending(fileno(np->fp_err))) {
	while (bu_fgets(line, RT_MAXLINE, np->fp_err) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    if (DG_QRAY_TEXT(gedp->i->ged_gdp)) {
	while (bu_fgets(line, RT_MAXLINE, np->fp_out) != (char *)NULL) {
	    if (bu_strncmp(line, "MGED-PARTITION-REPORT", 21) == 0)
		break;
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    if (!np->skip_drawn && DG_QRAY_GRAPHICS(gedp->i->ged_gdp)) {
	struct bv_vlblock *vbp = NULL;
	BU_LIST_INIT(&HeadQRayData.l);

	/* handle partitions */
	while (bu_fgets(line, RT_MAXLINE, np->fp_out) != (char *)NULL) {
	    if (bu_strncmp(line, "MGED-OVERLAP-REPORT", 19) == 0)
		break;
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);

	    if (line[0] == '\n' || line[0] == '\r') {
		bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&vbuf));
		break;
	    }

	    struct qray_dataList *ndlp = NULL;
	    BU_ALLOC(ndlp, struct qray_dataList);
	    BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	    double scan[4]; /* holds sscanf values */
	    ret = sscanf(bu_vls_cstr(&vbuf), "%le %le %le %le", &scan[0], &scan[1], &scan[2], &scan[3]);
	    ndlp->x_in = scan[0];
	    ndlp->y_in = scan[1];
	    ndlp->z_in = scan[2];
	    ndlp->los = scan[3];
	    if (ret != 4) {
		bu_log("WARNING: Unexpected nirt line [%s]\nExpecting four partition numbers.\n", bu_vls_cstr(&vbuf));
		break;
	    }
	}

	vbp = bv_vlblock_init(vlfree, 32);
	qray_data_to_vlist(gedp, vbp, &HeadQRayData, np->dir, 0);
	bu_list_free(&HeadQRayData.l);

	if (gedp->new_cmd_forms) {
	    struct bview *view = gedp->ged_gvp;
	    bv_vlblock_obj(vbp, view, "nirt");
	} else {
	    _ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_basename), 0);
	}

	bv_vlblock_free(vbp);

	/* handle overlaps */
	while (bu_fgets(line, RT_MAXLINE, np->fp_out) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);

	    if (line[0] == '\n' || line[0] == '\r') {
		bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&vbuf));
		break;
	    }

	    struct qray_dataList *ndlp = NULL;
	    BU_ALLOC(ndlp, struct qray_dataList);
	    BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	    double scan[4]; /* holds sscanf values */
	    ret = sscanf(bu_vls_cstr(&vbuf), "%le %le %le %le", &scan[0], &scan[1], &scan[2], &scan[3]);
	    ndlp->x_in = scan[0];
	    ndlp->y_in = scan[1];
	    ndlp->z_in = scan[2];
	    ndlp->los = scan[3];
	    if (ret != 4) {
		bu_log("WARNING: Unexpected nirt line [%s]\nExpecting four overlap numbers.\n", bu_vls_cstr(&vbuf));
		break;
	    }
	}

	vbp = bv_vlblock_init(vlfree, 32);
	qray_data_to_vlist(gedp, vbp, &HeadQRayData, np->dir, 1);
	bu_list_free(&HeadQRayData.l);
	_ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_basename), 0);
	bv_vlblock_free(vbp);
    }

    /* check if nirt wrote any errors from shots */
    if (bu_process_pending(fileno(np->fp_err))) {
	while (bu_fgets(line, RT_MAXLINE, np->fp_err) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    bu_vls_free(&vbuf);

    bu_process_file_close(np->p, BU_PROCESS_STDOUT);
    bu_process_file_close(np->p, BU_PROCESS_STDERR);

    int retcode = bu_process_wait_n(&np->p, 0);

    if (retcode != 0) {
	_ged_wait_status(gedp->ged_result_str, retcode);
    }

    dl_set_wflag(gedp->i->ged_gdp->gd_headDisplay, DOWN);

    return ret;
}



/**
 * Invoke nirt with the current view & stuff
 *
 * NOTE: invoked through mged, nirt is currently set up to fire THREE
 *       rays in order to get partition and overlap information.
 */
int
ged_nirt_core(struct ged *gedp, int argc, const char *argv[])
{
    // Container holding info common to both setup and printing stages
    struct nirt_print_info np = {NULL, NULL, NULL, NULL, 0, VINIT_ZERO};

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* If we don't have the nirt executable, we're out of luck. */
    char nexec_buf[MAXPATHLEN] = {'\0'};
    const char *nexec = bu_dir(nexec_buf, MAXPATHLEN, BU_DIR_BIN, "nirt", BU_DIR_EXT, NULL);
    if (!nexec) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: Unable to find 'nirt' executable.\n");
	return BRLCAD_ERROR;
    }

    /* We will be uniformly freeing all of the main argv contents at the end, so
     * make our own copy of the exec path. */
    char *nirt = bu_strdup(nexec_buf);

    /* Look for options which solely print information to user.  If we have one
     * of those, there's no point in doing the rest of the setup. */
    const char *info_arg = NULL;

    /* To mimic normal option parsing, we want a "last option supplied wins"
     * behavior. */
    for (int i = 0; i < argc; i++) {
        if (BU_STR_EQUAL(argv[i], "-h") ||
            BU_STR_EQUAL(argv[i], "-?") ||
            BU_STR_EQUAL(argv[i], "--help") ||
            BU_STR_EQUAL(argv[i], "-L")) {
	    info_arg = argv[i];
	}
    }

    // If we found something, print and return
    if (info_arg) {
	// load av with request
	const char *av[5] = {NULL};
	av[0] = (const char *)nirt;
	av[1] = info_arg;
	av[2] = "-X";
	av[3] = "ged";
	av[4] = NULL;

	bu_process_create(&np.p, (const char **)av, BU_PROCESS_HIDE_WINDOW);

	// open pipes
	np.fp_in = bu_process_file_open(np.p, BU_PROCESS_STDIN);

	/* use fp_out to read back the result */
	np.fp_out = bu_process_file_open(np.p, BU_PROCESS_STDOUT);

	/* use fp_err to read any error messages */
	np.fp_err = bu_process_file_open(np.p, BU_PROCESS_STDERR);

	/* send quit command to nirt */
	fprintf(np.fp_in, "q\n");
	bu_process_file_close(np.p, BU_PROCESS_STDIN);

	/* Not using the general argv array, so free nirt exec path
	 * individually. */
	bu_free(nirt, "nirt exec");

	return nirt_cmd_print(gedp, &np);
    }

    /* Doing work with the database - start setting up */
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    // There are two possible sources of settings - the ones we deduce
    // from view state, and the ones we get from the user.  User supplied
    // always overrides deduced, so we first set up the state-based values
    // to serve as defaults and then process the args to get any user
    // overrides.
    //
    // Execution scripts are an exception to the above - user specified
    // options there are additive to what we use, not instead of.
    struct nirt_opt_vals nv = NIRT_OPT_INIT;
    np->nv = &nv;

    /* For now, we need to support the deprecated x y z at end of args
     * feature.  Try to be smart about it by skipping this step if all
     * three args at the end aren't valid numbers. */
    int use_input_orig = 0;
    if (argc > 3) {
	int have_xyz = 1;
	const char *xyz_str[2] = {NULL};
	for (int i = 1; i <= 3; i++) {
	    xyz_str[0] = argv[argc-i];
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&xyz_str, NULL) != 1) {
		have_xyz = 0;
		break;
	    }
	}

	// If all three are numbers, assume it is an x y z assignment
	if (have_xyz) {
	    bu_log("WARNING: Specifying center with 'x y z' at the end of the nirt command is deprecated - use the --xyz option instead\n");
	    int avind = 3;
	    for (int i = 0; i < 3; i++) {
		fastf_t val;
		xyz_str[0] = argv[avind];
		bu_opt_fastf_t(NULL, 1, (const char **)&xyz_str, &val);
		nv.center_model[i] = val;
		avind--;
	    }

	    // The last three options are now handled
	    use_input_orig = 1;
	    argc -= 3;
	}
    }

    /* Do a nirt option parse to set nv values based on command line args.  The
     * contents of nv.init_scripts will have to be stashed temporarily, since we
     * want any user-supplied options to come after nirt introduced contents,
     * but doing the parsing here we can catch an --xyz assignment option. */
    {
	struct bu_opt_desc *d = nirt_opt_desc(&nv);
	struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
	int ac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d);
	if (ac < 0) {
	    bu_free(d, "nirt opt desc");
	    nirt_opt_vals_free(&nv);
	    bu_vls_printf(gedp->ged_result_str, "ERROR: option parsing failed: %s.\n", bu_vls_cstr(&optparse_msg));
	    bu_vls_free(&optparse_msg);
	    return BRLCAD_ERROR;
	} else if (ac > 0) {
	    size_t badopts = 0;
	    for (int i=0; i < ac; i++) {
		if (argv[i][0] == '-') {
		    bu_log("ERROR: unrecognized option %s\n", argv[i]);
		    badopts++;
		}
	    }
	    if (badopts) {
		bu_free(d, "nirt opt desc");
		nirt_opt_vals_free(&nv);
		bu_vls_free(&optparse_msg);
		return BRLCAD_ERROR;
	    }
	}
	argc = ac;
	bu_free(d, "bu_opt_desc");
	bu_vls_free(&optparse_msg);
    }

    /* Stash any user supplied init_scripts so they can be added on at the end. */
    struct bu_ptbl usr_init_scripts = BU_PTBL_INIT_ZERO;
    bu_ptbl_cat(&usr_init_scripts, &nv.init_scripts);
    bu_ptbl_reset(&nv.init_scripts);

    /* Calculate point from xyz point from which to fire the ray, if it was not
     * explicitly supplied by one of the above. */
    if (VNEAR_ZERO(nv.center_model, VUNITIZE_TOL)) {
	struct bview *bv = gedp->ged_gvp;
	VSET(nv.center_model, -bv->gv_center[MDX], -bv->gv_center[MDY], -bv->gv_center[MDZ]);
	/* Because we are preparing an input for the nirt command line, we need
	 * to convert to local units - lower level logic will be expecting
	 * those units and convert back. */
	VSCALE(nv.center_model, nv.center_model, gedp->dbip->dbi_base2local);
    }


    /* Concat any user specified scripts onto the initial set. */
    bu_ptbl_cat(&nv.init_scripts, &usr_init_scripts);
    bu_ptbl_free(&usr_init_scripts);

    /*****************************************************************/
    /* Options are complete - ready to start assembling the command. */
    /*****************************************************************/

    /* We'll use a bu_ptbl to collect everything we need for the nirt argv. */
    struct bu_ptbl av_args = BU_PTBL_INIT_ZERO;

    /* No matter what, we'll always start with the nirt executable */
    bu_ptbl_ins(&av_args, (long *)nirt);

    /* We will be launching from ged, so populate those options */
    bu_ptbl_ins(&av_args, (long *)bu_strdup("-X"));
    bu_ptbl_ins(&av_args, (long *)bu_strdup("ged"));

    /* We only need to provide options for things that aren't set to defaults,
     * so use the appropriate libanalyze function to set up the args.
     * We deliberately do not incorporate the init scripts into the argv,
     * as those will be fed in after the process is launched */
    struct nirt_opt_vals nv_default = NIRT_OPT_INIT;
    nirt_opt_mk_args(&av_args, &nv, &nv_default, 0);

    /* Options complete, add filename */
    bu_ptbl_ins(&av_args, (long *)bu_strdup(gedp->dbip->dbi_filename));

    /* Prepare the argv array.  Object specification will be done
     * via the draw command to limit argv length, so we don't
     * incorporate them here. */
    const char **av = (const char **)bu_calloc(BU_PTBL_LEN(&av_args) + 1, sizeof(char *), "av");
    for (size_t i = 0; i < BU_PTBL_LEN(&av_args); i++) {
	av[i] = (const char *)BU_PTBL_GET(&av_args, i);
    }
    av[BU_PTBL_LEN(&av_args)] = NULL;

    if (gedp->i->ged_gdp->gd_qray_cmd_echo) {
	/* Print out the command we are about to run */
	for (size_t i = 0; i < BU_PTBL_LEN(&av_args); i++)
	    bu_vls_printf(gedp->ged_result_str, "%s ", av[i]);

	bu_vls_printf(gedp->ged_result_str, "\n");
    }

    /* argv ready.  Start up the process */
    bu_process_create(&np.p, av, BU_PROCESS_HIDE_WINDOW);

    if (bu_process_pid(np.p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess:\n");
	for (size_t i = 0; i < BU_PTBL_LEN(&av_args); i++)
	    bu_vls_printf(gedp->ged_result_str, "%s ", av[i]);
	bu_vls_printf(gedp->ged_result_str, "\n");
	for (size_t i = 0; i < BU_PTBL_LEN(&av_args); i++) {
	    struct bu_vls *a = (struct bu_vls *)BU_PTBL_GET(&av_args, i);
	    bu_vls_free(a);
	    BU_PUT(a, struct bu_vls);
	}
	bu_ptbl_free(&av_args);
	bu_free(av, "av");
	return BRLCAD_ERROR;
    }

    /* Process successfully launched, no longer need args */
    for (size_t i = 0; i < BU_PTBL_LEN(&av_args); i++) {
	char *a = (char *)BU_PTBL_GET(&av_args, i);
	bu_free(a, "av arg");
    }
    bu_ptbl_free(&av_args);
    bu_free(av, "av");


    /* Set up the pipes.  fp_in is for sending commands.   */
    np.fp_in = bu_process_file_open(np.p, BU_PROCESS_STDIN);

    /* use fp_out to read back the result */
    np.fp_out = bu_process_file_open(np.p, BU_PROCESS_STDOUT);

    /* use fp_err to read any error messages */
    np.fp_err = bu_process_file_open(np.p, BU_PROCESS_STDERR);

    // After this we start running commands and looking for output
    struct bu_vls vbuf = BU_VLS_INIT_ZERO;
    char line[RT_MAXLINE] = {0};

    /* Check if nirt wrote anything to error on load */
    if (bu_process_pending(fileno(np.fp_err))) {
	while (bu_fgets(line, RT_MAXLINE, np.fp_err) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    //*********************
    // Begin NIRT commands
    //*********************

    // Include nirt script string
    if (bu_vls_strlen(&gedp->i->ged_gdp->gd_qray_script))
	fprintf(np.fp_in, "%s\n", bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_script));

    // NOTE: user specified -f, if present, will come later in option
    // processing and override these settings. These merely establish a
    // "default" suitable for standard uses.
    if (DG_QRAY_TEXT(gedp->i->ged_gdp))
	fprintf(np.fp_in, "%s\n", "default-ged");


    //**************************************************************************
    // Set up the xyz firing point and ray direction.  These will be consistent
    // for all processing unless overridden by user options, so we only need to
    // set them up once at the beginning.
    //**************************************************************************

    // xyz center - either from the view or by user setting
    struct bu_vls script = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&script, "xyz %0.17f %0.17f %0.17f", V3ARGS(nv.center_model));
    fprintf(np.fp_in, "%s\n", bu_vls_cstr(&script));
    // calculate the ray direction from the view.
    VMOVEN(np.dir, gedp->ged_gvp->gv_rotation + 8, 3);
    VSCALE(np.dir, np.dir, -1.0);
    bu_vls_sprintf(&script, "dir %0.17f %0.17f %0.17f", V3ARGS(np.dir));
    fprintf(np.fp_in, "%s\n", bu_vls_cstr(&script));

    if (DG_QRAY_GRAPHICS(gedp->i->ged_gdp)) {

	// wipe formatting
	bu_ptbl_ins(&nv.init_scripts, (long *)bu_strdup(DG_QRAY_FORMAT_NULL));

	/* set up partition reporting */
	bu_ptbl_ins(&nv.init_scripts, (long *)bu_strdup(DG_QRAY_FORMAT_P));
	bu_vls_sprintf(&script, "fmt r \"MGED-PARTITION-REPORT\\n\"");
	bu_ptbl_ins(&nv.init_scripts, (long *)bu_strdup(bu_vls_cstr(&script)));


	/* second ray: -> used for partition report */
	bu_ptbl_ins(&nv.init_scripts, (long *)bu_strdup("s"));

	/* set up overlap formatting */
	bu_ptbl_ins(&nv.init_scripts, (long *)bu_strdup(DG_QRAY_FORMAT_O));

	bu_vls_sprintf(&script, "fmt r \"MGED-OVERLAP-REPORT\\n\"");
	bu_ptbl_ins(&nv.init_scripts, (long *)bu_strdup(bu_vls_cstr(&script)));

	/* third ray: start, direction, and 's' command -> used for overlap report */
	bu_ptbl_ins(&nv.init_scripts, (long *)bu_strdup("s"));

    }





    // TODO:
    //
    // pass in objects to interrogate as draw commands
    //
    // Thinking I may need to rework how the plotting info is reported from nirt.
    // Right now libged is firing multiple rays, using specific text formatting
    // to capture and prepare the graphical output.  This means that if the
    // MGED nirt command line has -e scripts with shots in then, those shots will
    // NOT produce graphical output the way the custom managed shot does.
    //
    // A better answer might be to supply an option to NIRT itself to write out
    // either a plot file or a textual file with the necessary info, and then
    // have this command read it from the cache.  That way, if the necessary
    // outputs are generated by nirt(1) for ALL shots taken, the -e command will
    // produce graphical output as expected.
    struct bu_vls nirt_cmd = BU_VLS_INIT_ZERO;


    /* Check if user requested objects other than what is drawn */
    if (argc && argv[0][0] != '-') {	/* ignore obvious trailing flag */
	if (db_lookup(gedp->dbip, argv[0], 0) != RT_DIR_NULL)
	    np.skip_drawn = 1;
	/* It might be a path, not just a solid specifier - try that as well */
	struct db_full_path pp;
	db_full_path_init(&pp);
	if (!db_string_to_path(&pp, gedp->dbip, argv[0]))
	    np.skip_drawn = 1;
	db_free_full_path(&pp);
    }

    /* If we do have user specified objects, append.  Otherwise, pull from the
     * who list */
    if (!np.skip_drawn) {
	size_t drawn_cnt = ged_who_argc(gedp);
	char **who_argv = (char **)bu_calloc(drawn_cnt+1, sizeof(char *), "who_argv");
	/* Note - ged_who_argv sets the last vp to (char *)0 */
	drawn_cnt = ged_who_argv(gedp, who_argv, (const char **)&who_argv[drawn_cnt+1]);
	for (size_t i = 0; i < drawn_cnt; i++) {
	    bu_vls_sprintf(&nirt_cmd, "draw %s", who_argv[i]);
	    fprintf(np.fp_in, "%s\n", bu_vls_cstr(&nirt_cmd));
	}
	bu_free(who_argv, "who_argv");
    } else {
	// User specified, first one was valid - just let the command line nirt
	// deal with checking the rest.  Can revisit if that turns out to be
	// problematic for some reason.
	for (int i = 0; i < argc; i++) {
	    struct bu_vls *obj;
	    BU_GET(obj, struct bu_vls);
	    bu_vls_init(obj);
	    bu_vls_sprintf(obj, "%s", argv[i]);
	    bu_ptbl_ins(&av_args, (long *)obj);
	}
    }


    /* send commands down the pipe.
     * TODO - should we be reading after each command to prevent
     * long outputs potentially blocking pipes? */
    for (int i = 1; i < gd_rt_cmd_len - 2; i++) {
	if (gd_rt_cmd[i] != NULL && BU_STR_EQUAL(gd_rt_cmd[i], "-e")) {
	    fprintf(np.fp_in, "%s\n", gd_rt_cmd[++i]);
	}
    }


    // graphics shots


    // User output printing shot.
    // printing is formatted with either nirt default or user specified '-f fmt'
    if (use_input_orig) {
	bu_vls_printf(gedp->ged_result_str, "\nFiring from (%lf, %lf, %lf)...\n",
		      nv.center_model[X], nv.center_model[Y], nv.center_model[Z]);
    } else {
	bu_vls_printf(gedp->ged_result_str, "\nFiring from view center...\n");
    }
    fprintf(np.fp_in, "s\n");




    // Include nirt script string
    if (bu_vls_strlen(&gedp->i->ged_gdp->gd_qray_script))
	fprintf(np.fp_in, "%s\n", bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_script));

    // NOTE: user specified -f, if present, will come later in option
    // processing and override these settings. These merely establish a
    // "default" suitable for standard uses.
    if (DG_QRAY_TEXT(gedp->i->ged_gdp))
	fprintf(np.fp_in, "%s\n", "default-ged");





    /* Done sending commands - send quit command to nirt */
    fprintf(np.fp_in, "q\n");
    bu_process_file_close(np.p, BU_PROCESS_STDIN);

    bu_free(&nirt_cmd);

    return nirt_cmd_print(gedp, &np);
}


int
ged_vnirt_core(struct ged *gedp, int argc, const char *argv[])
{
    int status;
    fastf_t sf = 1.0 * INV_BV;
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
    scan[Z] = BV_MAX;
    argc -= 2;

    av = (char **)bu_calloc(1, sizeof(char *) * (argc + 5), "gd_vnirt_cmd: av");

    /* Calculate point from which to fire ray. */
    VSCALE(view_ray_orig, scan, sf);
    MAT4X3PNT(center_model, gedp->ged_gvp->gv_view2model, view_ray_orig);
    /* Initial center_model value will be in base units, and main nirt
     * evaluation path assumes inputs are in local units, so convert. */
    VSCALE(center_model, center_model, gedp->dbip->dbi_base2local);

    bu_vls_printf(&x_vls, "%lf", center_model[X]);
    bu_vls_printf(&y_vls, "%lf", center_model[Y]);
    bu_vls_printf(&z_vls, "%lf", center_model[Z]);

    av[0] = "nirt";

    /* pass modified coordinates to nirt */
    av[1] = "--xyz";
    av[2] = bu_vls_addr(&x_vls);
    av[3] = bu_vls_addr(&y_vls);
    av[4] = bu_vls_addr(&z_vls);

    /* pass remaining arguments to ged nirt command */
    for (int i = 1; i < argc; ++i)
	av[i+4] = (char *)argv[i];

    av[i] = (char *)NULL;

    status = ged_nirt_core(gedp, argc + 4, (const char **)av);

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
