/*                    D M - T E K 4 1 0 9 . C
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file dm-tek4109.c
 *
 * A modified version of the Tektronix 4014 display driver for
 * Tektronix 4109 compatible displays.
 *
 *  Authors -
 *	Michael John Muuss
 *      Glenn E. Martin (NRTC)
 *	Stephen Hunter Willson (NRTC) <willson@nrtc-gremlin.ARPA>
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



/*  Notes:
    You almost certainly want to use this with two terminals.  Otherwise
you can't use the crosshairs to manipulate the image.
    The following keys can be pressed on the 4109 (while the cursor
is showing):
    b -- make objects bigger (zoom in)
    s -- make objects smaller (zoom out)
    Z -- print out current x and y coordinates of crosshair
    ' ' -- pick thing or coordinates pointed at
    . -- slew view

    The cursor speed is set to a moderate rate (5); pressing SHIFT
while moving the cursor increases the rate to fast (10).  This
is different from the factory default.


	Stephen Hunter Willson,
	NRTC
*/

#include <stdio.h>
#include <sys/time.h>		/* for struct timeval */
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./dm.h"
#include "./solid.h"

/* Display Manager package interface */
 
#define TEKBOUND	1000.0	/* Max magnification in Rot matrix */
int	T49_init();
int	T49_open();
void	T49_close();
MGED_EXTERN(void	T49_input, (fd_set *input, int noblock) );
void	T49_prolog(), T49_epilog();
void	T49_normal(), T49_newrot();
void	T49_update();
void	T49_puts(), T49_2d_line(), T49_light();
int	T49_object();
unsigned T49_cvtvecs(), T49_load();
void	T49_statechange(), T49_viewchange(), T49_colorchange();
void	T49_window(), T49_debug();
int     T49_dm();

struct dm dm_T49 = {
  T49_init,
  T49_open, T49_close,
  T49_input,
  T49_prolog, T49_epilog,
  T49_normal, T49_newrot,
  T49_update,
  T49_puts, T49_2d_line,
  T49_light,
  T49_object,
  T49_cvtvecs, T49_load,
  T49_statechange,
  T49_viewchange,
  T49_colorchange,
  T49_window, T49_debug, T49_dm, 0,
  0,				/* no displaylist */
  0,				/* can't rt to this */
  TEKBOUND,
  "tek4109", "Tektronix 4109",		/* NRTC */
  0,
  0,
  0,
  0
};

extern struct device_values dm_values;	/* values read from devices */

static vect_t clipmin, clipmax;		/* for vector clipping */
static int oloy = -1;
static int ohiy = -1;
static int ohix = -1;
static int oextra = -1;

#define BELL	007
#define	FF	014		/* Form Feed  */
#define SUB	032		/* Turn on graphics cursor */
#define GS	035		/* Enter Graphics Mode (1st vec dark) */
#define ESC	033		/* Escape */
#define US	037		/* Enter Alpha Mode */

static int second_fd;		/* fd of Tektronix if not /dev/tty */
static FILE *outfp;		/* Tektronix device to output on */
static char ttybuf[BUFSIZ];

static void	t49move(), t49cont();
static void	t49get_cursor(), t49cancel_cursor();
static void	t49label(), t49point(), t49linemod();

/*
 * Display coordinate conversion:
 *  Tektronix is using 0..4096
 *  GED is using -2048..+2048
 */
 

#define	GED_TO_TEK4109(x)	(((x)+2048) * 780 / 1024)
#define TEK4109_TO_GED(x)	(((x) * 1024 / 780) - 2048)

T49_init()
{
  return TCL_OK;
}

