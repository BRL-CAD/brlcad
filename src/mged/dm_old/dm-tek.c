/*                        D M - T E K . C
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
/** @file dm-tek.c
 *
 * An unsatisfying (but useful) hack to allow GED to display
 * its images on Tektronix 4014 compatible displays.
 * Mostly, a precursor for BLIT and RasterTek code.
 *
 *  Author -
 *	Michael John Muuss
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



#include <stdio.h>
#include <sys/time.h>		/* for struct timeval */
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
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
int	Tek_init();
int	Tek_open();
void	Tek_close();
MGED_EXTERN(void	Tek_input, (fd_set *input, int noblock) );
void	Tek_prolog(), Tek_epilog();
void	Tek_normal(), Tek_newrot();
void	Tek_update();
void	Tek_puts(), Tek_2d_line(), Tek_light();
int	Tek_object();
unsigned Tek_cvtvecs(), Tek_load();
void	Tek_statechange(), Tek_viewchange(), Tek_colorchange();
void	Tek_window(), Tek_debug();
int     Tek_dm();

struct dm dm_Tek = {
  Tek_init,
  Tek_open, Tek_close,
  Tek_input,
  Tek_prolog, Tek_epilog,
  Tek_normal, Tek_newrot,
  Tek_update,
  Tek_puts, Tek_2d_line,
  Tek_light,
  Tek_object,
  Tek_cvtvecs, Tek_load,
  Tek_statechange,
  Tek_viewchange,
  Tek_colorchange,
  Tek_window, Tek_debug, Tek_dm, 0,
  0,				/* no displaylist */
  0,				/* can't rt to this */
  TEKBOUND,
  "tek", "Tektronix 4014",
  0,
  0,
  0,
  0
};

extern struct device_values dm_values;	/* values read from devices */
static int tek_count = 0;
static vect_t clipmin, clipmax;		/* for vector clipping */

#define BELL	007
#define	FF	014
#define SUB	032		/* Turn on graphics cursor */
#define GS	035		/* Enter Graphics Mode (1st vec dark) */
#define ESC	033
#define US	037		/* Enter Alpha Mode */

static int second_fd;		/* fd of Tektronix if not /dev/tty */
static FILE *outfp;		/* Tektronix device to output on */
static char ttybuf[BUFSIZ];
static int blit_emulator=0;	/* boolean: tek emulation on blit */

static void	tekmove(), tekcont(), get_cursor(), tekerase();
static void	teklabel(), teklinemod(), tekpoint();

/*
 * Display coordinate conversion:
 *  Tektronix is using 0..4096
 *  GED is using -2048..+2048
 */
#define	GED_TO_TEK(x)	(((x)+2048) * 780 / 1024)
#define TEK_TO_GED(x)	(((x) * 1024 / 780) - 2048)

Tek_init()
{
  return TCL_OK;
}

/*
 *			T E K _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
Tek_open()
{
  char line[64], line2[64];
  char *p;

  if(tek_count){
    ++tek_count;
    Tcl_AppendResult(interp, "Tek_open: tek is already open\n", (char *)NULL);
    return TCL_ERROR;
  }

  (void)sprintf(line, "%s", dname);

  /* check for blit emulator flag */
  if (p=strchr(line, ' ')){
    *p++ = '\0';
    while (*p && *p != '-') ++p;
    if (*p && p[1] == 'b') blit_emulator = 1;
  }

  if( (outfp = fopen(line, "r+w")) == NULL ) {
    (void)sprintf(line2, "/dev/tty%s%c", line, '\0' );
    if( (outfp = fopen(line2, "r+w")) == NULL ){
      if( (outfp = fopen("/dev/tty","r+w")) == NULL ){
	Tcl_AppendResult(interp, "Tek_open: failed to open ", dname,
			 ", ", line, " and /dev/tty", (char *)NULL);
	return TCL_ERROR;
      }else 
	second_fd = 0;          /* no second filedes */
    }else
      second_fd = fileno(outfp);
  }else
    second_fd = fileno(outfp);

  setbuf( outfp, ttybuf );

  bu_vls_printf(&pathName, ".dm_tek");

  if(second_fd)
    Tk_CreateFileHandler(second_fd, 1, get_cursor, (ClientData)NULL);

  tek_count = 1;
  return TCL_OK;
}

/*
 *  			T E K _ C L O S E
 *  
 *  Gracefully release the display.
 */
void
Tek_close()
{
  if(tek_count > 1){
    --tek_count;
    return;
  }
  
  (void)putc(US,outfp);
    (void)fflush(outfp);
    fclose(outfp);
    tek_count = 0;
}

