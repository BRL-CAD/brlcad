/*                         R T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file libged/rt.c
 *
 * The rt command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bresource.h"

#include "bu/app.h"
#include "bu/process.h"

#include "./ged_private.h"


struct _ged_rt_client_data {
    struct ged_subprocess *rrtp;
    struct ged *gedp;
};


void
_ged_rt_write(struct ged *gedp,
	      FILE *fp,
	      vect_t eye_model,
	      int argc,
	      const char **argv)
{
    quat_t quat;

    /* Double-precision IEEE floating point only guarantees 15-17
     * digits of precision; single-precision only 6-9 significant
     * decimal digits.  Using a %.15e precision specifier makes our
     * printed value dip into unreliable territory (1+15 digits).
     * Looking through our history, %.14e seems to be safe as the
     * value prior to printing quaternions was %.9e, although anything
     * from 9->14 "should" be safe as it's above our calculation
     * tolerance and above single-precision capability.
     */
    fprintf(fp, "viewsize %.14e;\n", gedp->ged_gvp->gv_size);
    quat_mat2quat(quat, gedp->ged_gvp->gv_rotation);
    fprintf(fp, "orientation %.14e %.14e %.14e %.14e;\n", V4ARGS(quat));
    fprintf(fp, "eye_pt %.14e %.14e %.14e;\n",
		  eye_model[X], eye_model[Y], eye_model[Z]);

    fprintf(fp, "start 0; clean;\n");

    /* If no objects were specified, activate all objects currently displayed.
     * Otherwise, simply draw the specified objects. If the caller passed
     * -1 in argc it means the objects are specified on the command line.
     * (TODO - we shouldn't be doing that anywhere for this; long command
     * strings make Windows unhappy.  Once all the callers have been
     * adjusted to pass the object lists for itemization via pipes below,
     * remove the -1 case.) */
    if (argc >= 0) {
	if (!argc) {
	    struct display_list *gdlp;
	    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
		if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR)
		    continue;
		fprintf(fp, "draw %s;\n", bu_vls_addr(&gdlp->dl_path));
	    }
	} else {
	    int i = 0;
	    while (i < argc) {
		fprintf(fp, "draw %s;\n", argv[i++]);
	    }
	}

	fprintf(fp, "prep;\n");
    }

    dl_bitwise_and_fullpath(gedp->ged_gdp->gd_headDisplay, ~RT_DIR_USED);

    dl_write_animate(gedp->ged_gdp->gd_headDisplay, fp);

    dl_bitwise_and_fullpath(gedp->ged_gdp->gd_headDisplay, ~RT_DIR_USED);

    fprintf(fp, "end;\n");
}


void
_ged_rt_set_eye_model(struct ged *gedp,
		      vect_t eye_model)
{
    if (gedp->ged_gvp->gv_zclip || gedp->ged_gvp->gv_perspective > 0) {
	vect_t temp;

	VSET(temp, 0.0, 0.0, 1.0);
	MAT4X3PNT(eye_model, gedp->ged_gvp->gv_view2model, temp);
    } else {
	/* not doing zclipping, so back out of geometry */
	int i;
	vect_t direction;
	vect_t extremum[2];
	double t_in;
	vect_t diag1;
	vect_t diag2;
	point_t ecenter;

	VSET(eye_model, -gedp->ged_gvp->gv_center[MDX],
	     -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);

	for (i = 0; i < 3; ++i) {
	    extremum[0][i] = INFINITY;
	    extremum[1][i] = -INFINITY;
	}

	(void)dl_bounding_sph(gedp->ged_gdp->gd_headDisplay, &(extremum[0]), &(extremum[1]), 1);

	VMOVEN(direction, gedp->ged_gvp->gv_rotation + 8, 3);
	for (i = 0; i < 3; ++i)
	    if (NEAR_ZERO(direction[i], 1e-10))
		direction[i] = 0.0;

	VSUB2(diag1, extremum[1], extremum[0]);
	VADD2(ecenter, extremum[1], extremum[0]);
	VSCALE(ecenter, ecenter, 0.5);
	VSUB2(diag2, ecenter, eye_model);
	t_in = MAGNITUDE(diag1) + MAGNITUDE(diag2);
	VJOIN1(eye_model, eye_model, t_in, direction);
    }
}


