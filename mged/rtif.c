/*
 *			R T I F . C
 *
 *  Routines to interface to RT, and RT-style command files
 *
 * Functions -
 *	f_rt		ray-trace
 *	f_rrt		ray-trace using any program
 *	f_rtcheck	ray-trace to check for overlaps
 *	f_saveview	save the current view parameters
 *	f_rmats		load views from a file
 *	f_savekey	save keyframe in file
 *	f_nirt          trace a single ray from current view
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <signal.h>
#include <sys/time.h>		/* For struct timeval */
#include <sys/stat.h>		/* for chmod() */

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "externs.h"
#include "mater.h"
#include "./sedit.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./mgedtcl.h"
#include "./qray.h"

extern int mged_svbase();
static void setup_rt();

static int tree_walk_needed;

struct run_rt head_run_rt;

struct rtcheck {
       int			fd;
       FILE			*fp;
       int			pid;
       struct bn_vlblock	*vbp;
       struct bu_list		*vhead;
       double			csize;  
};

/*
 *			P R _ W A I T _ S T A T U S
 *
 *  Interpret the status return of a wait() system call,
 *  for the edification of the watching luser.
 *  Warning:  This may be somewhat system specific, most especially
 *  on non-UNIX machines.
 */
void
pr_wait_status( status )
int	status;
{
  int	sig = status & 0x7f;
  int	core = status & 0x80;
  int	ret = status >> 8;
  struct bu_vls tmp_vls;

  if( status == 0 )  {
    Tcl_AppendResult(interp, "Normal exit\n", (char *)NULL);
    return;
  }

  bu_vls_init(&tmp_vls);
  bu_vls_printf(&tmp_vls, "Abnormal exit x%x", status);

  if( core )
    bu_vls_printf(&tmp_vls, ", core dumped");

  if( sig )
    bu_vls_printf(&tmp_vls, ", terminating signal = %d", sig );
  else
    bu_vls_printf(&tmp_vls, ", return (exit) code = %d", ret );

  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), "\n", (char *)NULL);
  bu_vls_free(&tmp_vls);
}

/*
 *  			R T _ O L D W R I T E
 *  
 *  Write out the information that RT's -M option needs to show current view.
 *  Note that the model-space location of the eye is a parameter,
 *  as it can be computed in different ways.
 *  The is the OLD format, needed only when sending to RT on a pipe,
 *  due to some oddball hackery in RT to determine old -vs- new format.
 */
HIDDEN void
rt_oldwrite(fp, eye_model)
FILE *fp;
vect_t eye_model;
{
	register int i;

	(void)fprintf(fp, "%.9e\n", VIEWSIZE );
	(void)fprintf(fp, "%.9e %.9e %.9e\n",
		eye_model[X], eye_model[Y], eye_model[Z] );
	for( i=0; i < 16; i++ )  {
		(void)fprintf( fp, "%.9e ", view_state->vs_Viewrot[i] );
		if( (i%4) == 3 )
			(void)fprintf(fp, "\n");
	}
	(void)fprintf(fp, "\n");
}

/*
 *  			R T _ W R I T E
 *  
 *  Write out the information that RT's -M option needs to show current view.
 *  Note that the model-space location of the eye is a parameter,
 *  as it can be computed in different ways.
 */
HIDDEN void
rt_write(fp, eye_model)
FILE *fp;
vect_t eye_model;
{
	register int	i;
	quat_t		quat;
	register struct solid *sp;

	(void)fprintf(fp, "viewsize %.15e;\n", VIEWSIZE );
#if 0
	(void)fprintf(fp, "viewrot ");
	for( i=0; i < 16; i++ )  {
		(void)fprintf( fp, "%.15e ", view_state->vs_Viewrot[i] );
		if( (i%4) == 3 )
			(void)fprintf(fp, "\n");
	}
	(void)fprintf(fp, ";\n");
#else
	quat_mat2quat( quat, view_state->vs_Viewrot );
	(void)fprintf(fp, "orientation %.15e %.15e %.15e %.15e;\n",
		V4ARGS( quat ) );
#endif
	(void)fprintf(fp, "eye_pt %.15e %.15e %.15e;\n",
		eye_model[X], eye_model[Y], eye_model[Z] );

	(void)fprintf(fp, "start 0; clean;\n");
	FOR_ALL_SOLIDS(sp, &HeadSolid.l) {
		for (i=0;i<sp->s_fullpath.fp_len;i++) {
			DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_flags &= ~DIR_USED;
		}
	}
	FOR_ALL_SOLIDS(sp, &HeadSolid.l) {
		for (i=0; i<sp->s_fullpath.fp_len; i++ ) {
			struct directory *dp;
			dp = DB_FULL_PATH_GET(&sp->s_fullpath,i);
			if (!(dp->d_flags & DIR_USED)) {
				register struct animate *anp;
				for (anp = dp->d_animate; anp;
				    anp=anp->an_forw) {
					db_write_anim(fp, anp);
				}
				dp->d_flags |= DIR_USED;
			}
		}
	}

	FOR_ALL_SOLIDS(sp, &HeadSolid.l) {
		for (i=0;i<sp->s_fullpath.fp_len;i++) {
			DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_flags &= ~DIR_USED;
		}
	}
#undef DIR_USED
	(void)fprintf(fp, "end;\n");
}

/*
 *  			R T _ R E A D
 *  
 *  Read in one view in the old RT format.
 */
HIDDEN int
rt_read(fp, scale, eye, mat)
FILE	*fp;
fastf_t	*scale;
vect_t	eye;
mat_t	mat;
{
	register int i;
	double d;

	if( fscanf( fp, "%lf", &d ) != 1 )  return(-1);
	*scale = d*0.5;
	if( fscanf( fp, "%lf", &d ) != 1 )  return(-1);
	eye[X] = d;
	if( fscanf( fp, "%lf", &d ) != 1 )  return(-1);
	eye[Y] = d;
	if( fscanf( fp, "%lf", &d ) != 1 )  return(-1);
	eye[Z] = d;
	for( i=0; i < 16; i++ )  {
		if( fscanf( fp, "%lf", &d ) != 1 )
			return(-1);
		mat[i] = d;
	}
	return(0);
}

/*			B U I L D _ T O P S
 *
 *  Build a command line vector of the tops of all objects in view.
 */
int
build_tops(char **start, char **end)
{
	register char **vp = start;
	register struct solid *sp;

	/*
	 * Find all unique top-level entries.
	 *  Mark ones already done with s_wflag == UP
	 */
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)
		sp->s_wflag = DOWN;
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
		register struct solid *forw;
		struct directory *dp = FIRST_SOLID(sp);

		if( sp->s_wflag == UP )
			continue;
		if( dp->d_addr == RT_DIR_PHONY_ADDR )
			continue;	/* Ignore overlays, predictor, etc */
		if( vp < end )
			*vp++ = dp->d_namep;
		else  {
		  Tcl_AppendResult(interp, "mged: ran out of comand vector space at ",
				   dp->d_namep, "\n", (char *)NULL);
		  break;
		}
		sp->s_wflag = UP;
		for(BU_LIST_PFOR(forw, sp, solid, &HeadSolid.l)){
			if( FIRST_SOLID(forw) == dp )
				forw->s_wflag = UP;
		}
	}
	*vp = (char *) 0;
	return vp-start;
}
/*
 *			S E T U P _ R T
 *
 *  Set up command line for one of the RT family of programs,
 *  with all objects in view enumerated.
 */
static char	*rt_cmd_vec[MAXARGS];
static int	rt_cmd_vec_len;
static char	rt_cmd_storage[MAXARGS*9];

static void
setup_rt( vp, printcmd )
register char	**vp;
int printcmd;
{
  rt_cmd_vec_len = vp - rt_cmd_vec;
  rt_cmd_vec_len += build_tops(vp, &rt_cmd_vec[MAXARGS]);

  if(printcmd){
    /* Print out the command we are about to run */
    vp = &rt_cmd_vec[0];
    while( *vp )
      Tcl_AppendResult(interp, *vp++, " ", (char *)NULL);

    Tcl_AppendResult(interp, "\n", (char *)NULL);
  }
}

