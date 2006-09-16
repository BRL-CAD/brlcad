/*                          N I R T . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup librt */

/*@{*/
/** @file nirt.c
 *  Routines to interface to nirt.
 *
 *  Functions -
 *	dgo_nirt_cmd          trace a single ray from current view
 *
 *  Author -
 *	Michael John Muuss
 *	Robert G. Parker
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *
 *  Description -
 *	This code was imported from MGED's/rtif.c and modified to work as part
 * 	of the drawable geometry object.
 *
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>
#include <signal.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* For struct timeval */
#endif
#include <sys/stat.h>		/* for chmod() */
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#include "tcl.h"
#include "machine.h"
#include "cmd.h"			/* includes bu.h */
#include "vmath.h"
#include "raytrace.h"
#include "solid.h"
#include "./qray.h"


/* defined in qray.c */
extern void dgo_qray_data_to_vlist(struct dg_obj *dgop, struct bn_vlblock *vbp, struct dg_qray_dataList *headp, fastf_t *dir, int do_overlaps);

/* defined in dg_obj.c */
extern int dgo_build_tops(Tcl_Interp *interp, struct solid *hsp, char **start, register char **end);
extern void dgo_cvt_vlblock_to_solids(struct dg_obj *dgop, Tcl_Interp *interp, struct bn_vlblock *vbp, char *name, int copy);
extern void dgo_pr_wait_status(Tcl_Interp *interp, int status);

/*
 *			F _ N I R T
 *
 *  Invoke nirt with the current view & stuff
 */