void
_ged_rt_output_handler(void *clientData, int UNUSED(mask))
{
    struct _ged_rt_client_data *drcdp = (struct _ged_rt_client_data *)clientData;
    struct ged_subprocess *run_rtp;
    int count = 0;
    int retcode = 0;
    int read_failed = 0;
    char line[RT_MAXLINE+1] = {0};

    if (drcdp == (struct _ged_rt_client_data *)NULL ||
	drcdp->gedp == (struct ged *)NULL ||
	drcdp->rrtp == (struct ged_subprocess *)NULL)
	return;

    run_rtp = drcdp->rrtp;

    /* Get data from rt */
    if (bu_process_read((char *)line, &count, run_rtp->p, BU_PROCESS_STDERR, RT_MAXLINE) <= 0) {
	read_failed = 1;
    }

    if (read_failed) {
	int aborted;

	/* Done watching for output, undo subprocess I/O hooks. */
	if (drcdp->gedp->ged_delete_io_handler) {
	    (*drcdp->gedp->ged_delete_io_handler)(drcdp->gedp->ged_interp, run_rtp->chan,
		    run_rtp->p, BU_PROCESS_STDERR, (void *)drcdp,
		    _ged_rt_output_handler);
	}
	/* Either EOF has been sent or there was a read error.
	 * there is no need to block indefinitely */
	retcode = bu_process_wait(&aborted, run_rtp->p, 120);

	if (aborted)
	    bu_log("Raytrace aborted.\n");
	else if (retcode)
	    bu_log("Raytrace failed.\n");
	else
	    bu_log("Raytrace complete.\n");

	if (drcdp->gedp->ged_gdp->gd_rtCmdNotify != (void (*)(int))0)
	    drcdp->gedp->ged_gdp->gd_rtCmdNotify(aborted);

	/* free run_rtp */
	BU_LIST_DEQUEUE(&run_rtp->l);
	BU_PUT(run_rtp, struct ged_subprocess);
	BU_PUT(drcdp, struct _ged_rt_client_data);

	return;
    }

    /* for feelgoodedness */
    line[count] = '\0';

    /* handle (i.e., probably log to stderr) the resulting line */
    if (drcdp->gedp->ged_output_handler != (void (*)(struct ged *, char *))0)
	drcdp->gedp->ged_output_handler(drcdp->gedp, line);
    else
	bu_vls_printf(drcdp->gedp->ged_result_str, "%s", line);
}

int
_ged_run_rt(struct ged *gedp, int cmd_len, const char **gd_rt_cmd, int argc, const char **argv)
{
    FILE *fp_in;
    vect_t eye_model;
    struct ged_subprocess *run_rtp;
    struct _ged_rt_client_data *drcdp;
    struct bu_process *p = NULL;

    bu_process_exec(&p, gd_rt_cmd[0], cmd_len, gd_rt_cmd, 0, 0);
    fp_in = bu_process_open(p, BU_PROCESS_STDIN);

    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	for (int i = 0; i < cmd_len; i++) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", gd_rt_cmd[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
	return GED_ERROR;
    }

    _ged_rt_set_eye_model(gedp, eye_model);
    _ged_rt_write(gedp, fp_in, eye_model, argc, argv);

    bu_process_close(p, BU_PROCESS_STDIN);

    BU_GET(run_rtp, struct ged_subprocess);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&gedp->gd_headSubprocess.l, &run_rtp->l);

    run_rtp->p = p;
    run_rtp->aborted = 0;

    BU_GET(drcdp, struct _ged_rt_client_data);
    drcdp->gedp = gedp;
    drcdp->rrtp = run_rtp;

    /* If we know how, set up hooks so the parent process knows to watch for output. */
    if (gedp->ged_create_io_handler) {
	(*gedp->ged_create_io_handler)(&(run_rtp->chan), p, BU_PROCESS_STDERR, gedp->io_mode, (void *)drcdp, _ged_rt_output_handler);
    }
    return GED_OK;
}


int
ged_rt(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    int units_supplied = 0;
    char pstring[32];
    int args;
    char **gd_rt_cmd = NULL;
    int gd_rt_cmd_len = 0;
    int ret = GED_OK;

    const char *bin;
    char rt[256] = {0};

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    args = argc + 7 + 2 + ged_who_argc(gedp);
    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    bin = bu_brlcad_root("bin", 1);
    if (bin) {
	snprintf(rt, 256, "%s/%s", bin, argv[0]);
    }

    vp = &gd_rt_cmd[0];
    *vp++ = rt;
    *vp++ = "-M";

    if (gedp->ged_gvp->gv_perspective > 0) {
	(void)sprintf(pstring, "-p%g", gedp->ged_gvp->gv_perspective);
	*vp++ = pstring;
    }

    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-' && argv[i][1] == 'u' &&
	    BU_STR_EQUAL(argv[1], "-u")) {
	    units_supplied=1;
	} else if (argv[i][0] == '-' && argv[i][1] == '-' &&
		   argv[i][2] == '\0') {
	    ++i;
	    break;
	}
	*vp++ = (char *)argv[i];
    }

    /* default to local units when not specified on command line */
    if (!units_supplied) {
	*vp++ = "-u";
	*vp++ = "model";
    }

    *vp++ = gedp->ged_wdbp->dbip->dbi_filename;
    gd_rt_cmd_len = vp - gd_rt_cmd;

    ret = _ged_run_rt(gedp, gd_rt_cmd_len, (const char **)gd_rt_cmd, (argc - i), &(argv[i]));

    bu_free(gd_rt_cmd, "free gd_rt_cmd");

    return ret;
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
