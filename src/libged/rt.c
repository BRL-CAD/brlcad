/*                         R T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2018 United States Government as represented by
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

#include "tcl.h"

#include "bu/app.h"
#include "bu/env.h"
#include "bu/vls.h"

#include "./ged_private.h"

#if defined(HAVE_FDOPEN) && !defined(HAVE_DECL_FDOPEN)
extern FILE *fdopen(int fd, const char *mode);
#endif


struct _ged_rt_client_data {
    struct ged_run_rt *rrtp;
    struct ged *gedp;
    char *tmpfil;
};


void
_ged_rt_write(struct ged *gedp,
	      FILE *fp,
	      vect_t eye_model)
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
_ged_rt_output_handler(ClientData clientData, int UNUSED(mask))
{
    struct _ged_rt_client_data *drcdp = (struct _ged_rt_client_data *)clientData;
    struct ged_run_rt *run_rtp;
#ifndef _WIN32
    int count = 0;
#else
    DWORD count = 0;
#endif
    int read_failed = 0;
    char line[RT_MAXLINE+1] = {0};

    if (drcdp == (struct _ged_rt_client_data *)NULL ||
	drcdp->gedp == (struct ged *)NULL ||
	drcdp->rrtp == (struct ged_run_rt *)NULL)
	return;

    run_rtp = drcdp->rrtp;

    /* Get data from rt */
#ifndef _WIN32
    count = read((int)run_rtp->fd, line, RT_MAXLINE);
    if (count <= 0) {
	read_failed = 1;
    }
#else
    if (Tcl_Eof((Tcl_Channel)run_rtp->chan) ||
	(!ReadFile(run_rtp->fd, line, RT_MAXLINE, &count, 0))) {
	read_failed = 1;
    }
#endif

    /* sanity clamping */
    if (count < 0) {
	perror("READ ERROR");
	count = 0;
    } else if (count > RT_MAXLINE) {
	count = RT_MAXLINE;
    }

    if (read_failed) {
	int aborted;

	/* was it aborted? */
#ifndef _WIN32
	int rpid;
	int retcode = 0;

	Tcl_DeleteFileHandler(run_rtp->fd);
	close(run_rtp->fd);

	/* wait for the forked process */
	while ((rpid = wait(&retcode)) != run_rtp->pid && rpid != -1);

	aborted = run_rtp->aborted;
#else
	DWORD retcode = 0;
	Tcl_DeleteChannelHandler((Tcl_Channel)run_rtp->chan,
				 _ged_rt_output_handler,
				 (ClientData)drcdp);
	Tcl_Close((Tcl_Interp *)drcdp->gedp->ged_interp, (Tcl_Channel)run_rtp->chan);

	/* wait for the forked process
	 * either EOF has been sent or there was a read error.
	 * there is no need to block indefinitely
	 */
	WaitForSingleObject(run_rtp->hProcess, 120);
	/* !!! need to observe implications of being non-infinite
	 * WaitForSingleObject(run_rtp->hProcess, INFINITE);
	 */

	if (GetLastError() == ERROR_PROCESS_ABORTED) {
	    run_rtp->aborted = 1;
	}

	GetExitCodeProcess(run_rtp->hProcess, &retcode);
	/* may be useful to try pr_wait_status() here */

	aborted = run_rtp->aborted;
#endif

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
	BU_PUT(run_rtp, struct ged_run_rt);
	if (drcdp->tmpfil) {
	    bu_file_delete(drcdp->tmpfil);
	    bu_free(drcdp->tmpfil, "rt temp file");
	}
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
_ged_run_rt(struct ged *gedp, char *tmpfil)
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
    vect_t eye_model;
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

    _ged_rt_set_eye_model(gedp, eye_model);
    _ged_rt_write(gedp, fp_in, eye_model);
    (void)fclose(fp_in);

    BU_GET(run_rtp, struct ged_run_rt);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&gedp->ged_gdp->gd_headRunRt.l, &run_rtp->l);

    run_rtp->fd = pipe_err[0];
    run_rtp->pid = pid;

    BU_GET(drcdp, struct _ged_rt_client_data);
    drcdp->gedp = gedp;
    drcdp->rrtp = run_rtp;
    if (tmpfil) {
	drcdp->tmpfil = bu_strdup(tmpfil);
    }

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
	bu_vls_printf(&line, "%s ", gedp->ged_gdp->gd_rt_cmd[i]);
    }

    CreateProcess(NULL, bu_vls_addr(&line), NULL, NULL, TRUE,
		  DETACHED_PROCESS, NULL, NULL,
		  &si, &pi);
    bu_vls_free(&line);

    CloseHandle(pipe_in[0]);
    CloseHandle(pipe_err[1]);

    /* As parent, send view information down pipe */
    fp_in = _fdopen(_open_osfhandle((intptr_t)pipe_inDup, _O_TEXT), "wb");

    _ged_rt_set_eye_model(gedp, eye_model);
    _ged_rt_write(gedp, fp_in, eye_model);
    (void)fclose(fp_in);

    BU_GET(run_rtp, struct ged_run_rt);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&gedp->ged_gdp->gd_headRunRt.l, &run_rtp->l);

    run_rtp->fd = pipe_errDup;
    run_rtp->hProcess = pi.hProcess;
    run_rtp->pid = pi.dwProcessId;
    run_rtp->aborted=0;
    run_rtp->chan = Tcl_MakeFileChannel(run_rtp->fd, TCL_READABLE);

    BU_GET(drcdp, struct _ged_rt_client_data);
    drcdp->gedp = gedp;
    drcdp->rrtp = run_rtp;

    Tcl_CreateChannelHandler((Tcl_Channel)run_rtp->chan,
			     TCL_READABLE,
			     _ged_rt_output_handler,
			     (ClientData)drcdp);

    return 0;
