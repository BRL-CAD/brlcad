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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>		/* For struct timeval */

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"
#include "./sedit.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

void		setup_rt();

static int	tree_walk_needed;

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

	if( status == 0 )  {
		rt_log("Normal exit\n");
		return;
	}
	rt_log("Abnormal exit x%x", status);
	if( core )
		rt_log(", core dumped");
	if( sig )
		rt_log(", terminating signal = %d", sig );
	else
		rt_log(", return (exit) code = %d", ret );
	rt_log("\n");
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
		(void)fprintf( fp, "%.9e ", Viewrot[i] );
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
	(void)fprintf(fp, "eye_pt %.15e %.15e %.15e;\n",
		eye_model[X], eye_model[Y], eye_model[Z] );
#if 0
	(void)fprintf(fp, "viewrot ");
	for( i=0; i < 16; i++ )  {
		(void)fprintf( fp, "%.15e ", Viewrot[i] );
		if( (i%4) == 3 )
			(void)fprintf(fp, "\n");
	}
	(void)fprintf(fp, ";\n");
#else
	quat_mat2quat( quat, Viewrot );
	(void)fprintf(fp, "orientation %.15e %.15e %.15e %.15e;\n",
		V4ARGS( quat ) );
#endif
#define DIR_USED	0x80
	(void)fprintf(fp, "start 0; clean;\n");
	FOR_ALL_SOLIDS(sp) {
		for (i=0;i<=sp->s_last;i++) {
			sp->s_path[i]->d_flags &= ~DIR_USED;
		}
	}
	FOR_ALL_SOLIDS(sp) {
		for (i=0; i<=sp->s_last; i++ ) {
			if (!(sp->s_path[i]->d_flags & DIR_USED)) {
				register struct animate *anp;
				for (anp = sp->s_path[i]->d_animate; anp;
				    anp=anp->an_forw) {
					db_write_anim(fp, anp);
				}
				sp->s_path[i]->d_flags |= DIR_USED;
			}
		}
	}

	FOR_ALL_SOLIDS(sp) {
		for (i=0;i<=sp->s_last;i++) {
			sp->s_path[i]->d_flags &= ~DIR_USED;
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
build_tops(start, end)
char **start;
register char **end;
{
	register char **vp = start;
	register struct solid *sp;

	/*
	 * Find all unique top-level entrys.
	 *  Mark ones already done with s_iflag == UP
	 */
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
	FOR_ALL_SOLIDS( sp )  {
		register struct solid *forw;

		if( sp->s_iflag == UP )
			continue;
		if( sp->s_path[0]->d_addr == RT_DIR_PHONY_ADDR )
			continue;	/* Ignore overlays, predictor, etc */
		if( vp < end )
			*vp++ = sp->s_path[0]->d_namep;
		else  {
			rt_log("mged: ran out of comand vector space  at %s\n",
				sp->s_path[0]->d_namep );
			break;
		}
		sp->s_iflag = UP;
		for( forw=sp->s_forw; forw != &HeadSolid; forw=forw->s_forw) {
			if( forw->s_path[0] == sp->s_path[0] )
				forw->s_iflag = UP;
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
#define LEN	2000
static char	*rt_cmd_vec[LEN];
static int	rt_cmd_vec_len;
static char	rt_cmd_storage[LEN*9];

void
setup_rt( vp )
register char	**vp;
{
	rt_cmd_vec_len = vp - rt_cmd_vec;
	rt_cmd_vec_len += build_tops(vp, &rt_cmd_vec[LEN]);

	/* Print out the command we are about to run */
	vp = &rt_cmd_vec[0];
	while( *vp )
		rt_log("%s ", *vp++ );
	rt_log("\n");

}

/*
 *			R U N _ R T
 */
run_rt()
{
	register struct solid *sp;
	register int i;
	int pid, rpid;
	int retcode;
	int o_pipe[2];
	FILE *fp;

	(void)pipe( o_pipe );
	(void)signal( SIGINT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
		(void)close(0);
		(void)dup( o_pipe[0] );
		for( i=3; i < 20; i++ )
			(void)close(i);
		(void)signal( SIGINT, SIG_DFL );
		(void)execvp( rt_cmd_vec[0], rt_cmd_vec );
		perror( rt_cmd_vec[0] );
		exit(16);
	}

	/* As parent, send view information down pipe */
	(void)close( o_pipe[0] );
	fp = fdopen( o_pipe[1], "w" );
	{
		vect_t temp;
		vect_t eye_model;

		VSET( temp, 0, 0, 1 );
		MAT4X3PNT( eye_model, view2model, temp );
#if 0
		/* This old way is no longer needed for RT */
		rt_oldwrite(fp, eye_model );
#endif
		rt_write(fp, eye_model );
	}
	(void)fclose( fp );

	/* Wait for program to finish */
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;	/* NULL */
	if( retcode != 0 )
		pr_wait_status( retcode );
	(void)signal(SIGINT, cur_sigint);

	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;

	return(retcode);
}

/*
 *			F _ R T
 */
int
f_rt( argc, argv )
int	argc;
char	**argv;
{
	register char **vp;
	register int i;
	int retcode;
	char *dm;
	int	needs_reattach;
	char	pstring[32];

	if( not_state( ST_VIEW, "Ray-trace of current view" ) )
		return CMD_BAD;

	/*
	 * This may be a workstation where RT and MGED have to share the
	 * display, so let display go.  We will try to reattach at the end.
	 */
	dm = dmp->dmr_name;
	if( needs_reattach = dmp->dmr_releasedisplay )
		release();		/* changes dmp */

	vp = &rt_cmd_vec[0];
	*vp++ = "rt";
	*vp++ = "-s50";
	*vp++ = "-M";
	if( mged_variables.perspective > 0 )  {
		(void)sprintf(pstring, "-p%g", mged_variables.perspective);
		*vp++ = pstring;
	}
	for( i=1; i < argc; i++ )
		*vp++ = argv[i];
	*vp++ = dbip->dbi_filename;

	setup_rt( vp );
	retcode = run_rt();
	if( needs_reattach && retcode == 0 )  {
		/* Wait for a return, then reattach display */
		rt_log("Press RETURN to reattach\007\n");
		while( getchar() != '\n' )
			/* NIL */  ;
	}
	if( needs_reattach )
		attach( dm );

	return CMD_OK;
}

/*
 *			F _ R R T
 *
 *  Invoke any program with the current view & stuff, just like
 *  an "rt" command (above).
 *  Typically used to invoke a remote RT (hence the name).
 */
int
f_rrt( argc, argv )
int	argc;
char	**argv;
{
	register char **vp;
	register int i;
	int	retcode;
	char	*dm;
	int	needs_reattach;

	if( not_state( ST_VIEW, "Ray-trace of current view" ) )
		return CMD_BAD;

	/*
	 * This may be a workstation where RT and MGED have to share the
	 * display, so let display go.  We will try to reattach at the end.
	 */
	dm = dmp->dmr_name;
	if( needs_reattach = dmp->dmr_releasedisplay )
		release();		/* changes dmp */

	vp = &rt_cmd_vec[0];
	for( i=1; i < argc; i++ )
		*vp++ = argv[i];
	*vp++ = dbip->dbi_filename;

	setup_rt( vp );
	retcode = run_rt();
	if( needs_reattach && retcode == 0 )  {
		/* Wait for a return, then reattach display */
		rt_log("Press RETURN to reattach\007\n");
		while( getchar() != '\n' )
			/* NIL */  ;
	}
	if( needs_reattach )
		attach( dm );

	return CMD_OK;
}

/*
 *			F _ R T C H E C K
 *
 *  Invoke "rtcheck" to find overlaps, and display them as a vector overlay.
 */
int
f_rtcheck( argc, argv )
int	argc;
char	**argv;
{
	register char **vp;
	register int i;
	int	pid, rpid;
	int	retcode;
	int	o_pipe[2];
	int	i_pipe[2];
	FILE	*fp;
	struct solid *sp;
	struct rt_vlblock	*vbp;

	if( not_state( ST_VIEW, "Overlap check in current view" ) )
		return CMD_BAD;

	vp = &rt_cmd_vec[0];
	*vp++ = "rtcheck";
	*vp++ = "-s50";
	*vp++ = "-M";
	for( i=1; i < argc; i++ )
		*vp++ = argv[i];
	*vp++ = dbip->dbi_filename;

	setup_rt( vp );

	(void)pipe( o_pipe );			/* output from mged */
	(void)pipe( i_pipe );			/* input back to mged */
	(void)signal( SIGINT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
		(void)close(0);
		(void)dup( o_pipe[0] );
		(void)close(1);
		(void)dup( i_pipe[1] );
		for( i=3; i < 20; i++ )
			(void)close(i);

		(void)signal( SIGINT, SIG_DFL );
		(void)execvp( rt_cmd_vec[0], rt_cmd_vec );
		perror( rt_cmd_vec[0] );
		exit(16);
	}

	/* As parent, send view information down pipe */
	(void)close( o_pipe[0] );
	fp = fdopen( o_pipe[1], "w" );
	{
		vect_t temp;
		vect_t eye_model;

		VSET( temp, 0, 0, 1 );
		MAT4X3PNT( eye_model, view2model, temp );
#if 0
		/* This old way is no longer needed for RT */
		rt_oldwrite(fp, eye_model );
#endif
		rt_write(fp, eye_model );
	}
	(void)fclose( fp );

	/* Prepare to receive UNIX-plot back from child */
	(void)close(i_pipe[1]);
	fp = fdopen(i_pipe[0], "r");
	vbp = rt_vlblock_init();
	(void)rt_uplot_to_vlist( vbp, fp, Viewscale * 0.01 );
	fclose(fp);

	/* Wait for program to finish */
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;	/* NULL */
	if( retcode != 0 )
		pr_wait_status( retcode );
	(void)signal(SIGINT, cur_sigint);

	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;

	/* Add overlay */
	cvt_vlblock_to_solids( vbp, "OVERLAPS", 0 );
	rt_vlblock_free(vbp);
	dmaflag = 1;

	return CMD_OK;
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
 */
int
f_saveview( argc, argv )
int	argc;
char	**argv;
{
	register struct solid *sp;
	register int i;
	register FILE *fp;
	char *base;

	if( (fp = fopen( argv[1], "a")) == NULL )  {
		perror(argv[1]);
		return CMD_BAD;
	}
	base = basename( argv[1], ".sh" );
	(void)chmod( argv[1], 0755 );	/* executable */
	(void)fprintf(fp, "#!/bin/sh\nrt -M ");
	if( mged_variables.perspective > 0 )
		(void)fprintf(fp, "-p%g", mged_variables.perspective);
	for( i=2; i < argc; i++ )
		(void)fprintf(fp,"%s ", argv[i]);
	(void)fprintf(fp,"\\\n $*\\\n -o %s.pix\\\n", base);
	(void)fprintf(fp," %s\\\n ", dbip->dbi_filename);

	/* Find all unique top-level entrys.
	 *  Mark ones already done with s_iflag == UP
	 */
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
	FOR_ALL_SOLIDS( sp )  {
		register struct solid *forw;	/* XXX */

		if( sp->s_iflag == UP )
			continue;
		if (sp->s_path[0]->d_addr == RT_DIR_PHONY_ADDR) continue;
		(void)fprintf(fp, "'%s' ", sp->s_path[0]->d_namep);
		sp->s_iflag = UP;
		for( forw=sp->s_forw; forw != &HeadSolid; forw=forw->s_forw) {
			if( forw->s_path[0] == sp->s_path[0] )
				forw->s_iflag = UP;
		}
	}
	(void)fprintf(fp,"\\\n 2>> %s.log\\\n", base);
	(void)fprintf(fp," <<EOF\n");
	{
		vect_t temp;
		vect_t eye_model;

		VSET( temp, 0, 0, 1 );
		MAT4X3PNT( eye_model, view2model, temp );
		rt_write(fp, eye_model);
	}
	(void)fprintf(fp,"\nEOF\n");
	(void)fclose( fp );
	
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;

	return CMD_OK;
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
f_rmats( argc, argv )
int	argc;
char	**argv;
{
	register FILE *fp;
	register struct directory *dp;
	register struct solid *sp = SOLID_NULL;
	union record	rec;
	vect_t	eye_model;
	vect_t	xlate;
	vect_t	sav_center;
	vect_t	sav_start;
	int	mode;
	fastf_t	scale;
	mat_t	rot;
	register struct rt_vlist *vp;

	if( not_state( ST_VIEW, "animate from matrix file") )
		return CMD_BAD;

	if( (fp = fopen(argv[1], "r")) == NULL )  {
		perror(argv[1]);
		return CMD_BAD;
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
		db_get( dbip,  dp, &rec, 0 , 1);
		FOR_ALL_SOLIDS(sp)  {
			if( sp->s_path[sp->s_last] != dp )  continue;
			if( RT_LIST_IS_EMPTY( &(sp->s_vlist) ) )  continue;
			vp = RT_LIST_LAST( rt_vlist, &(sp->s_vlist) );
			VMOVE( sav_start, vp->pt[vp->nused-1] );
			VMOVE( sav_center, sp->s_center );
			rt_log("animating EYE solid\n");
			goto work;
		}
		/* Fall through */
	default:
	case -1:
		mode = -1;
		rt_log("default mode:  eyepoint at (0,0,1) viewspace\n");
		break;
	case 0:
		rt_log("rotation supressed, center is eyepoint\n");
		break;
	}
work:
	/* If user hits ^C, this will stop, but will leave hanging filedes */
	(void)signal(SIGINT, cur_sigint);
	while( !feof( fp ) &&
	    rt_read( fp, &scale, eye_model, rot ) >= 0 )  {
	    	switch(mode)  {
	    	case -1:
	    		/* First step:  put eye in center */
		       	Viewscale = scale;
		       	mat_copy( Viewrot, rot );
			MAT_DELTAS( toViewcenter,
				-eye_model[X],
				-eye_model[Y],
				-eye_model[Z] );
	    		new_mats();
	    		/* Second step:  put eye in front */
	    		VSET( xlate, 0, 0, -1 );	/* correction factor */
	    		MAT4X3PNT( eye_model, view2model, xlate );
			MAT_DELTAS( toViewcenter,
				-eye_model[X],
				-eye_model[Y],
				-eye_model[Z] );
	    		new_mats();
	    		break;
	    	case 0:
		       	Viewscale = scale;
			mat_idn(Viewrot);	/* top view */
			MAT_DELTAS( toViewcenter,
				-eye_model[X],
				-eye_model[Y],
				-eye_model[Z] );
			new_mats();
	    		break;
	    	case 1:
	    		/* Adjust center for displaylist devices */
	    		VMOVE( sp->s_center, eye_model );

	    		/* Adjust vector list for non-dl devices */
	    		if( RT_LIST_IS_EMPTY( &(sp->s_vlist) ) )  break;
			vp = RT_LIST_LAST( rt_vlist, &(sp->s_vlist) );
	    		VSUB2( xlate, eye_model, vp->pt[vp->nused-1] );
			for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
				register int	i;
				register int	nused = vp->nused;
				register int	*cmd = vp->cmd;
				register point_t *pt = vp->pt;
				for( i = 0; i < nused; i++,cmd++,pt++ )  {
					switch( *cmd )  {
					case RT_VLIST_POLY_START:
					case RT_VLIST_POLY_VERTNORM:
						break;
					case RT_VLIST_LINE_MOVE:
					case RT_VLIST_LINE_DRAW:
					case RT_VLIST_POLY_MOVE:
					case RT_VLIST_POLY_DRAW:
					case RT_VLIST_POLY_END:
						VADD2( *pt, *pt, xlate );
						break;
					}
				}
			}
	    		break;
	    	}
		dmaflag = 1;
		refresh();	/* Draw new display */
	}
	if( mode == 1 )  {
    		VMOVE( sp->s_center, sav_center );
		if( RT_LIST_NON_EMPTY( &(sp->s_vlist) ) )  {
			vp = RT_LIST_LAST( rt_vlist, &(sp->s_vlist) );
	    		VSUB2( xlate, sav_start, vp->pt[vp->nused-1] );
			for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
				register int	i;
				register int	nused = vp->nused;
				register int	*cmd = vp->cmd;
				register point_t *pt = vp->pt;
				for( i = 0; i < nused; i++,cmd++,pt++ )  {
					switch( *cmd )  {
					case RT_VLIST_POLY_START:
					case RT_VLIST_POLY_VERTNORM:
						break;
					case RT_VLIST_LINE_MOVE:
					case RT_VLIST_LINE_DRAW:
					case RT_VLIST_POLY_MOVE:
					case RT_VLIST_POLY_DRAW:
					case RT_VLIST_POLY_END:
						VADD2( *pt, *pt, xlate );
						break;
					}
				}
			}
		}
	}
	dmaflag = 1;
	fclose(fp);

	return CMD_OK;
}

/* Save a keyframe to a file */
int
f_savekey( argc, argv )
int	argc;
char	**argv;
{
	register FILE *fp;
	fastf_t	time;
	vect_t	eye_model;
	vect_t temp;

	if( (fp = fopen( argv[1], "a")) == NULL )  {
		perror(argv[1]);
		return CMD_BAD;
	}
	if( argc > 2 ) {
		time = atof( argv[2] );
		(void)fprintf(fp,"%f\n", time);
	}
	/*
	 *  Eye is in conventional place.
	 */
	VSET( temp, 0, 0, 1 );
	MAT4X3PNT( eye_model, view2model, temp );
	rt_oldwrite(fp, eye_model);
	(void)fclose( fp );

	return CMD_OK;
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
	"start", "frame number", "start a new frame",
		cm_start,	2, 2,
	"viewsize", "size in mm", "set view size",
		cm_vsize,	2, 2,
	"eye_pt", "xyz of eye", "set eye point",
		cm_eyept,	4, 4,
	"lookat_pt", "x y z [yflip]", "set eye look direction, in X-Y plane",
		cm_lookat_pt,	4, 5,
	"orientation", "quaturnion", "set view direction from quaturnion",
		cm_orientation,	5, 5,
	"viewrot", "4x4 matrix", "set view direction from matrix",
		cm_vrot,	17,17,
	"end", 	"", "end of frame setup, begin raytrace",
		cm_end,		1, 1,
	"multiview", "", "produce stock set of views",
		cm_multiview,	1, 1,
	"anim", 	"path type args", "specify articulation animation",
		cm_anim,	4, 999,
	"tree", 	"treetop(s)", "specify alternate list of tree tops",
		cm_tree,	1, 999,
	"clean", "", "clean articulation from previous frame",
		cm_clean,	1, 1,
	"set", 	"", "show or set parameters",
		cm_set,		1, 999,
	(char *)0, (char *)0, (char *)0,
		0,		0, 0	/* END */
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
static struct rt_vlblock	*rtif_vbp;
static FILE	*rtif_fp;
static double	rtif_delay;
static struct mged_variables	rtif_saved_state;	/* saved state variables */
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
 *  WARNING:  If the ^C happened when rt_free() had already done a RES_ACQUIRE,
 *  then any further calls to rt_free() will hang.
 *  It isn't clear how to handle this.
 */
static void
rtif_sigint( num )
int	num;
{
	write( 2, "rtif_sigint\n", 12);

	/* Restore state variables */
	mged_variables = rtif_saved_state;	/* struct copy */

	if(rtif_vbp)  {
		rt_vlblock_free(rtif_vbp);
		rtif_vbp = (struct rt_vlblock *)NULL;
	}
	db_free_anim(dbip);	/* Forget any anim commands */
	sig2();			/* Call main SIGINT handler */
	/* NOTREACHED */
}

/*
 *			F _ P R E V I E W
 */
int
f_preview( argc, argv )
int	argc;
char	**argv;
{
	char	*cmd;
	int	c;
	vect_t	temp;

	if( not_state( ST_VIEW, "animate viewpoint from new RT file") )
		return CMD_BAD;

	/* Save any state variables we plan on changing */
	rtif_saved_state = mged_variables;	/* struct copy */
	mged_variables.autosize = 0;

	rtif_delay = 0;			/* Full speed, by default */
	rtif_mode = 1;			/* wireframe drawing */
	rtif_desiredframe = 0;
	rtif_finalframe = 0;

	/* Parse options */
	optind = 1;			/* re-init getopt() */
	while( (c=getopt(argc,argv,"d:vD:K:")) != EOF )  {
		switch(c)  {
		case 'd':
			rtif_delay = atof(optarg);
			break;
		case 'D':
			rtif_desiredframe = atof(optarg);
			break;
		case 'K':
			rtif_finalframe = atof(optarg);
			break;
		case 'v':
			rtif_mode = 3;	/* Like "ev" */
			break;
		default:
			rt_log("option '%c' unknown\n", c);
			rt_log("	-d#	inter-frame delay\n");
			rt_log("	-v	polygon rendering (visual)\n");
			rt_log("	-D#	desired starting frame\n");
			rt_log("	-K#	final frame\n");
			break;
		}
	}
	argc -= optind-1;
	argv += optind-1;

	/* If file is still open from last cmd getting SIGINT, close it */
	if(rtif_fp)  fclose(rtif_fp);
	if( (rtif_fp = fopen(argv[1], "r")) == NULL )  {
		perror(argv[1]);
		return CMD_BAD;
	}

	/* Build list of top-level objects in view, in rt_cmd_vec[] */
	rt_cmd_vec[0] = "tree";
	setup_rt( &rt_cmd_vec[1] );

	rtif_vbp = rt_vlblock_init();

	rt_log("eyepoint at (0,0,1) viewspace\n");

	/*
	 *  Initialize the view to the current one in MGED
	 *  in case a view specification is never given.
	 */
	mat_copy(rtif_viewrot, Viewrot);
	VSET(temp, 0, 0, 1);
	MAT4X3PNT(rtif_eye_model, view2model, temp);

	/* If user hits ^C, preview will stop, and clean up */
	(void)signal(SIGINT, rtif_sigint);

	while( ( cmd = rt_read_cmd( rtif_fp )) != NULL )  {
		/* Hack to prevent running framedone scripts prematurely */
		if( cmd[0] == '!' )  {
			if( rtif_currentframe < rtif_desiredframe ||
			    rtif_finalframe && rtif_currentframe > rtif_finalframe )  {
				rt_free( cmd, "preview ! cmd" );
			    	continue;
			}
		}
		if( rt_do_cmd( (struct rt_i *)0, cmd, cmdtab ) < 0 )
			rt_log("command failed: %s\n", cmd);
		rt_free( cmd, "preview cmd" );
	}
	fclose(rtif_fp);
	rtif_fp = NULL;

	cvt_vlblock_to_solids( rtif_vbp, "EYE_PATH", 0 );
	if(rtif_vbp)  {
		rt_vlblock_free(rtif_vbp);
		rtif_vbp = (struct rt_vlblock *)NULL;
	}
	db_free_anim(dbip);	/* Forget any anim commands */

	/* Restore state variables */
	mged_variables = rtif_saved_state;	/* struct copy */

	return CMD_OK;
}

/*
 *			F _ N I R T
 *
 *  Invoke NIRT with the current view & stuff
 */
int
f_nirt( argc, argv )
int	argc;
char	**argv;
{
	register char **vp;
	register int i;
	register struct solid *sp;
	double	t;
	double	t_in;
	FILE *fp;
	int pid, rpid;
	int retcode;
	int o_pipe[2];
	vect_t	center_model;
	vect_t	direction;
	vect_t	extremum[2];
	vect_t	minus, plus;	/* vers of this solid's bounding box */
	vect_t	unit_H, unit_V;

	if( not_state( ST_VIEW, "Single ray-trace from current view" ) )
		return CMD_BAD;

	vp = &rt_cmd_vec[0];
	*vp++ = "nirt";
	*vp++ = "-M";
	for( i=1; i < argc; i++ )
		*vp++ = argv[i];
	*vp++ = dbip->dbi_filename;

	setup_rt( vp );

	(void)pipe( o_pipe );
	(void)signal( SIGINT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
		(void)close(0);
		(void)dup( o_pipe[0] );
		for( i=3; i < 20; i++ )
			(void)close(i);
		(void)signal( SIGINT, SIG_DFL );
		(void)execvp( rt_cmd_vec[0], rt_cmd_vec );
		perror( rt_cmd_vec[0] );
		exit(16);
	}

	/* As parent, send view information down pipe */
	(void)close( o_pipe[0] );
	fp = fdopen( o_pipe[1], "w" );
	{
		VSET(center_model, -toViewcenter[MDX],
		    -toViewcenter[MDY], -toViewcenter[MDZ]);
		if (adcflag)
		{
		    rt_log("Firing through angle/distance cursor...\n");
		    /*
		     * Compute bounding box of all objects displayed.
		     * Borrowed from size_reset() in chgview.c
		     */
		    for (i = 0; i < 3; ++i)
		    {
			extremum[0][i] = INFINITY;
			extremum[1][i] = -INFINITY;
		    }
		    FOR_ALL_SOLIDS (sp)
		    {
			    minus[X] = sp->s_center[X] - sp->s_size;
			    minus[Y] = sp->s_center[Y] - sp->s_size;
			    minus[Z] = sp->s_center[Z] - sp->s_size;
			    VMIN( extremum[0], minus );
			    plus[X] = sp->s_center[X] + sp->s_size;
			    plus[Y] = sp->s_center[Y] + sp->s_size;
			    plus[Z] = sp->s_center[Z] + sp->s_size;
			    VMAX( extremum[1], plus );
#if 0
			    rt_log("(%g,%g,%g)+-%g->(%g,%g,%g)(%g,%g,%g)->(%g,%g,%g)(%g,%g,%g)\n",
				sp->s_center[X], sp->s_center[Y], sp->s_center[Z],
				sp->s_size,
				minus[X], minus[Y], minus[Z],
				plus[X], plus[Y], plus[Z],
				extremum[0][X], extremum[0][Y], extremum[0][Z],
				extremum[1][X], extremum[1][Y], extremum[1][Z]);
#endif
		    }
		    VMOVEN(direction, Viewrot + 8, 3);
		    VSCALE(direction, direction, -1.0);
		    for (i = 0; i < 3; ++i)
			if (NEAR_ZERO(direction[i], 1e-10))
			    direction[i] = 0.0;
		    if ((center_model[X] >= extremum[0][X]) &&
			(center_model[X] <= extremum[1][X]) &&
			(center_model[Y] >= extremum[0][Y]) &&
			(center_model[Y] <= extremum[1][Y]) &&
			(center_model[Z] >= extremum[0][Z]) &&
			(center_model[Z] <= extremum[1][Z]))
		    {
			t_in = -INFINITY;
			for (i = 0; i < 6; ++i)
			{
			    if (direction[i%3] == 0)
				continue;
			    t = (extremum[i/3][i%3] - center_model[i%3]) /
				    direction[i%3];
			    if ((t < 0) && (t > t_in))
				t_in = t;
			}
			VJOIN1(center_model, center_model, t_in, direction);
		    }

		    VMOVEN(unit_H, model2view, 3);
		    VMOVEN(unit_V, model2view + 4, 3);
		    VJOIN1(center_model,
			center_model, curs_x * Viewscale / 2047.0, unit_H);
		    VJOIN1(center_model,
			center_model, curs_y * Viewscale / 2047.0, unit_V);
		}
		else
		    rt_log("Firing from view center...\n");
		fflush(stdout);
		rt_write(fp, center_model );
	}
	(void)fclose( fp );

	/* Wait for program to finish */
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;	/* NULL */
	if( retcode != 0 )
		pr_wait_status( retcode );
	(void)signal(SIGINT, cur_sigint);

	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;

	return CMD_OK;
}

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

cm_vsize(argc, argv)
char	**argv;
int	argc;
{
	if( argc < 2 )
		return(-1);
	Viewscale = atof(argv[1])*0.5;
	return(0);
}

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

cm_lookat_pt(argc, argv)
int	argc;
char	**argv;
{
	point_t	pt;
	vect_t	dir;
	int	yflip = 0;

	if( argc < 4 )
		return(-1);
	pt[X] = atof(argv[1]);
	pt[Y] = atof(argv[2]);
	pt[Z] = atof(argv[3]);
	if( argc > 4 )
		yflip = atoi(argv[4]);

	VSUB2( dir, pt, rtif_eye_model );
	VUNITIZE( dir );
	mat_lookat( rtif_viewrot, dir, yflip );
	/*  Final processing is deferred until cm_end(), but eye_pt
	 *  must have been specified before here (for now)
	 */
	return(0);
}

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
	struct rt_list		*vhead = &rtif_vbp->head[0];

	/* Only display the frames the user is interested in */
	if( rtif_currentframe < rtif_desiredframe )  return 0;
	if( rtif_finalframe && rtif_currentframe > rtif_finalframe )  return 0;

	/* Record eye path as a polyline.  Move, then draws */
	if( RT_LIST_IS_EMPTY( vhead ) )  {
		RT_ADD_VLIST( vhead, rtif_eye_model, RT_VLIST_LINE_MOVE );
	} else {
		RT_ADD_VLIST( vhead, rtif_eye_model, RT_VLIST_LINE_DRAW );
	}
	
	/* First step:  put eye at view center (view 0,0,0) */
       	mat_copy( Viewrot, rtif_viewrot );
	MAT_DELTAS( toViewcenter,
		-rtif_eye_model[X],
		-rtif_eye_model[Y],
		-rtif_eye_model[Z] );
	new_mats();

	/*
	 * Compute camera orientation notch to right (+X) and up (+Y)
	 * Done here, with eye in center of view.
	 */
	VSET( xv, 0.05, 0, 0 );
	VSET( yv, 0, 0.05, 0 );
	MAT4X3PNT( xm, view2model, xv );
	MAT4X3PNT( ym, view2model, yv );
	RT_ADD_VLIST( vhead, xm, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, rtif_eye_model, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, ym, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, rtif_eye_model, RT_VLIST_LINE_MOVE );

	/*  Second step:  put eye at view 0,0,1.
	 *  For eye to be at 0,0,1, the old 0,0,-1 needs to become 0,0,0.
	 */
	VSET( xlate, 0, 0, -1 );	/* correction factor */
	MAT4X3PNT( new_cent, view2model, xlate );
	MAT_DELTAS( toViewcenter,
		-new_cent[X],
		-new_cent[Y],
		-new_cent[Z] );
	new_mats();

	/* If new treewalk is needed, get new objects into view. */
	if( tree_walk_needed )  {
		(void)f_zap( 0, 0 );
		edit_com( rt_cmd_vec_len, rt_cmd_vec, rtif_mode, 0 );
	}

	dmaflag = 1;
	refresh();	/* Draw new display */
	dmaflag = 1;
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

	if( db_parse_anim( dbip, argc, argv ) < 0 )  {
		rt_log("cm_anim:  %s %s failed\n", argv[1], argv[2]);
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

	for( i = 1;  i < argc && i < LEN; i++ )  {
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
cm_clean(argc, argv)
char	**argv;
int	argc;
{
	f_zap( 0, (char **)0 );

	/* Free animation structures */
	db_free_anim(dbip);

	tree_walk_needed = 1;
	return 0;
}

cm_set(argc, argv)
char	**argv;
int	argc;
{
	return(-1);
}
