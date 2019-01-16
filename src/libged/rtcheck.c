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
#include "bresource.h"

#include "bu/app.h"

#include "./ged_private.h"

struct ged_rtcheck {
    Tcl_Channel chan;
    FILE *fp;
    struct bn_vlblock *vbp;
    struct bu_list *vhead;
    double csize;
    struct ged *gedp;
    struct bu_process *p;
    Tcl_Interp *interp;
};

struct rtcheck_output {
    Tcl_Channel chan;
    struct ged *gedp;
    struct bu_process *p;
    Tcl_Interp *interp;
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


static void
rtcheck_vector_handler(ClientData clientData, int UNUSED(mask))
{
    int value;
    struct ged_rtcheck *rtcp = (struct ged_rtcheck *)clientData;

    /* Get vector output from rtcheck */
    if (feof(rtcp->fp) || ((value = getc(rtcp->fp)) == EOF)) {
#ifndef _WIN32
	int *fdp = (int *)bu_process_fd(rtcp->p, 1);
	bu_process_close(rtcp->p, 1);
	Tcl_DeleteFileHandler(*fdp);
#else
	Tcl_DeleteChannelHandler((Tcl_Channel)rtcp->chan,
				 rtcheck_vector_handler,
				 (ClientData)rtcp);
	Tcl_Close(rtcp->interp, (Tcl_Channel)rtcp->chan);
#endif
	bu_process_close(rtcp->p, 1);

	dl_set_flag(rtcp->gedp->ged_gdp->gd_headDisplay, DOWN);

	/* Add overlay */
	_ged_cvt_vlblock_to_solids(rtcp->gedp, rtcp->vbp, "OVERLAPS", 0);
	bn_vlblock_free(rtcp->vbp);

	/* wait for the forked process */
	(void)bu_process_wait(NULL, rtcp->p, 0);

	BU_PUT(rtcp, struct ged_rtcheck);

	return;
    }

    (void)rt_process_uplot_value(&rtcp->vhead,
				 rtcp->vbp,
				 rtcp->fp,
				 value,
				 rtcp->csize,
				 rtcp->gedp->ged_gdp->gd_uplotOutputMode);
}

static void
rtcheck_output_handler(ClientData clientData, int UNUSED(mask))
{
    int read_failed = 0;
    int count;
    char line[RT_MAXLINE] = {0};
    struct rtcheck_output *rtcop = (struct rtcheck_output *)clientData;

    /* Get textual output from rtcheck */
    if (bu_process_read((char *)line, &count, rtcop->p, 2, RT_MAXLINE) <= 0) {
	read_failed = 1;
    }
    if (read_failed) {
#ifndef _WIN32
	int *fdp = (int *)bu_process_fd(rtcop->p, 2);
	Tcl_DeleteFileHandler(*fdp);
	close(*fdp);
#else
	Tcl_DeleteChannelHandler((Tcl_Channel)rtcop->chan,
				 rtcheck_output_handler,
				 (ClientData)rtcop);
	Tcl_Close(rtcop->interp, (Tcl_Channel)rtcop->chan);
#endif

	if (rtcop->gedp->ged_gdp->gd_rtCmdNotify != (void (*)(int))0)
	    rtcop->gedp->ged_gdp->gd_rtCmdNotify(0);

	BU_PUT(rtcop, struct rtcheck_output);
	return;
    }

    line[count] = '\0';
    if (rtcop->gedp->ged_output_handler != (void (*)(struct ged *, char *))0)
	rtcop->gedp->ged_output_handler(rtcop->gedp, line);
    else
	bu_vls_printf(rtcop->gedp->ged_result_str, "%s", line);
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
    struct rtcheck_output *rtcop;
    vect_t eye_model;
    struct bu_process *p;

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
    gedp->ged_gdp->gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gedp->ged_gdp->gd_rt_cmd[0];
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
	gedp->ged_gdp->gd_rt_cmd_len = vp - gedp->ged_gdp->gd_rt_cmd;
	gedp->ged_gdp->gd_rt_cmd_len += ged_build_tops(gedp, vp, &gedp->ged_gdp->gd_rt_cmd[args]);
    } else {
	while (i < argc)
	    *vp++ = (char *)argv[i++];
	*vp = 0;
	vp = &gedp->ged_gdp->gd_rt_cmd[0];
	while (*vp)
	    Tcl_AppendResult((Tcl_Interp *)gedp->ged_interp, *vp++, " ", (char *)NULL);

	Tcl_AppendResult((Tcl_Interp *)gedp->ged_interp, "\n", (char *)NULL);
    }

    bu_process_exec(&p, gedp->ged_gdp->gd_rt_cmd[0], gedp->ged_gdp->gd_rt_cmd_len, (const char **)gedp->ged_gdp->gd_rt_cmd, 0, 0);

    /* As parent, send view information down pipe */
    fp = bu_process_open(p, 0);
    _ged_rt_set_eye_model(gedp, eye_model);
    _ged_rt_write(gedp, fp, eye_model, -1, NULL);
    bu_process_close(p, 0);

    BU_GET(rtcp, struct ged_rtcheck);

    /* initialize the rtcheck struct */
    rtcp->fp = bu_process_open(p, 1);
    rtcp->vbp = rt_vlblock_init();
    rtcp->vhead = bn_vlblock_find(rtcp->vbp, 0xFF, 0xFF, 0x00);
    rtcp->csize = gedp->ged_gvp->gv_scale * 0.01;
    rtcp->gedp = gedp;
    rtcp->p = p;
    rtcp->interp = (Tcl_Interp *)gedp->ged_interp;

    BU_GET(rtcop, struct rtcheck_output);
    rtcop->p = p;
    rtcop->gedp = gedp;
    rtcop->interp = (Tcl_Interp *)gedp->ged_interp;
#ifndef _WIN32
    /* file handlers */
    {
	int *fdp;
	fdp = (int *)bu_process_fd(p, 1);
	Tcl_CreateFileHandler(*fdp, TCL_READABLE,
		rtcheck_vector_handler, (ClientData)rtcp);
	fdp = (int *)bu_process_fd(p, 2);
	Tcl_CreateFileHandler(*fdp, TCL_READABLE,
		rtcheck_output_handler, (ClientData)rtcop);
    }
#else
    {
	HANDLE *fdp;
	fdp = (HANDLE *)bu_process_fd(p, 1);
	rtcp->chan = (void *)Tcl_MakeFileChannel(*fdp, TCL_READABLE);
	Tcl_CreateChannelHandler((Tcl_Channel)rtcp->chan, TCL_READABLE,
		rtcheck_vector_handler, (ClientData)rtcp);
	fdp = (HANDLE *)bu_process_fd(p, 2);
	rtcop->chan = (void *)Tcl_MakeFileChannel(*fdp, TCL_READABLE);
	Tcl_CreateChannelHandler((Tcl_Channel)rtcop->chan, TCL_READABLE,
		rtcheck_output_handler, (ClientData)rtcop);
    }
#endif

    bu_free(gedp->ged_gdp->gd_rt_cmd, "free gd_rt_cmd");
    gedp->ged_gdp->gd_rt_cmd = NULL;

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
