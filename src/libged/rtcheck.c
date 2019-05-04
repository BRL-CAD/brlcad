/*                         R T C H E C K . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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
/** @file libged/rtcheck.c
 *
 * The rtcheck command.
 *
 */

#include "common.h"

#include <stdlib.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#include "bio.h"
#include "bresource.h"

#include "bu/app.h"

#include "./ged_private.h"

struct ged_rtcheck {
    struct ged_subprocess *rrtp;
    struct ged *gedp;
    FILE *fp;
    struct bn_vlblock *vbp;
    struct bu_list *vhead;
    double csize;
    void *chan;
    struct bu_process *p;
    int read_failed;
    int draw_read_failed;
};

void
_ged_wait_status(struct bu_vls *logstr,
		 int status)
{
    int sig = status & 0x7f;
    int core = status & 0x80;
    int ret = status >> 8;

    if (status == 0) {
	bu_vls_printf(logstr, "Normal exit\n");
	return;
    }

    bu_vls_printf(logstr, "Abnormal exit x%x", status);

    if (core)
	bu_vls_printf(logstr, ", core dumped");

    if (sig)
	bu_vls_printf(logstr, ", terminating signal = %d", sig);
    else
	bu_vls_printf(logstr, ", return (exit) code = %d", ret);
}

static void rtcheck_output_handler(ClientData clientData, int mask);
static void rtcheck_vector_handler(ClientData clientData, int mask);

static void
rtcheck_handler_cleanup(struct ged_rtcheck *rtcp)
{
    int retcode;

    _ged_delete_io_handler(rtcp->gedp->ged_interp, rtcp->rrtp->chan,
	    rtcp->rrtp->p, BU_PROCESS_STDOUT, (void *)rtcp,
	    rtcheck_vector_handler);

    _ged_delete_io_handler(rtcp->gedp->ged_interp, rtcp->chan,
	    rtcp->p, BU_PROCESS_STDERR, (void *)rtcp,
	    rtcheck_output_handler);

    bu_process_close(rtcp->rrtp->p, BU_PROCESS_STDOUT);

    /* wait for the forked process */
    retcode = bu_process_wait(NULL, rtcp->rrtp->p, 0);
    if (retcode != 0) {
	_ged_wait_status(rtcp->gedp->ged_result_str, retcode);
    }

    BU_LIST_DEQUEUE(&rtcp->rrtp->l);
    BU_PUT(rtcp->rrtp, struct ged_subprocess);
    BU_PUT(rtcp, struct ged_rtcheck);
}

static void
rtcheck_vector_handler(ClientData clientData, int UNUSED(mask))
{
    int value = 0;
    struct ged_rtcheck *rtcp = (struct ged_rtcheck *)clientData;

    /* Get vector output from rtcheck */
    if (!rtcp->draw_read_failed && (feof(rtcp->fp) || (value = getc(rtcp->fp)) == EOF)) {
	size_t i;
	int have_visual = 0;
	const char *sname = "OVERLAPS";

	rtcp->draw_read_failed = 1;

	dl_set_flag(rtcp->gedp->ged_gdp->gd_headDisplay, DOWN);

	/* Add overlay (or, if nothing to draw, clear any stale overlay) */
	if (rtcp->vbp) {
	    for (i = 0; i < rtcp->vbp->nused; i++) {
		if (!BU_LIST_IS_EMPTY(&(rtcp->vbp->head[i]))) {
		    have_visual = 1;
		    break;
		}
	    }
	}

	if (have_visual) {
	    _ged_cvt_vlblock_to_solids(rtcp->gedp, rtcp->vbp, sname, 0);
	    bn_vlblock_free(rtcp->vbp);
	} else {
	    /* TODO - yuck.  This name is a product of the internals of the
	     * "_ged_cvt_vlblock_to_solids" routine.  We should have a way to kill
	     * command-specific display related objects cleanly...  hard-coding
	     * this name ties proper stale drawing cleanup to the internals of the
	     * current phony object drawing system.*/
	    const char *sname_obj = "OVERLAPSffff00";
	    struct directory *dp;
	    if ((dp = db_lookup(rtcp->gedp->ged_wdbp->dbip, sname_obj, LOOKUP_QUIET)) != RT_DIR_NULL) {
		dl_erasePathFromDisplay(rtcp->gedp->ged_gdp->gd_headDisplay, rtcp->gedp->ged_wdbp->dbip,
			rtcp->gedp->ged_free_vlist_callback, sname_obj, 0, rtcp->gedp->freesolid);
	    }
	}

	rtcp->vbp = NULL;
    }

    if (rtcp->read_failed && rtcp->draw_read_failed) {
	rtcheck_handler_cleanup(rtcp);
	return;
    }

    if (!rtcp->draw_read_failed) {
	(void)rt_process_uplot_value(&rtcp->vhead,
		rtcp->vbp,
		rtcp->fp,
		value,
		rtcp->csize,
		rtcp->gedp->ged_gdp->gd_uplotOutputMode);
    }
}

