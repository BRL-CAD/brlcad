/*
 *			G E D . C
 *
 * Mainline portion of the graphics editor
 *
 *  Functions -
 *	prprompt	print prompt
 *	main		Mainline portion of the graphics editor
 *	refresh		Internal routine to perform displaylist output writing
 *	usejoy		Apply joystick to viewing perspective
 *	setview		Set the current view
 *	slewview	Slew the view
 *	log_event	Log an event in the log file
 *	mged_finish	Terminate with logging.  To be used instead of exit().
 *	quit		General Exit routine
 *	sig2		user interrupt catcher
 *	new_mats	derive inverse and editing matrices, as required
 *	f_source	Open a file and process the commands within
 *
 *  Authors -
 *	Michael John Muuss
 *	Charles M Kennedy
 *	Douglas A Gwyn
 *	Bob Suckling
 *	Gary Steven Moss
 *	Earl P Weaver
 *	Phil Dykstra
 *	Bob Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

char MGEDCopyRight_Notice[] = "@(#) \
Re-distribution of this software is restricted, as described in \
your 'Statement of Terms and Conditions for the Release of \
The BRL-CAD Pacakge' agreement. \
This software is Copyright (C) 1985,1987,1990,1993 by the United States Army \
in all countries except the USA.  All rights reserved.";

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#ifdef NONBLOCK
#	include <termio.h>
#	undef VMIN	/* also used in vmath.h */
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "db.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./solid.h"
#include "./sedit.h"
#include "./dm.h"

#ifndef XMGED
#  define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */
#  include "tcl.h"
#  include "tk.h"

/* defined in cmd.c */
extern Tcl_Interp *interp;
extern Tk_Window tkwin;
#endif

#ifdef XMGED
void	slewview();

extern void     (*axis_color_hook)();
extern int     (*bindkey_hook)();
extern int      (*editline_hook)();
extern int	(*fgetc_hook)();
extern int	(*fputc_hook)();
extern int	(*fputs_hook)();
extern char	*(*fgets_hook)();
extern char	*(*gets_hook)();
extern void	(*fprintf_hook)();
extern int	(*button_hook)();
extern int	(*slider_hook)();
extern int	(*openw_hook)();
extern int	(*closew_hook)();
extern int	(*knob_hook)();
extern int	(*adc_hook)();
extern void	(*wait_hook)();
extern void	(*dotitles_hook)();
extern int     (*cue_hook)(), (*zclip_hook)(), (*zbuffer_hook)();
extern int     (*light_hook)(), (*perspective_hook)(), (*tran_hook)();
extern int     (*rot_hook)(), (*set_tran_hook)();
extern void set_tran();
extern point_t orig_pos;
extern int tran_set;
extern double tran_x;
extern double tran_y;
extern double tran_z;
extern int journal;
#endif

#ifndef	LOGFILE
#define LOGFILE	"/vld/lib/gedlog"	/* usage log */
#endif

struct db_i	*dbip;			/* database instance pointer */

struct device_values dm_values;		/* Dev Values, filled by dm-XX.c */

int		dmaflag;		/* Set to 1 to force new screen DMA */
double		frametime = 1.0;	/* time needed to draw last frame */
mat_t		ModelDelta;		/* Changes to Viewrot this frame */

int		(*cmdline_hook)() = NULL;
void		(*viewpoint_hook)() = NULL;
void		(*extrapoll_hook)() = NULL;
int		extrapoll_fd;		/* XXX Ultra hack XXX */

static int	windowbounds[6];	/* X hi,lo;  Y hi,lo;  Z hi,lo */

static jmp_buf	jmp_env;		/* For non-local gotos */
void		(*cur_sigint)();	/* Current SIGINT status */
void		sig2();
void		new_mats();
void		usejoy();
int		interactive = 0;	/* !0 means interactive */
static int	do_rc();
static void	log_event();
extern char	version[];		/* from vers.c */

struct rt_tol	mged_tol;		/* calculation tolerance */

#ifdef XMGED
extern int    update_views;
extern int      already_scandb;
#endif

static char *units_str[] = {
	"none",
	"mm",
	"cm",
	"meters",
	"inches",
	"feet",
	"extra"
};

void
pr_prompt()  {
	rt_log("mged> ");
}

/* 
 *			M A I N
 */

