/*                         R T W I Z A R D . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2013 United States Government as represented by
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
/** @file libged/rtwizard.c
 *
 * The rtwizard command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#include "bio.h"

#include "tcl.h"
#include "cmd.h"
#include "solid.h"

#include "./ged_private.h"


struct _ged_rt_client_data {
    struct ged_run_rt *rrtp;
    struct ged *gedp;
};


int
_ged_run_rtwizard(struct ged *gedp)
{
    int i;
    FILE *fp_in;
#ifndef _WIN32
    int pipe_in[2];
    int pipe_err[2];
#else
    HANDLE pipe_in[2], pipe_inDup;
    HANDLE pipe_err[2], pipe_errDup;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa = {0};
    struct bu_vls line = BU_VLS_INIT_ZERO;
#endif
    struct ged_run_rt *run_rtp;
    struct _ged_rt_client_data *drcdp;
#ifndef _WIN32
    int pid;
    int ret;

    ret = pipe(pipe_in);
    if (ret < 0)
	perror("pipe");
    ret = pipe(pipe_err);
    if (ret < 0)
	perror("pipe");

    if ((pid = fork()) == 0) {
	/* make this a process group leader */
	setpgid(0, 0);

	/* Redirect stdin and stderr */
	(void)close(0);
	ret = dup(pipe_in[0]);
	if (ret < 0)
	    perror("dup");
	(void)close(2);
	ret = dup(pipe_err[1]);
	if (ret < 0)
	    perror("dup");

	/* close pipes */
	(void)close(pipe_in[0]);
	(void)close(pipe_in[1]);
	(void)close(pipe_err[0]);
	(void)close(pipe_err[1]);

	for (i = 3; i < 20; i++)
	    (void)close(i);

	(void)execvp(gedp->ged_gdp->gd_rt_cmd[0], gedp->ged_gdp->gd_rt_cmd);
	perror(gedp->ged_gdp->gd_rt_cmd[0]);
	exit(16);
    }

    /* As parent, send view information down pipe */
    (void)close(pipe_in[0]);
    fp_in = fdopen(pipe_in[1], "w");

    (void)close(pipe_err[1]);

    (void)fclose(fp_in);

    /* must be BU_GET() to match release in _ged_rt_output_handler */
    BU_GET(run_rtp, struct ged_run_rt);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&gedp->ged_gdp->gd_headRunRt.l, &run_rtp->l);

    run_rtp->fd = pipe_err[0];
    run_rtp->pid = pid;

    /* must be BU_GET() to match release in _ged_rt_output_handler */
    BU_GET(drcdp, struct _ged_rt_client_data);
    drcdp->gedp = gedp;
    drcdp->rrtp = run_rtp;

    Tcl_CreateFileHandler(run_rtp->fd,
			  TCL_READABLE,
			  _ged_rt_output_handler,
			  (ClientData)drcdp);

    return 0;

#else
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Create a pipe for the child process's STDOUT. */
    CreatePipe(&pipe_err[0], &pipe_err[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle(GetCurrentProcess(), pipe_err[0],
		    GetCurrentProcess(),  &pipe_errDup ,
		    0,  FALSE,
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_err[0]);

    /* Create a pipe for the child process's STDIN. */
    CreatePipe(&pipe_in[0], &pipe_in[1], &sa, 0);

    /* Duplicate the write handle to the pipe so it is not inherited. */
    DuplicateHandle(GetCurrentProcess(), pipe_in[1],
		    GetCurrentProcess(), &pipe_inDup,
		    0, FALSE,                  /* not inherited */
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_in[1]);


    si.cb = sizeof(STARTUPINFO);
    si.lpReserved = NULL;
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;
    si.lpDesktop = NULL;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput   = pipe_in[0];
    si.hStdOutput  = pipe_err[1];
    si.hStdError   = pipe_err[1];

    for (i = 0; i < gedp->ged_gdp->gd_rt_cmd_len; i++) {
	bu_vls_printf(&line, "\"%s\" ", gedp->ged_gdp->gd_rt_cmd[i]);
    }

    CreateProcess(NULL, bu_vls_addr(&line), NULL, NULL, TRUE,
		  DETACHED_PROCESS, NULL, NULL,
		  &si, &pi);
    bu_vls_free(&line);

    CloseHandle(pipe_in[0]);
    CloseHandle(pipe_err[1]);

    /* As parent, send view information down pipe */
    fp_in = _fdopen(_open_osfhandle((intptr_t)pipe_inDup, _O_TEXT), "wb");

    (void)fclose(fp_in);

    /* must be BU_GET() to match release in _ged_rt_output_handler */
    BU_GET(run_rtp, struct ged_run_rt);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&gedp->ged_gdp->gd_headRunRt.l, &run_rtp->l);

    run_rtp->fd = pipe_errDup;
    run_rtp->hProcess = pi.hProcess;
    run_rtp->pid = pi.dwProcessId;
    run_rtp->aborted=0;
    run_rtp->chan = Tcl_MakeFileChannel(run_rtp->fd, TCL_READABLE);

    /* must be BU_GET() to match release in _ged_rt_output_handler */
    BU_GET(drcdp, struct _ged_rt_client_data);
    drcdp->gedp = gedp;
    drcdp->rrtp = run_rtp;

    Tcl_CreateChannelHandler(run_rtp->chan,
			     TCL_READABLE,
			     _ged_rt_output_handler,
			     (ClientData)drcdp);

    return 0;
#endif
}