static void
rtcheck_output_handler(ClientData clientData, int UNUSED(mask))
{
    int count;
    char line[RT_MAXLINE] = {0};
    struct ged_rtcheck *rtcp = (struct ged_rtcheck *)clientData;

    /* Get textual output from rtcheck */
    if (bu_process_read((char *)line, &count, rtcp->p, BU_PROCESS_STDERR, RT_MAXLINE) <= 0) {
	rtcp->read_failed = 1;
	if (rtcp->gedp->ged_gdp->gd_rtCmdNotify != (void (*)(int))0)
	    rtcp->gedp->ged_gdp->gd_rtCmdNotify(0);
    }


    if (rtcp->read_failed && rtcp->draw_read_failed) {
	rtcheck_handler_cleanup(rtcp);
	return;
    }

    line[count] = '\0';
    if (rtcp->gedp->ged_output_handler != (void (*)(struct ged *, char *))0) {
	rtcp->gedp->ged_output_handler(rtcp->gedp, line);
    }	else {
	bu_vls_printf(rtcp->gedp->ged_result_str, "%s", line);
    }
}

/*
 * Check for overlaps in the current view.
 *
 * Usage:
 * rtcheck options
 *
 */
int
ged_rtcheck(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    FILE *fp;
    struct ged_rtcheck *rtcp;
    struct bu_process *p = NULL;
    char ** gd_rt_cmd = NULL;
    int gd_rt_cmd_len = 0;

    vect_t eye_model;

    const char *bin;
    char rtcheck[256] = {0};
    size_t args = 0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bin = bu_brlcad_root("bin", 1);
    if (bin) {
	snprintf(rtcheck, 256, "%s/%s", bin, argv[0]);
    }

    args = argc + 7 + 2 + ged_count_tops(gedp);
    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gd_rt_cmd[0];
    *vp++ = rtcheck;
    *vp++ = "-M";
    for (i = 1; i < argc; i++)
	*vp++ = (char *)argv[i];

    *vp++ = gedp->ged_wdbp->dbip->dbi_filename;

    /*
     * Now that we've grabbed all the options, if no args remain,
     * append the names of all stuff currently displayed.
     * Otherwise, simply append the remaining args.
     */
    if (i == argc) {
	gd_rt_cmd_len = vp - gd_rt_cmd;
	gd_rt_cmd_len += ged_build_tops(gedp, vp, (const char **)&gd_rt_cmd[args]);
    } else {
	while (i < argc)
	    *vp++ = (char *)argv[i++];
	*vp = 0;
	vp = &gd_rt_cmd[0];
	while (*vp)
	    Tcl_AppendResult((Tcl_Interp *)gedp->ged_interp, *vp++, " ", (char *)NULL);

	Tcl_AppendResult((Tcl_Interp *)gedp->ged_interp, "\n", (char *)NULL);
    }


    bu_process_exec(&p, gd_rt_cmd[0], gd_rt_cmd_len, (const char **)gd_rt_cmd, 0, 0);

    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	for (int pi = 0; pi < gd_rt_cmd_len; pi++) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", gd_rt_cmd[pi]);
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
	bu_free(gd_rt_cmd, "free gd_rt_cmd");
	return GED_ERROR;
    }

    fp = bu_process_open(p, BU_PROCESS_STDIN);

    _ged_rt_set_eye_model(gedp, eye_model);
    _ged_rt_write(gedp, fp, eye_model, -1, NULL);

    bu_process_close(p, BU_PROCESS_STDIN);

    /* create the rtcheck struct */
    BU_GET(rtcp, struct ged_rtcheck);
    rtcp->gedp = gedp;
    rtcp->fp = bu_process_open(p, BU_PROCESS_STDOUT);
    /* Needed on Windows for successful rtcheck drawing data communication */
    setmode(fileno(rtcp->fp), O_BINARY);
    rtcp->vbp = rt_vlblock_init();
    rtcp->vhead = bn_vlblock_find(rtcp->vbp, 0xFF, 0xFF, 0x00);
    rtcp->csize = gedp->ged_gvp->gv_scale * 0.01;
    rtcp->p = p;
    rtcp->read_failed = 0;
    rtcp->draw_read_failed = 0;

    /* create the ged_subprocess container */
    BU_GET(rtcp->rrtp, struct ged_subprocess);
    rtcp->rrtp->p = p;
    rtcp->rrtp->aborted = 0;
    _ged_create_io_handler(&(rtcp->rrtp->chan), p, BU_PROCESS_STDOUT, TCL_READABLE, (void *)rtcp, rtcheck_vector_handler);
    BU_LIST_INIT(&rtcp->rrtp->l);
    BU_LIST_APPEND(&gedp->gd_headSubprocess.l, &rtcp->rrtp->l);

    _ged_create_io_handler(&(rtcp->chan), p, BU_PROCESS_STDERR, TCL_READABLE, (void *)rtcp, rtcheck_output_handler);

    bu_free(gd_rt_cmd, "free gd_rt_cmd");

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
