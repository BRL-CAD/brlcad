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
 *
 *  Authors -
 *	Michael John Muuss
 *	Charles M Kennedy
 *	Douglas A Gwyn
 *	Bob Suckling
 *	Gary Steven Moss
 *	Earl P Weaver
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985,1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

char MGEDCopyRight_Notice[] = "@(#) Copyright (C) 1985,1987,1990 by the United States Army";

#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
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

#ifndef	LOGFILE
#define LOGFILE	"/vld/lib/gedlog"	/* usage log */
#endif

struct db_i	*dbip;			/* database instance pointer */

struct device_values dm_values;		/* Dev Values, filled by dm-XX.c */

int		dmaflag;		/* Set to 1 to force new screen DMA */
double		frametime = 1.0;	/* time needed to draw last frame */
mat_t		ModelDelta;		/* Changes to Viewrot this frame */

static int	windowbounds[6];	/* X hi,lo;  Y hi,lo;  Z hi,lo */

static jmp_buf	jmp_env;		/* For non-local gotos */
#if defined(sgi) && !defined(mips)
int		(*cur_sigint)();	/* Current SIGINT status */
int		sig2();
#else
void		(*cur_sigint)();	/* Current SIGINT status */
void		sig2();
#endif
void		new_mats();
void		usejoy();
static int	do_rc();
static void	log_event();
extern char	version[];		/* from vers.c */

extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */

static char *units_str[] = {
	"none",
	"mm",
	"cm",
	"meters",
	"inches",
	"feet",
	"extra"
};

FILE *infile;
FILE *outfile;

