/*                       N I R T . C P P
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
/** @file libged/nirt.cpp
 *
 * Routines to interface to nirt.
 *
 * This code was imported from the RTUIF and modified to work as part
 * of the drawable geometry object.
 *
 */
/** @} */

#include "common.h"

#include <string>
#include <fstream>

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
};


static int
nirt_cmd_print(struct ged *gedp, struct nirt_info *np)
{
    int ret = BRLCAD_OK;
    char line[RT_MAXLINE] = {0};
    // for bu_fgets space trimming
    struct bu_vls vbuf = BU_VLS_INIT_ZERO;

    if (!gedp || !np)
	return BRLCAD_ERROR;

    // ensure nirt has started and has something to read from - with 5 second timeout
    int64_t start = bu_gettime();

    while (!bu_process_pending(fileno(np->fp_err)) && !bu_process_pending(fileno(np->fp_out))) {
	if ((bu_gettime() - start) > BU_SEC2USEC(5))
	    break;
    }

    // check if nirt wrote anything to error on load
    if (bu_process_pending(fileno(np->fp_err))) {
	while (bu_fgets(line, RT_MAXLINE, np->fp_err) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    // If we're outputting text, handle that
    if (DG_QRAY_TEXT(gedp->i->ged_gdp)) {
	while (bu_fgets(line, RT_MAXLINE, np->fp_out) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    // check if nirt wrote any errors from shots
    if (bu_process_pending(fileno(np->fp_err))) {
	while (bu_fgets(line, RT_MAXLINE, np->fp_err) != (char *)NULL) {
	    bu_vls_strcpy(&vbuf, line);
	    bu_vls_trimspace(&vbuf);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&vbuf));
	}
    }

    bu_vls_free(&vbuf);

   return ret;
}



/**
 * Invoke nirt, optionally using the current view info to drive
 * shot center and direction.
 */
int
ged_nirt_core(struct ged *gedp, int argc, const char *argv[])
{
    // Container holding info common to both setup and printing stages
    struct nirt_info np = {NULL, NULL, NULL, NULL, NULL};

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* we know we're the nirt command */
    argc-=(argc>0); argv+=(argc>0);

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

	nirt_cmd_print(gedp, &np);

	bu_process_file_close(np.p, BU_PROCESS_STDOUT);
	bu_process_file_close(np.p, BU_PROCESS_STDERR);

	int retcode = bu_process_wait_n(&np.p, 0);
	if (retcode != 0)
	    _ged_wait_status(gedp->ged_result_str, retcode);

	dl_set_wflag(gedp->i->ged_gdp->gd_headDisplay, DOWN);

	return BRLCAD_OK;
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
    np.nv = &nv;

    /* For now, we need to support the deprecated x y z at end of args
     * feature.  Try to be smart about it by skipping this step if all
     * three args at the end aren't valid numbers. */
    int use_input_orig = 0;
    if (argc == 3) {
	int have_xyz = 1;

	// First, see if they are all numbers
	const char *xyz_str[2] = {NULL};
	for (int i = 0; i < 3; i++) {
	    xyz_str[0] = argv[i];
	    if (bu_opt_fastf_t(NULL, 1, (const char **)xyz_str, NULL) != 1) {
		have_xyz = 0;
		break;
	    }
	}

	// If all three are numbers, assume it is an x y z assignment
	if (have_xyz) {
	    bu_log("WARNING: Specifying center with 'x y z' at the end of the nirt command is deprecated - use the --xyz option instead\n");
	    for (int i = 0; i < 3; i++) {
		fastf_t val;
		xyz_str[0] = argv[i];
		bu_opt_fastf_t(NULL, 1, (const char **)xyz_str, &val);
		nv.center_model[i] = val;
	    }

	    // The last three options are now handled
	    use_input_orig = 1;
	    argc -= 3;
	}
    }

    /* Grab settings from ged_drawable */
    unsigned char rgbc[3];

    /* Odd color */
    rgbc[RED] = gedp->i->ged_gdp->gd_qray_odd_color.r;
    rgbc[GRN] = gedp->i->ged_gdp->gd_qray_odd_color.g;
    rgbc[BLU] = gedp->i->ged_gdp->gd_qray_odd_color.b;
    bu_color_from_rgb_chars(&nv.color_odd, rgbc);
    /* Even color */
    rgbc[RED] = gedp->i->ged_gdp->gd_qray_even_color.r;
    rgbc[GRN] = gedp->i->ged_gdp->gd_qray_even_color.g;
    rgbc[BLU] = gedp->i->ged_gdp->gd_qray_even_color.b;
    bu_color_from_rgb_chars(&nv.color_even, rgbc);
    /* Void color */
    rgbc[RED] = gedp->i->ged_gdp->gd_qray_void_color.r;
    rgbc[GRN] = gedp->i->ged_gdp->gd_qray_void_color.g;
    rgbc[BLU] = gedp->i->ged_gdp->gd_qray_void_color.b;
    bu_color_from_rgb_chars(&nv.color_gap, rgbc);
    /* Ovlp color */
    rgbc[RED] = gedp->i->ged_gdp->gd_qray_overlap_color.r;
    rgbc[GRN] = gedp->i->ged_gdp->gd_qray_overlap_color.g;
    rgbc[BLU] = gedp->i->ged_gdp->gd_qray_overlap_color.b;
    bu_color_from_rgb_chars(&nv.color_ovlp, rgbc);

    /* Do a nirt option parse to set nv values based on command line args. */
    if (argc > 0) {
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

    int have_user_plotfile = bu_vls_strlen(&nv.plotfile);
    if (!have_user_plotfile) {
	// The user didn't ask us to save the plot file, so we will
	// make a temporary one in the cache and delete it once the
	// run is complete.
	char dir[MAXPATHLEN];
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, NULL);
	if (!bu_file_exists(dir, NULL))
	    bu_mkdir(dir);

	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, "nirt", NULL);
	if (!bu_file_exists(dir, NULL))
	    bu_mkdir(dir);

	struct bu_vls ncachefile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ncachefile, "%d_plots.pl", bu_pid());
	bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, "nirt", bu_vls_cstr(&ncachefile), NULL);
	bu_vls_sprintf(&nv.plotfile, "%s", dir);
    }

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

    /* Before launching the process, check if user requested objects other than
     * what is drawn and whether those objects are valid */
    int skip_drawn = 0;
    int invalid_obj = 0;
    for (int i = 0; i < argc; i++) {
	if (argv[i][0] == '-') {
	    /* ignore obvious trailing flag */
	    break;
	}
	if (db_lookup(gedp->dbip, argv[0], 0) != RT_DIR_NULL) {
	    skip_drawn = 1;
	    continue;
	}
	/* It might be a path, not just a solid specifier - try that as well */
	struct db_full_path pp;
	db_full_path_init(&pp);
	if (!db_string_to_path(&pp, gedp->dbip, argv[0])) {
	    skip_drawn = 1;
	    db_free_full_path(&pp);
	    continue;
	}
	db_free_full_path(&pp);
	invalid_obj = 1;
    }
    if (invalid_obj) {
	bu_vls_printf(gedp->ged_result_str, "\nUser specified invalid objects.\n");
	return BRLCAD_ERROR;

    }

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


    // NOTE: user specified -f, if present, will come later in option
    // processing and override these settings.
    if (DG_QRAY_TEXT(gedp->i->ged_gdp)) {

	if (gedp->i->ged_gdp->gd_qray_fmts) {

	    for (int i = 0; gedp->i->ged_gdp->gd_qray_fmts[i].type != (char)0; ++i) {
		struct ged_qray_fmt *d = &gedp->i->ged_gdp->gd_qray_fmts[i];
		fprintf(np.fp_in, "fmt %c %s\n", d->type, bu_vls_cstr(&d->fmt));
	    }

	} else {

	    struct bu_vls ged_fmt_file = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&ged_fmt_file, "%s/ged.nrt", bu_dir(NULL, 0, BU_DIR_DATA, "nirt", NULL));
	    std::string s;
	    std::ifstream file;
	    file.open(bu_vls_cstr(&ged_fmt_file));
	    while (std::getline(file, s))
		fprintf(np.fp_in, "%s\n", s.c_str());
	    file.close();
	    bu_vls_free(&ged_fmt_file);
	}

    }


    //**************************************************************************
    // Set up the xyz firing point and ray direction.  These will be consistent
    // for all processing unless overridden by user options, so we only need to
    // set them up once at the beginning.
    //**************************************************************************

    // xyz center - either from the view or by user setting
    struct bu_vls nirt_cmd = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&nirt_cmd, "xyz %0.17f %0.17f %0.17f", V3ARGS(nv.center_model));
    fprintf(np.fp_in, "%s\n", bu_vls_cstr(&nirt_cmd));
    // calculate the ray direction from the view.
    vect_t dir = VINIT_ZERO;
    VMOVEN(dir, gedp->ged_gvp->gv_rotation + 8, 3);
    VSCALE(dir, dir, -1.0);
    bu_vls_sprintf(&nirt_cmd, "dir %0.17f %0.17f %0.17f", V3ARGS(dir));
    fprintf(np.fp_in, "%s\n", bu_vls_cstr(&nirt_cmd));


    /* If we do have user specified objects, append.  Otherwise, pull from the
     * who list */
    if (skip_drawn) {
	for (int i = 0; i < argc; i++) {
	    struct bu_vls *obj;
	    BU_GET(obj, struct bu_vls);
	    bu_vls_init(obj);
	    bu_vls_sprintf(obj, "%s", argv[i]);
	    bu_ptbl_ins(&av_args, (long *)obj);
	}
    } else {
	size_t drawn_cnt = ged_who_argc(gedp);
	char **who_argv = (char **)bu_calloc(drawn_cnt+1, sizeof(char *), "who_argv");
	/* Note - ged_who_argv sets the last vp to (char *)0 */
	drawn_cnt = ged_who_argv(gedp, who_argv, (const char **)&who_argv[drawn_cnt+1]);
	for (size_t i = 0; i < drawn_cnt; i++) {
	    bu_vls_sprintf(&nirt_cmd, "draw %s", who_argv[i]);
	    fprintf(np.fp_in, "%s\n", bu_vls_cstr(&nirt_cmd));
	}
	bu_free(who_argv, "who_argv");
    }

    // If we have a script stored in the GED structure, start there
    int have_script = 0;
    if (bu_vls_strlen(&gedp->i->ged_gdp->gd_qray_script)) {
	fprintf(np.fp_in, "%s\n", bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_script));
	have_script = 1;
    }

    /* If the user specified their own scripts, do the work they have asked for.
     * Otherwise, we do a single shot using the view xyz and dir.
     *
     * If we only got -f scripts, those are for formatting setting and we still
     * need to take a shot ourselves.  Otherwise, allow the user scripts full
     * control over what does or doesn't happen shot wise. */
    if (BU_PTBL_LEN(&nv.init_scripts)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&nv.init_scripts); i++) {
	    const char *escript = (const char *)BU_PTBL_GET(&nv.init_scripts, i);
	    fprintf(np.fp_in, "%s\n", escript);
	}
    }
    if (!have_script && !nv.script_set) {
	// User output printing shot.
	// printing is formatted with either nirt default or user specified '-f fmt'
	if (use_input_orig) {
	    bu_vls_printf(gedp->ged_result_str, "\nFiring from (%lf, %lf, %lf)...\n",
		    nv.center_model[X], nv.center_model[Y], nv.center_model[Z]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\nFiring from view center...\n");
	}
	fprintf(np.fp_in, "s\n");
    }

    /* Done sending commands - send quit command to nirt */
    fprintf(np.fp_in, "q\n");
    bu_process_file_close(np.p, BU_PROCESS_STDIN);
    bu_vls_free(&nirt_cmd);

    /* Export output */
    nirt_cmd_print(gedp, &np);

    /* Shut down the subprocess */
    bu_process_file_close(np.p, BU_PROCESS_STDOUT);
    bu_process_file_close(np.p, BU_PROCESS_STDERR);

    int retcode = bu_process_wait_n(&np.p, 0);
    if (retcode != 0)
	_ged_wait_status(gedp->ged_result_str, retcode);

    dl_set_wflag(gedp->i->ged_gdp->gd_headDisplay, DOWN);

    /* Whether or not we're doing graphics, if we took a shot we should clear any
     * old objects from prior shots. */
    if (gedp->new_cmd_forms) {
	struct bview *view = gedp->ged_gvp;
	struct bv_scene_obj *nobj = bv_find_obj(view, bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_basename));
	if (nobj)
	    bv_obj_put(nobj);
    } else {
	struct directory **dpv;
	struct bu_vls dp_pattern = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&dp_pattern, "%s*", bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_basename));
	size_t lscnt = db_ls(gedp->dbip, DB_LS_PHONY, bu_vls_cstr(&dp_pattern), &dpv);
	for (size_t i = 0; i < lscnt; i++)
	    dl_erasePathFromDisplay(gedp, dpv[i]->d_namep, 0);
	bu_vls_free(&dp_pattern);
    }

    /* If we're supposed to do graphics, look for the plot file */
    struct bu_list *vlfree = &rt_vlfree;
    if (DG_QRAY_GRAPHICS(gedp->i->ged_gdp) && bu_vls_strlen(&nv.plotfile)) {
	FILE *fp = fopen(bu_vls_cstr(&nv.plotfile), "rb");
	if (fp) {
	    struct bv_vlblock*vbp = bv_vlblock_init(vlfree, 32);
	    fastf_t csize = gedp->ged_gvp->gv_scale * 0.01;
	    int pret = rt_uplot_to_vlist(vbp, fp, csize, gedp->i->ged_gdp->gd_uplotOutputMode);
	    fclose(fp);
	    if (pret < 0) {
		bu_log("Error loading plot data from %s\n", bu_vls_cstr(&nv.plotfile));
	    } else {
		if (gedp->new_cmd_forms) {
		    struct bview *view = gedp->ged_gvp;
		    struct bv_scene_obj *nobj = bv_vlblock_obj(vbp, view, bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_basename));
		    bu_vls_sprintf(&nobj->s_name, "%s", bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_basename));
		} else {
		    _ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_cstr(&gedp->i->ged_gdp->gd_qray_basename), 0);
		}

		bv_vlblock_free(vbp);
	    }
	}
	bu_file_delete(bu_vls_cstr(&nv.plotfile));
    }

    /* Delete the plot file, if the user didn't ask for it. */
    if (!have_user_plotfile)
	bu_file_delete(bu_vls_cstr(&nv.plotfile));

    /* Free option memory */
    nirt_opt_vals_free(&nv);

    // Success
    return BRLCAD_OK;
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

    av[0] = bu_strdup("nirt");

    /* pass modified coordinates to nirt */
    av[1] = bu_strdup("--xyz");
    av[2] = bu_vls_addr(&x_vls);
    av[3] = bu_vls_addr(&y_vls);
    av[4] = bu_vls_addr(&z_vls);

    /* pass remaining arguments to ged nirt command */
    int i = 0;
    for (i = 1; i < argc; ++i)
	av[i+4] = (char *)argv[i];

    av[i] = (char *)NULL;

    status = ged_nirt_core(gedp, argc + 4, (const char **)av);

    bu_free(av[0], "av0");
    bu_free(av[1], "av1");
    bu_vls_free(&x_vls);
    bu_vls_free(&y_vls);
    bu_vls_free(&z_vls);
    bu_free((void *)av, "ged_vnirt_core: av");
    av = NULL;

    return status;
}

extern "C" {
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
}
#endif /* GED_PLUGIN */



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