int
main(argc,argv)
int argc;
char **argv;
{
	int	rateflag = 0;

#ifdef XMGED
	axis_color_hook = NULL;
	bindkey_hook = NULL;
	editline_hook = NULL;
	fputc_hook = NULL;
	fputs_hook = NULL;
	fprintf_hook = NULL;
	fgets_hook = NULL;
	gets_hook = NULL;
	fgetc_hook = NULL;
	button_hook = NULL;
	slider_hook = NULL;
	openw_hook = NULL;
	closew_hook = NULL;
	knob_hook = NULL;
	adc_hook = NULL;
	dotitles_hook = NULL;
	wait_hook = NULL;
	cue_hook = NULL;
	zclip_hook = NULL;
	zbuffer_hook = NULL;
	light_hook = NULL;
	perspective_hook = NULL;
	tran_hook = NULL;
	rot_hook = NULL;
	set_tran_hook = NULL;
#endif

	/* Check for proper invocation */
	if( argc < 2 )  {
		fprintf(stdout, "Usage:  %s database [command]\n", argv[0]);
		fflush(stdout);
		return(1);		/* NOT finish() */
	}

	/* Identify ourselves if interactive */
	if( argc == 2 )  {
		interactive = 1;
		fprintf(stdout, "%s\n", version+5);	/* skip @(#) */
		fflush(stdout);
	}

	(void)signal( SIGPIPE, SIG_IGN );

	/*
	 *  Sample and hold current SIGINT setting, so any commands that
	 *  might be run (e.g., by .mgedrc) which establish cur_sigint
	 *  as their signal handler get the initial behavior.
	 *  This will change after setjmp() is called, below.
	 */
	cur_sigint = signal( SIGINT, SIG_IGN );		/* sample */
	(void)signal( SIGINT, cur_sigint );		/* restore */

	/* If multiple processors might be used, initialize for it.
	 * Do not run any commands before here.
	 * Do not use rt_log() or rt_malloc() before here.
	 */
	if( rt_avail_cpus() > 1 )  {
		rt_g.rtg_parallel = 1;
		RES_INIT( &rt_g.res_syscall );
		RES_INIT( &rt_g.res_worker );
		RES_INIT( &rt_g.res_stats );
		RES_INIT( &rt_g.res_results );
		RES_INIT( &rt_g.res_model );
	}

	/* Set up linked lists */
	HeadSolid.s_forw = &HeadSolid;
	HeadSolid.s_back = &HeadSolid;
	FreeSolid = SOLID_NULL;
	RT_LIST_INIT( &rt_g.rtg_vlfree );

	state = ST_VIEW;
	es_edflag = -1;
	inpara = newedge = 0;

	/* init rotation matrix */
	mat_idn( identity );		/* Handy to have around */
	Viewscale = 500;		/* => viewsize of 1000mm (1m) */
	mat_idn( Viewrot );
	mat_idn( toViewcenter );
	mat_idn( modelchanges );
	mat_idn( ModelDelta );

	/* These values match old GED.  Use 'tol' command to change them. */
	mged_tol.magic = RT_TOL_MAGIC;
	mged_tol.dist = 0.005;
	mged_tol.dist_sq = mged_tol.dist * mged_tol.dist;
	mged_tol.perp = 1e-6;
	mged_tol.para = 1 - mged_tol.perp;

	rt_prep_timer();		/* Initialize timer */

	new_mats();

	setview( 0.0, 0.0, 0.0 );

	no_memory = 0;		/* memory left */
	es_edflag = -1;		/* no solid editing just now */

	rt_vls_init( &dm_values.dv_string );

	/* Initialize the menu mechanism to be off, but ready. */
	mmenu_init();
	btn_head_menu(0,0,0);		/* unlabeled menu */

	windowbounds[0] = XMAX;		/* XHR */
	windowbounds[1] = XMIN;		/* XLR */
	windowbounds[2] = YMAX;		/* YHR */
	windowbounds[3] = YMIN;		/* YLR */
	windowbounds[4] = 2047;		/* ZHR */
	windowbounds[5] = -2048;	/* ZLR */

	dmaflag = 1;

	/* Perform any necessary initializations for the command parser */
	cmd_setup(interactive);

	/* Open the database, attach a display manager */
	f_opendb( argc, argv );

	dmp->dmr_window(windowbounds);

	/* --- Now safe to process commands. --- */
	if( interactive )  {
		/* This is an interactive mged, process .mgedrc */
		do_rc();
	}

	/* --- Now safe to process geometry. --- */

	/* If this is an argv[] invocation, do it now */
	if( argc > 2 )  {
		mged_cmd( argc-2, argv+2 );
		f_quit(0, NULL);
		/* NOTREACHED */
	}

	refresh();			/* Put up faceplate */

	/* Reset the lights */
	dmp->dmr_light( LIGHT_RESET, 0 );

	/* Caught interrupts take us back here, via longjmp() */
	if( setjmp( jmp_env ) == 0 )  {
		/* First pass, determine SIGINT handler for interruptable cmds */
		if( cur_sigint == (void (*)())SIG_IGN )
			cur_sigint = (void (*)())SIG_IGN; /* detached? */
		else
			cur_sigint = sig2;	/* back to here w/!0 return */
	} else {
		rt_log("\nAborted.\n");
		/* If parallel routine was interrupted, need to reset */
		RES_RELEASE( &rt_g.res_syscall );
		RES_RELEASE( &rt_g.res_worker );
		RES_RELEASE( &rt_g.res_stats );
		RES_RELEASE( &rt_g.res_results );
	}
	pr_prompt();

	/****************  M A I N   L O O P   *********************/
	while(1) {
		(void)signal( SIGINT, SIG_IGN );

		/* This test stops optimizers from complaining about an infinite loop */
		if( (rateflag = event_check( rateflag )) < 0 )  break;

#ifdef XMGED
		/* now being done in cmd.c/mged_cmd() */
#else
		/* apply solid editing changes if necessary */
		if( sedraw > 0) {
			sedit();
			sedraw = 0;
			dmaflag = 1;
		}
#endif

		/*
		 * Cause the control portion of the displaylist to be
		 * updated to reflect the changes made above.
		 */
		refresh();
	}
	return(0);
}