/*
 *			T E K _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
T49_open()
{
	char line[64], line2[64];

	bu_log("Output tty [stdout]? ");
	(void)fgets( line, sizeof(line), stdin ); /* \n, null terminated */
	line[strlen(line)-1] = '\0';		/* remove newline */
	if( feof(stdin) )  
		quit();
	if( line[0] != '\0' )  {
#if VMS
		if( (outfp = fopen(line,"r+")) == NULL )  {
			if( (outfp = fopen(line,"r+w")) == NULL )  {
				perror(line);
				return(1);		/* BAD */
			}
		}
#else
		if( (outfp = fopen(line,"r+w")) == NULL )  {
			(void)sprintf( line2, "/dev/tty%s%c", line, '\0' );
			if( (outfp = fopen(line2,"r+w")) == NULL )  {
				perror(line);
				return(1);		/* BAD */
			}
		}
#endif
		second_fd = fileno(outfp);
	} 
	else 
	{
#if VMS
		if( (outfp = fopen("SYS$OUTPUT","r+")) == NULL )
#else
		if( (outfp = fopen("/dev/tty","r+w")) == NULL )
#endif
			return(1);	/* BAD */
		second_fd = 0;		/* no second filedes */
	}
	setbuf( outfp, ttybuf );
	fprintf(outfp,"%c%%!0",ESC);	/* Place in TEK mode */
	fprintf(outfp, "%cNF1", ESC);	/* Set dc1/dc3 flow ctrl */
	fprintf(outfp,"%cMCA>B8:",ESC);	/* Set Graphics Font Size */
	fprintf(outfp,"%cLLA>",ESC);	/* Set Dialog to 30 Lines */
	fprintf(outfp,"%cKI0",ESC);	/* Process Delete Characters */
	fprintf(outfp,"%cIJ5:",ESC);	/* Change GIN cursor speed to 5,10 */ 
	fprintf(outfp,"%cTCK4C2F4",ESC);/* Change GIN cursor to yellow */
	fprintf(outfp,"%cML2",ESC);	/* set graphics line index -> 2 (red) 
					   helps reduce alpha/graphics clutter 
					*/
	return(0);			/* OK */
}

/*
 *  			T E K _ C L O S E
 *  
 *  Gracefully release the display.
 */
void
T49_close()
{
	t49cancel_cursor();
	fprintf(outfp,"%cLZ", ESC);	/* clear screen */
	fprintf(outfp,"%cLLA8",ESC);	/* NRTC - Set Dialog to 24 Lines */
	fprintf(outfp,"%c%%!1",ESC);	/* NRTC - Place in ANSI mode */
	(void)putc(US,outfp);
	(void)fflush(outfp);
	fclose(outfp);
}

/*
 *			T E K _ R E S T A R T
 *
 * Used when the display processor wanders off.
 */
void
T49_restart()
{
	bu_log("%cTek_restart\n",US);		/* NRTC */
}

/*
 *			T E K _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
T49_prolog()
{
	if( !dmaflag )
		return;

	/* If something significant has happened, clear screen and redraw */

	(void)putc(ESC,outfp);	/* Miniature typeface */
	(void)putc(';',outfp);

	(void)fprintf(outfp, "%cLZ", ESC);	/* clear dialog area */
	(void)putc(ESC,outfp);			/* clear screen area */
	(void)putc(FF,outfp);
	(void)fflush(outfp);
	t49cancel_cursor();
	ohix = ohiy = oloy = oextra = -1;

	/* Put the center point up */
	t49point( 0, 0 );
}

/*
 *			T E K _ E P I L O G
 */
void
T49_epilog()
{
	if( !dmaflag )
		return;
        t49move( TITLE_XBASE, SOLID_YBASE );
	(void)putc(US,outfp);
}

/*
 *  			T E K _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
void
T49_newrot(mat)
mat_t mat;
{
	return;
}

/*
 *  			T E K _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */
/* ARGSUSED */
int
T49_object( sp, mat, ratio, white )
register struct solid *sp;
mat_t mat;
double ratio;
{
	static vect_t last;
	register struct rt_vlist	*vp;
	int useful = 0;

	if(  sp->s_soldash )
		fprintf(outfp,"%cMV2",ESC);		/* Dot Dash    NRTC */
	else	
		fprintf(outfp,"%cMV0",ESC);		/* Solid Line  NRTC */

	for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
		register int	i;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;
		for( i = 0; i < nused; i++,cmd++,pt++ )  {
			static vect_t	start, fin;
			switch( *cmd )  {
			case RT_VLIST_POLY_START:
			case RT_VLIST_POLY_VERTNORM:
				continue;
			case RT_VLIST_POLY_MOVE:
			case RT_VLIST_LINE_MOVE:
				/* Move, not draw */
				MAT4X3PNT( last, model2view, *pt );
				continue;
			case RT_VLIST_POLY_DRAW:
			case RT_VLIST_POLY_END:
			case RT_VLIST_LINE_DRAW:
				/* draw */
				MAT4X3PNT( fin, model2view, *pt );
				VMOVE( start, last );
				VMOVE( last, fin );
				break;
			}
			if(
				vclip( start, fin, clipmin, clipmax ) == 0
			)  continue;

			t49move(	(int)( start[0] * 2047 ),
				(int)( start[1] * 2047 ) );
			t49cont(	(int)( fin[0] * 2047 ),
				(int)( fin[1] * 2047 ) );
			useful = 1;
		}
	}
	return(useful);
}

/*
 *			T E K _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
void
T49_normal()
{
	return;
}

/*
 *			T E K _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void
T49_update()
{
	if( second_fd )  {
		/* put up graphics cursor */
		(void)putc(ESC,outfp);
		(void)putc(SUB,outfp);
	} else
		(void)putc(US,outfp);		/* Alpha mode */
	(void)fflush(outfp);
}