void
pr_prompt()  {
	(void)fprintf(outfile, "mged> ");
	(void)fflush(outfile);
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

	/* In some ANSI implementations, symbol "stdin" is a variable. */
	infile = stdin;
	outfile = stdout;

	/* Check for proper invocation */
	if( argc < 2 )  {
		(void)fprintf(outfile, "Usage:  %s database [command]\n",
		  argv[0]);
		return(1);		/* NOT finish() */
	}

	/* Identify ourselves if interactive */
	if( argc == 2 )
		(void)fprintf(outfile, "%s\n", version+5);	/* skip @(#) */

	(void)signal( SIGPIPE, SIG_IGN );

	/*
	 *  Sample and hold current SIGINT setting, so any commands that
	 *  might be run (e.g., by .mgedrc) which establish cur_sigint
	 *  as their signal handler get the initial behavior.
	 *  This will change after setjmp() is called, below.
	 */
	cur_sigint = signal( SIGINT, SIG_IGN );		/* sample */
	(void)signal( SIGINT, cur_sigint );		/* restore */

	/* Get input file */
	if( ((dbip = db_open( argv[1], "r+w" )) == DBI_NULL ) &&
	    ((dbip = db_open( argv[1], "r"   )) == DBI_NULL ) )  {
		char line[128];

		if( isatty(0) ) {

		    perror( argv[1] );
		    (void)fprintf(outfile, "Create new database (y|n)[n]? ");
		    fflush(outfile);
		    (void)fgets(line, sizeof(line), infile);
		    if( line[0] != 'y' && line[0] != 'Y' )
			exit(0);		/* NOT finish() */
		} else
		    (void)fprintf(outfile,
		    	"Creating new database \"%s\"\n", argv[1]);
			

		if( (dbip = db_create( argv[1] )) == DBI_NULL )  {
			perror( argv[1] );
			exit(2);		/* NOT finish() */
		}
	}
	if( dbip->dbi_read_only )
		(void)printf("%s:  READ ONLY\n", dbip->dbi_filename );

	/* Quick -- before he gets away -- write a logfile entry! */
	log_event( "START", argv[1] );

	/* If multiple processors might be used, initialize for it.
	 * Do not run any commands before here.
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

	rt_prep_timer();		/* Initialize timer */

	new_mats();

	setview( 0.0, 0.0, 0.0 );

	no_memory = 0;		/* memory left */
	es_edflag = -1;		/* no solid editing just now */

	rt_vls_init( &dm_values.dv_string );

	/* Initialize the menu mechanism to be off, but ready. */
	mmenu_init();
	btn_head_menu(0,0,0);		/* unlabeled menu */

	dmaflag = 1;

	/* --- Now safe to process commands.  BUT, no geometry yet. --- */

	if( argc == 2 )  {
		/* This is an interactive mged */

		/* Process any .mgedrc file, before attaching */
		do_rc();

		/* Ask which one, and fire up the display manager */
		get_attached();
	}

	windowbounds[0] = XMAX;		/* XHR */
	windowbounds[1] = XMIN;		/* XLR */
	windowbounds[2] = YMAX;		/* YHR */
	windowbounds[3] = YMIN;		/* YLR */
	windowbounds[4] = 2047;		/* ZHR */
	windowbounds[5] = -2048;	/* ZLR */
	dmp->dmr_window(windowbounds);

	/* --- Scan geometry database and build in-memory directory --- */
	db_scan( dbip, (int (*)())db_diradd, 1);
	/* XXX - save local units */
	localunit = dbip->dbi_localunit;
	local2base = dbip->dbi_local2base;
	base2local = dbip->dbi_base2local;

	/* Print title/units information */
	if( argc == 2 )
		(void)printf("%s (units=%s)\n", dbip->dbi_title,
			units_str[dbip->dbi_localunit] );

	/* If this is an argv[] invocation, do it now */
	if( argc > 2 )  {
		mged_cmd( argc-2, argv+2 );
		f_quit();
		/* NOTREACHED */
	}

	refresh();			/* Put up faceplate */

	/* Reset the lights */
	dmp->dmr_light( LIGHT_RESET, 0 );

	/* Caught interrupts take us back here, via longjmp() */
	if( setjmp( jmp_env ) == 0 )  {
		/* First pass, determine SIGINT handler for interruptable cmds */
#if defined(sgi) && !defined(mips)
		if( cur_sigint == (int (*)())SIG_IGN )
			cur_sigint = (int (*)())SIG_IGN; /* detached? */
		else
			cur_sigint = sig2;	/* back to here w/!0 return */
#else
		if( cur_sigint == (void (*)())SIG_IGN )
			cur_sigint = (void (*)())SIG_IGN; /* detached? */
		else
			cur_sigint = sig2;	/* back to here w/!0 return */
#endif
	} else {
		(void)fprintf(outfile, "\nAborted.\n");
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

		/* apply solid editing changes if necessary */
		if( sedraw > 0) {
			sedit();
			sedraw = 0;
			dmaflag = 1;
		}

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
	vect_t		knobvec;	/* knob slew */
	int		i;
	int		len;
	int		formerly_non_blocking = non_blocking;
	static int	need_penup = 0;
	static struct device_values	old_values;

	/*
	 * dmr_input() will suspend until some change has occured,
	 * either on the device peripherals, or a command has been
	 * entered on the keyboard, unless the non-blocking flag is set.
	 */
	i = dmp->dmr_input( 0, non_blocking );	/* fd 0 for cmds */
	if( i )  {
		struct rt_vls	str;
		rt_vls_init(&str);

		/* Read input line */
		if( rt_vls_gets( &str, stdin ) >= 0 )  {
			rt_vls_strcat( &str, "\n" );
			if( cmdline( &str ) )
				pr_prompt();
		} else {
			/* Check for Control-D (EOF) */
			if( feof( stdin ) )  {
				/* EOF, let's hit the road */
				f_quit();
				/* NOTREACHED */
			}
		}
		rt_vls_free(&str);
	}
	non_blocking = 0;

	/********************************
	 *  Begin compatability code	*
	 ********************************/

	/* Process any function button presses */
	if( dm_values.dv_buttonpress )  {
		rt_vls_printf( &dm_values.dv_string, "press %s\n",
			label_button(dm_values.dv_buttonpress) );
		dm_values.dv_buttonpress = 0;
	}

	/* Detect any joystick activity */
	if( dm_values.dv_xjoy != old_values.dv_xjoy )  {
		rt_vls_printf( &dm_values.dv_string, "knob x %f\n",
			dm_values.dv_xjoy );
	}
	if( dm_values.dv_yjoy != old_values.dv_yjoy )  {
		rt_vls_printf( &dm_values.dv_string, "knob y %f\n",
			dm_values.dv_yjoy );
	}
	if( dm_values.dv_zjoy != old_values.dv_zjoy )  {
		rt_vls_printf( &dm_values.dv_string, "knob z %f\n",
			dm_values.dv_zjoy );
	}

	/*
	 * Use pen input.
	 */		 
	switch( dm_values.dv_penpress )  {
	case DV_INZOOM:
		rt_vls_strcat( &dm_values.dv_string, "zoom 2\n" );
		break;

	case DV_OUTZOOM:
		rt_vls_strcat( &dm_values.dv_string, "zoom 0.5\n" );
		break;

	case DV_SLEW:		/* Move view center to here */
		{
			vect_t tabvec;
			tabvec[X] =  dm_values.dv_xpen / 2047.0;
			tabvec[Y] =  dm_values.dv_ypen / 2047.0;
			tabvec[Z] = 0;
			slewview( tabvec );
			/* XXX still needs conversion to a command */
		}
		break;

	case DV_PICK:		/* transition 0 --> 1 */
		rt_vls_printf( &dm_values.dv_string, "M 1 %d %d\n",
			dm_values.dv_xpen, dm_values.dv_ypen);
		non_blocking++;	/* to catch transition back to 0 */
		need_penup = 1;
		break;
	case 0:			/* transition 1 --> 0 */
		if( need_penup && formerly_non_blocking )  {
			need_penup = 0;
			rt_vls_printf( &dm_values.dv_string, "M 0 %d %d\n",
				dm_values.dv_xpen, dm_values.dv_ypen);
		}
		break;
	default:
		(void)fprintf(outfile, "pen(%d,%d,x%x) -- bad dm press code\n",
		dm_values.dv_xpen,
		dm_values.dv_ypen,
		dm_values.dv_penpress);
		break;
	}

	/*
	 * Apply the knob slew factor to the view center
	 */
	if( dm_values.dv_xslew != old_values.dv_xslew )  {
		rt_vls_printf( &dm_values.dv_string, "knob X %f\n",
			dm_values.dv_xslew );
	}
	if( dm_values.dv_yslew != old_values.dv_yslew )  {
		rt_vls_printf( &dm_values.dv_string, "knob Y %f\n",
			dm_values.dv_yslew );
	}
	if( dm_values.dv_zslew != old_values.dv_zslew )  {
		rt_vls_printf( &dm_values.dv_string, "knob Z %f\n",
			dm_values.dv_zslew );
	}

	/* Apply zoom rate input to current view window size */
	if( dm_values.dv_zoom != old_values.dv_zoom )  {
		rt_vls_printf( &dm_values.dv_string, "knob S %f\n",
			dm_values.dv_zoom );
	}

	/* See if the angle/distance cursor is doing anything */
	/* XXX still needs conversion to a command */
	if(  adcflag && dm_values.dv_flagadc )
		dmaflag = 1;	/* Make refresh call dozoom */

	/********************************
	 *  End compatability code	*
	 ********************************/

	/*
	 *  Process any "string commands" sent to us by the display manager.
	 *  (Or "invented" here, for compatability with old dm's).
	 *  Each one is expected to be newline terminated.
	 */
	if( (len = rt_vls_strlen( &dm_values.dv_string )) > 0 )  {
		register char	*cp = rt_vls_addr( &dm_values.dv_string );
		char		*ep;
		char		*end = cp + len;
		struct rt_vls	cmd;

		rt_vls_init( &cmd );

		while( cp < end )  {
#ifdef BSD
			ep = index( cp, '\n' );
#else
			ep = strchr( cp, '\n' );
#endif
			if( ep == NULL )  break;

			/* Copy one cmd, incl newline.  Null terminate */
			rt_vls_strncpy( &cmd, cp, ep-cp+1 );
			/* cmdline insists on ending with newline&null */
			(void)cmdline( &cmd );
			cp = ep+1;
		}
		rt_vls_trunc( &dm_values.dv_string, 0 );
		rt_vls_free( &cmd );
	}

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

		factor = 1.0 - (dm_values.dv_zoom / 10);
		Viewscale *= factor;
		if( Viewscale < MINVIEW )
			Viewscale = MINVIEW;
		else  {
			non_blocking++;
		}
		{
			/* Scaling (zooming) takes place around view center */
			mat_t	scale_mat;
			mat_t	from_Viewcenter;
			mat_t	temp;

			mat_idn( scale_mat );
			scale_mat[15] = 1/factor;

			mat_idn( from_Viewcenter );
			from_Viewcenter[MDX] = -toViewcenter[MDX];
			from_Viewcenter[MDY] = -toViewcenter[MDY];
			from_Viewcenter[MDZ] = -toViewcenter[MDZ];
			mat_mul( temp, scale_mat, toViewcenter );
			mat_mul2( from_Viewcenter, temp );
			mat_mul2( temp, ModelDelta );
		}
		dmaflag = 1;
		new_mats();
	}

	/* Keep a copy of knob values, for later comparison */
	old_values = dm_values;	/* struct copy */

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

		rt_prep_timer();

		if( mged_variables.predictor )
			predictor_frame();

		dmp->dmr_prolog();	/* update displaylist prolog */

		/* Apply zoom & rotate translation vectors */
		dozoom();

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
 *			U S E J O Y
 *
 * Apply the "joystick" delta rotation to the viewing direction.
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
		mat_mul2( newinv, ModelDelta );
	}
	new_mats();
}