/*
 *			E V E N T _ C H E C K
 *
 *  Check for events, and dispatch them.
 *  Eventually, this will be done entirely by generating commands
 *
 *  Returns -
 *	 0	no events detected
 *	>0	number of events detected
 */
int
event_check( non_blocking )
int	non_blocking;
{
	fd_set		input;
	vect_t		knobvec;	/* knob slew */

	/*
	 * dmr_input() will suspend until some change has occured,
	 * either on the device peripherals, or a command has been
	 * entered on the keyboard, unless the non-blocking flag is set.
	 */
	FD_ZERO( &input );
	if( extrapoll_fd )  FD_SET( extrapoll_fd, &input );
	FD_SET( fileno(stdin), &input );

#ifndef XMGED
	if( mged_tk_dontwait() )  non_blocking = 1;
#endif

	/* await an input event. Bits will be set in 'input' as appropriate */
	dmp->dmr_input( &input, non_blocking );

	if(extrapoll_fd && FD_ISSET(extrapoll_fd,&input) && extrapoll_hook)  {
		(*extrapoll_hook)();
	}
#ifndef XMGED
	else {
		mged_tk_idle(non_blocking);
	}
#endif

	/* Acquire anything present on stdin */
	if( FD_ISSET( fileno(stdin), &input ) )  {
		struct rt_vls	str;
		rt_vls_init(&str);

		/* Read input line */
		if( rt_vls_gets( &str, stdin ) >= 0 )  {
			rt_vls_strcat( &str, "\n" );
		if( cmdline_hook ) {
			if( (*cmdline_hook)(&str)) 
				pr_prompt();
		} else
			if( cmdline( &str ) )
				pr_prompt();
		} else {
			/* Check for Control-D (EOF) */
			if( feof( stdin ) )  {
				/* EOF, let's hit the road */
				f_quit(0, NULL);
				/* NOTREACHED */
			}
		}
		rt_vls_free(&str);
	}
	non_blocking = 0;

	/*
	 *  Process any "string commands" sent to us by the display manager.
	 *  (Or "invented" here, for compatability with old dm's).
	 *  Each one is expected to be newline terminated.
	 */
	if( cmdline_hook )  
		(*cmdline_hook)(&dm_values.dv_string); 
	else {
		/* Some commands (e.g. mouse events) queue further events. */
		int oldlen;
again:
		oldlen = rt_vls_strlen( &dm_values.dv_string );
		(void)cmdline( &dm_values.dv_string );
		if( rt_vls_strlen( &dm_values.dv_string ) > oldlen ) {
			/* Remove cmds already done, and go again */
			rt_vls_nibble( &dm_values.dv_string, oldlen );
			goto again;
		}
	}

	rt_vls_trunc( &dm_values.dv_string, 0 );

	/*
	 * Set up window so that drawing does not run over into the
	 * status line area, and menu area (if present).
	 */
	windowbounds[1] = XMIN;		/* XLR */
	if( illump != SOLID_NULL )
		windowbounds[1] = MENUXLIM;
	windowbounds[3] = TITLE_YBASE-TEXT1_DY;	/* YLR */
	dmp->dmr_window(windowbounds);	/* hack */

	/*********************************
	 *  Handle rate-based processing *
	 *********************************/
	if( rateflag_rotate )  {
		non_blocking++;

		/* Compute delta x,y,z parameters */
		usejoy( rate_rotate[X] * 6 * degtorad,
			rate_rotate[Y] * 6 * degtorad,
			rate_rotate[Z] * 6 * degtorad );
	}
	if( rateflag_slew )  {
		non_blocking++;

		/* slew 1/10th of the view per update */
		knobvec[X] = -rate_slew[X] / 10;
		knobvec[Y] = -rate_slew[Y] / 10;
		knobvec[Z] = -rate_slew[Z] / 10;
		slewview( knobvec );
	}
	if( rateflag_zoom )  {
		fastf_t	factor;
#define MINVIEW		0.001	/* smallest view.  Prevents runaway zoom */

		factor = 1.0 - (rate_zoom / 10);
		Viewscale *= factor;
		if( Viewscale < MINVIEW )
			Viewscale = MINVIEW;
		else  {
			non_blocking++;
		}
		{
			/* Scaling (zooming) takes place around view center */
			mat_t	scale_mat;

			mat_idn( scale_mat );
			scale_mat[15] = 1/factor;

			wrt_view( ModelDelta, scale_mat, ModelDelta );
		}
		dmaflag = 1;
		new_mats();
	}

	return( non_blocking );
}

