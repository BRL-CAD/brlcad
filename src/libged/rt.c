/*                         R T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
ged_rt(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    int units_supplied = 0;
    char pstring[32];
    int args;

    const char *bin;
    char rt[256] = {0};

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    args = argc + 7 + 2 + ged_count_tops(gedp);
    gedp->ged_gdp->gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

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
    *vp++ = "-M";

    if (gedp->ged_gvp->gv_perspective > 0) {
	(void)sprintf(pstring, "-p%g", gedp->ged_gvp->gv_perspective);
	*vp++ = pstring;
    }

    for (i=1; i < argc; i++) {
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
	    bu_vls_printf(gedp->ged_result_str, "%s ", *vp++);

	bu_vls_printf(gedp->ged_result_str, "\n");
    }
    (void)_ged_run_rt(gedp);
    bu_free(gedp->ged_gdp->gd_rt_cmd, "free gd_rt_cmd");
    gedp->ged_gdp->gd_rt_cmd = NULL;

    return GED_OK;
}


int
_ged_run_rt(struct ged *gedp)
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
    struct bu_vls line;
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

	for (i=3; i < 20; i++)
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

    bu_vls_init(&line);
    for (i=0; i<gedp->ged_gdp->gd_rt_cmd_len; i++) {
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

    Tcl_CreateChannelHandler(run_rtp->chan,
			     TCL_READABLE,
			     _ged_rt_output_handler,
			     (ClientData)drcdp);

    return 0;
#endif
}


void
_ged_rt_write(struct ged *gedp,
	      FILE *fp,
	      vect_t eye_model)
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    size_t i;
    quat_t quat;
    struct solid *sp;

    (void)fprintf(fp, "viewsize %.15e;\n", gedp->ged_gvp->gv_size);
    quat_mat2quat(quat, gedp->ged_gvp->gv_rotation);
    (void)fprintf(fp, "orientation %.15e %.15e %.15e %.15e;\n", V4ARGS(quat));
    (void)fprintf(fp, "eye_pt %.15e %.15e %.15e;\n",
		  eye_model[X], eye_model[Y], eye_model[Z]);

    (void)fprintf(fp, "start 0; clean;\n");

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    for (i=0;i<sp->s_fullpath.fp_len;i++) {
		DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags &= ~RT_DIR_USED;
	    }
	}

	gdlp = next_gdlp;
    }

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    for (i=0; i<sp->s_fullpath.fp_len; i++) {
		if (!(DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags & RT_DIR_USED)) {
		    struct animate *anp;
		    for (anp = DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_animate; anp;
			 anp=anp->an_forw) {
			db_write_anim(fp, anp);
		    }
		    DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags |= RT_DIR_USED;
		}
	    }
	}

	gdlp = next_gdlp;
    }

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    for (i=0;i< sp->s_fullpath.fp_len;i++) {
		DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags &= ~RT_DIR_USED;
	    }
	}

	gdlp = next_gdlp;
    }
    (void)fprintf(fp, "end;\n");
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
	drcdp->rrtp == (struct ged_run_rt *)NULL ||
	brlcad_interp == (Tcl_Interp *)NULL)
	return;

    run_rtp = drcdp->rrtp;

    /* Get data from rt */
#ifndef _WIN32
    count = read((int)run_rtp->fd, line, RT_MAXLINE);
    if (count <= 0) {
	read_failed = 1;
    }
#else
    if (Tcl_Eof(run_rtp->chan) ||
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
	Tcl_DeleteChannelHandler(run_rtp->chan,
				 _ged_rt_output_handler,
				 (ClientData)drcdp);
	Tcl_Close(brlcad_interp, run_rtp->chan);

	/* wait for the forked process
	 * either EOF has been sent or there was a read error.
	 * there is no need to block indefinately
	 */
	WaitForSingleObject(run_rtp->hProcess, 120);
	/* !!! need to observe implications of being non-infinate
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
	bu_free((genptr_t)run_rtp, "_ged_rt_output_handler: run_rtp");

	bu_free((genptr_t)drcdp, "_ged_rt_output_handler: drcdp");

	return;
    }

    /* for feelgoodedness */
    line[count] = '\0';

    /* handle (i.e., probably log to stderr) the resulting line */
    if (drcdp->gedp->ged_output_handler != (void (*)())0)
	drcdp->gedp->ged_output_handler(drcdp->gedp, line);
    else
	bu_vls_printf(drcdp->gedp->ged_result_str, "%s", line);
}


/**
 *
 */
size_t
ged_count_tops(struct ged *gedp)
{
    struct ged_display_list *gdlp = NULL;
    size_t visibleCount = 0;
    for (BU_LIST_FOR(gdlp, ged_display_list, &gedp->ged_gdp->gd_headDisplay)) {
	visibleCount++;
    }
    return visibleCount;
}


/**
 * G E D _ B U I L D _ T O P S
 *
 * Build a command line vector of the tops of all objects in view.
 */
int
ged_build_tops(struct ged *gedp, char **start, char **end)
{
    struct ged_display_list *gdlp;
    char **vp = start;

    for (BU_LIST_FOR(gdlp, ged_display_list, &gedp->ged_gdp->gd_headDisplay)) {
	if (gdlp->gdl_dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	if ((vp != NULL) && (vp < end))
	    *vp++ = bu_strdup(bu_vls_addr(&gdlp->gdl_path));
	else {
	    bu_vls_printf(gedp->ged_result_str, "libged: ran out of command vector space at %s\n", gdlp->gdl_dp->d_namep);
	    break;
	}
    }

    if ((vp != NULL) && (vp < end)) {
	*vp = (char *) 0;
    }

    return vp-start;
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
	struct ged_display_list *gdlp;
	struct ged_display_list *next_gdlp;
	struct solid *sp;
	int i;
	vect_t direction;
	vect_t extremum[2];
	vect_t minus, plus;    /* vers of this solid's bounding box */

	VSET(eye_model, -gedp->ged_gvp->gv_center[MDX],
	     -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);

	for (i = 0; i < 3; ++i) {
	    extremum[0][i] = INFINITY;
	    extremum[1][i] = -INFINITY;
	}

	gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		minus[X] = sp->s_center[X] - sp->s_size;
		minus[Y] = sp->s_center[Y] - sp->s_size;
		minus[Z] = sp->s_center[Z] - sp->s_size;
		VMIN(extremum[0], minus);
		plus[X] = sp->s_center[X] + sp->s_size;
		plus[Y] = sp->s_center[Y] + sp->s_size;
		plus[Z] = sp->s_center[Z] + sp->s_size;
		VMAX(extremum[1], plus);
	    }

	    gdlp = next_gdlp;
	}

	VMOVEN(direction, gedp->ged_gvp->gv_rotation + 8, 3);
	for (i = 0; i < 3; ++i)
	    if (NEAR_ZERO(direction[i], 1e-10))
		direction[i] = 0.0;
	if ((eye_model[X] >= extremum[0][X]) &&
	    (eye_model[X] <= extremum[1][X]) &&
	    (eye_model[Y] >= extremum[0][Y]) &&
	    (eye_model[Y] <= extremum[1][Y]) &&
	    (eye_model[Z] >= extremum[0][Z]) &&
	    (eye_model[Z] <= extremum[1][Z])) {
	    double t_in;
	    vect_t diag;

	    VSUB2(diag, extremum[1], extremum[0]);
	    t_in = MAGNITUDE(diag);
	    VJOIN1(eye_model, eye_model, t_in, direction);
	}
    }
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
