/*                         R T . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file rt.c
 *
 * The rt command.
 *
 */

#include "common.h"

#include "bio.h"
#include "ged_private.h"
#include "cmd.h"
#include "solid.h"

#if GED_USE_RUN_RT
static int ged_run_rt(struct ged *gdp);
static void ged_rt_write(struct ged *gedp,
		  FILE *fp,
		  vect_t eye_model);
static void ged_rt_output_handler(ClientData clientData,
				  int	 mask);
static int ged_build_tops(struct ged	*gedp,
			  struct solid	*hsp,
			  char		**start,
			  register char	**end);


int
ged_rt(struct ged *gedp, int argc, const char *argv[])
{
    register char **vp;
    register int i;
    char pstring[32];
    static const char *usage = "options";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc < 1 || MAXARGS < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    vp = &gedp->ged_gdp->gd_rt_cmd[0];
    *vp++ = (char *)argv[0];
    *vp++ = "-M";

    if (gedp->ged_gvp->gv_perspective > 0) {
	(void)sprintf(pstring, "-p%g", gedp->ged_gvp->gv_perspective);
	*vp++ = pstring;
    }

    for (i=1; i < argc; i++) {
	if (argv[i][0] == '-' && argv[i][1] == '-' &&
	    argv[i][2] == '\0') {
	    ++i;
	    break;
	}
	*vp++ = (char *)argv[i];
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
	gedp->ged_gdp->gd_rt_cmd_len += ged_build_tops(gedp,
						       (struct solid *)&gedp->ged_gdp->gd_headSolid,
						       vp,
						       &gedp->ged_gdp->gd_rt_cmd[MAXARGS]);
    } else {
	while (i < argc)
	    *vp++ = (char *)argv[i++];
	*vp = 0;
	vp = &gedp->ged_gdp->gd_rt_cmd[0];
	while (*vp)
	    bu_vls_printf(&gedp->ged_result_str, "%s ", *vp++);

	bu_vls_printf(&gedp->ged_result_str, "\n");
    }
    (void)ged_run_rt(gedp);

    return BRLCAD_OK;
}