/*
 *			T E K _ R E S T A R T
 *
 * Used when the display processor wanders off.
 */
void
Tek_restart()
{
  Tcl_AppendResult(interp, "Tek_restart\n", (char *)NULL);
}

/*
 *			T E K _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
Tek_prolog()
{
	/* If something significant has happened, clear screen and redraw */
	tekerase();
	/* Miniature typeface */
	(void)putc(ESC,outfp);
	(void)putc(';',outfp);

	/* Put the center point up */
	tekpoint( 0, 0 );
}

/*
 *			T E K _ E P I L O G
 */
void
Tek_epilog()
{
	tekmove( TITLE_XBASE, SOLID_YBASE );
	(void)putc(US,outfp);
	(void)fflush(outfp);
}

/*
 *  			T E K _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
void
Tek_newrot(mat)
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
Tek_object( sp, mat, ratio, white )
register struct solid *sp;
mat_t mat;
double ratio;
{
	static vect_t			last;
	register struct rt_vlist	*vp;
	int				useful = 0;

	if(  sp->s_soldash )
		(void)putc('b',outfp);	/* Dot dash */
	else
		(void)putc('`',outfp);	/* Solid */

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
			tekmove(	(int)( start[0] * 2047 ),
				(int)( start[1] * 2047 ) );
			tekcont(	(int)( fin[0] * 2047 ),
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
Tek_normal()
{
	return;
}

/*
 *			T E K _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void
Tek_update()
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
Tek_puts( str, x, y, size, color )
register char *str;
{
	tekmove(x,y);
	teklabel(str);
}

/*
 *			T E K _ 2 D _ G O T O
 *
 */
void
Tek_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
	if( dashed )
		teklinemod("dotdashed");
	else
		teklinemod("solid");
	tekmove(x1,y1);
	tekcont(x2,y2);
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
get_cursor(clientData, mask)
ClientData clientData;
int mask;
{
	register char *cp;
	char ibuf[64];
	register int i;
	int hix, hiy, lox, loy;
	int xpen, ypen;

	if ( blit_emulator ) {
		i = read( second_fd, ibuf, sizeof(ibuf) );
		if (i < 4) {
			register int j;
			Tcl_AppendResult(interp, "Short Blit read\n", (char *)NULL);
			for (j=0 ; j < i ; ++j){
			  struct bu_vls tmp_vls;

			  bu_vls_init(&tmp_vls);
			  bu_vls_printf(&tmp_vls, "%c(%x)\n", ibuf[j], ibuf[j]);
			  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			  bu_vls_free(&tmp_vls);
			}
			return;
		}
		cp = &ibuf[i-6];
		if ( i > 4 && (cp[4] != '\n' || cp[5] != 4 )) {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "saw:%c(%x) %c(%x) %c(%x) %c(%x) %c(%x) %c(%x)\n",
				cp[0], cp[0], cp[1], cp[1], cp[2], cp[2],
				cp[3], cp[3], cp[4], cp[4], cp[5], cp[5] );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		  return;
		}
		hix = ((int)cp[0]&037)<<7;
		lox = ((int)cp[1]&037)<<2;
		hiy = ((int)cp[2]&037)<<7;
		loy = ((int)cp[3]&037)<<2;
/*		bu_log("mouse: %d,%d\n",
			TEK_TO_GED(hix|lox), TEK_TO_GED(hiy|loy)); */

		xpen = TEK_TO_GED(hix|lox);
		ypen = TEK_TO_GED(hiy|loy);
		if( xpen < -2048 || xpen > 2048 )
			xpen = 0;
		if( ypen < -2048 || ypen > 2048 )
			ypen = 0;

		bu_vls_printf( &dm_values.dv_string , "M 1 %d %d\n", xpen, ypen );

		return;
	} else {
		/* ASSUMPTION:  Input is line buffered (tty cooked) */
		i = read( second_fd, ibuf, sizeof(ibuf) );
		/* The LAST 6 chars are the string from the tektronix */
		if( i < 6 )  {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "short read of %d\n", i);
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		  return;		/* Fails if he hits RETURN */
			
		}
		cp = &ibuf[i-6];
		if( cp[5] != '\n' )  {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls,
				"cursor synch?\nsaw:%c(%x) %c(%x) %c(%x) %c(%x) %c(%x) %c(%x)\n",
				cp[0], cp[0], cp[1], cp[1], cp[2], cp[2],
				cp[3], cp[3], cp[4], cp[4], cp[5], cp[5] );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
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
		xpen = TEK_TO_GED(hix|lox);
		ypen = TEK_TO_GED(hiy|loy);
		if( xpen < -2048 || xpen > 2048 )
			xpen = 0;
		if( ypen < -2048 || ypen > 2048 )
			ypen = 0;
		
		switch(cp[0])  {
		case 'Z':
		  {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "x=%d,y=%d\n", xpen, ypen);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		  }
		  break;		/* NOP */
		case 'b':
			bu_vls_strcat( &dm_values.dv_string, "zoom 2\n");
			break;
		case 's':
			bu_vls_strcat( &dm_values.dv_string, "zoom 0.5\n");
			break;
		case '.':
			bu_vls_printf( &dm_values.dv_string, "M 1 %d %d\n", xpen, ypen );
			break;
		default:
		  Tcl_AppendResult(interp, "s=smaller, b=bigger, .=slew, space=pick/slew\n",
				   (char *)NULL);
		  return;
		case ' ':
			bu_vls_printf( &dm_values.dv_string, "M 1 %d %d\n", xpen, ypen );
			break;
		}
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
Tek_input( input, noblock )
fd_set		*input;
int		noblock;
{
	struct timeval	tv;
	int		width;
	int		cnt;
#if 0
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
		get_cursor();
#endif
}

