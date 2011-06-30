/*                          N I R T . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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
#include <string.h>
#include <math.h>
#include <signal.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* For struct timeval */
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "bio.h"

#include "tcl.h"
#include "bu.h"
#include "bn.h"
#include "cmd.h"
#include "vmath.h"
#include "solid.h"
#include "dg.h"

#include "./qray.h"
#include "./ged_private.h"


/* defined in draw.c */
extern void _ged_cvt_vlblock_to_solids(struct ged *gedp, struct bn_vlblock *vbp, char *name, int copy);


/**
 * F _ N I R T
 *
 * Invoke nirt with the current view & stuff
 */
int
ged_nirt(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp = NULL;
    struct ged_display_list *next_gdlp = NULL;
    char **vp = NULL;
    FILE *fp_in = NULL;
    FILE *fp_out = NULL;
    FILE *fp_err = NULL;
    int ret;
#ifndef _WIN32
    int pid = 0;
    int rpid = 0;
    int retcode = 0;
    int pipe_in[2] = {0, 0};
    int pipe_out[2] = {0, 0};
    int pipe_err[2] = {0, 0};
#else
    HANDLE pipe_in[2], pipe_inDup;
    HANDLE pipe_out[2], pipe_outDup;
    HANDLE pipe_err[2], pipe_errDup;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    char name[1024] = {0};
    char line1[2048] = {0};
    size_t rem = 2048;
#endif
    int use_input_orig = 0;
    vect_t center_model;
    vect_t dir;
    vect_t cml;
    int i = 9;
    struct solid *sp = NULL;
    char line[RT_MAXLINE] = {0};
    char *val = NULL;
    struct bu_vls o_vls;
    struct bu_vls p_vls;
    struct bu_vls t_vls;
    struct bn_vlblock *vbp = NULL;
    struct qray_dataList *ndlp = NULL;
    struct qray_dataList HeadQRayData;

    const char *bin = NULL;
    char nirt[256] = {0};
    int args;

    /* for bu_fgets space trimming */
    struct bu_vls v;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    args = argc + 20 + 2 + ged_count_tops(gedp);
    gedp->ged_gdp->gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    bin = bu_brlcad_root("bin", 1);
    if (bin) {
#ifdef _WIN32
	/* FIXME: is this really necessary? can we wrap command in
	 * quotes for all platforms?
	 */
	snprintf(nirt, 256, "\"%s/%s\"", bin, argv[0]);
#else
	snprintf(nirt, 256, "%s/%s", bin, argv[0]);
#endif
    }

    vp = &gedp->ged_gdp->gd_rt_cmd[0];
    *vp++ = nirt;

    /* swipe x, y, z off the end if present */
    if (argc > 3) {
	if (sscanf(argv[argc-3], "%lf", &center_model[X]) == 1 &&
	    sscanf(argv[argc-2], "%lf", &center_model[Y]) == 1 &&
	    sscanf(argv[argc-1], "%lf", &center_model[Z]) == 1) {
	    use_input_orig = 1;
	    argc -= 3;
	    VSCALE(center_model, center_model, gedp->ged_wdbp->dbip->dbi_local2base);
	}
    }

    /* Calculate point from which to fire ray */
    if (!use_input_orig) {
	VSET(center_model, -gedp->ged_gvp->gv_center[MDX],
	     -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);
    }

    VSCALE(cml, center_model, gedp->ged_wdbp->dbip->dbi_base2local);
    VMOVEN(dir, gedp->ged_gvp->gv_rotation + 8, 3);
    VSCALE(dir, dir, -1.0);

    bu_vls_init(&p_vls);
    bu_vls_printf(&p_vls, "xyz %lf %lf %lf;",
		  cml[X], cml[Y], cml[Z]);
    bu_vls_printf(&p_vls, "dir %lf %lf %lf; s",
		  dir[X], dir[Y], dir[Z]);

    i = 0;
    if (DG_QRAY_GRAPHICS(gedp->ged_gdp)) {

	*vp++ = "-e";
	*vp++ = DG_QRAY_FORMAT_NULL;

	/* first ray ---- returns partitions */
	*vp++ = "-e";
	*vp++ = DG_QRAY_FORMAT_P;

	/* ray start, direction, and 's' command */
	*vp++ = "-e";
	*vp++ = bu_vls_addr(&p_vls);

	/* second ray ---- returns overlaps */
	*vp++ = "-e";
	*vp++ = DG_QRAY_FORMAT_O;

	/* ray start, direction, and 's' command */
	*vp++ = "-e";
	*vp++ = bu_vls_addr(&p_vls);

	if (DG_QRAY_TEXT(gedp->ged_gdp)) {
	    char *cp;
	    int count = 0;

	    bu_vls_init(&o_vls);

	    /* get 'r' format now; prepend its' format string with a newline */
	    val = bu_vls_addr(&gedp->ged_gdp->gd_qray_fmts[0].fmt);

	    /* find first '"' */
	    while (*val != '"' && *val != '\0')
		++val;

	    if (*val == '\0')
		goto done;
	    else
		++val;	    /* skip first '"' */

	    /* find last '"' */
	    cp = (char *)strrchr(val, '"');

	    if (cp != (char *)NULL) /* found it */
		count = cp - val;

	done:
	    if (*val == '\0')
		bu_vls_printf(&o_vls, " fmt r \"\\n\" ");
	    else {
		bu_vls_printf(&o_vls, " fmt r \"\\n%*s\" ", count, val);

		if (count)
		    val += count + 1;
		bu_vls_printf(&o_vls, "%s", val);
	    }

	    i = 1;

	    *vp++ = "-e";
	    *vp++ = bu_vls_addr(&o_vls);
	}
    }

    if (DG_QRAY_TEXT(gedp->ged_gdp)) {

	bu_vls_init(&t_vls);

	/* load vp with formats for printing */
	for (; gedp->ged_gdp->gd_qray_fmts[i].type != (char)0; ++i)
	    bu_vls_printf(&t_vls, "fmt %c %s; ",
			  gedp->ged_gdp->gd_qray_fmts[i].type,
			  bu_vls_addr(&gedp->ged_gdp->gd_qray_fmts[i].fmt));

	*vp++ = "-e";
	*vp++ = bu_vls_addr(&t_vls);

	/* nirt does not like the trailing ';' */
	bu_vls_trunc(&t_vls, -2);
    }

    /* include nirt script string */
    if (bu_vls_strlen(&gedp->ged_gdp->gd_qray_script)) {
	*vp++ = "-e";
	*vp++ = bu_vls_addr(&gedp->ged_gdp->gd_qray_script);
    }

    *vp++ = "-e";
    *vp++ = bu_vls_addr(&p_vls);

    for (i=1; i < argc; i++)
	*vp++ = (char *)argv[i];
    *vp++ = gedp->ged_wdbp->dbip->dbi_filename;

    gedp->ged_gdp->gd_rt_cmd_len = vp - gedp->ged_gdp->gd_rt_cmd;

    /* Note - ged_build_tops sets the last vp to (char *)0 */
    gedp->ged_gdp->gd_rt_cmd_len += ged_build_tops(gedp, vp, &gedp->ged_gdp->gd_rt_cmd[args]);

    if (gedp->ged_gdp->gd_qray_cmd_echo) {
	/* Print out the command we are about to run */
	vp = &gedp->ged_gdp->gd_rt_cmd[0];

	while (*vp)
	    bu_vls_printf(gedp->ged_result_str, "%s ", *vp++);

	bu_vls_printf(gedp->ged_result_str, "\n");
    }

    if (use_input_orig) {
	bu_vls_printf(gedp->ged_result_str, "\nFiring from (%lf, %lf, %lf)...\n",
		      center_model[X], center_model[Y], center_model[Z]);
    } else
	bu_vls_printf(gedp->ged_result_str, "\nFiring from view center...\n");

#ifndef _WIN32
    ret = pipe(pipe_in);
    if (ret < 0)
	perror("pipe");
    ret = pipe(pipe_out);
    if (ret < 0)
	perror("pipe");
    ret = pipe(pipe_err);
    if (ret < 0)
	perror("pipe");

    (void)signal(SIGINT, SIG_IGN);
    if ((pid = fork()) == 0) {
	/* Redirect stdin, stdout, stderr */
	(void)close(0);
	ret = dup(pipe_in[0]);
	if (ret < 0)
	    perror("dup");
	(void)close(1);
	ret = dup(pipe_out[1]);
	if (ret < 0)
	    perror("dup");
	(void)close(2);
	ret = dup (pipe_err[1]);
	if (ret < 0)
	    perror("dup");

	/* close pipes */
	(void)close(pipe_in[0]);
	(void)close(pipe_in[1]);
	(void)close(pipe_out[0]);
	(void)close(pipe_out[1]);
	(void)close(pipe_err[0]);
	(void)close(pipe_err[1]);
	for (i=3; i < 20; i++)
	    (void)close(i);
	(void)signal(SIGINT, SIG_DFL);
	(void)execvp(gedp->ged_gdp->gd_rt_cmd[0], gedp->ged_gdp->gd_rt_cmd);
	perror (gedp->ged_gdp->gd_rt_cmd[0]);
	exit(16);
    }

    /* use fp_in to feed view info to nirt */
    (void)close(pipe_in[0]);
    fp_in = fdopen(pipe_in[1], "w");

    /* use fp_out to read back the result */
    (void)close(pipe_out[1]);
    fp_out = fdopen(pipe_out[0], "r");

    /* use fp_err to read any error messages */
    (void)close(pipe_err[1]);
    fp_err = fdopen(pipe_err[0], "r");

    /* send quit command to nirt */
    ret = fwrite("q\n", 1, 2, fp_in);
    if (ret != 2)
	perror("fwrite");
    (void)fclose(fp_in);

#else
    memset((void *)&si, 0, sizeof(STARTUPINFO));
    memset((void *)&pi, 0, sizeof(PROCESS_INFORMATION));
    memset((void *)&sa, 0, sizeof(SECURITY_ATTRIBUTES));

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Create a pipe for the child process's STDOUT. */
    CreatePipe(&pipe_out[0], &pipe_out[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle(GetCurrentProcess(), pipe_out[0],
		    GetCurrentProcess(),  &pipe_outDup ,
		    0,  FALSE,
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_out[0]);

    /* Create a pipe for the child process's STDERR. */
    CreatePipe(&pipe_err[0], &pipe_err[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle(GetCurrentProcess(), pipe_err[0],
		    GetCurrentProcess(),  &pipe_errDup ,
		    0,  FALSE,
		    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_err[0]);

    /* The steps for redirecting child process's STDIN:
     * 1.  Save current STDIN, to be restored later.
     * 2.  Create anonymous pipe to be STDIN for child process.
     * 3.  Set STDIN of the parent to be the read handle to the
     *     pipe, so it is inherited by the child process.
     * 4.  Create a noninheritable duplicate of the write handle,
     *     and close the inheritable write handle.
     */

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
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput   = pipe_in[0];
    si.hStdOutput  = pipe_out[1];
    si.hStdError   = pipe_err[1];
    si.wShowWindow = SW_HIDE;

    snprintf(line1, rem, "%s ", gedp->ged_gdp->gd_rt_cmd[0]);
    rem -= strlen(line1) - 1;

    for (i=1; i<gedp->ged_gdp->gd_rt_cmd_len; i++) {
	/* skip commands */
	if (!strcmp(gedp->ged_gdp->gd_rt_cmd[i], "-e"))
	    ++i;
	else {
	    /* append other arguments (i.e. options, file and obj(s)) */
	    snprintf(name, 1024, "\"%s\" ", gedp->ged_gdp->gd_rt_cmd[i]);
	    if (rem - strlen(name) < 1) {
		bu_log("Ran out of buffer space!");
		bu_free(gedp->ged_gdp->gd_rt_cmd, "free gd_rt_cmd");
		gedp->ged_gdp->gd_rt_cmd = NULL;
		return TCL_ERROR;
	    }
	    bu_strlcat(line1, name, sizeof(line1));
	    rem -= strlen(name);
	}
    }

    CreateProcess(NULL, line1, NULL, NULL, TRUE,
		  DETACHED_PROCESS, NULL, NULL,
		  &si, &pi);

    /* use fp_in to feed view info to nirt */
    CloseHandle(pipe_in[0]);
    fp_in = _fdopen(_open_osfhandle((intptr_t)pipe_inDup, _O_TEXT), "w");

    /* send commands down the pipe */
    for (i=1; i<gedp->ged_gdp->gd_rt_cmd_len-2; i++)
	if (strstr(gedp->ged_gdp->gd_rt_cmd[i], "-e") != NULL)
	    fprintf(fp_in, "%s\n", gedp->ged_gdp->gd_rt_cmd[++i]);

    /* use fp_out to read back the result */
    CloseHandle(pipe_out[1]);
    fp_out = _fdopen(_open_osfhandle((intptr_t)pipe_outDup, _O_TEXT), "r");

    /* use fp_err to read any error messages */
    CloseHandle(pipe_err[1]);
    fp_err = _fdopen(_open_osfhandle((intptr_t)pipe_errDup, _O_TEXT), "r");

    /* send quit command to nirt */
    fwrite("q\n", 1, 2, fp_in);
    (void)fclose(fp_in);

#endif

    bu_vls_init(&v);

    bu_vls_free(&p_vls);   /* use to form "partition" part of nirt command above */
    if (DG_QRAY_GRAPHICS(gedp->ged_gdp)) {

	if (DG_QRAY_TEXT(gedp->ged_gdp))
	    bu_vls_free(&o_vls); /* used to form "overlap" part of nirt command above */

	BU_LIST_INIT(&HeadQRayData.l);

	/* handle partitions */
	while (bu_fgets(line, RT_MAXLINE, fp_out) != (char *)NULL) {
	    bu_vls_trunc(&v, 0);
	    bu_vls_strcpy(&v, line);
	    bu_vls_trimspace(&v);

	    if (line[0] == '\n' || line[0] == '\r') {
		bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&v));
		break;
	    }

	    BU_GETSTRUCT(ndlp, qray_dataList);
	    BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	    ret = sscanf(bu_vls_addr(&v), "%le %le %le %le", &ndlp->x_in, &ndlp->y_in, &ndlp->z_in, &ndlp->los);
	    if (ret != 4) {
		bu_log("WARNING: Unexpected nirt line [%s]\nExpecting four numbers.\n", bu_vls_addr(&v));
		break;
	    }
	}

	vbp = rt_vlblock_init();
	qray_data_to_vlist(gedp, vbp, &HeadQRayData, dir, 0);
	bu_list_free(&HeadQRayData.l);
	_ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_addr(&gedp->ged_gdp->gd_qray_basename), 0);
	rt_vlblock_free(vbp);

	/* handle overlaps */
	while (bu_fgets(line, RT_MAXLINE, fp_out) != (char *)NULL) {
	    bu_vls_trunc(&v, 0);
	    bu_vls_strcpy(&v, line);
	    bu_vls_trimspace(&v);

	    if (line[0] == '\n' || line[0] == '\r') {
		bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&v));
		break;
	    }

	    BU_GETSTRUCT(ndlp, qray_dataList);
	    BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	    ret = sscanf(bu_vls_addr(&v), "%le %le %le %le",
			 &ndlp->x_in, &ndlp->y_in, &ndlp->z_in, &ndlp->los);
	    if (ret != 4) {
		bu_log("WARNING: Unexpected nirt line [%s]\nExpecting four numbers.\n", bu_vls_addr(&v));
		break;
	    }
	}
	bu_vls_free(&v);

	vbp = rt_vlblock_init();
	qray_data_to_vlist(gedp, vbp, &HeadQRayData, dir, 1);
	bu_list_free(&HeadQRayData.l);
	_ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_addr(&gedp->ged_gdp->gd_qray_basename), 0);
	rt_vlblock_free(vbp);
    }

    if (DG_QRAY_TEXT(gedp->ged_gdp)) {
	bu_vls_free(&t_vls);

	while (bu_fgets(line, RT_MAXLINE, fp_out) != (char *)NULL) {
	    bu_vls_trunc(&v, 0);
	    bu_vls_strcpy(&v, line);
	    bu_vls_trimspace(&v);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&v));
	}
    }

    (void)fclose(fp_out);

    while (bu_fgets(line, RT_MAXLINE, fp_err) != (char *)NULL) {
	bu_vls_trunc(&v, 0);
	bu_vls_strcpy(&v, line);
	bu_vls_trimspace(&v);
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&v));
    }
    (void)fclose(fp_err);

    bu_vls_free(&v);