/*
 *			S E T V I E W
 *
 * Set the view.  Angles are DOUBLES, in degrees.
 *
 * Given that viewvec = scale . rotate . (xlate to view center) . modelvec,
 * we just replace the rotation matrix.
 */
void
setview( a1, a2, a3 )
double a1, a2, a3;		/* DOUBLE angles, in degrees */
{
	buildHrot( Viewrot, a1 * degtorad, a2 * degtorad, a3 * degtorad );
	new_mats();
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
	mat_t	temp;

	MAT_DELTAS_GET_NEG( old_model_center, toViewcenter );

	MAT4X3PNT( new_model_center, view2model, view_pos );
	MAT_DELTAS_VEC_NEG( toViewcenter, new_model_center );

	VSUB2( diff, new_model_center, old_model_center );
	mat_idn( delta );
	MAT_DELTAS_VEC( delta, diff );
	mat_mul2( delta, ModelDelta );
	new_mats();
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
	long now;
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
#if defined(sgi) && !defined(mips)
int
sig2()
{
	longjmp( jmp_env, 1 );
	/* NOTREACHED */
}
#else
void
sig2()
{
	longjmp( jmp_env, 1 );
	/* NOTREACHED */
}
#endif


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

	if( (path = getenv("MGED_RCFILE")) != (char *)NULL )  {
		if( (fp = fopen(path, "r")) != NULL )  {
			mged_source_file( fp );
			fclose(fp);
			return(0);
		}
	}
	if( (path = getenv("HOME")) != (char *)NULL )  {
		struct rt_vls	str;
		rt_vls_init(&str);
		rt_vls_strcpy( &str, path );
		rt_vls_strcat( &str, "/.mgedrc" );
		if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )  {
			mged_source_file( fp );
			fclose(fp);
			rt_vls_free(&str);
			return(0);
		}
		rt_vls_free(&str);
	}
	if( (fp = fopen( ".mgedrc", "r" )) != NULL )  {
		mged_source_file( fp );
		fclose(fp);
		return(0);
	}
	return(-1);
}