#endif
}


size_t
ged_count_tops(struct ged *gedp)
{
    struct display_list *gdlp = NULL;
    size_t visibleCount = 0;
    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
	visibleCount++;
    }
    return visibleCount;
}


/**
 * Build a command line vector of the tops of all objects in view.
 */
int
ged_build_tops(struct ged *gedp, char **start, char **end)
{
    struct display_list *gdlp;
    char **vp = start;

    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
	if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	if ((vp != NULL) && (vp < end))
	    *vp++ = bu_strdup(bu_vls_addr(&gdlp->dl_path));
	else {
	    bu_vls_printf(gedp->ged_result_str, "libged: ran out of command vector space at %s\n", ((struct directory *)gdlp->dl_dp)->d_namep);
	    break;
	}
    }

    if ((vp != NULL) && (vp < end)) {
	*vp = (char *) 0;
    }

    return vp-start;
}

int
_ged_rt_tmpfile_uniq(struct bu_vls *f, void *UNUSED(data))
{
    if (!f || bu_file_exists(bu_vls_addr(f), NULL)) {
	return 0;
    }
    return 1;
}

int
_ged_rt_tmpfile(struct bu_vls *rstr)
{
    const char *tmpf = NULL;
    struct bu_vls pidstr = BU_VLS_INIT_ZERO;
    unsigned long int cpid = bu_pid();
    if (!rstr) {
	return -1;
    }
    bu_vls_sprintf(&pidstr, "BRL-CAD_ged_rt_%ld-0", cpid);
    tmpf = bu_dir(NULL, 0, BU_DIR_TEMP, bu_vls_addr(&pidstr), NULL);
    bu_vls_sprintf(rstr, "%s", tmpf);
    return bu_vls_incr(rstr, NULL, "0:0:0:0", &_ged_rt_tmpfile_uniq, NULL);
}