/*			R E F R E S H
 *
 * NOTE that this routine is not to be casually used to
 * refresh the screen.  The normal procedure for screen
 * refresh is to manipulate the necessary global variables,
 * and wait for refresh to be called at the bottom of the while loop
 * in main().  However, when it is absolutely necessary to
 * flush a change in the solids table out to the display in
 * the middle of a routine somewhere (such as the "B" command
 * processor), then this routine may be called.
 *
 * If you don't understand the ramifications of using this routine,
 * then you don't want to call it.
 */
void
refresh()
{
	/*
	 * if something has changed, then go update the display.
	 * Otherwise, we are happy with the view we have
	 */
	if( dmaflag )  {
		double	elapsed_time;

		/* XXX VR hack */
		if( viewpoint_hook )  (*viewpoint_hook)();

		rt_prep_timer();

		if( mged_variables.predictor )
			predictor_frame();

		dmp->dmr_prolog();	/* update displaylist prolog */

		/*  Draw each solid in it's proper place on the screen
		 *  by applying zoom, rotation, & translation.
		 *  Calls dmp->dmr_newrot() and dmp->dmr_object().
		 */
		if( mged_variables.eye_sep_dist <= 0 )  {
			/* Normal viewing */
			dozoom(0);
		} else {
			/* Stereo viewing */
			dozoom(1);
			dozoom(2);
		}

		/* Restore to non-rotated, full brightness */
		dmp->dmr_normal();

		/* Compute and display angle/distance cursor */
		if (adcflag)
			adcursor();

		/* Display titles, etc., if desired */
		if( mged_variables.faceplate > 0 )
			dotitles();

		dmp->dmr_epilog();

		(void)rt_get_timer( (struct rt_vls *)0, &elapsed_time );
		/* Only use reasonable measurements */
		if( elapsed_time > 1.0e-5 && elapsed_time < 30 )  {
			/* Smoothly transition to new speed */
			frametime = 0.9 * frametime + 0.1 * elapsed_time;
		}
	} else {
		/* For displaylist machines??? */
		dmp->dmr_prolog();	/* update displaylist prolog */
		dmp->dmr_update();
	}

	dmaflag = 0;
}