#if defined(BSD) && !defined(__convexc__)
extern void bcopy();
void memcpy(to,from,cnt)
{
	bcopy(from,to,cnt);
}
#endif

#define MAX_TRAIL	32
struct trail {
	int	cur_index;	/* index of first free entry */
	int	nused;		/* max index in use */
	point_t	pt[MAX_TRAIL];
}

init_trail(tp)
struct trail	*tp;
{
	tp->cur_index = 0;
	tp->nused = 0;
}

/*
 *  Add a new point to the end of the trail.
 */
push_trail(tp, pt)
struct trail	*tp;
point_t		pt;
{
	VMOVE( tp->pt[tp->cur_index], pt );
	if( tp->cur_index >= tp->nused )  tp->nused++;
	tp->cur_index++;
	if( tp->cur_index >= MAX_TRAIL )  tp->cur_index = 0;
}

/*
 *  Draw from the most recently added point, backwards.
 */
draw_trail(vhead, tp)
struct rt_list	*vhead;
struct trail	*tp;
{
	int	i;
	int	todo = tp->nused;

	RT_LIST_INIT( vhead );
	if( tp->nused <= 0 )  return;
	if( (i = tp->cur_index-1) < 0 )  i = tp->nused-1;
	for( ; todo > 0; todo-- )  {
		if( todo == tp->nused )  {
			RT_ADD_VLIST( vhead, tp->pt[i], RT_VLIST_LINE_MOVE );
		}  else  {
			RT_ADD_VLIST( vhead, tp->pt[i], RT_VLIST_LINE_DRAW );
		}
		if( (--i) < 0 )  i = tp->nused-1;
	}
}

static struct trail	ll;
static struct trail	ul;
static struct trail	lr;
static struct trail	ur;

#define PREDICTOR_NAME	"_PREDIC_FRAME_"

predictor_kill()
{
	struct rt_vls	str;

	rt_vls_init( &str );
	rt_vls_printf( &str, "kill %s\n", PREDICTOR_NAME );
	(void)cmdline( &str );
	rt_vls_trunc( &str, 0 );

	rt_vls_strcat( &str, "kill _PREDIC_TRAIL_*\n" );
	(void)cmdline( &str );
	rt_vls_free( &str );

	init_trail( &ll );
	init_trail( &ul );
	init_trail( &lr );
	init_trail( &ur );
}

#define TF_X	0.14
#define TF_Y	0.07
#define TF_Z	(1.0-0.15)	/* To prevent Z clipping of TF_X */