int
ged_rt(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    int units_supplied = 0;
    char pstring[32];
    int args;
    struct bu_vls tmpfil = BU_VLS_INIT_ZERO;
    FILE *objfp = NULL;

    const char *bin;
    char rt[256] = {0};

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* see if we can get a tmp file - if so, use that to feed rt the object
     * list to avoid any potential problems with long command lines - see
     * https://support.microsoft.com/en-us/help/830473/command-prompt-cmd-exe-command-line-string-limitation */
    if (!_ged_rt_tmpfile(&tmpfil)) {
	objfp = fopen(bu_vls_addr(&tmpfil), "w");
    }

    /* Make argv array for command definition (used for execvp - for
     * other platforms a string will be built up from the array) */
    if (objfp) {
	args = argc + 9 + 2;
    } else {
	args = argc + 7 + 2 + ged_count_tops(gedp);
    }
    gedp->ged_gdp->gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    /* Get full path to the correct rt executable */
    bin = bu_brlcad_root("bin", 1);
    if (bin) {
#ifdef _WIN32
	snprintf(rt, 256, "\"%s/%s\"", bin, argv[0]);
#else
	snprintf(rt, 256, "%s/%s", bin, argv[0]);
#endif
    }
    vp = &gedp->ged_gdp->gd_rt_cmd[0];
    *vp++ = rt;

    /* If we're using an obj file, let rt know */
    if (objfp) {
	*vp++ = "-I";
	*vp++ = bu_strdup(bu_vls_addr(&tmpfil));
    }

    /* View matrix and perspective related options */
    *vp++ = "-M";

    if (gedp->ged_gvp->gv_perspective > 0) {
	(void)sprintf(pstring, "-p%g", gedp->ged_gvp->gv_perspective);
	*vp++ = pstring;
    }

    /* Units */
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


    /* Add database filename */
    /* XXX why is this different for win32 only? */
#ifdef _WIN32
    {
	char buf[512];

	snprintf(buf, 512, "\"%s\"", gedp->ged_wdbp->dbip->dbi_filename);
	*vp++ = buf;
    }
#else
    *vp++ = gedp->ged_wdbp->dbip->dbi_filename;
#endif

    /*
     * Now that we've grabbed all the options, if no args remain,
     * append the names of all objects currently displayed.
     * Otherwise, simply append the remaining args.
     *
     * Once we have the object list, do the run.
     */
    if (objfp) {
	gedp->ged_gdp->gd_rt_cmd_len = vp - gedp->ged_gdp->gd_rt_cmd;
	if (i == argc) {
	    struct display_list *gdlp;
	    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
		if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR) {
		    continue;
		}
		fprintf(objfp, "%s", bu_vls_addr(&gdlp->dl_path));
		fprintf(objfp, "\n");
	    }
	} else {
	    while (i < argc) {
		fprintf(objfp, "%s", argv[i++]);
		fprintf(objfp, "\n");
	    }
	}
	(void)fclose(objfp);
	(void)_ged_run_rt(gedp, bu_vls_addr(&tmpfil));
    } else {
	if (i == argc) {
	    gedp->ged_gdp->gd_rt_cmd_len = vp - gedp->ged_gdp->gd_rt_cmd;
	    gedp->ged_gdp->gd_rt_cmd_len += ged_build_tops(gedp, vp, &gedp->ged_gdp->gd_rt_cmd[args]);
	} else {
	    while (i < argc) {
		*vp++ = (char *)argv[i++];
	    }
	    *vp = 0;
	    vp = &gedp->ged_gdp->gd_rt_cmd[0];
	    while (*vp) {
		bu_vls_printf(gedp->ged_result_str, "%s ", *vp++);
	    }

	    bu_vls_printf(gedp->ged_result_str, "\n");
	}
	/* Check the command length if we're listing objects on the command
	 * line and we know what the OS specific limit is - too long and we
	 * need to abort with an informative error message rather than silently
	 * attempting (and failing) to run. */
	if (bu_cmdline_arg_max() > 0) {
	    struct bu_vls test_cmd_line = BU_VLS_INIT_ZERO;
	    for (i = 0; i < gedp->ged_gdp->gd_rt_cmd_len; i++) {
		bu_vls_printf(&test_cmd_line, "%s ", gedp->ged_gdp->gd_rt_cmd[i]);
	    }
	    if (bu_vls_strlen(&test_cmd_line) > (unsigned long int)bu_cmdline_arg_max()) {
		bu_vls_printf(gedp->ged_result_str, "generated rt command line is longer than allowed platform maximum (%ld)\n", bu_cmdline_arg_max());
		bu_vls_free(&test_cmd_line);
		return GED_ERROR;
	    }
	    bu_vls_free(&test_cmd_line);
	}
	(void)_ged_run_rt(gedp, NULL);
    }
    bu_vls_free(&tmpfil);
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
