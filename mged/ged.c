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
 *	finish		Terminate with logging.  To be used instead of exit().
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
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

char CopyRight_Notice[] = "@(#) Copyright (C) 1985 by the United States Army";

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <setjmp.h>

#include "ged_types.h"
#include "ged.h"
#include "solid.h"
#include "../h/db.h"
#include "sedit.h"
#include "dm.h"
#include "../h/vmath.h"

#ifndef	LOGFILE
#define LOGFILE	"/vld/lib/gedlog"	/* usage log */
#endif

extern void	exit(), perror(), sync();
extern char	*malloc(), *tempnam();
extern int	close(), dup(), execl(), fork(), getuid(), open(), pipe(),
		printf(), unlink(), write();
extern long	time();
#ifdef BSD42
extern char	*sprintf();
#else
extern int	sprintf();
#endif

extern struct dm dm_Mg;
struct device_values dm_values;		/* Dev Values, filled by dm-XX.c */

int	dmaflag;			/* Set to 1 to force new screen DMA */

/**** Begin global display information, used by dm.c *******/
int		xcross = 0;
int		ycross = 0;		/* tracking cross position */
int		inten_offset;		/* intensity offset (IOR) */
int		inten_scale;		/* intensity scale (ISR) */
int		windowbounds[6];	/* X hi,lo;  Y hi,lo;  Z hi,lo */
/**** End global display information ******/

static jmp_buf	jmp_env;		/* For non-local gotos */
void		(*cur_sigint)();	/* Current SIGINT status */
void		sig2();
void		new_mats();
void		usejoy();
static void	log_event();
extern char	version[];		/* from vers.c */

void
pr_prompt()  {
	(void)printf("mged> ");
	(void)fflush(stdout);
}

/* 
 *			M A I N
 */