/*
 *			T E K _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
void
T49_puts( str, x, y, size, color )
register char *str;
{
	t49move(x,y);
	t49label(str);
}

/*
 *			T E K _ 2 D _ G O T O
 *
 */
void
T49_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
	if( dashed )
		t49linemod("dotdashed");
	else
		t49linemod("solid");
	t49move(x1,y1);
	t49cont(x2,y2);
}

/*
 *			G E T _ C U R S O R
 *  
 *  Read the Tektronix cursor.  The Tektronix sends
 *  6 bytes:  The key the user struck, 4 bytes of
 *  encoded position, and a return (newline).
 *  Note this is is complicated if the user types
 *  a return or linefeed.
 *  (The terminal is assumed to be in cooked mode)
 */
static void
t49get_cursor()
{
	register char *cp;
	char ibuf[64];
	register int i;
	int hix, hiy, lox, loy;
	int xpen, ypen;

	/* ASSUMPTION:  Input is line buffered (tty cooked) */
	i = read( second_fd, ibuf, sizeof(ibuf) );
	/* The LAST 6 chars are the string from the tektronix */
	if( i < 6 )  {
		bu_log("short read of %d\n", i);
		return;		/* Fails if he hits RETURN */
	}
	cp = &ibuf[i-6];
	if( cp[5] != '\n' )  {
		bu_log("cursor synch?\n");
		bu_log("saw:%c%c%c%c%c%c\n",
			cp[0], cp[1], cp[2], cp[3], cp[4], cp[5] );
		return;
	}

	/* cp[0] is what user typed, followed by 4pos + NL */
	hix = ((int)cp[1]&037)<<7;
	lox = ((int)cp[2]&037)<<2;
	hiy = ((int)cp[3]&037)<<7;
	loy = ((int)cp[4]&037)<<2;

	/* Tek positioning is 0..4096,
	 * The desired range is -2048 <= x,y <= +2048.
	 */
	xpen = TEK4109_TO_GED(hix|lox);
	ypen = TEK4109_TO_GED(hiy|loy);
	if( xpen < -2048 || xpen > 2048 )
		xpen = 0;
	if( ypen < -2048 || ypen > 2048 )
		ypen = 0;

	switch(cp[0])  {
	case 'Z':
		bu_log("x=%d,y=%d\n", xpen, ypen);
		break;		/* NOP */
	case 'b':
		bu_vls_strcat( &dm_values.dv_string , "zoom 0.5\n" );
		break;
	case 's':
		bu_vls_strcat( &dm_values.dv_string , "zoom 2\n" );
		break;
	case '.':
		bu_vls_printf( &dm_values.dv_string , "M 1 %d %d\n", xpen, ypen );
		break;
	default:
		bu_log("s=smaller, b=bigger, .=slew, space=pick/slew\n");
		return;
	case ' ':
		bu_vls_printf( &dm_values.dv_string , "M 1 %d %d\n", xpen, ypen );
		break;
	}
}

/*
 *			T E K _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream, or a device event has
 * occured, unless "noblock" is set.
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 */
void
T49_input( input, noblock )
fd_set		*input;
int		noblock;
{
	struct timeval	tv;
	int		width;
	int		cnt;

#if defined(_SC_OPEN_MAX)
	if( (width = sysconf(_SC_OPEN_MAX)) <= 0 )
#endif
		width = 32;

	if( second_fd )  FD_SET( second_fd, input );

	/*
	 * Check for input on the keyboard
	 *
	 * Suspend execution until either
	 *  1)  User types a full line
	 *  2)  The timelimit on SELECT has expired
	 *
	 * If a RATE operation is in progress (zoom, rotate, slew)
	 * in which we still have to update the display,
	 * do not suspend execution.
	 */
	tv.tv_sec = 0;
	if( noblock )  {
		tv.tv_usec = 0;
	}  else  {
		/* 1/20th second */
		tv.tv_usec = 50000;
	}
	cnt = select( width, input, (fd_set *)0,  (fd_set *)0, &tv );
	if( cnt < 0 )  {
		perror("dm-tek/select");
	}

	if( second_fd && FD_ISSET(second_fd, input) )
		t49get_cursor();
}

/* 
 *			T E K _ L I G H T
 */
/* ARGSUSED */
void
T49_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
unsigned
T49_cvtvecs( sp )
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
unsigned
T49_load( addr, count )
unsigned addr, count;
{
	bu_log("%cTek_load(x%x, %d.)\n",US, addr, count );
	return( 0 );
}