int
cmd_rt_abort(ClientData clientData,
	     Tcl_Interp *interp,
	     int argc,
	     char **argv)
{
	struct run_rt *rrp;

	for (BU_LIST_FOR(rrp, run_rt, &head_run_rt.l)) {
		kill(rrp->pid, SIGKILL);
		rrp->aborted = 1;
	}

	return TCL_OK;
}

static void
rt_output_handler(ClientData clientData, int mask)
{
	struct run_rt *run_rtp = (struct run_rt *)clientData;
	int count;
#if 0
	char line[RT_MAXLINE+1];

	/* Get data from rt */
	if ((count = read((int)run_rtp->fd, line, RT_MAXLINE)) == 0) {
#else
	char line[5120+1];

	/* Get data from rt */
	if ((count = read((int)run_rtp->fd, line, 5120)) == 0) {
#endif
		int retcode;
		int rpid;
		int aborted;

		Tcl_DeleteFileHandler(run_rtp->fd);
		close(run_rtp->fd);

		/* wait for the forked process */
		while ((rpid = wait(&retcode)) != run_rtp->pid && rpid != -1)
			pr_wait_status(retcode);

		aborted = run_rtp->aborted;

		/* free run_rtp */
 		BU_LIST_DEQUEUE(&run_rtp->l);
		bu_free((genptr_t)run_rtp, "rt_output_handler: run_rtp");

		if (aborted)
			bu_log("Raytrace aborted.\n");
		else
			bu_log("Raytrace complete.\n");
		return;
	}

	line[count] = '\0';

	/*XXX For now just blather to stderr */
	bu_log("%s", line);
}

static void
rt_set_eye_model(eye_model)
vect_t eye_model;
{
  if(dmp->dm_zclip || mged_variables->mv_perspective_mode){
    vect_t temp;

    VSET( temp, 0.0, 0.0, 1.0 );
    MAT4X3PNT( eye_model, view_state->vs_view2model, temp );
  }else{ /* not doing zclipping, so back out of geometry */
    register struct solid *sp;
    register int i;
    double  t;
    double  t_in;
    vect_t  direction;
    vect_t  extremum[2];
    vect_t  minus, plus;    /* vers of this solid's bounding box */

    VSET(eye_model, -view_state->vs_toViewcenter[MDX],
	 -view_state->vs_toViewcenter[MDY], -view_state->vs_toViewcenter[MDZ]);

    for (i = 0; i < 3; ++i){
      extremum[0][i] = INFINITY;
      extremum[1][i] = -INFINITY;
    }
    FOR_ALL_SOLIDS (sp, &HeadSolid.l){
      minus[X] = sp->s_center[X] - sp->s_size;
      minus[Y] = sp->s_center[Y] - sp->s_size;
      minus[Z] = sp->s_center[Z] - sp->s_size;
      VMIN( extremum[0], minus );
      plus[X] = sp->s_center[X] + sp->s_size;
      plus[Y] = sp->s_center[Y] + sp->s_size;
      plus[Z] = sp->s_center[Z] + sp->s_size;
      VMAX( extremum[1], plus );
    }
    VMOVEN(direction, view_state->vs_Viewrot + 8, 3);
    VSCALE(direction, direction, -1.0);
    for(i = 0; i < 3; ++i)
      if (NEAR_ZERO(direction[i], 1e-10))
	direction[i] = 0.0;
    if ((eye_model[X] >= extremum[0][X]) &&
	(eye_model[X] <= extremum[1][X]) &&
	(eye_model[Y] >= extremum[0][Y]) &&
	(eye_model[Y] <= extremum[1][Y]) &&
	(eye_model[Z] >= extremum[0][Z]) &&
	(eye_model[Z] <= extremum[1][Z])){
      t_in = -INFINITY;
      for(i = 0; i < 6; ++i){
	if (direction[i%3] == 0)
	  continue;
	t = (extremum[i/3][i%3] - eye_model[i%3]) /
	  direction[i%3];
	if ((t < 0) && (t > t_in))
	  t_in = t;
      }
      VJOIN1(eye_model, eye_model, t_in, direction);
    }
  }
}

/*
 *			R U N _ R T
 */
int
run_rt()
{
	register struct solid *sp;
	register int i;
	FILE *fp_in;
	int pipe_in[2];
	int pipe_err[2];
	vect_t eye_model;
	int		pid; 	 
	struct run_rt	*run_rtp;

	(void)pipe( pipe_in );
	(void)pipe( pipe_err );
	(void)signal( SIGINT, SIG_IGN );
	if ((pid = fork()) == 0) {
	  /* make this a process group leader */
	  setpgid(0, 0);

	  /* Redirect stdin and stderr */
	  (void)close(0);
	  (void)dup( pipe_in[0] );
	  (void)close(2);
	  (void)dup ( pipe_err[1] );

	  /* close pipes */
	  (void)close(pipe_in[0]);
	  (void)close(pipe_in[1]);
	  (void)close(pipe_err[0]);
	  (void)close(pipe_err[1]);

	  for( i=3; i < 20; i++ )
	    (void)close(i);

	  (void)signal( SIGINT, SIG_DFL );
	  (void)execvp( rt_cmd_vec[0], rt_cmd_vec );
	  perror( rt_cmd_vec[0] );
	  exit(16);
	}

	/* As parent, send view information down pipe */
	(void)close( pipe_in[0] );
	fp_in = fdopen( pipe_in[1], "w" );

	(void)close( pipe_err[1] );

	rt_set_eye_model(eye_model);
	rt_write(fp_in, eye_model);
	(void)fclose( fp_in );

	FOR_ALL_SOLIDS(sp, &HeadSolid.l)
		sp->s_wflag = DOWN;

	BU_GETSTRUCT(run_rtp, run_rt);
	BU_LIST_APPEND(&head_run_rt.l, &run_rtp->l);
	run_rtp->fd = pipe_err[0];
	run_rtp->pid = pid;

	Tcl_CreateFileHandler(run_rtp->fd, TCL_READABLE,
			      rt_output_handler, (ClientData)run_rtp);

	return 0;
}

/*
 *			F _ R T
 */
int
f_rt(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register char **vp;
	register int i;
	char	pstring[32];

	CHECK_DBI_NULL;

	if(argc < 1 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help rt");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_VIEW, "Ray-trace of current view" ) )
	  return TCL_ERROR;

	vp = &rt_cmd_vec[0];
	*vp++ = "rt";
	*vp++ = "-s512";
	*vp++ = "-M";
	*vp++ = "-v60";		/* Reduced RT logging when run interactively */
	if( mged_variables->mv_perspective > 0 )  {
		(void)sprintf(pstring, "-p%g", mged_variables->mv_perspective);
		*vp++ = pstring;
	}
	for( i=1; i < argc; i++ ) {
	    if( argv[i][0] == '-' && argv[i][1] == '-' &&
		argv[i][2] == '\0' ) {
		++i;
		break;
	    }
	    *vp++ = argv[i];
	}
	*vp++ = dbip->dbi_filename;

	/*
	 * Now that we've grabbed all the options, if no args remain,
	 * have setup_rt() append the names of all stuff currently displayed.
	 * Otherwise, simply append the remaining args.
	 */
	if ( i == argc )
	    setup_rt( vp, 1 );
	else {
	    while( i < argc )
		*vp++ = argv[i++];
	    *vp = 0;
	    vp = &rt_cmd_vec[0];
	    while( *vp )
	      Tcl_AppendResult(interp, *vp++, " ", (char *)NULL);

	    Tcl_AppendResult(interp, "\n", (char *)NULL);
	}
	(void)run_rt();

	return TCL_OK;
}

/*
 *			F _ R R T
 *
 *  Invoke any program with the current view & stuff, just like
 *  an "rt" command (above).
 *  Typically used to invoke a remote RT (hence the name).
 */
int
f_rrt(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register char **vp;
	register int i;

	CHECK_DBI_NULL;

	if(argc < 2 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help rrt");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_VIEW, "Ray-trace of current view" ) )
	  return TCL_ERROR;

	vp = &rt_cmd_vec[0];
	for( i=1; i < argc; i++ )
		*vp++ = argv[i];
	*vp++ = dbip->dbi_filename;

	setup_rt( vp, 1 );
	(void)run_rt();

	return TCL_OK;
}

static void
rtcheck_vector_handler(clientData, mask)
ClientData clientData;
int mask;
{
  int value;
  struct solid *sp;
  struct rtcheck *rtcp = (struct rtcheck *)clientData;

  /* Get vector output from rtcheck */
  if ((value = getc(rtcp->fp)) == EOF) {
    int retcode;
    int rpid;

    Tcl_DeleteFileHandler(rtcp->fd);
    fclose(rtcp->fp);

    FOR_ALL_SOLIDS(sp, &HeadSolid.l)
      sp->s_wflag = DOWN;

    /* Add overlay */
    cvt_vlblock_to_solids( rtcp->vbp, "OVERLAPS", 0 );
    rt_vlblock_free(rtcp->vbp);

    /* wait for the forked process */
    while ((rpid = wait(&retcode)) != rtcp->pid && rpid != -1)
      pr_wait_status(retcode);

    /* free rtcp */
    bu_free((genptr_t)rtcp, "rtcheck_vector_handler: rtcp");

    update_views = 1;
    return;
  }

  (void)rt_process_uplot_value( &rtcp->vhead,
				rtcp->vbp,
				rtcp->fp,
				value,
				rtcp->csize );
}

static void
rtcheck_output_handler(clientData, mask)
ClientData clientData;
int mask;
{
  int count;
  char line[RT_MAXLINE];
  int fd = (int)((long)clientData & 0xFFFF);	/* fd's will be small */

  /* Get textual output from rtcheck */
#if 0
  if((count = read((int)fd, line, RT_MAXLINE)) == 0){
#else
  if((count = read((int)fd, line, 5120)) == 0){
#endif
    Tcl_DeleteFileHandler(fd);
    close(fd);

    return;
  }

  line[count] = '\0';
  bu_log("%s", line);
}

int
f_rtcheck(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register char **vp;
	register int i;
	int	pid; 	 
	int	i_pipe[2];	/* MGED reads results for building vectors */
	int	o_pipe[2];	/* MGED writes view parameters */
	int	e_pipe[2];	/* MGED reads textual results */
	FILE	*fp;
	struct rtcheck *rtcp;

	CHECK_DBI_NULL;

	if(argc < 1 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help rtcheck");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_VIEW, "Overlap check in current view" ) )
	  return TCL_ERROR;

	vp = &rt_cmd_vec[0];
	*vp++ = "rtcheck";
	*vp++ = "-s50";
	*vp++ = "-M";
	for( i=1; i < argc; i++ )
		*vp++ = argv[i];
	*vp++ = dbip->dbi_filename;

	setup_rt( vp, 1 );

	(void)pipe( i_pipe );
	(void)pipe( o_pipe );
	(void)pipe( e_pipe );
	(void)signal( SIGINT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
		/* Redirect stdin, stdout and stderr */
		(void)close(0);
		(void)dup(i_pipe[0]);
		(void)close(1);
		(void)dup(o_pipe[1]);
		(void)close(2);
		(void)dup(e_pipe[1]);

		/* close pipes */
		(void)close(i_pipe[0]);
		(void)close(i_pipe[1]);
		(void)close(o_pipe[0]);
		(void)close(o_pipe[1]);
		(void)close(e_pipe[0]);
		(void)close(e_pipe[1]);

		for( i=3; i < 20; i++ )
			(void)close(i);

		(void)signal( SIGINT, SIG_DFL );
		(void)execvp( rt_cmd_vec[0], rt_cmd_vec );
		perror( rt_cmd_vec[0] );
		exit(16);
	}

	/* As parent, send view information down pipe */
	(void)close(i_pipe[0]);
	fp = fdopen(i_pipe[1], "w");
	{
		vect_t temp;
		vect_t eye_model;

		VSET( temp, 0.0, 0.0, 1.0 );
		MAT4X3PNT( eye_model, view_state->vs_view2model, temp );
		rt_write(fp, eye_model );
	}
	(void)fclose(fp);

	/* close write end of pipes */
	(void)close(o_pipe[1]);
	(void)close(e_pipe[1]);

	BU_GETSTRUCT(rtcp, rtcheck);

	/* initialize the rtcheck struct */
	rtcp->fd = o_pipe[0];
	rtcp->fp = fdopen(o_pipe[0], "r");
	rtcp->pid = pid;
	rtcp->vbp = rt_vlblock_init();
	rtcp->vhead = rt_vlblock_find( rtcp->vbp, 0xFF, 0xFF, 0x00 );
	rtcp->csize = view_state->vs_Viewscale * 0.01;

	/* register file handlers */
	Tcl_CreateFileHandler(rtcp->fd, TCL_READABLE,
			      rtcheck_vector_handler, (ClientData)rtcp);
	Tcl_CreateFileHandler(e_pipe[0], TCL_READABLE,
			      rtcheck_output_handler, (ClientData)e_pipe[0]);

	return TCL_OK;
}


/*
 *			B A S E N A M E
 *  
 *  Return basename of path, removing leading slashes and trailing suffix.
 */
static char *
basename( p1, suff )
register char *p1, *suff;
{
	register char *p2, *p3;
	static char buf[128];

	p2 = p1;
	while (*p1) {
		if (*p1++ == '/')
			p2 = p1;
	}
	for(p3=suff; *p3; p3++) 
		;
	while(p1>p2 && p3>suff)
		if(*--p3 != *--p1)
			return(p2);
	strncpy( buf, p2, p1-p2 );
	return(buf);
}

/*
 *			F _ S A V E V I E W
 *
 *  Create a shell script to ray-trace this view.
 *  Any arguments to this command are passed as arguments to RT
 *  in the generated shell script
 */
int
f_saveview(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct solid *sp;
	register int i;
	register FILE *fp;
	char *base;

	CHECK_DBI_NULL;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help saveview");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( (fp = fopen( argv[1], "a")) == NULL )  {
	  perror(argv[1]);
	  return TCL_ERROR;
	}

	base = basename( argv[1], ".sh" );
	(void)chmod( argv[1], 0755 );	/* executable */
	/* Do not specify -v option to rt; batch jobs must print everything. -Mike */
	(void)fprintf(fp, "#!/bin/sh\nrt -M ");
	if( mged_variables->mv_perspective > 0 )
		(void)fprintf(fp, "-p%g", mged_variables->mv_perspective);
	for( i=2; i < argc; i++ )
		(void)fprintf(fp,"%s ", argv[i]);
	(void)fprintf(fp,"\\\n -o %s.pix\\\n $*\\\n", base);
	(void)fprintf(fp," %s\\\n ", dbip->dbi_filename);

	/* Find all unique top-level entries.
	 *  Mark ones already done with s_wflag == UP
	 */
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)
		sp->s_wflag = DOWN;
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
		register struct solid *forw;	/* XXX */
		struct directory *dp = FIRST_SOLID(sp);

		if( sp->s_wflag == UP )
			continue;
		if (dp->d_addr == RT_DIR_PHONY_ADDR) continue;
		(void)fprintf(fp, "'%s' ", dp->d_namep);
		sp->s_wflag = UP;
		for(BU_LIST_PFOR(forw, sp, solid, &HeadSolid.l)){
			if( FIRST_SOLID(forw) == dp )
				forw->s_wflag = UP;
		}
	}
	(void)fprintf(fp,"\\\n 2>> %s.log\\\n", base);
	(void)fprintf(fp," <<EOF\n");

	{
	  vect_t eye_model;

	  rt_set_eye_model(eye_model);
	  rt_write(fp, eye_model);
	}

	(void)fprintf(fp,"\nEOF\n");
	(void)fclose( fp );
	
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)
		sp->s_wflag = DOWN;

	return TCL_OK;
}

/*
 *			F _ R M A T S
 *
 * Load view matrixes from a file.  rmats filename [mode]
 *
 * Modes:
 *	-1	put eye in viewcenter (default)
 *	0	put eye in viewcenter, don't rotate.
 *	1	leave view alone, animate solid named "EYE"
 */
int
f_rmats(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register FILE *fp;
	register struct directory *dp;
	register struct solid *sp = SOLID_NULL;
	vect_t	eye_model;
	vect_t	xlate;
	vect_t	sav_center;
	vect_t	sav_start;
	int	mode;
	fastf_t	scale;
	mat_t	rot;
	register struct bn_vlist *vp;

	CHECK_DBI_NULL;

	if(argc < 2 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help rmats");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_VIEW, "animate from matrix file") )
	  return TCL_ERROR;

	if( (fp = fopen(argv[1], "r")) == NULL )  {
	  perror(argv[1]);
	  return TCL_ERROR;
	}
	mode = -1;
	if( argc > 2 )
		mode = atoi(argv[2]);
	switch(mode)  {
	case 1:
		if( (dp = db_lookup(dbip, "EYE", LOOKUP_NOISY)) == DIR_NULL )  {
			mode = -1;
			break;
		}
		FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
			if( LAST_SOLID(sp) != dp )  continue;
			if( BU_LIST_IS_EMPTY( &(sp->s_vlist) ) )  continue;
			vp = BU_LIST_LAST( bn_vlist, &(sp->s_vlist) );
			VMOVE( sav_start, vp->pt[vp->nused-1] );
			VMOVE( sav_center, sp->s_center );
			Tcl_AppendResult(interp, "animating EYE solid\n", (char *)NULL);
			goto work;
		}
		/* Fall through */
	default:
	case -1:
	  mode = -1;
	  Tcl_AppendResult(interp, "default mode:  eyepoint at (0,0,1) viewspace\n", (char *)NULL);
	  break;
	case 0:
	  Tcl_AppendResult(interp, "rotation supressed, center is eyepoint\n", (char *)NULL);
	  break;
	}
work:
#if 0
	/* If user hits ^C, this will stop, but will leave hanging filedes */
	(void)signal(SIGINT, cur_sigint);
#else
        if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;
#endif
	while( !feof( fp ) &&
	    rt_read( fp, &scale, eye_model, rot ) >= 0 )  {
	    	switch(mode)  {
	    	case -1:
	    		/* First step:  put eye in center */
		       	view_state->vs_Viewscale = scale;
		       	MAT_COPY( view_state->vs_Viewrot, rot );
			MAT_DELTAS( view_state->vs_toViewcenter,
				-eye_model[X],
				-eye_model[Y],
				-eye_model[Z] );
	    		new_mats();
	    		/* Second step:  put eye in front */
	    		VSET( xlate, 0.0, 0.0, -1.0 );	/* correction factor */
	    		MAT4X3PNT( eye_model, view_state->vs_view2model, xlate );
			MAT_DELTAS( view_state->vs_toViewcenter,
				-eye_model[X],
				-eye_model[Y],
				-eye_model[Z] );
	    		new_mats();
	    		break;
	    	case 0:
		       	view_state->vs_Viewscale = scale;
			MAT_IDN(view_state->vs_Viewrot);	/* top view */
			MAT_DELTAS( view_state->vs_toViewcenter,
				-eye_model[X],
				-eye_model[Y],
				-eye_model[Z] );
			new_mats();
	    		break;
	    	case 1:
	    		/* Adjust center for displaylist devices */
	    		VMOVE( sp->s_center, eye_model );

	    		/* Adjust vector list for non-dl devices */
	    		if( BU_LIST_IS_EMPTY( &(sp->s_vlist) ) )  break;
			vp = BU_LIST_LAST( bn_vlist, &(sp->s_vlist) );
	    		VSUB2( xlate, eye_model, vp->pt[vp->nused-1] );
			for( BU_LIST_FOR( vp, bn_vlist, &(sp->s_vlist) ) )  {
				register int	i;
				register int	nused = vp->nused;
				register int	*cmd = vp->cmd;
				register point_t *pt = vp->pt;
				for( i = 0; i < nused; i++,cmd++,pt++ )  {
					switch( *cmd )  {
					case BN_VLIST_POLY_START:
					case BN_VLIST_POLY_VERTNORM:
						break;
					case BN_VLIST_LINE_MOVE:
					case BN_VLIST_LINE_DRAW:
					case BN_VLIST_POLY_MOVE:
					case BN_VLIST_POLY_DRAW:
					case BN_VLIST_POLY_END:
						VADD2( *pt, *pt, xlate );
						break;
					}
				}
			}
	    		break;
	    	}
		view_state->vs_flag = 1;
		refresh();	/* Draw new display */
	}
	if( mode == 1 )  {
    		VMOVE( sp->s_center, sav_center );
		if( BU_LIST_NON_EMPTY( &(sp->s_vlist) ) )  {
			vp = BU_LIST_LAST( bn_vlist, &(sp->s_vlist) );
	    		VSUB2( xlate, sav_start, vp->pt[vp->nused-1] );
			for( BU_LIST_FOR( vp, bn_vlist, &(sp->s_vlist) ) )  {
				register int	i;
				register int	nused = vp->nused;
				register int	*cmd = vp->cmd;
				register point_t *pt = vp->pt;
				for( i = 0; i < nused; i++,cmd++,pt++ )  {
					switch( *cmd )  {
					case BN_VLIST_POLY_START:
					case BN_VLIST_POLY_VERTNORM:
						break;
					case BN_VLIST_LINE_MOVE:
					case BN_VLIST_LINE_DRAW:
					case BN_VLIST_POLY_MOVE:
					case BN_VLIST_POLY_DRAW:
					case BN_VLIST_POLY_END:
						VADD2( *pt, *pt, xlate );
						break;
					}
				}
			}
		}
	}

	fclose(fp);
	(void)mged_svbase();

	(void)signal( SIGINT, SIG_IGN );
	return TCL_OK;
}

/* Save a keyframe to a file */
int
f_savekey(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register FILE *fp;
	fastf_t	time;
	vect_t	eye_model;
	vect_t temp;

	if(argc < 2 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help savekey");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( (fp = fopen( argv[1], "a")) == NULL )  {
	  perror(argv[1]);
	  return TCL_ERROR;
	}
	if( argc > 2 ) {
	  time = atof( argv[2] );
	  (void)fprintf(fp,"%f\n", time);
	}
	/*
	 *  Eye is in conventional place.
	 */
	VSET( temp, 0.0, 0.0, 1.0 );
	MAT4X3PNT( eye_model, view_state->vs_view2model, temp );
	rt_oldwrite(fp, eye_model);
	(void)fclose( fp );

	return TCL_OK;
}

extern int	cm_start();
extern int	cm_vsize();
extern int	cm_eyept();
extern int	cm_lookat_pt();
extern int	cm_vrot();
extern int	cm_end();
extern int	cm_multiview();
extern int	cm_anim();
extern int	cm_tree();
extern int	cm_clean();
extern int	cm_set();
extern int	cm_orientation();

static struct command_tab cmdtab[] = {
	{"start", "frame number", "start a new frame",
		cm_start,	2, 2},
	{"viewsize", "size in mm", "set view size",
		cm_vsize,	2, 2},
	{"eye_pt", "xyz of eye", "set eye point",
		cm_eyept,	4, 4},
	{"lookat_pt", "x y z [yflip]", "set eye look direction, in X-Y plane",
		cm_lookat_pt,	4, 5},
	{"orientation", "quaturnion", "set view direction from quaturnion",
		cm_orientation,	5, 5},
	{"viewrot", "4x4 matrix", "set view direction from matrix",
		cm_vrot,	17,17},
	{"end", 	"", "end of frame setup, begin raytrace",
		cm_end,		1, 1},
	{"multiview", "", "produce stock set of views",
		cm_multiview,	1, 1},
	{"anim", 	"path type args", "specify articulation animation",
		cm_anim,	4, 999},
	{"tree", 	"treetop(s)", "specify alternate list of tree tops",
		cm_tree,	1, 999},
	{"clean", "", "clean articulation from previous frame",
		cm_clean,	1, 1},
	{"set", 	"", "show or set parameters",
		cm_set,		1, 999},
	{(char *)0, (char *)0, (char *)0,
		0,		0, 0}	/* END */
};

/*
 *			F _ P R E V I E W
 *
 *  Preview a new style RT animation scrtip.
 *  Note that the RT command parser code is used, rather than the
 *  MGED command parser, because of the differences in format.
 *  The RT parser expects command handlers of the form "cm_xxx()",
 *  and all communications are done via global variables.
 *
 *  For the moment, the only preview mode is the normal one,
 *  moving the eyepoint as directed.
 *  However, as a bonus, the eye path is left behind as a vector plot.
 */
static vect_t	rtif_eye_model;
static mat_t	rtif_viewrot;
static struct bn_vlblock	*rtif_vbp;
static FILE	*rtif_fp;
static double	rtif_delay;
static struct _mged_variables    rtif_saved_state;       /* saved state variable\s */
static int	rtif_mode;
static int	rtif_desiredframe;
static int	rtif_finalframe;
static int	rtif_currentframe;

/*
 *			R T I F _ S I G I N T
 *
 *  Called on SIGINT from within preview.
 *  Close things down and abort.
 *
 *  WARNING:  If the ^C happened when bu_free() had already done a bu_semaphore_acquire,
 *  then any further calls to bu_free() will hang.
 *  It isn't clear how to handle this.
 */
static void
rtif_sigint( num )
int	num;
{
	if(dbip == DBI_NULL)
	  return;

	write( 2, "rtif_sigint\n", 12);

	/* Restore state variables */
	*mged_variables = rtif_saved_state;	/* struct copy */

	if(rtif_vbp)  {
		rt_vlblock_free(rtif_vbp);
		rtif_vbp = (struct bn_vlblock *)NULL;
	}
	db_free_anim(dbip);	/* Forget any anim commands */
	sig3();			/* Call main SIGINT handler */
	/* NOTREACHED */
}

/*
 *			F _ P R E V I E W
 */
int
f_preview(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	char	*cmd;
	int	c;
	vect_t	temp;

	CHECK_DBI_NULL;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help preview");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_VIEW, "animate viewpoint from new RT file") )
	  return TCL_ERROR;

	/* Save any state variables we plan on changing */
	rtif_saved_state = *mged_variables;	/* struct copy */
	mged_variables->mv_autosize = 0;

	rtif_delay = 0;			/* Full speed, by default */
	rtif_mode = 1;			/* wireframe drawing */
	rtif_desiredframe = 0;
	rtif_finalframe = 0;

	/* Parse options */
	bu_optind = 1;			/* re-init bu_getopt() */
	while( (c=bu_getopt(argc,argv,"d:vD:K:")) != EOF )  {
		switch(c)  {
		case 'd':
			rtif_delay = atof(bu_optarg);
			break;
		case 'D':
			rtif_desiredframe = atof(bu_optarg);
			break;
		case 'K':
			rtif_finalframe = atof(bu_optarg);
			break;
		case 'v':
			rtif_mode = 3;	/* Like "ev" */
			break;
		default:
		  {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "option '%c' unknown\n", c);
		    bu_vls_printf(&tmp_vls, "        -d#     inter-frame delay\n");
		    bu_vls_printf(&tmp_vls, "        -v      polygon rendering (visual)\n");
		    bu_vls_printf(&tmp_vls, "        -D#     desired starting frame\n");
		    bu_vls_printf(&tmp_vls, "        -K#     final frame\n");
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		  }

		  break;
		}
	}
	argc -= bu_optind-1;
	argv += bu_optind-1;

	/* If file is still open from last cmd getting SIGINT, close it */
	if(rtif_fp)  fclose(rtif_fp);
	if( (rtif_fp = fopen(argv[1], "r")) == NULL )  {
	  perror(argv[1]);
	  return TCL_ERROR;
	}

	/* Build list of top-level objects in view, in rt_cmd_vec[] */
	rt_cmd_vec[0] = "tree";
	setup_rt( &rt_cmd_vec[1], 1 );

	rtif_vbp = rt_vlblock_init();

	Tcl_AppendResult(interp, "eyepoint at (0,0,1) viewspace\n", (char *)NULL);

	/*
	 *  Initialize the view to the current one in MGED
	 *  in case a view specification is never given.
	 */
	MAT_COPY(rtif_viewrot, view_state->vs_Viewrot);
	VSET(temp, 0.0, 0.0, 1.0);
	MAT4X3PNT(rtif_eye_model, view_state->vs_view2model, temp);

	if( setjmp( jmp_env ) == 0 )
	  /* If user hits ^C, preview will stop, and clean up */
	  (void)signal(SIGINT, rtif_sigint);
	else
	  return TCL_OK;

	while( ( cmd = rt_read_cmd( rtif_fp )) != NULL )  {
		/* Hack to prevent running framedone scripts prematurely */
		if( cmd[0] == '!' )  {
			if( rtif_currentframe < rtif_desiredframe ||
			    (rtif_finalframe && rtif_currentframe > rtif_finalframe) )  {
				bu_free( (genptr_t)cmd, "preview ! cmd" );
			    	continue;
			}
		}
		if( rt_do_cmd( (struct rt_i *)0, cmd, cmdtab ) < 0 )
		   Tcl_AppendResult(interp, "command failed: ", cmd,
				    "\n", (char *)NULL);
		bu_free( (genptr_t)cmd, "preview cmd" );
	}
	fclose(rtif_fp);
	rtif_fp = NULL;

	cvt_vlblock_to_solids( rtif_vbp, "EYE_PATH", 0 );
	if(rtif_vbp)  {
		rt_vlblock_free(rtif_vbp);
		rtif_vbp = (struct bn_vlblock *)NULL;
	}
	db_free_anim(dbip);	/* Forget any anim commands */

	/* Restore state variables */
	*mged_variables = rtif_saved_state;	/* struct copy */

	(void)mged_svbase();

	(void)signal( SIGINT, SIG_IGN );
	return TCL_OK;
}