int
dgo_nirt_cmd(struct dg_obj	*dgop,
	     struct view_obj	*vop,
	     Tcl_Interp		*interp,
	     int		argc,
	     char		**argv)
{
	register char **vp;
	FILE *fp_in;
	FILE *fp_out, *fp_err;
	int pid, rpid;
	int retcode;
#ifndef _WIN32
	int pipe_in[2];
	int pipe_out[2];
	int pipe_err[2];
#else
	HANDLE pipe_in[2],hSaveStdin,pipe_inDup;
	HANDLE pipe_out[2],hSaveStdout,pipe_outDup;
	HANDLE pipe_err[2],hSaveStderr,pipe_errDup;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES sa;
	char name[1024];
	char line1[2048];
#endif
	int use_input_orig = 0;
	vect_t	center_model;
	vect_t dir;
	vect_t cml;
	register int i;
	register struct solid *sp;
	char line[RT_MAXLINE];
	char *val;
	struct bu_vls vls;
	struct bu_vls o_vls;
	struct bu_vls p_vls;
	struct bu_vls t_vls;
	struct bn_vlblock *vbp;
	struct dg_qray_dataList *ndlp;
	struct dg_qray_dataList HeadQRayData;

	vp = &dgop->dgo_rt_cmd[0];
	*vp++ = "nirt";

	/* swipe x, y, z off the end if present */
	if (argc > 3) {
		if (sscanf(argv[argc-3], "%lf", &center_model[X]) == 1 &&
		    sscanf(argv[argc-2], "%lf", &center_model[Y]) == 1 &&
		    sscanf(argv[argc-1], "%lf", &center_model[Z]) == 1){
			use_input_orig = 1;
			argc -= 3;
			VSCALE(center_model, center_model, dgop->dgo_wdbp->dbip->dbi_local2base);
		}
	}

	/* Calculate point from which to fire ray */
	if (!use_input_orig) {
		VSET(center_model, -vop->vo_center[MDX],
		     -vop->vo_center[MDY], -vop->vo_center[MDZ]);
	}

#if 0
	if (mged_variables->mv_perspective_mode) {
		point_t pt, eye;

		/* get eye point */
		VSET(pt, 0.0, 0.0, 1.0);
		MAT4X3PNT(eye, vop->vo_view2model, pt);
		VSCALE(eye, eye, base2local);

		/* point passed in is actually the aim point */
		VSCALE(cml, center_model, base2local);
		VSUB2(dir, cml, eye);
		VUNITIZE(dir);

		/* copy eye point to cml (cml is used for the "xyz" command to nirt */
		VMOVE(cml, eye);
	} else {
		VSCALE(cml, center_model, base2local);
		VMOVEN(dir, vop->vo_rotation + 8, 3);
		VSCALE(dir, dir, -1.0);
	}
#else
	VSCALE(cml, center_model, dgop->dgo_wdbp->dbip->dbi_base2local);
	VMOVEN(dir, vop->vo_rotation + 8, 3);
	VSCALE(dir, dir, -1.0);
#endif

	bu_vls_init(&p_vls);
	bu_vls_printf(&p_vls, "xyz %lf %lf %lf;",
		cml[X], cml[Y], cml[Z]);
	bu_vls_printf(&p_vls, "dir %lf %lf %lf; s",
		dir[X], dir[Y], dir[Z]);

	i = 0;
	if (DG_QRAY_GRAPHICS(dgop)) {

		*vp++ = "-e";
		*vp++ = DG_QRAY_FORMAT_NULL;

		/* first ray  ---- returns partitions */
		*vp++ = "-e";
		*vp++ = DG_QRAY_FORMAT_P;

		/* ray start, direction, and 's' command */
		*vp++ = "-e";
		*vp++ = bu_vls_addr(&p_vls);

		/* second ray  ---- returns overlaps */
		*vp++ = "-e";
		*vp++ = DG_QRAY_FORMAT_O;

		/* ray start, direction, and 's' command */
		*vp++ = "-e";
		*vp++ = bu_vls_addr(&p_vls);

		if (DG_QRAY_TEXT(dgop)) {
			char *cp;
			int count = 0;

			bu_vls_init(&o_vls);

			/* get 'r' format now; prepend its' format string with a newline */
			val = bu_vls_addr(&dgop->dgo_qray_fmts[0].fmt);

			/* find first '"' */
			while(*val != '"' && *val != '\0')
				++val;

			if(*val == '\0')
				goto done;
			else
				++val;	    /* skip first '"' */

			/* find last '"' */
			cp = (char *)strrchr(val, '"');

			if (cp != (char *)NULL) /* found it */
				count = cp - val;

done:
			if(*val == '\0')
			    bu_vls_printf(&o_vls, " fmt r \"\\n\" ");
			else{
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

	if (DG_QRAY_TEXT(dgop)) {

		bu_vls_init(&t_vls);

		/* load vp with formats for printing */
		for(; dgop->dgo_qray_fmts[i].type != (char)NULL; ++i)
			bu_vls_printf(&t_vls, "fmt %c %s; ",
				      dgop->dgo_qray_fmts[i].type,
				      bu_vls_addr(&dgop->dgo_qray_fmts[i].fmt));

		*vp++ = "-e";
		*vp++ = bu_vls_addr(&t_vls);

		/* nirt does not like the trailing ';' */
		bu_vls_trunc(&t_vls, -2);
	}

	/* include nirt script string */
	if (bu_vls_strlen(&dgop->dgo_qray_script)) {
		*vp++ = "-e";
		*vp++ = bu_vls_addr(&dgop->dgo_qray_script);
	}

        *vp++ = "-e";
        *vp++ = bu_vls_addr(&p_vls);

	for (i=1; i < argc; i++)
		*vp++ = argv[i];
	*vp++ = dgop->dgo_wdbp->dbip->dbi_filename;

	dgop->dgo_rt_cmd_len = vp - dgop->dgo_rt_cmd;

	/* Note - dgo_build_tops sets the last vp to (char *)0 */
	dgop->dgo_rt_cmd_len += dgo_build_tops(interp,
					       (struct solid *)&dgop->dgo_headSolid,
					       vp,
					       &dgop->dgo_rt_cmd[RT_MAXARGS]);

	if (dgop->dgo_qray_cmd_echo) {
		/* Print out the command we are about to run */
		vp = &dgop->dgo_rt_cmd[0];
		while (*vp)
			Tcl_AppendResult(interp, *vp++, " ", (char *)NULL);

		Tcl_AppendResult(interp, "\n", (char *)NULL);
	}

	if (use_input_orig) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "\nFiring from (%lf, %lf, %lf)...\n",
			      center_model[X], center_model[Y], center_model[Z]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
	} else
		Tcl_AppendResult(interp, "\nFiring from view center...\n", (char *)NULL);

#ifndef _WIN32
	(void)pipe(pipe_in);
	(void)pipe(pipe_out);
	(void)pipe(pipe_err);
	(void)signal(SIGINT, SIG_IGN);
	if ((pid = fork()) == 0) {
 	        /* Redirect stdin, stdout, stderr */
		(void)close(0);
		(void)dup( pipe_in[0] );
		(void)close(1);
		(void)dup( pipe_out[1] );
		(void)close(2);
		(void)dup ( pipe_err[1] );

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
		(void)execvp(dgop->dgo_rt_cmd[0], dgop->dgo_rt_cmd);
		perror (dgop->dgo_rt_cmd[0]);
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
	fwrite("q\n", 1, 2, fp_in);
	(void)fclose(fp_in);

#else
	memset((void *)&si, 0, sizeof(STARTUPINFO));
	memset((void *)&pi, 0, sizeof(PROCESS_INFORMATION));
	memset((void *)&sa, 0, sizeof(SECURITY_ATTRIBUTES));

	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	/* Create a pipe for the child process's STDOUT. */
	CreatePipe( &pipe_out[0], &pipe_out[1], &sa, 0);

	/* Create noninheritable read handle and close the inheritable read handle. */
	DuplicateHandle(GetCurrentProcess(), pipe_out[0],
			GetCurrentProcess(),  &pipe_outDup ,
			0,  FALSE,
			DUPLICATE_SAME_ACCESS);
	CloseHandle(pipe_out[0]);

	/* Create a pipe for the child process's STDERR. */
	CreatePipe( &pipe_err[0], &pipe_err[1], &sa, 0);

	/* Create noninheritable read handle and close the inheritable read handle. */
	DuplicateHandle(GetCurrentProcess(), pipe_err[0],
			GetCurrentProcess(),  &pipe_errDup ,
			0,  FALSE,
			DUPLICATE_SAME_ACCESS);
	CloseHandle(pipe_err[0]);

	/* The steps for redirecting child process's STDIN:
	 *     1.  Save current STDIN, to be restored later.
	 *     2.  Create anonymous pipe to be STDIN for child process.
	 *     3.  Set STDIN of the parent to be the read handle to the
	 *         pipe, so it is inherited by the child process.
	 *     4.  Create a noninheritable duplicate of the write handle,
	 *         and close the inheritable write handle.
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

	sprintf(line1,"%s ", dgop->dgo_rt_cmd[0]);
	for (i=1; i<dgop->dgo_rt_cmd_len; i++) {
	    /* skip commands */
	    if (strstr(dgop->dgo_rt_cmd[i], "-e") != NULL)
		++i;
	    else { /* append other arguments (i.e. options, file and obj(s)) */
		sprintf(name,"\"%s\" ", dgop->dgo_rt_cmd[i]);
		strcat(line1, name);
	    }
	}

	CreateProcess(NULL, line1, NULL, NULL, TRUE,
		      DETACHED_PROCESS, NULL, NULL,
		      &si, &pi);

	/* use fp_in to feed view info to nirt */
	CloseHandle(pipe_in[0]);
	fp_in = _fdopen(_open_osfhandle((HFILE)pipe_inDup,_O_TEXT), "wb");
	_setmode(_fileno(fp_in), _O_BINARY);

	/* send commands down the pipe */
	for (i=1; i<dgop->dgo_rt_cmd_len-2; i++)
	    if (strstr(dgop->dgo_rt_cmd[i], "-e") != NULL)
		fprintf(fp_in, "%s\n", dgop->dgo_rt_cmd[++i]);

	/* use fp_out to read back the result */
	CloseHandle(pipe_out[1]);
	fp_out = _fdopen(_open_osfhandle((HFILE)pipe_outDup,_O_TEXT), "rb");
	_setmode(_fileno(fp_out), _O_BINARY);

	/* use fp_err to read any error messages */
	CloseHandle(pipe_err[1]);
	fp_err = _fdopen(_open_osfhandle((HFILE)pipe_errDup,_O_TEXT), "rb");
	_setmode(_fileno(fp_err), _O_BINARY);

	/* send quit command to nirt */
	fwrite( "q\n", 1, 2, fp_in );
	(void)fclose(fp_in);

#endif

	bu_vls_free(&p_vls);   /* use to form "partition" part of nirt command above */
	if (DG_QRAY_GRAPHICS(dgop)) {

		if (DG_QRAY_TEXT(dgop))
			bu_vls_free(&o_vls); /* used to form "overlap" part of nirt command above */

		BU_LIST_INIT(&HeadQRayData.l);

		/* handle partitions */
		while (fgets(line, RT_MAXLINE, fp_out) != (char *)NULL) {
			if (line[0] == '\n') {
				Tcl_AppendResult(interp, line+1, (char *)NULL);
				break;
			}

			BU_GETSTRUCT(ndlp, dg_qray_dataList);
			BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

			if (sscanf(line, "%le %le %le %le",
				   &ndlp->x_in, &ndlp->y_in, &ndlp->z_in, &ndlp->los) != 4)
				break;
		}

		vbp = rt_vlblock_init();
		dgo_qray_data_to_vlist(dgop, vbp, &HeadQRayData, dir, 0);
		bu_list_free(&HeadQRayData.l);
		dgo_cvt_vlblock_to_solids(dgop, interp, vbp, bu_vls_addr(&dgop->dgo_qray_basename), 0);
		rt_vlblock_free(vbp);

		/* handle overlaps */
		while (fgets(line, RT_MAXLINE, fp_out) != (char *)NULL) {
			if (line[0] == '\n') {
				Tcl_AppendResult(interp, line+1, (char *)NULL);
				break;
			}

			BU_GETSTRUCT(ndlp, dg_qray_dataList);
			BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

			if (sscanf(line, "%le %le %le %le",
				   &ndlp->x_in, &ndlp->y_in, &ndlp->z_in, &ndlp->los) != 4)
				break;
		}
		vbp = rt_vlblock_init();
		dgo_qray_data_to_vlist(dgop, vbp, &HeadQRayData, dir, 1);
		bu_list_free(&HeadQRayData.l);
		dgo_cvt_vlblock_to_solids(dgop, interp, vbp, bu_vls_addr(&dgop->dgo_qray_basename), 0);
		rt_vlblock_free(vbp);
	}

	/*
	 * Notify observers, if any, before generating textual output since
	 * such an act (observer notification) wipes out whatever gets stuffed
	 * into interp->result.
	 */
	dgo_notify(dgop, interp);

	if (DG_QRAY_TEXT(dgop)) {
		bu_vls_free(&t_vls);

		while (fgets(line, RT_MAXLINE, fp_out) != (char *)NULL)
			Tcl_AppendResult(interp, line, (char *)NULL);
	}

	(void)fclose(fp_out);

	while (fgets(line, RT_MAXLINE, fp_err) != (char *)NULL)
		Tcl_AppendResult(interp, line, (char *)NULL);
	(void)fclose(fp_err);


#ifndef _WIN32

	/* Wait for program to finish */
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;	/* NULL */

	if( retcode != 0 )
		dgo_pr_wait_status(interp, retcode);
#else
	/* Wait for program to finish */
	WaitForSingleObject( pi.hProcess, INFINITE );

#endif

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)
		sp->s_wflag = DOWN;

	return TCL_OK;
}

int
dgo_vnirt_cmd(struct dg_obj	*dgop,
	      struct view_obj	*vop,
	      Tcl_Interp	*interp,
	      int		argc,
	      char		**argv) {
    register int i;
    int status;
    fastf_t sf = 1.0 * DG_INV_GED;
    vect_t view_ray_orig;
    vect_t center_model;
    struct bu_vls x_vls;
    struct bu_vls y_vls;
    struct bu_vls z_vls;
    char **av;

    if (argc < 3 || MAXARGS < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dgo_vnirt %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    /*
     * The last two arguments are expected to be x,y in view coordinates.
     * It is also assumed that view z will be the front of the viewing cube.
     * These coordinates are converted to x,y,z in model coordinates and then
     * converted to local units before being handed to nirt. All other
     * arguments are passed straight through to nirt.
     */
    if(sscanf(argv[argc-2], "%lf", &view_ray_orig[X]) != 1 ||
       sscanf(argv[argc-1], "%lf", &view_ray_orig[Y]) != 1){
	return TCL_ERROR;
    }
    view_ray_orig[Z] = DG_GED_MAX;
    argc -= 2;

    av = (char **)bu_malloc(sizeof(char *) * (argc + 4), "dgo_vnirt_cmd: av");

    /* Calculate point from which to fire ray */
    VSCALE(view_ray_orig, view_ray_orig, sf);
    MAT4X3PNT(center_model, vop->vo_view2model, view_ray_orig);
    VSCALE(center_model, center_model, dgop->dgo_wdbp->dbip->dbi_base2local);

    bu_vls_init(&x_vls);
    bu_vls_init(&y_vls);
    bu_vls_init(&z_vls);
    bu_vls_printf(&x_vls, "%lf", center_model[X]);
    bu_vls_printf(&y_vls, "%lf", center_model[Y]);
    bu_vls_printf(&z_vls, "%lf", center_model[Z]);

    /* pass remaining arguments to nirt */
    av[0] = "nirt";
    for(i = 1; i < argc; ++i)
	av[i] = argv[i];

    /* pass modified coordinates to nirt */
    av[i++] = bu_vls_addr(&x_vls);
    av[i++] = bu_vls_addr(&y_vls);
    av[i++] = bu_vls_addr(&z_vls);
    av[i] = (char *)NULL;

    status = dgo_nirt_cmd(dgop, vop, interp, argc + 3, av);

    bu_vls_free(&x_vls);
    bu_vls_free(&y_vls);
    bu_vls_free(&z_vls);
    bu_free((genptr_t)av, "dgo_vnirt_cmd: av");

    return status;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