#ifndef _WIN32

    /* Wait for program to finish */
    while ((rpid = wait(&retcode)) != pid && rpid != -1)
	;	/* NULL */

    if (retcode != 0)
	_ged_wait_status(gedp->ged_result_str, retcode);
#else
    /* Wait for program to finish */
    WaitForSingleObject(pi.hProcess, INFINITE);

#endif

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid)
	    sp->s_wflag = DOWN;

	gdlp = next_gdlp;
    }

    bu_free(gedp->ged_gdp->gd_rt_cmd, "free gd_rt_cmd");
    gedp->ged_gdp->gd_rt_cmd = NULL;

    return GED_OK;
}


int
ged_vnirt(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int status;
    fastf_t sf = 1.0 * DG_INV_GED;
    vect_t view_ray_orig;
    vect_t center_model;
    struct bu_vls x_vls;
    struct bu_vls y_vls;
    struct bu_vls z_vls;
    char **av;
    static const char *usage = "vnirt options vX vY";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /*
     * The last two arguments are expected to be x, y in view
     * coordinates.  It is also assumed that view z will be the front
     * of the viewing cube.  These coordinates are converted to x, y,
     * z in model coordinates and then converted to local units before
     * being handed to nirt. All other arguments are passed straight
     * through to nirt.
     */
    if (sscanf(argv[argc-2], "%lf", &view_ray_orig[X]) != 1 ||
	sscanf(argv[argc-1], "%lf", &view_ray_orig[Y]) != 1) {
	return GED_ERROR;
    }
    view_ray_orig[Z] = DG_GED_MAX;
    argc -= 2;

    av = (char **)bu_calloc(1, sizeof(char *) * (argc + 4), "gd_vnirt_cmd: av");

    /* Calculate point from which to fire ray */
    VSCALE(view_ray_orig, view_ray_orig, sf);
    MAT4X3PNT(center_model, gedp->ged_gvp->gv_view2model, view_ray_orig);
    VSCALE(center_model, center_model, gedp->ged_wdbp->dbip->dbi_base2local);

    bu_vls_init(&x_vls);
    bu_vls_init(&y_vls);
    bu_vls_init(&z_vls);
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

    status = ged_nirt(gedp, argc + 3, (const char **)av);

    bu_vls_free(&x_vls);
    bu_vls_free(&y_vls);
    bu_vls_free(&z_vls);
    bu_free((genptr_t)av, "ged_vnirt: av");
    av = NULL;

    return status;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