static int
ged_run_rt(struct ged *gedp)
{
    register int i;
    FILE *fp_in;
#ifndef _WIN32
    int	pipe_in[2];
    int	pipe_err[2];
#else
    HANDLE pipe_in[2], pipe_inDup;
    HANDLE pipe_err[2], pipe_errDup;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa = {0};
    char line[2048];
    char name[256];
#endif
    vect_t eye_model;
    struct ged_run_rt *run_rtp;
    struct ged_rt_client_data *drcdp;
#ifndef _WIN32
    int pid;

    (void)pipe(pipe_in);
    (void)pipe(pipe_err);

    if ((pid = fork()) == 0) {
	/* make this a process group leader */
	setpgid(0, 0);

	/* Redirect stdin and stderr */
	(void)close(0);
	(void)dup(pipe_in[0]);
	(void)close(2);
	(void)dup(pipe_err[1]);

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

    ged_rt_set_eye_model(gedp, eye_model);
    ged_rt_write(gedp, fp_in, eye_model);
    (void)fclose(fp_in);

    BU_GETSTRUCT(run_rtp, ged_run_rt);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&gedp->ged_gdp->gd_headRunRt.l, &run_rtp->l);

    run_rtp->fd = pipe_err[0];
    run_rtp->pid = pid;

    BU_GETSTRUCT(drcdp, ged_rt_client_data);
    drcdp->gedp = gedp;
    drcdp->rrtp = run_rtp;

#if 0
    /*XXX We still need some way to handle output from rt */
    drcdp->interp = gedp->ged_wdbp->wdb_interp;

    Tcl_CreateFileHandler(run_rtp->fd,
			  TCL_READABLE,
			  ged_rt_output_handler,
			  (ClientData)drcdp);
#endif

    return 0;

#else
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Create a pipe for the child process's STDOUT. */
    CreatePipe( &pipe_err[0], &pipe_err[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle( GetCurrentProcess(), pipe_err[0],
		     GetCurrentProcess(),  &pipe_errDup ,
		     0,  FALSE,
		     DUPLICATE_SAME_ACCESS );
    CloseHandle( pipe_err[0] );

    /* Create a pipe for the child process's STDIN. */
    CreatePipe(&pipe_in[0], &pipe_in[1], &sa, 0);

    /* Duplicate the write handle to the pipe so it is not inherited. */
    DuplicateHandle(GetCurrentProcess(), pipe_in[1],
		    GetCurrentProcess(), &pipe_inDup,
		    0, FALSE,                  /* not inherited */
		    DUPLICATE_SAME_ACCESS );
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

    snprintf(line, sizeof(line), "%s ", gedp->ged_gdp->gd_rt_cmd[0]);
    for (i=1; i<gedp->ged_gdp->gd_rt_cmd_len; i++) {
	snprintf(name, sizeof(name), "%s ", gedp->ged_gdp->gd_rt_cmd[i]);
	bu_strlcat(line, name, sizeof(line));
    }

    CreateProcess(NULL, line, NULL, NULL, TRUE,
		  DETACHED_PROCESS, NULL, NULL,
		  &si, &pi);

    CloseHandle(pipe_in[0]);
    CloseHandle(pipe_err[1]);

    /* As parent, send view information down pipe */
    fp_in = _fdopen( _open_osfhandle((HFILE)pipe_inDup, _O_TEXT), "wb" );

    ged_rt_set_eye_model(gedp, eye_model);
    ged_rt_write(gedp, fp_in, eye_model);
    (void)fclose(fp_in);

    BU_GETSTRUCT(run_rtp, ged_run_rt);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&gedp->ged_gdp->gd_headRunRt.l, &run_rtp->l);

    run_rtp->fd = pipe_errDup;
    run_rtp->hProcess = pi.hProcess;
    run_rtp->pid = pi.dwProcessId;
    run_rtp->aborted=0;
    run_rtp->chan = Tcl_MakeFileChannel(run_rtp->fd, TCL_READABLE);

    BU_GETSTRUCT(drcdp, ged_rt_client_data);
    drcdp->gedp = gedp;
    drcdp->rrtp = run_rtp;
#if 0
    /*XXX We still need some way to handle output from rt */
    drcdp->interp = gedp->ged_wdbp->wdb_interp;

    Tcl_CreateChannelHandler(run_rtp->chan,
			     TCL_READABLE,
			     ged_rt_output_handler,
			     (ClientData)drcdp);
#endif

    return 0;
#endif
}

static void
ged_rt_write(struct ged *gedp,
	     FILE	*fp,
	     vect_t	eye_model)
{
    register int	i;
    quat_t		quat;
    register struct solid *sp;

    (void)fprintf(fp, "viewsize %.15e;\n", gedp->ged_gvp->gv_size);
    quat_mat2quat(quat, gedp->ged_gvp->gv_rotation );
    (void)fprintf(fp, "orientation %.15e %.15e %.15e %.15e;\n", V4ARGS(quat));
    (void)fprintf(fp, "eye_pt %.15e %.15e %.15e;\n",
		  eye_model[X], eye_model[Y], eye_model[Z] );

    (void)fprintf(fp, "start 0; clean;\n");
    FOR_ALL_SOLIDS (sp, &gedp->ged_gdp->gd_headSolid) {
	for (i=0;i<sp->s_fullpath.fp_len;i++) {
	    DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags &= ~DIR_USED;
	}
    }
    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid) {
	for (i=0; i<sp->s_fullpath.fp_len; i++ ) {
	    if (!(DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags & DIR_USED)) {
		register struct animate *anp;
		for (anp = DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_animate; anp;
		     anp=anp->an_forw) {
		    db_write_anim(fp, anp);
		}
		DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags |= DIR_USED;
	    }
	}
    }

    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid) {
	for (i=0;i< sp->s_fullpath.fp_len;i++) {
	    DB_FULL_PATH_GET(&sp->s_fullpath, i)->d_flags &= ~DIR_USED;
	}
    }
    (void)fprintf(fp, "end;\n");
}