/*
 *			F _ N I R T
 *
 *  Invoke nirt with the current view & stuff
 */
int
f_nirt(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register char **vp;
	FILE *fp_in;
	FILE *fp_out, *fp_err;
	int pid, rpid;
	int retcode;
	int pipe_in[2];
	int pipe_out[2];
	int pipe_err[2];
	int use_input_orig = 0;
	vect_t	center_model;
	vect_t dir;
	vect_t cml;
	register int i;
	register struct solid *sp;
	char line[MAXLINE];
	char *val;
	struct bu_vls vls;
	struct bu_vls o_vls;
	struct bu_vls p_vls;
	struct bu_vls t_vls;
	struct bn_vlblock *vbp;
	struct qray_dataList *ndlp;
	struct qray_dataList HeadQRayData;

	CHECK_DBI_NULL;

	if(argc < 1 || MAXARGS < argc){
	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help %s", argv[0]);
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);

	  return TCL_ERROR;
	}

	vp = &rt_cmd_vec[0];
	*vp++ = "nirt";

	/* swipe x, y, z off the end if present */
	if(argc > 3){
	  if(sscanf(argv[argc-3], "%lf", &center_model[X]) == 1 &&
	     sscanf(argv[argc-2], "%lf", &center_model[Y]) == 1 &&
	     sscanf(argv[argc-1], "%lf", &center_model[Z]) == 1){
	    use_input_orig = 1;
	    argc -= 3;
	    VSCALE(center_model, center_model, local2base);
	  }else if(adc_state->adc_draw)
	    *vp++ = "-b";
	}else if(adc_state->adc_draw)
	  *vp++ = "-b";

	if(mged_variables->mv_use_air){
	  *vp++ = "-u";
	  *vp++ = "1";
	}

	/* Calculate point from which to fire ray */
	if(!use_input_orig && adc_state->adc_draw){
	  vect_t  view_ray_orig;

	  VSET(view_ray_orig, (fastf_t)adc_state->adc_dv_x, (fastf_t)adc_state->adc_dv_y, GED_MAX);
	  VSCALE(view_ray_orig, view_ray_orig, INV_GED);
	  MAT4X3PNT(center_model, view_state->vs_view2model, view_ray_orig);
	}else if(!use_input_orig){
	  VSET(center_model, -view_state->vs_toViewcenter[MDX],
	       -view_state->vs_toViewcenter[MDY], -view_state->vs_toViewcenter[MDZ]);
	}

	if( mged_variables->mv_perspective_mode )
	{
		point_t pt, eye;

		/* get eye point */
		VSET(pt, 0.0, 0.0, 1.0);
		MAT4X3PNT(eye, view_state->vs_view2model, pt);
		VSCALE(eye, eye, base2local);

		/* point passed in is actually the aim point */
		VSCALE(cml, center_model, base2local);
		VSUB2(dir, cml, eye);
		VUNITIZE(dir);

		/* copy eye point to cml (cml is used for the "xyz" command to nirt */
		VMOVE(cml, eye);
	} else {
		VSCALE(cml, center_model, base2local);
		VMOVEN(dir, view_state->vs_Viewrot + 8, 3);
		VSCALE(dir, dir, -1.0);
	}

	bu_vls_init(&p_vls);
	bu_vls_printf(&p_vls, "xyz %lf %lf %lf;",
		cml[X], cml[Y], cml[Z]);
	bu_vls_printf(&p_vls, "dir %lf %lf %lf; s",
		dir[X], dir[Y], dir[Z]);

	i = 0;
	if(QRAY_GRAPHICS){

	  *vp++ = "-e";
	  *vp++ = QRAY_FORMAT_NULL;

	  /* first ray  ---- returns partitions */
	  *vp++ = "-e";
	  *vp++ = QRAY_FORMAT_P;

	  /* ray start, direction, and 's' command */
	  *vp++ = "-e";
	  *vp++ = bu_vls_addr(&p_vls);

	  /* second ray  ---- returns overlaps */
	  *vp++ = "-e";
	  *vp++ = QRAY_FORMAT_O;

	  /* ray start, direction, and 's' command */
	  *vp++ = "-e";
	  *vp++ = bu_vls_addr(&p_vls);

	  if(QRAY_TEXT){
	    char *cp;
	    int count;

	    bu_vls_init(&o_vls);

	    /* get 'r' format now; prepend its' format string with a newline */
	    val = bu_vls_addr(&qray_fmts[0].fmt);

	    /* find first '"' */
	    while(*val != '"' && *val != '\0')
	      ++val;

	    if(*val == '\0')
	      goto done;
	    else
	      ++val;	    /* skip first '"' */

	    /* find last '"' */
	    cp = (char *)strrchr(val, '"');

	    if(cp != (char *)NULL) /* found it */
	      count = cp - val;
	    else
	      count = 0;

done:
	    if(*val == '\0')
	      bu_vls_printf(&o_vls, " fmt r \"\\n\" ");
	    else{
	      bu_vls_printf(&o_vls, " fmt r \"\\n%*s\" ", count, val);
	      if(count)
		val += count + 1;
	      bu_vls_printf(&o_vls, "%s", val);
	    }

	    i = 1;

	    *vp++ = "-e";
	    *vp++ = bu_vls_addr(&o_vls);
	  }
	}

	if(QRAY_TEXT){

	  bu_vls_init(&t_vls);

	  /* load vp with formats for printing */
	  for(; qray_fmts[i].type != (char)NULL; ++i)
	    bu_vls_printf(&t_vls, "fmt %c %s; ",
			  qray_fmts[i].type,
			  bu_vls_addr(&qray_fmts[i].fmt));

	  *vp++ = "-e";
	  *vp++ = bu_vls_addr(&t_vls);

	  /* nirt does not like the trailing ';' */
	  bu_vls_trunc(&t_vls, -2);
	}

	/* include nirt script string */
	if (bu_vls_strlen(&qray_script)) {
	  *vp++ = "-e";
	  *vp++ = bu_vls_addr(&qray_script);
	}

        *vp++ = "-e";
        *vp++ = bu_vls_addr(&p_vls);

	for( i=1; i < argc; i++ )
		*vp++ = argv[i];
	*vp++ = dbip->dbi_filename;

	setup_rt( vp, qray_cmd_echo );

	if(use_input_orig){
	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "\nFiring from (%lf, %lf, %lf)...\n",
			center_model[X], center_model[Y], center_model[Z]);
	  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	  bu_vls_free(&vls);
	}else if(adc_state->adc_draw)
	  Tcl_AppendResult(interp, "\nFiring through angle/distance cursor...\n",
			   (char *)NULL);
	else
	  Tcl_AppendResult(interp, "\nFiring from view center...\n", (char *)NULL);


	(void)pipe( pipe_in );
	(void)pipe( pipe_out );
	(void)pipe( pipe_err );
	(void)signal( SIGINT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
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
		for( i=3; i < 20; i++ )
			(void)close(i);
		(void)signal( SIGINT, SIG_DFL );
		(void)execvp( rt_cmd_vec[0], rt_cmd_vec );
		perror( rt_cmd_vec[0] );
		exit(16);
	}

	/* use fp_in to feed view info to nirt */
	(void)close( pipe_in[0] );
	fp_in = fdopen( pipe_in[1], "w" );

	/* use fp_out to read back the result */
	(void)close( pipe_out[1] );
	fp_out = fdopen( pipe_out[0], "r" );

	/* use fp_err to read any error messages */
	(void)close( pipe_err[1] );
	fp_err = fdopen( pipe_err[0], "r" );

	/* send quit command to nirt */
	fwrite( "q\n", 1, 2, fp_in );
	(void)fclose( fp_in );

	bu_vls_free(&p_vls);   /* use to form "partition" part of nirt command above */
	if(QRAY_GRAPHICS){

	  if(QRAY_TEXT)
	    bu_vls_free(&o_vls); /* used to form "overlap" part of nirt command above */

	  BU_LIST_INIT(&HeadQRayData.l);

	  /* handle partitions */
	  while(fgets(line, MAXLINE, fp_out) != (char *)NULL){
	    if(line[0] == '\n'){
	      Tcl_AppendResult(interp, line+1, (char *)NULL);
	      break;
	    }

	    BU_GETSTRUCT(ndlp, qray_dataList);
	    BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	    if(sscanf(line, "%le %le %le %le",
		      &ndlp->x_in, &ndlp->y_in, &ndlp->z_in, &ndlp->los) != 4)
	      break;
	  }

	  vbp = rt_vlblock_init();
	  qray_data_to_vlist(vbp, &HeadQRayData, dir, 0);
	  bu_list_free(&HeadQRayData.l);
	  cvt_vlblock_to_solids(vbp, bu_vls_addr(&qray_basename), 0);
	  rt_vlblock_free(vbp);

	  /* handle overlaps */
	  while(fgets(line, MAXLINE, fp_out) != (char *)NULL){
	    if(line[0] == '\n'){
	      Tcl_AppendResult(interp, line+1, (char *)NULL);
	      break;
	    }

	    BU_GETSTRUCT(ndlp, qray_dataList);
	    BU_LIST_APPEND(HeadQRayData.l.back, &ndlp->l);

	    if(sscanf(line, "%le %le %le %le",
		      &ndlp->x_in, &ndlp->y_in, &ndlp->z_in, &ndlp->los) != 4)
	      break;
	  }
	  vbp = rt_vlblock_init();
	  qray_data_to_vlist(vbp, &HeadQRayData, dir, 1);
	  bu_list_free(&HeadQRayData.l);
	  cvt_vlblock_to_solids(vbp, bu_vls_addr(&qray_basename), 0);
	  rt_vlblock_free(vbp);

	  update_views = 1;
	}

	if(QRAY_TEXT){
	  bu_vls_free(&t_vls);

	  while(fgets(line, MAXLINE, fp_out) != (char *)NULL)
	    Tcl_AppendResult(interp, line, (char *)NULL);
	}

	(void)fclose(fp_out);

	while(fgets(line, MAXLINE, fp_err) != (char *)NULL)
	  Tcl_AppendResult(interp, line, (char *)NULL);
	(void)fclose(fp_err);

	/* Wait for program to finish */
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;	/* NULL */

	if( retcode != 0 )
		pr_wait_status( retcode );