/*
 *			F _ V R O T _ C E N T E R
 *
 *  Set the center of rotation, either in model coordinates, or
 *  in view (+/-1) coordinates.
 *  The default is to rotate around the view center: v=(0,0,0).
 */
int
f_vrot_center( argc, argv )
int	argc;
char	**argv;
{
	rt_log("Not ready until tomorrow.\n");
	return CMD_OK;
}

/*
 *			U S E J O Y
 *
 *  Apply the "joystick" delta rotation to the viewing direction,
 *  where the delta is specified in terms of the *viewing* axes.
 *  Rotation is performed about the view center, for now.
 *  Angles are in radians.
 */
void
usejoy( xangle, yangle, zangle )
double	xangle, yangle, zangle;
{
	mat_t	newrot;		/* NEW rot matrix, from joystick */

	dmaflag = 1;

	if( state == ST_S_EDIT )  {
		if( sedit_rotate( xangle, yangle, zangle ) > 0 )
			return;		/* solid edit claimed event */
	} else if( state == ST_O_EDIT )  {
		if( objedit_rotate( xangle, yangle, zangle ) > 0 )
			return;		/* object edit claimed event */
	}

	/* NORMAL CASE.
	 * Apply delta viewing rotation for non-edited parts.
	 * The view rotates around the VIEW CENTER.
	 */
	mat_idn( newrot );
	buildHrot( newrot, xangle, yangle, zangle );

	mat_mul2( newrot, Viewrot );
	{
		mat_t	newinv;
		mat_inv( newinv, newrot );
		wrt_view( ModelDelta, newinv, ModelDelta );
	}
	new_mats();
}

/*
 *			A B S V I E W _ V
 *
 *  The "angle" ranges from -1 to +1.
 *  Assume rotation around view center, for now.
 */
void
absview_v( ang )
CONST point_t	ang;
{
	point_t	rad;

	VSCALE( rad, ang, rt_pi );	/* range from -pi to +pi */
	buildHrot( Viewrot, rad[X], rad[Y], rad[Z] );
	new_mats();
}

/*
 *			S E T V I E W
 *
 * Set the view.  Angles are DOUBLES, in degrees.
 *
 * Given that viewvec = scale . rotate . (xlate to view center) . modelvec,
 * we just replace the rotation matrix.
 * (This assumes rotation around the view center).
 */
void
setview( a1, a2, a3 )
double a1, a2, a3;		/* DOUBLE angles, in degrees */
{
#ifdef XMGED
  point_t old_pos;
  point_t new_pos;
  point_t new_pos2;
  point_t diff;
  vect_t view_pos;
  
  MAT_DELTAS_GET(old_pos, toViewcenter);

  if(state == ST_S_EDIT || state == ST_O_EDIT){
    VSET(view_pos, tran_x, tran_y, tran_z);
    MAT4X3PNT( new_pos2, view2model, view_pos );
  }
#endif

	buildHrot( Viewrot, a1 * degtorad, a2 * degtorad, a3 * degtorad );
	new_mats();

#ifdef XMGED
  MAT_DELTAS_GET_NEG(new_pos, toViewcenter);

  if(state == ST_S_EDIT || state == ST_O_EDIT){
    MAT4X3PNT(view_pos, model2view, new_pos2);
    tran_x = view_pos[X];
    tran_y = view_pos[Y];
    tran_z = view_pos[Z];
  }else{
    VSUB2(diff, new_pos, orig_pos);
    VADD2(new_pos, old_pos, diff);
    VSET(view_pos, new_pos[X], new_pos[Y], new_pos[Z]);
    MAT4X3PNT( new_pos, model2view, view_pos);
    tran_x = new_pos[X];
    tran_y = new_pos[Y];
    tran_z = new_pos[Z];
  }

  if(tran_hook)
          (*tran_hook)();
#endif
}

/*
 *			S L E W V I E W
 *
 *  Given a position in view space,
 *  make that point the new view center.
 */
void
slewview( view_pos )
vect_t view_pos;
{
	point_t	old_model_center;
	point_t	new_model_center;
	vect_t	diff;
	mat_t	delta;

	MAT_DELTAS_GET_NEG( old_model_center, toViewcenter );

	MAT4X3PNT( new_model_center, view2model, view_pos );
	MAT_DELTAS_VEC_NEG( toViewcenter, new_model_center );

	VSUB2( diff, new_model_center, old_model_center );
	mat_idn( delta );
	MAT_DELTAS_VEC( delta, diff );
	mat_mul2( delta, ModelDelta );
	new_mats();

#ifdef XMGED
	if(!tran_set){
	  tran_x -= view_pos[X];
	  tran_y -= view_pos[Y];
	  tran_z -= view_pos[Z];
	}

	if(tran_hook)
	  (*tran_hook)();
#endif
}