void
T49_statechange()
{
}

void
T49_viewchange()
{
}

void
T49_colorchange()
{
}

/*
 * Perform the interface functions
 * for the Tektronix 4014-1 or 4109 with Extended Graphics Option.
 * The Extended Graphics Option makes available a field of
 * 10 inches vertical, and 14 inches horizontal, with a resolution
 * of 287 points per inch.
 *
 * The Tektronix is Quadrant I, 4096x4096 (not all visible).
 */

/* The input we see is -2048..+2047 */
/* Continue motion from last position */
static void
t49cont(x,y)
register int x,y;
{
	int hix,hiy,lox,loy,extra;

	x = GED_TO_TEK4109(x);
	y = GED_TO_TEK4109(y);

	hix=(x>>7) & 037;
	hiy=(y>>7) & 037;
	lox = (x>>2)&037;
	loy=(y>>2)&037;
	extra=x&03+(y<<2)&014;
#if 0
	n = (abs(hix-ohix) + abs(hiy-ohiy) + 6) / 12;
#endif
	if(hiy != ohiy){
		(void)putc(hiy|040,outfp);
		ohiy=hiy;
	}
	if(hix != ohix) {
		if(extra != oextra) {
			(void)putc(extra|0140,outfp);
			oextra=extra;
		}
		(void)putc(loy|0140,outfp);
		(void)putc(hix|040,outfp);
		ohix=hix;
		oloy=loy;
	} else {
		if(extra != oextra) {
			(void)putc(extra|0140,outfp);
			(void)putc(loy|0140,outfp);
			oextra=extra;
			oloy=loy;
		} else if(loy != oloy) {
			(void)putc(loy|0140,outfp);
			oloy=loy;
		}
	}
	(void)putc(lox|0100,outfp);
#if 0
	while(n--)
		(void)putc(0,outfp);
#endif
}

static void
t49move(xi,yi)
{
/*	fprintf(outfp,"%cTekmove: x=%d, y=%d \n",US,xi+2048,yi+2048);   */
								/* NRTC */
	(void)putc(GS,outfp);			/* Next vector blank */
	t49cont(xi,yi);
}

static void
t49cancel_cursor()
{
	extern unsigned sleep();
	(void)fprintf(outfp, "%cKC", ESC);	/* Cancel crosshairs */
	(void)fflush(outfp);
	sleep(2);	/* Have to wait for terminal reset */
}

static void t49label(s)
register char *s;
{
int	length;				/* NRTC */
char	hi, low;				/* NRTC */
	length= strlen(s);			/* NRTC */
	if ( length <= 0 ) {			/* NRTC */
	   (void)putc(US,outfp);			/* NRTC */
	   ohix = ohiy = oloy = oextra = -1;		/* NRTC */
	   return;				/* NRTC */
	   }					/* NRTC */
	hi = (length>>4) + 64;			/* NRTC */
	low= (length & 15) + 48;		/* NRTC */
	(void)fprintf(outfp,"%cLT%c%c%s",ESC,hi,low,s); /* NRTC */
	(void)putc(US,outfp);			/* NRTC */
	ohix = ohiy = oloy = oextra = -1;
}

/* Line Mode Command - Select Tektronics Preset Line Display 4014 */

static void t49linemod(s)
register char *s;
{
	char  c;

	switch(s[0]){
	case 'l':	
		c = '7';                         /* Long Dashed Line   NRTC */
		break;
	case 'd':	
		if(s[3] != 'd')c='1';		/* Dot Line   NRTC */
		else c='2';			/* Dot-Dashed Line  NRTC  */
		break;
	case 's':
		if(s[5] != '\0')c='3';		/* Short Dash Line  NRTC  */
		else c='0';			/* Solid Line       NRTC  */
		break;
	default:			/* DAG -- added support for colors */
		c = '0';			/* Solid Line        NRTC */
		break;
	}
	fprintf(outfp,"%cMV%c",ESC,c);		/* Set Line Mode  NRTC */
 
}

static void
t49point(xi,yi){
        t49move(xi,yi);
	t49cont(xi,yi);
}

/* ARGSUSED */
void
T49_debug(lvl)
{
}

void
T49_window(w)
register int w[];
{
	/* Compute the clipping bounds */
	clipmin[0] = w[1] / 2048.;
	clipmin[1] = w[3] / 2048.;
	clipmin[2] = w[5] / 2048.;
	clipmax[0] = w[0] / 2047.;
	clipmax[1] = w[2] / 2047.;
	clipmax[2] = w[4] / 2047.;
}

int
T49_dm(argc, argv)
int argc;
char *argv[];
{
  return TCL_OK;
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