#if 0
	(void)signal(SIGINT, cur_sigint);
#endif

	FOR_ALL_SOLIDS(sp, &HeadSolid.l)
		sp->s_wflag = DOWN;

	return TCL_OK;
}

int
f_vnirt(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  register int i;
  int status;
  fastf_t sf = 1.0 * INV_GED;
  vect_t view_ray_orig;
  vect_t center_model;
  struct bu_vls vls;
  struct bu_vls x_vls;
  struct bu_vls y_vls;
  struct bu_vls z_vls;
  char **av;

  CHECK_DBI_NULL;

  if(argc < 3){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
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
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }
  view_ray_orig[Z] = GED_MAX;
  argc -= 2;

  av = (char **)bu_malloc(sizeof(char *) * (argc + 4), "f_vnirt: av");

  /* Calculate point from which to fire ray */
  VSCALE(view_ray_orig, view_ray_orig, sf);
  MAT4X3PNT(center_model, view_state->vs_view2model, view_ray_orig);
  VSCALE(center_model, center_model, base2local);

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

  status = f_nirt(clientData, interp, argc + 3, av);

  bu_vls_free(&x_vls);
  bu_vls_free(&y_vls);
  bu_vls_free(&z_vls);
  bu_free((genptr_t)av, "f_vnirt: av");

  return status;
}