/*
 *			L O G _ E V E N T
 *
 * Logging routine
 */
static void
log_event( event, arg )
char *event;
char *arg;
{
	char line[128];
	time_t now;
	char *timep;
	int logfd;

	(void)time( &now );
	timep = ctime( &now );	/* returns 26 char string */
	timep[24] = '\0';	/* Chop off \n */

	(void)sprintf(line, "%s [%s] time=%ld uid=%d (%s) %s\n",
			event,
			dmp->dmr_name,
			now,
			getuid(),
			timep,
			arg
	);

	if( (logfd = open( LOGFILE, O_WRONLY|O_APPEND )) >= 0 )  {
		(void)write( logfd, line, (unsigned)strlen(line) );
		(void)close( logfd );
	}
}

/*
 *			F I N I S H
 *
 * This routine should be called in place of exit() everywhere
 * in GED, to permit an accurate finish time to be recorded in
 * the (ugh) logfile, also to remove the device access lock.
 */
void
mged_finish( exitcode )
int	exitcode;
{
	char place[64];

	(void)sprintf(place, "exit_status=%d", exitcode );
	log_event( "CEASE", place );
	dmp->dmr_light( LIGHT_RESET, 0 );	/* turn off the lights */
	dmp->dmr_close();
	exit( exitcode );
}

/*
 *			Q U I T
 *
 * Handles finishing up.  Also called upon EOF on STDIN.
 */
void
quit()
{
#ifdef NONBLOCK
	int off = 0;
	(void)ioctl( 0, FIONBIO, &off );
#endif
	mged_finish(0);
	/* NOTREACHED */
}

/*
 *  			S I G 2
 */
void
sig2()
{
	longjmp( jmp_env, 1 );
	/* NOTREACHED */
}


/*
 *  			N E W _ M A T S
 *  
 *  Derive the inverse and editing matrices, as required.
 *  Centralized here to simplify things.
 */
void
new_mats()
{
	mat_mul( model2view, Viewrot, toViewcenter );
	model2view[15] = Viewscale;
	mat_inv( view2model, model2view );
	if( state != ST_VIEW )  {
		mat_mul( model2objview, model2view, modelchanges );
		mat_inv( objview2model, model2objview );
	}
	dmaflag = 1;
}

/*
 *			D O _ R C
 *
 *  If an mgedrc file exists, open it and process the commands within.
 *  Look first for a Shell environment variable, then for a file in
 *  the user's home directory, and finally in the current directory.
 *
 *  Returns -
 *	-1	FAIL
 *	 0	OK
 */
static int
do_rc()
{
	FILE	*fp;
	char	*path;
	int 	found;
	struct	rt_vls str;
	int bogus;

	found = 0;
	rt_vls_init( &str );

#define ENVRC	"MGED_RCFILE"
#define RCFILE	".mgedrc"

	if( (path = getenv(ENVRC)) != (char *)NULL ) {
		if ((fp = fopen(path, "r")) != NULL ) {
			rt_vls_strcpy( &str, path );
			found = 1;
		}
	}

	if( !found ) {
		if( (path = getenv("HOME")) != (char *)NULL )  {
			rt_vls_strcpy( &str, path );
			rt_vls_strcat( &str, "/" );
			rt_vls_strcat( &str, RCFILE );
		}
		if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
			found = 1;
	}

	if( !found ) {
		if( (fp = fopen( RCFILE, "r" )) != NULL )  {
			rt_vls_strcpy( &str, RCFILE );
			found = 1;
		}
	}

    /* At this point, if none of the above attempts panned out, give up. */

	if( !found )
		return -1;

#ifndef XMGED
	bogus = 0;
	while( !feof(fp) ) {
	    char buf[80];

	    /* Get beginning of line */
	    fgets( buf, 80, fp );
	    /* Discard rest of line */
	    while( !feof(fp) && fgetc(fp)!='\n' )
		;
	    while( !feof(fp) && fgetc(fp)!='\n' )
		;
    /* If the user has a set command with an equal sign, remember to warn */
	    if( strstr(buf, "set") != NULL )
		if( strchr(buf, '=') != NULL )
		    bogus = 1;
	}
	
	fclose( fp );
	if( bogus ) {
	    rt_log("\nWARNING: The new format of the \"set\" command is:\n");
	    rt_log("    set varname value\n");
	    rt_log("If you are setting variables in your .mgedrc, you will ");
	    rt_log("need to change those\ncommands.\n\n");
	}
	Tcl_EvalFile( interp, rt_vls_addr(&str) );
#else
	mged_source_file( fp );
	fclose( fp );
#endif
	return 0;

}