#define TF_VL( _m, _v, _op ) \
	{ vect_t edgevect_m; \
	MAT4X3VEC( edgevect_m, predictorXv2m, _v ); \
	VADD2( _m, framecenter_m, edgevect_m ); \
	RT_ADD_VLIST( &vhead, _m, _op ); }

/*
 *			P R E D I C T O R _ F R A M E
 * 
 *  Note that the first time through, PREDICTOR_NAME won't be found yet,
 *  so the frame will be drawn.  As soon as motion starts & stops, it
 *  will vanish.  Consider this a user feedback cue, acknowledging
 *  the "set predictor=1" command.
 */
predictor_frame()
{
	int	i;
	int	nframes;
	mat_t	predictor;
	mat_t	predictorXv2m;
	point_t	v;		/* view coords */
	point_t	m;		/* model coords */
	struct rt_list	vhead;
	struct rt_list	trail;
	point_t	framecenter_m;
	point_t	framecenter_v;
	point_t	center_m;
	vect_t	delta_v;

	if( rateflag_rotate == 0 && rateflag_slew == 0 && rateflag_zoom == 0 )  {
		if( db_lookup( dbip, PREDICTOR_NAME, LOOKUP_QUIET ) != DIR_NULL )  {
			predictor_kill();
			dmaflag = 1;
		}
		return;
	}

	/* Advance into the future */
	nframes = (int)(mged_variables.predictor_advance / frametime);
	if( nframes < 1 )  nframes = 1;

	/* Build view2model matrix for the future time */
	mat_idn( predictor );
	for( i=0; i < nframes; i++ )  {
		mat_mul2( ModelDelta, predictor );
	}
	mat_mul( predictorXv2m, predictor, view2model );

	MAT_DELTAS_GET_NEG( center_m, toViewcenter );
	MAT4X3PNT( framecenter_m, predictor, center_m );
	MAT4X3PNT( framecenter_v, model2view, framecenter_m );
#if 0
VPRINT("\ncenter_m", center_m);
mat_print("ModelDelta", ModelDelta);
VPRINT("framecenter_m", framecenter_m);
VPRINT("framecenter_v", framecenter_v);
	framecenter_v[Z] = TF_Z;
VPRINT("framecenter_v", framecenter_v);
	MAT4X3PNT( framecenter_m, view2model, framecenter_v );
VPRINT("framecenter_m", framecenter_m);
#endif

	/* Draw frame around point framecenter_v. */
	RT_LIST_INIT( &vhead );

	VSETALL( delta_v, 0 );		/* Centering dot */
	TF_VL( m, delta_v, RT_VLIST_LINE_MOVE );
	TF_VL( m, delta_v, RT_VLIST_LINE_DRAW );

	VSET( delta_v, -TF_X, -TF_Y, 0 );
	TF_VL( m, delta_v, RT_VLIST_LINE_MOVE );
#if 0
VPRINT("delta_v", delta_v );
VPRINT("ll_m", m );
#endif
	push_trail( &ll, m );

	VSET( delta_v,  TF_X, -TF_Y, 0 );
	TF_VL( m, delta_v, RT_VLIST_LINE_DRAW );
	push_trail( &lr, m );

	VSET( delta_v,  TF_X,  TF_Y, 0 );
	TF_VL( m, delta_v, RT_VLIST_LINE_DRAW );
	push_trail( &ur, m );

	VSET( delta_v, -TF_X,  TF_Y, 0 );
	TF_VL( m, delta_v, RT_VLIST_LINE_DRAW );
	push_trail( &ul, m );

	VSET( delta_v, -TF_X, -TF_Y, 0 );
	TF_VL( m, delta_v, RT_VLIST_LINE_DRAW );

	invent_solid( PREDICTOR_NAME, &vhead, 0x00FFFFFFL );

	draw_trail( &trail, &ll );
	invent_solid( "_PREDIC_TRAIL_LL_", &trail, 0x00FF00FFL );

	draw_trail( &trail, &lr );
	invent_solid( "_PREDIC_TRAIL_LR_", &trail, 0x0000FFFFL );

	draw_trail( &trail, &ur );
	invent_solid( "_PREDIC_TRAIL_UR_", &trail, 0x00FF00FFL );

	draw_trail( &trail, &ul );
	invent_solid( "_PREDIC_TRAIL_UL_", &trail, 0x0000FFFFL );

	/* Done */
	mat_idn( ModelDelta );
}

void
predictor_hook()
{
	if( mged_variables.predictor > 0 )  {
		/* Allocate storage? */
	} else {
		/* Release storage? */
		predictor_kill();
	}
	dmaflag = 1;
}