int
cm_start(argc, argv)
char	**argv;
int	argc;
{
	if( argc < 2 )
		return(-1);
	rtif_currentframe = atoi(argv[1]);
	tree_walk_needed = 0;
	return(0);
}

int
cm_vsize(argc, argv)
char	**argv;
int	argc;
{
	if( argc < 2 )
		return(-1);
	view_state->vs_Viewscale = atof(argv[1])*0.5;
	return(0);
}

int
cm_eyept(argc, argv)
char	**argv;
int	argc;
{
	if( argc < 4 )
		return(-1);
	rtif_eye_model[X] = atof(argv[1]);
	rtif_eye_model[Y] = atof(argv[2]);
	rtif_eye_model[Z] = atof(argv[3]);
	/* Processing is deferred until cm_end() */
	return(0);
}

int
cm_lookat_pt(argc, argv)
int	argc;
char	**argv;
{
	point_t	pt;
	vect_t	dir;

	if( argc < 4 )
		return(-1);
	pt[X] = atof(argv[1]);
	pt[Y] = atof(argv[2]);
	pt[Z] = atof(argv[3]);

	VSUB2( dir, pt, rtif_eye_model );
	VUNITIZE( dir );

#if 1
	/*
	   At the moment bn_mat_lookat will return NAN's if the direction vector
	   is aligned with the Z axis. The following is a temporary workaround.
	 */
	{
	  vect_t neg_Z_axis;

	  VSET(neg_Z_axis, 0.0, 0.0, -1.0);
	  bn_mat_fromto( rtif_viewrot, dir, neg_Z_axis);
	}
#else
	bn_mat_lookat( rtif_viewrot, dir, yflip );
#endif

	/*  Final processing is deferred until cm_end(), but eye_pt
	 *  must have been specified before here (for now)
	 */
	return(0);
}