#ifndef _WIN32
static void
ged_rt_output_handler(ClientData	clientData,
		      int		mask)
{
    struct ged_rt_client_data *drcdp = (struct ged_rt_client_data *)clientData;
    struct ged_run_rt *run_rtp;
    int count;
    char line[RT_MAXLINE+1];

    if (drcdp == (struct ged_rt_client_data *)NULL ||
	drcdp->gedp == (struct ged *)NULL ||
	drcdp->rrtp == (struct ged_run_rt *)NULL ||
	brlcad_interp == (Tcl_Interp *)NULL)
	return;

    run_rtp = drcdp->rrtp;

    /* Get data from rt */
    count = read((int)run_rtp->fd, line, RT_MAXLINE);
    if (count <= 0) {
	int retcode = 0;
	int rpid;
	int aborted;

	if (count < 0) {
	    perror("READ ERROR");
	}

	Tcl_DeleteFileHandler(run_rtp->fd);
	close(run_rtp->fd);

	/* wait for the forked process */
	while ((rpid = wait(&retcode)) != run_rtp->pid && rpid != -1);

	aborted = run_rtp->aborted;

#if 0
	/*XXX Still need to address this */
	if (drcdp->gedp->ged_gdp->gd_outputHandler != NULL) {
	    struct bu_vls vls;

	    bu_vls_init(&vls);

	    if (aborted)
		bu_vls_printf(&vls, "%s \"Raytrace aborted.\n\"",
			      drcdp->gedp->ged_gdp->gd_outputHandler);
	    else if (retcode)
		bu_vls_printf(&vls, "%s \"Raytrace failed.\n\"",
			      drcdp->gedp->ged_gdp->gd_outputHandler);
	    else
		bu_vls_printf(&vls, "%s \"Raytrace complete.\n\"",
			      drcdp->gedp->ged_gdp->gd_outputHandler);

	    Tcl_Eval(drcdp->interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	} else {
	    if (aborted)
		bu_log("Raytrace aborted.\n");
	    else if (retcode)
		bu_log("Raytrace failed.\n");
	    else
		bu_log("Raytrace complete.\n");
	}
#endif

	if (drcdp->gedp->ged_gdp->gd_rtCmdNotify != (void (*)())0)
	    drcdp->gedp->ged_gdp->gd_rtCmdNotify();

	/* free run_rtp */
	BU_LIST_DEQUEUE(&run_rtp->l);
	bu_free((genptr_t)run_rtp, "ged_rt_output_handler: run_rtp");

	bu_free((genptr_t)drcdp, "ged_rt_output_handler: drcdp");

	return;
    }

    line[count] = '\0';

    /*XXX For now just blather to stderr */
    if (drcdp->gedp->ged_gdp->gd_outputHandler != NULL) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s \"%s\"", drcdp->gedp->ged_gdp->gd_outputHandler, line);
	Tcl_Eval(brlcad_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    } else
	bu_log("%s", line);
}

#else
static void
ged_rt_output_handler(ClientData	clientData,
		      int		mask)
{
    struct ged_rt_client_data *drcdp = (struct ged_rt_client_data *)clientData;
    struct ged_run_rt *run_rtp;
    int count;
    char line[10240+1] = {0};

    if (drcdp == (struct ged_rt_client_data *)NULL ||
	drcdp->gedp == (struct ged *)NULL ||
	drcdp->rrtp == (struct ged_run_rt *)NULL ||
	brlcad_interp == (Tcl_Interp *)NULL)
	return;

    run_rtp = drcdp->rrtp;

    /* Get data from rt */
    if (Tcl_Eof(run_rtp->chan) ||
	(!ReadFile(run_rtp->fd, line, 10240, &count, 0))) {
	int aborted;
	int retcode;

	Tcl_DeleteChannelHandler(run_rtp->chan,
				 ged_rt_output_handler,
				 (ClientData)drcdp);
	Tcl_Close(brlcad_interp, run_rtp->chan);

	/* wait for the forked process
	 * either EOF has been sent or there was a read error.
	 * there is no need to block indefinately
	 */
	WaitForSingleObject( run_rtp->hProcess, 120 );
	/* !!! need to observe implications of being non-infinate
	 *	WaitForSingleObject( run_rtp->hProcess, INFINITE );
	 */

	if (GetLastError() == ERROR_PROCESS_ABORTED) {
	    run_rtp->aborted = 1;
	}

	GetExitCodeProcess( run_rtp->hProcess, &retcode );
	/* may be useful to try pr_wait_status() here */

	aborted = run_rtp->aborted;

#if 0
	/*XXX Still need to address this */
	if (drcdp->gedp->ged_gdp->gd_outputHandler != NULL) {
	    struct bu_vls vls;

	    bu_vls_init(&vls);

	    if (aborted)
		bu_vls_printf(&vls, "%s \"Raytrace aborted.\n\"",
			      drcdp->gedp->ged_gdp->gd_outputHandler);
	    else if (retcode)
		bu_vls_printf(&vls, "%s \"Raytrace failed.\n\"",
			      drcdp->gedp->ged_gdp->gd_outputHandler);
	    else
		bu_vls_printf(&vls, "%s \"Raytrace complete.\n\"",
			      drcdp->gedp->ged_gdp->gd_outputHandler);

	    Tcl_Eval(brlcad_interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	} else {
	    if (aborted)
		bu_log("Raytrace aborted.\n");
	    else if (retcode)
		bu_log("Raytrace failed.\n");
	    else
		bu_log("Raytrace complete.\n");
	}
#endif

	if (drcdp->gedp->ged_gdp->gd_rtCmdNotify != (void (*)())0)
	    drcdp->gedp->ged_gdp->gd_rtCmdNotify();

	/* free run_rtp */
	BU_LIST_DEQUEUE(&run_rtp->l);
	bu_free((genptr_t)run_rtp, "ged_rt_output_handler: run_rtp");

	bu_free((genptr_t)drcdp, "ged_rt_output_handler: drcdp");

	return;
    }

    line[count] = '\0';

    /*XXX For now just blather to stderr */
    if (drcdp->gedp->ged_gdp->gd_outputHandler != NULL) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s \"%s\"", drcdp->gedp->ged_gdp->gd_outputHandler, line);
	Tcl_Eval(brlcad_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    } else
	bu_log("%s", line);
}