int
ged_rtwizard(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    char pstring[32];
    int args;
    quat_t quat;
    vect_t eye_model;
    struct bu_vls perspective_vls = BU_VLS_INIT_ZERO;
    struct bu_vls size_vls = BU_VLS_INIT_ZERO;
    struct bu_vls orient_vls = BU_VLS_INIT_ZERO;
    struct bu_vls eye_vls = BU_VLS_INIT_ZERO;

    const char *bin;
    char rt[256] = {0};
    char rtscript[256] = {0};

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (gedp->ged_gvp->gv_perspective > 0)
	/* btclsh rtwizard --no_gui -perspective p -i db.g --viewsize size --orientation "A B C D} --eye_pt "X Y Z" */
	args = argc + 1 + 1 + 1 + 2 + 2 + 2 + 2 + 2;
    else
	/* btclsh rtwizard --no_gui -i db.g --viewsize size --orientation "A B C D} --eye_pt "X Y Z" */
	args = argc + 1 + 1 + 1 + 2 + 2 + 2 + 2;

    gedp->ged_gdp->gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    bin = bu_brlcad_root("bin", 1);
    if (bin) {
	snprintf(rt, 256, "%s/btclsh", bin);
	snprintf(rtscript, 256, "%s/rtwizard", bin);
    } else {
	snprintf(rt, 256, "btclsh");
	snprintf(rtscript, 256, "rtwizard");
    }

    _ged_rt_set_eye_model(gedp, eye_model);
    quat_mat2quat(quat, gedp->ged_gvp->gv_rotation);

    bu_vls_printf(&size_vls, "%.15e", gedp->ged_gvp->gv_size);
    bu_vls_printf(&orient_vls, "%.15e %.15e %.15e %.15e", V4ARGS(quat));
    bu_vls_printf(&eye_vls, "%.15e %.15e %.15e", V3ARGS(eye_model));

    vp = &gedp->ged_gdp->gd_rt_cmd[0];
    *vp++ = rt;
    *vp++ = rtscript;
    *vp++ = "--no-gui";
    *vp++ = "--viewsize";
    *vp++ = bu_vls_addr(&size_vls);
    *vp++ = "--orientation";
    *vp++ = bu_vls_addr(&orient_vls);
    *vp++ = "--eye_pt";
    *vp++ = bu_vls_addr(&eye_vls);

    if (gedp->ged_gvp->gv_perspective > 0) {
	*vp++ = "--perspective";
	(void)sprintf(pstring, "%g", gedp->ged_gvp->gv_perspective);
	*vp++ = pstring;
    }

    *vp++ = "-i";
    *vp++ = gedp->ged_wdbp->dbip->dbi_filename;

    /* Append all args */
    for (i = 1; i < argc; i++)
	*vp++ = (char *)argv[i];
    *vp = 0;

    /*
     * Accumulate the command string.
     */
    vp = &gedp->ged_gdp->gd_rt_cmd[0];
    while (*vp)
	bu_vls_printf(gedp->ged_result_str, "%s ", *vp++);
    bu_vls_printf(gedp->ged_result_str, "\n");

    gedp->ged_gdp->gd_rt_cmd_len = vp - gedp->ged_gdp->gd_rt_cmd;
    (void)_ged_run_rtwizard(gedp);
    bu_free(gedp->ged_gdp->gd_rt_cmd, "free gd_rt_cmd");
    gedp->ged_gdp->gd_rt_cmd = NULL;

    bu_vls_free(&perspective_vls);
    bu_vls_free(&size_vls);
    bu_vls_free(&orient_vls);
    bu_vls_free(&eye_vls);

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