int
cm_vrot(argc, argv)
char	**argv;
int	argc;
{
	register int	i;

	if( argc < 17 )
		return(-1);
	for( i=0; i<16; i++ )
		rtif_viewrot[i] = atof(argv[i+1]);
	/* Processing is deferred until cm_end() */
	return(0);
}

int
cm_orientation( argc, argv )
int	argc;
char	**argv;
{
	register int	i;
	quat_t		quat;

	for( i=0; i<4; i++ )
		quat[i] = atof( argv[i+1] );
	quat_quat2mat( rtif_viewrot, quat );
	return(0);
}

/*
 *			C M _ E N D
 */
int
cm_end(argc, argv)
char	**argv;
int	argc;
{
	vect_t	xlate;
	vect_t	new_cent;
	vect_t	xv, yv;			/* view x, y */
	vect_t	xm, ym;			/* model x, y */
	struct bu_list		*vhead = &rtif_vbp->head[0];

	/* Only display the frames the user is interested in */
	if( rtif_currentframe < rtif_desiredframe )  return 0;
	if( rtif_finalframe && rtif_currentframe > rtif_finalframe )  return 0;

	/* Record eye path as a polyline.  Move, then draws */
	if( BU_LIST_IS_EMPTY( vhead ) )  {
		RT_ADD_VLIST( vhead, rtif_eye_model, BN_VLIST_LINE_MOVE );
	} else {
		RT_ADD_VLIST( vhead, rtif_eye_model, BN_VLIST_LINE_DRAW );
	}
	
	/* First step:  put eye at view center (view 0,0,0) */
       	MAT_COPY( view_state->vs_Viewrot, rtif_viewrot );
	MAT_DELTAS_VEC_NEG( view_state->vs_toViewcenter, rtif_eye_model );
	new_mats();

	/*
	 * Compute camera orientation notch to right (+X) and up (+Y)
	 * Done here, with eye in center of view.
	 */
	VSET( xv, 0.05, 0.0, 0.0 );
	VSET( yv, 0.0, 0.05, 0.0 );
	MAT4X3PNT( xm, view_state->vs_view2model, xv );
	MAT4X3PNT( ym, view_state->vs_view2model, yv );
	RT_ADD_VLIST( vhead, xm, BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, rtif_eye_model, BN_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, ym, BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, rtif_eye_model, BN_VLIST_LINE_MOVE );

	/*  Second step:  put eye at view 0,0,1.
	 *  For eye to be at 0,0,1, the old 0,0,-1 needs to become 0,0,0.
	 */
	VSET( xlate, 0.0, 0.0, -1.0 );	/* correction factor */
	MAT4X3PNT( new_cent, view_state->vs_view2model, xlate );
	MAT_DELTAS_VEC_NEG( view_state->vs_toViewcenter, new_cent );
	new_mats();

	/* If new treewalk is needed, get new objects into view. */
	if( tree_walk_needed )  {
	  char *av[2];

	  av[0] = "Z";
	  av[1] = NULL;

	  (void)f_zap( (ClientData)NULL, interp, 1, av );
	  edit_com( rt_cmd_vec_len, rt_cmd_vec, rtif_mode, 0 );
	}

	view_state->vs_flag = 1;
	refresh();	/* Draw new display */
	view_state->vs_flag = 1;
	if( rtif_delay > 0 )  {
		struct timeval tv;
		fd_set readfds;
	
		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		tv.tv_sec = (long)rtif_delay;
		tv.tv_usec = (long)((rtif_delay - tv.tv_sec) * 1000000);
		select( fileno(stdin)+1, &readfds, (fd_set *)0, (fd_set *)0, &tv );
	}
	return(0);
}