/*
 *			F _ O P E N D B
 *
 *  Close the current database, if open, and then open a new database.
 *  May also open a display manager, if interactive and none selected yet.
 *
 *  argv[1] is the filename.
 *
 *  There are two invocations:
 *	main()
 *	cmdline()		Only one arg is permitted.
 */
int
f_opendb( argc, argv )
int	argc;
char	**argv;
{
#ifdef XMGED
        struct db_i *o_dbip;

	o_dbip = dbip;  /* save pointer to old database */

	/* Get input file */
	if( ((dbip = db_open( argv[1], "r+w" )) == DBI_NULL ) &&
	    ((dbip = db_open( argv[1], "r"   )) == DBI_NULL ) )  {
		char line[128];

		dbip = o_dbip; /* reinitialize in case of control-C */

		if( isatty(0) ) {
		    perror( argv[1] );
		    rt_log("Create new database (y|n)[n]? ");
		    (void)mged_fgets(line, sizeof(line), stdin);
		    if( line[0] != 'y' && line[0] != 'Y' ){
		      if(dbip != DBI_NULL){
			rt_log( "Still using %s\n", dbip->dbi_title);
			return(CMD_OK);
		      }

		      exit(0);		/* NOT finish() */
		    }
		} else
		  rt_log("Creating new database \"%s\"\n", argv[1]);

		if( (dbip = db_create( argv[1] )) == DBI_NULL )  {
			perror( argv[1] );
			exit(2);		/* NOT finish() */
		}
	}

	already_scandb = 0;
	if( o_dbip )  {
		/* Clear out anything in the display */
		f_zap( 0, (char **)NULL );

		/* Close current database.  Releases rt_material_head, etc. too. */
		db_close(o_dbip);

		log_event( "CEASE", "(close)" );
	}

	if( dbip->dbi_read_only )
		(void)rt_log( "%s:  READ ONLY\n", dbip->dbi_filename );

	/* Quick -- before he gets away -- write a logfile entry! */
	log_event( "START", argv[1] );

	if( interactive && is_dm_null() )  {
		/*
		 * This is an interactive mged, with no display yet.
		 * Ask which DM, and fire up the display manager.
		 * Ask this question BEFORE the db_scan, because
		 * that can take a long time for large models.
		 */
		get_attached();
	}

	/* --- Scan geometry database and build in-memory directory --- */
	if(!already_scandb){	/* need this because the X display manager may
					scan the database */
		db_scan( dbip, (int (*)())db_diradd, 1);
		already_scandb = 1;
	}

	/* XXX - save local units */
	localunit = dbip->dbi_localunit;
	local2base = dbip->dbi_local2base;
	base2local = dbip->dbi_base2local;

	/* Print title/units information */
	if( interactive )
		(void)rt_log( "%s (units=%s)\n", dbip->dbi_title,
			units_str[dbip->dbi_localunit] );

	update_views = 1;
	return(CMD_OK);
#else
	if( dbip )  {
		/* Clear out anything in the display */
		f_zap( 0, (char **)NULL );

		/* Close current database.  Releases MaterHead, etc. too. */
		db_close(dbip);
		dbip = DBI_NULL;

		log_event( "CEASE", "(close)" );
	}

	/* Get input file */
	if( ((dbip = db_open( argv[1], "r+w" )) == DBI_NULL ) &&
	    ((dbip = db_open( argv[1], "r"   )) == DBI_NULL ) )  {
		char line[128];

		if( isatty(0) ) {
		    perror( argv[1] );
		    rt_log("Create new database (y|n)[n]? ");
		    (void)fgets(line, sizeof(line), stdin);
		    if( line[0] != 'y' && line[0] != 'Y' )
			exit(0);		/* NOT finish() */
		} else
		    rt_log("Creating new database \"%s\"\n", argv[1]);
			

		if( (dbip = db_create( argv[1] )) == DBI_NULL )  {
			perror( argv[1] );
			exit(2);		/* NOT finish() */
		}
	}
	if( dbip->dbi_read_only )
		rt_log("%s:  READ ONLY\n", dbip->dbi_filename );

	/* Quick -- before he gets away -- write a logfile entry! */
	log_event( "START", argv[1] );

	if( interactive && is_dm_null() )  {
		/*
		 * This is an interactive mged, with no display yet.
		 * Ask which DM, and fire up the display manager.
		 * Ask this question BEFORE the db_scan, because
		 * that can take a long time for large models.
		 */
		get_attached();
	}

	/* --- Scan geometry database and build in-memory directory --- */
	db_scan( dbip, (int (*)())db_diradd, 1);
	/* XXX - save local units */
	localunit = dbip->dbi_localunit;
	local2base = dbip->dbi_local2base;
	base2local = dbip->dbi_base2local;

	/* Print title/units information */
	if( interactive )
		rt_log("%s (units=%s)\n", dbip->dbi_title,
			units_str[dbip->dbi_localunit] );
	
	return CMD_OK;
#endif
}