int
main(argc,argv)
int argc;
char **argv;
{
	register int i;

	/* Check for proper invocation */
	if( argc != 2 )  {
		(void)printf("Usage:  %s database\n", argv[0]);
		return(1);		/* NOT finish() */
	}

	/* Identify ourselves */
	(void)printf("%s\n", version);

	/* Get input file */
	db_open( argv[1] );

	/* Quick -- before he gets away -- write a logfile entry! */
	log_event( "START", argv[1] );

	/* Set up linked lists */
	HeadSolid.s_forw = &HeadSolid;
	HeadSolid.s_back = &HeadSolid;
	FreeSolid = SOLID_NULL;
	
	state = ST_VIEW;
	es_edflag = -1;
	inpara = newedge = 0;

	/* init rotation matrix */
	mat_idn( identity );		/* Handy to have around */
	Viewscale = .125;		/* also in chgview.c */
	mat_idn( Viewrot );
	mat_idn( toViewcenter );
	mat_idn( modelchanges );

	new_mats();

	setview( 0, 0, 0 );

	drawreg = 0;		/* no region processing */
	regmemb = -1;		/* no members to process */
	reg_error = 0;		/* no errors yet */
	no_memory = 0;		/* memory left */
	es_edflag = -1;		/* no solid editing just now */

	inten_scale = 0x7FF0;	/* full positive, to start with */

	windowbounds[0] = 2047;		/* XHR */
	windowbounds[1] = -2048;	/* XLR */
	windowbounds[2] = 2047;		/* YHR */
	windowbounds[3] = -2048;	/* YLR */
	windowbounds[4] = 2047;		/* ZHR */
	windowbounds[5] = -2048;	/* ZLR */

	dmaflag = 1;

	/* Fire up the display manager, and the display processor */
	get_attached();

	/* Here we should print out a "message of the day" file */

	/*	 Scan input file and build the directory	 */
	dir_build();

	/* Initialize the menu mechanism to be off, but ready. */
	menu_init();

	refresh();			/* Put up faceplate */

	/* Caught interrupts take us here */
	if( setjmp( jmp_env ) == 0 )  {
		/* First pass through */
		if( signal( SIGINT, SIG_IGN ) == SIG_IGN )
			cur_sigint = (void (*)())SIG_IGN; /* detached? */
		else
			cur_sigint = sig2;	/* back to here w/!0 return */
	} else {
		(void)printf("\nAborted.\n");
	}
	(void)signal( SIGINT, SIG_IGN );
	(void)signal( SIGPIPE, SIG_IGN );
	pr_prompt();

	/****************  M A I N   L O O P   *********************/
	while(1) {
		static vect_t knobvec;	/* knob slew */
		static int rateflag;	/* != 0 means change RATE */

		/*
		 * dmr_input() will suspend until some change has occured,
		 * either on the device peripherals, or a command on the
		 * keyboard.
		 */
		i = dmp->dmr_input( 0, rateflag );	/* fd 0 for cmds */
		if( i )  {
			cmdline();
			pr_prompt();
		}

		rateflag = 0;

		/* Process any function button presses */
		if( dm_values.dv_buttonpress )
			button( (long)dm_values.dv_buttonpress );

		/* Process any joystick activity */
		if(	dmaflag
		   ||	dm_values.dv_xjoy != 0.0
		   ||	dm_values.dv_yjoy != 0.0
		   ||	dm_values.dv_zjoy != 0.0
		)  {
			rateflag++;

			/* Compute delta x,y,z parameters */
			usejoy( dm_values.dv_xjoy * 6 * degtorad,
				dm_values.dv_yjoy * 6 * degtorad,
				dm_values.dv_zjoy * 6 * degtorad );
		}

		/*
		 * Use data tablet inputs.
		 */		 
		if( dm_values.dv_penpress )
			rateflag++;	/* to catch transition back to 0 */

		switch( dm_values.dv_penpress )  {
		case DV_INZOOM:
			Viewscale *= 0.5;
			new_mats();
			break;

		case DV_OUTZOOM:
			Viewscale *= 2.0;
			new_mats();
			break;

		case DV_SLEW:		/* Move view center to here */
			{
				vect_t tabvec;
				tabvec[X] =  dm_values.dv_xpen / 2047.0;
				tabvec[Y] =  dm_values.dv_ypen / 2047.0;
				tabvec[Z] = 0;
				slewview( tabvec );
			}
			break;

		case DV_PICK:		/* transition 0 --> 1 */
		case 0:			/* transition 1 --> 0 */
			usepen();
			break;
		default:
			(void)printf("pen(%d,%d,x%x) -- bad dm press code\n",
			dm_values.dv_xpen,
			dm_values.dv_ypen,
			dm_values.dv_penpress);
			break;
		}

		/*
		 * Set up window so that drawing does not run over into the
		 * status line area, and menu area (if present).
		 */
		windowbounds[0] = 2047;		/* XHR */
		if( illump != SOLID_NULL )
			windowbounds[0] = XLIM;

		windowbounds[3] = TITLE_YBASE-TEXT1_DY;	/* YLR */

		/* Apply the knob slew factor to the view center */
		if( dm_values.dv_xslew != 0.0 || dm_values.dv_yslew != 0.0 )  {
			/* slew 1/10th of the view per update */
			knobvec[X] = -dm_values.dv_xslew / 10;
			knobvec[Y] = -dm_values.dv_yslew / 10;
			knobvec[Z] = 0;
			slewview( knobvec );
			rateflag++;
		}

		/* Apply zoom rate input to current view window size */
		if( dm_values.dv_zoom != 0 )  {
#define MINVIEW		0.001	/* smallest view.  Prevents runaway zoom */

			Viewscale *= 1.0 - (dm_values.dv_zoom / 10);
			if( Viewscale < MINVIEW )
				Viewscale = MINVIEW;
			else  {
				if( Viewscale > maxview * 15.0 )
					Viewscale = maxview * 15.0;
				else
					rateflag++;
			}
			new_mats();
			dmaflag = 1;
		}

		/* apply solid editing changes if necessary */
		if( sedraw > 0) {
			sedit();			/* e13.c */
			sedraw = 0;
			dmaflag = 1;
		}

		/* See if the angle/distance cursor is doing anything */
		if(  adcflag && dm_values.dv_flagadc )
			dmaflag = 1;	/* Make refresh call dozoom */

		/*
		 * Cause the control portion of the displaylist to be
		 * updated to reflect the changes made above.
		 */	 
		refresh();
	}
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
	dmp->dmr_prolog();	/* update displaylist prolog */
	/*
	 * if something has changed, then go update the display.
	 * Otherwise, we are happy with the view we have
	 */
	if( dmaflag )  {
		/* Apply zoom & rotate translation vectors */
		dozoom();

		/* Restore to non-rotated, full brightness */
		dmp->dmr_normal();

		/* Compute and display angle/distance cursor */
		if (adcflag)
			adcursor();

		/* Display titles, etc */
		dotitles();

		dmp->dmr_epilog();
	}
	dmp->dmr_update();

	dmaflag = 0;
}