int
cm_multiview(argc, argv)
char	**argv;
int	argc;
{
	return(-1);
}

/*
 *			C M _ A N I M
 *
 *  Parse any "anim" commands, and lodge their info in the directory structs.
 */
int
cm_anim(argc, argv)
int	argc;
char	**argv;
{

  if(dbip == DBI_NULL)
    return 0;

  if( db_parse_anim( dbip, argc, (const char **)argv ) < 0 )  {
    Tcl_AppendResult(interp, "cm_anim:  ", argv[1], " ", argv[2], " failed\n", (char *)NULL);
    return(-1);		/* BAD */
  }

  tree_walk_needed = 1;

  return(0);
}

/*
 *			C M _ T R E E
 *
 *  Replace list of top-level objects in rt_cmd_vec[].
 */
int
cm_tree(argc, argv)
char	**argv;
int	argc;
{
	register int	i = 1;
	char *cp = rt_cmd_storage;

	for( i = 1;  i < argc && i < MAXARGS; i++ )  {
		strcpy(cp, argv[i]);
		rt_cmd_vec[i] = cp;
		cp += strlen(cp) + 1;
	}
	rt_cmd_vec[i] = (char *)0;
	rt_cmd_vec_len = i;

	tree_walk_needed = 1;

	return(0);
}