/* 
 *			T E K _ L I G H T
 */
/* ARGSUSED */
void
Tek_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
unsigned
Tek_cvtvecs( sp )
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
unsigned
Tek_load( addr, count )
unsigned addr, count;
{
  struct bu_vls tmp_vls;

  bu_vls_init(&tmp_vls);
  bu_vls_printf(&tmp_vls, "Tek_load(x%x, %d.)\n", addr, count );
  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
  bu_vls_free(&tmp_vls);

  return( 0 );
}

void
Tek_statechange()
{
}

void
Tek_viewchange()
{
}

void
Tek_colorchange()
{
}

/*
 * Perform the interface functions
 * for the Tektronix 4014-1 with Extended Graphics Option.
 * The Extended Graphics Option makes available a field of
 * 10 inches vertical, and 14 inches horizontal, with a resolution
 * of 287 points per inch.
 *
 * The Tektronix is Quadrant I, 4096x4096 (not all visible).
 */
static int oloy = -1;
static int ohiy = -1;
static int ohix = -1;
static int oextra = -1;

/* The input we see is -2048..+2047 */
/* Continue motion from last position */
static void
tekcont(x,y)
register int x,y;
{
	int hix,hiy,lox,loy,extra;
	int n;

	x = GED_TO_TEK(x);
	y = GED_TO_TEK(y);

	hix=(x>>7) & 037;
	hiy=(y>>7) & 037;
	lox = (x>>2)&037;
	loy=(y>>2)&037;
	extra=x&03+(y<<2)&014;
	n = (abs(hix-ohix) + abs(hiy-ohiy) + 6) / 12;
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
	while(n--)
		(void)putc(0,outfp);
}

static void
tekmove(xi,yi)
{
	(void)putc(GS,outfp);			/* Next vector blank */
	tekcont(xi,yi);
}

static void
tekerase()
{
	extern unsigned sleep();	/* DAG -- was missing */

	(void)putc(ESC,outfp);
	(void)putc(FF,outfp);
	ohix = ohiy = oloy = oextra = -1;
	(void)fflush(outfp);

	/* If 2 FD's, it's probably a BLIT, otherwise assume real Tek */
#ifdef BLIT
	if( !second_fd )
#endif
		(void)sleep(3);
}

static void
teklabel(s)
register char *s;
{
	(void)putc(US,outfp);
	for( ; *s; s++ )
		(void)putc(*s,outfp);
	ohix = ohiy = oloy = oextra = -1;
}

static void
teklinemod(s)
register char *s;
{
	register int c;				/* DAG -- was char */

	(void)putc(ESC,outfp);
	switch(s[0]){
	case 'l':	
		c = 'd';
		break;
	case 'd':	
		if(s[3] != 'd')c='a';
		else c='b';
		break;
	case 's':
		if(s[5] != '\0')c='c';
		else c='`';
		break;
	default:			/* DAG -- added support for colors */
		c = '`';
		break;
	}
	(void)putc(c,outfp);
}

static void
tekpoint(xi,yi){
	tekmove(xi,yi);
	tekcont(xi,yi);
}

/* ARGSUSED */
void
Tek_debug(lvl)
{
}

void
Tek_window(w)
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
Tek_dm(argc, argv)
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