/*
 *			U S E J O Y
 *
 * Apply the "joystick" rotation to the viewing perspective.
 *
 * Xangle = angle of rotation about the X axis, and is done 3rd.
 * Yangle = angle of rotation about the Y axis, and is done 2nd.
 * Zangle = angle of rotation about the Z axis, and is done 1st.
 */
void
usejoy( xangle, yangle, zangle )
float xangle, yangle, zangle;
{
	static mat_t	newrot;		/* NEW rot matrix, from joystick */

	dmaflag = 1;

	/*
	 * Joystick is used for parameter or solid rotation (ST_S_EDIT),
	 * or for object rotation (ST_O_EDIT).
	 */
	if( es_edflag == PROT || es_edflag == SROT || movedir == ROTARROW ) {
		static mat_t tempp;
		static vect_t point;

		mat_idn( incr_change );
		buildHrot( incr_change, xangle, yangle, zangle );

		if( es_edflag == SROT || es_edflag == PROT )  {
			/* accumulate the translations */
			mat_mul(tempp, incr_change, acc_rot_sol);
			mat_copy(acc_rot_sol, tempp);
			sedit();	/* change es_rec only, NOW */
			return;
		}
		/* accumulate change matrix - do it wrt a point NOT view center */
		mat_mul(tempp, modelchanges, es_mat);
		MAT4X3PNT(point, tempp, es_rec.s.s_values);
		wrt_point(modelchanges, incr_change, modelchanges, point);

		new_mats();
		return;
	}

	/* NORMAL CASE.
	 * Apply delta viewing rotation for non-edited parts.
	 * The view rotates around the VIEW CENTER.
	 * ***** THIS IS BROKEN!! ****** (rotation is around origin, humbug)
	 */
	mat_idn( newrot );
	buildHrot( newrot, xangle, yangle, zangle );
	{
		static mat_t temp;
		mat_mul( temp, newrot, Viewrot );
		mat_copy( Viewrot, temp );
		new_mats();
	}
}

/*
 *			S E T V I E W
 *
 * Set the view.  Angles are integers, in degrees.
 *
 * Given that viewvec = scale . rotate . (xlate to view center) . modelvec,
 * we just replace the rotation matrix.
 */
void
setview( a1, a2, a3 )
int a1, a2, a3;		/* integer angles, in degrees */
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
	static vect_t model_pos;

	MAT4X3PNT( model_pos, view2model, view_pos );
	toViewcenter[MDX] = -model_pos[X];
	toViewcenter[MDY] = -model_pos[Y];
	toViewcenter[MDZ] = -model_pos[Z];
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

	(void)sprintf( line, "%s [%s] time=%ld uid=%d (%s) %s\n",
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
finish( exitcode )
int	exitcode;
{
	char place[64];

	(void)sprintf( place, "exit_status=%d", exitcode );
	log_event( "CEASE", place );
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
	finish(0);
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