/*
 *			C M _ C L E A N
 *
 *  Clear current view.
 */
int
cm_clean(argc, argv)
char	**argv;
int	argc;
{
	if(dbip == DBI_NULL)
	  return 0;

	/*f_zap( (ClientData)NULL, interp, 0, (char **)0 );*/

	/* Free animation structures */
	db_free_anim(dbip);

	tree_walk_needed = 1;
	return 0;
}

int
cm_set(argc, argv)
char	**argv;
int	argc;
{
	return(-1);
}

extern char **skewer_solids ();

int
cmd_solids_on_ray (clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char		**argv;
{
    char			**snames;
    int				h = 0;
    int				v = 0;
    int				i;		/* Dummy loop index */
    register struct solid	*sp;
    double			t;
    double			t_in;
    struct bu_vls		vls;
    point_t			ray_orig;
    vect_t			ray_dir;
    point_t			extremum[2];
    point_t			minus, plus;	/* vrts of solid's bnding bx */
    vect_t			unit_H, unit_V;

    if(argc < 1 || 3 < argc){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "helpdevel solids_on_ray");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    if ((argc != 1) && (argc != 3))
    {
	Tcl_AppendResult(interp, "Usage: 'solids_on_ray [h v]'", (char *)NULL);
	return (TCL_ERROR);
    }
    if ((argc == 3) &&
        ((Tcl_GetInt(interp, argv[1], &h) != TCL_OK)
      || (Tcl_GetInt(interp, argv[2], &v) != TCL_OK)))
    {
	Tcl_AppendResult(interp, "\nUsage: 'solids_on_ray h v'", NULL);
	return (TCL_ERROR);
    }

    if (((int)GED_MIN > h)  || (h > (int)GED_MAX) || ((int)GED_MIN > v)  || (v > (int)GED_MAX))
    {
	Tcl_AppendResult(interp, "Screen coordinates out of range\n",
	    "Must be between +/-2048", NULL);
	return (TCL_ERROR);
    }

    VSET(ray_orig, -view_state->vs_toViewcenter[MDX],
	-view_state->vs_toViewcenter[MDY], -view_state->vs_toViewcenter[MDZ]);
    /*
     * Compute bounding box of all objects displayed.
     * Borrowed from size_reset() in chgview.c
     */
    for (i = 0; i < 3; ++i)
    {
	extremum[0][i] = INFINITY;
	extremum[1][i] = -INFINITY;
    }
    FOR_ALL_SOLIDS (sp, &HeadSolid.l)
    {
	    minus[X] = sp->s_center[X] - sp->s_size;
	    minus[Y] = sp->s_center[Y] - sp->s_size;
	    minus[Z] = sp->s_center[Z] - sp->s_size;
	    VMIN( extremum[0], minus );
	    plus[X] = sp->s_center[X] + sp->s_size;
	    plus[Y] = sp->s_center[Y] + sp->s_size;
	    plus[Z] = sp->s_center[Z] + sp->s_size;
	    VMAX( extremum[1], plus );
    }
    VMOVEN(ray_dir, view_state->vs_Viewrot + 8, 3);
    VSCALE(ray_dir, ray_dir, -1.0);
    for (i = 0; i < 3; ++i)
	if (NEAR_ZERO(ray_dir[i], 1e-10))
	    ray_dir[i] = 0.0;
    if ((ray_orig[X] >= extremum[0][X]) &&
	(ray_orig[X] <= extremum[1][X]) &&
	(ray_orig[Y] >= extremum[0][Y]) &&
	(ray_orig[Y] <= extremum[1][Y]) &&
	(ray_orig[Z] >= extremum[0][Z]) &&
	(ray_orig[Z] <= extremum[1][Z]))
    {
	t_in = -INFINITY;
	for (i = 0; i < 6; ++i)
	{
	    if (ray_dir[i%3] == 0)
		continue;
	    t = (extremum[i/3][i%3] - ray_orig[i%3]) /
		    ray_dir[i%3];
	    if ((t < 0) && (t > t_in))
		t_in = t;
	}
	VJOIN1(ray_orig, ray_orig, t_in, ray_dir);
    }

    VMOVEN(unit_H, view_state->vs_model2view, 3);
    VMOVEN(unit_V, view_state->vs_model2view + 4, 3);
    VJOIN1(ray_orig, ray_orig, h * view_state->vs_Viewscale * INV_GED, unit_H);
    VJOIN1(ray_orig, ray_orig, v * view_state->vs_Viewscale * INV_GED, unit_V);

    /*
     *	Build a list of all the top-level objects currently displayed
     */
    rt_cmd_vec_len = build_tops(&rt_cmd_vec[0], &rt_cmd_vec[MAXARGS]);
    
    bu_vls_init(&vls);
    start_catching_output(&vls);
    snames = skewer_solids(rt_cmd_vec_len, rt_cmd_vec, ray_orig, ray_dir, 1);
    stop_catching_output(&vls);

    if (snames == 0)
    {
	Tcl_AppendResult(interp, "Error executing skewer_solids: ", (char *)NULL);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return (TCL_ERROR);
    }

    bu_vls_free(&vls);

    for (i = 0; snames[i] != 0; ++i)
	Tcl_AppendElement(interp, snames[i]);
    
    bu_free((genptr_t) snames, "solid names");

    return TCL_OK;
}


/*
 * List the objects currently being drawn.
 */
int 
cmd_who (clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char 		**argv;
{
#if 0
	CHECK_DBI_NULL;

	return invoke_db_wrapper(interp, argc, argv);
#else
	register struct solid *sp;
	int skip_real, skip_phony;

	if(argc < 1 || 2 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help who");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	skip_real = 0;
	skip_phony = 1;
	if (argc > 1) {
		switch (argv[1][0]) {
		case 'b':
			skip_real = 0;
			skip_phony = 0;
			break;
		case 'p':
			skip_real = 1;
			skip_phony = 0;
			break;
		case 'r':
			skip_real = 0;
			skip_phony = 1;
			break;
		default:
			Tcl_AppendResult(interp,"who: argument not understood\n", (char *)NULL);
			return TCL_ERROR;
		}
	}
		

	/* Find all unique top-level entries.
	 *  Mark ones already done with s_wflag == UP
	 */
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)
	  sp->s_wflag = DOWN;
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
	  register struct solid *forw;	/* XXX */
	  struct directory *dp = FIRST_SOLID(sp);

	  if( sp->s_wflag == UP )
	    continue;
	  if (dp->d_addr == RT_DIR_PHONY_ADDR){
	    if (skip_phony) continue;
	  } else {
	    if (skip_real) continue;
	  }
	  Tcl_AppendResult(interp, dp->d_namep, " ", (char *)NULL);
	  sp->s_wflag = UP;
	  FOR_REST_OF_SOLIDS(forw, sp, &HeadSolid.l){
	    if( FIRST_SOLID(forw) == dp )
	      forw->s_wflag = UP;
	  }
	}
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)
		sp->s_wflag = DOWN;

	return TCL_OK;
#endif
}