#endif

/*
 *                    G E D _ B U I L D _ T O P S
 *
 *  Build a command line vector of the tops of all objects in view.
 */
static int
ged_build_tops(struct ged	*gedp,
	       struct solid	*hsp,
	       char		**start,
	       register char	**end)
{
    register char **vp = start;
    register struct solid *sp;

    /*
     * Find all unique top-level entries.
     *  Mark ones already done with s_flag == UP
     */
    FOR_ALL_SOLIDS(sp, &hsp->l)
	sp->s_flag = DOWN;
    FOR_ALL_SOLIDS(sp, &hsp->l)  {
	register struct solid *forw;
	struct directory *dp = FIRST_SOLID(sp);

	if (sp->s_flag == UP)
	    continue;
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;	/* Ignore overlays, predictor, etc */
	if (vp < end)
	    *vp++ = dp->d_namep;
	else  {
	    bu_vls_printf(&gedp->ged_result_str, "mged: ran out of comand vector space at %s\n", dp->d_namep);
	    break;
	}
	sp->s_flag = UP;
	for (BU_LIST_PFOR(forw, sp, solid, &hsp->l)) {
	    if (FIRST_SOLID(forw) == dp)
		forw->s_flag = UP;
	}
    }
    *vp = (char *) 0;
    return vp-start;
}
#endif


void
ged_rt_set_eye_model(struct ged *gedp,
		     vect_t eye_model)
{
    if (gedp->ged_gvp->gv_zclip || gedp->ged_gvp->gv_perspective > 0) {
	vect_t temp;

	VSET(temp, 0.0, 0.0, 1.0);
	MAT4X3PNT(eye_model, gedp->ged_gvp->gv_view2model, temp);
    } else {
	/* not doing zclipping, so back out of geometry */
	register struct solid *sp;
	register int i;
	vect_t  direction;
	vect_t  extremum[2];
	vect_t  minus, plus;    /* vers of this solid's bounding box */

	VSET(eye_model, -gedp->ged_gvp->gv_center[MDX],
	     -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);

	for (i = 0; i < 3; ++i) {
	    extremum[0][i] = INFINITY;
	    extremum[1][i] = -INFINITY;
	}

	FOR_ALL_SOLIDS (sp, &gedp->ged_gdp->gd_headSolid) {
	    minus[X] = sp->s_center[X] - sp->s_size;
	    minus[Y] = sp->s_center[Y] - sp->s_size;
	    minus[Z] = sp->s_center[Z] - sp->s_size;
	    VMIN( extremum[0], minus );
	    plus[X] = sp->s_center[X] + sp->s_size;
	    plus[Y] = sp->s_center[Y] + sp->s_size;
	    plus[Z] = sp->s_center[Z] + sp->s_size;
	    VMAX( extremum[1], plus );
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
	    double  t_in;
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