#ifdef XMGED
int
f_source(argc, argv)
int	argc;
char	*argv[];
{
	FILE	*fp;
	char	*path;
	struct rt_vls	str;
	char option = 'e';	/* default is to execute only */
	int save_journal;

	save_journal = journal;
	journal = 0;	/* temporarily shut off journalling */

	if(argc > 2){
	  sscanf(argv[1], "%c", &option);
	  ++argv;
	}

	if( (path = getenv("MGED_SRC_DIR")) != (char *)NULL )  {
		rt_vls_init(&str);
		rt_vls_strcpy( &str, path );
		rt_vls_strcat( &str, "/" );
		rt_vls_strcat( &str, argv[1] );
		if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )  {
			mged_source_file( fp, option );
			fclose(fp);
			rt_vls_free(&str);
			journal = save_journal;
			return CMD_OK;
		}
		rt_vls_free(&str);
	}
	if( (fp = fopen( argv[1], "r" )) != NULL )  {
		mged_source_file( fp, option );
		fclose(fp);
		journal = save_journal;
		return CMD_OK;
	}
	if( (path = getenv("HOME")) != (char *)NULL )  {
		rt_vls_init(&str);
		rt_vls_strcpy( &str, path );
		rt_vls_strcat( &str, "/" );
		rt_vls_strcat( &str, argv[1] );
		if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )  {
			mged_source_file( fp, option );
			fclose(fp);
			rt_vls_free(&str);
			journal = save_journal;
			return CMD_OK;
		}
		rt_vls_free(&str);
	}
	journal = save_journal;
	return CMD_BAD;
}
#else
/*
 *			F _ S O U R C E
 *
 *  Open a file/pipe and process the commands within.
 *
 *  argv[1] is the filename.
 *
 */
int
f_source (argc, argv)
int	argc;
char	**argv;
{
    char		*path;
    int			pipe = 0;	/* Read from pipe (vs. file)? */
    int			status;
    FILE		*fp;
    struct rt_vls	str;

    if (*(path = *++argv) == '|')
    {
	pipe = 1;
	rt_vls_init(&str);
	while (isspace(*++path))
	    ;
	rt_vls_strcpy(&str, path);
	while (--argc > 1)
	{
	    rt_vls_strcat(&str, " ");
	    rt_vls_strcat(&str, *++argv);
	}
	path = rt_vls_addr(&str);
    }

    if ((pipe && ((fp = popen(path, "r")) == NULL))
     || (!pipe && ((fp = fopen(path, "r")) == NULL)))
    {
	rt_log( "f_source: Cannot open %s '%s'\n", 
		pipe ? "pipe" : "command file", path );
	return( CMD_BAD );
    }
    mged_source_file(fp);
    if (pipe)
    {
	rt_vls_free(&str);
	if (status = pclose(fp))
	    rt_log("f_source: Exit status of pipe: %d\n", status);
    }
    else
	(void) fclose(fp);
    return(CMD_OK);
}
#endif
