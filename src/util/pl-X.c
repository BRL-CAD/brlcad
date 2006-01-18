/*                          P L - X . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2006 United States Government as represented by
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
/** @file pl-X.c
 *
 *  Display plot3(5) on an X Window System display (X11R2)
 *
 *  Author -
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "machine.h"

#define	TBAD	0	/* no such command */
#define TNONE	1	/* no arguments */
#define TSHORT	2	/* Vax 16-bit short */
#define	TIEEE	3	/* IEEE 64-bit floating */
#define	TCHAR	4	/* unsigned chars */
#define	TSTRING	5	/* linefeed terminated string */

struct uplot {
	int	targ;	/* type of args */
	int	narg;	/* number or args */
	char	*desc;	/* description */
};
struct uplot uerror = { 0, 0, 0 };
struct uplot letters[] = {
/*A*/	{ 0, 0, 0 },
/*B*/	{ 0, 0, 0 },
/*C*/	{ TCHAR, 3, "color" },
/*D*/	{ 0, 0, 0 },
/*E*/	{ 0, 0, 0 },
/*F*/	{ TNONE, 0, "flush" },
/*G*/	{ 0, 0, 0 },
/*H*/	{ 0, 0, 0 },
/*I*/	{ 0, 0, 0 },
/*J*/	{ 0, 0, 0 },
/*K*/	{ 0, 0, 0 },
/*L*/	{ TSHORT, 6, "3line" },
/*M*/	{ TSHORT, 3, "3move" },
/*N*/	{ TSHORT, 3, "3cont" },
/*O*/	{ TIEEE, 3, "d_3move" },
/*P*/	{ TSHORT, 3, "3point" },
/*Q*/	{ TIEEE, 3, "d_3cont" },
/*R*/	{ 0, 0, 0 },
/*S*/	{ TSHORT, 6, "3space" },
/*T*/	{ 0, 0, 0 },
/*U*/	{ 0, 0, 0 },
/*V*/	{ TIEEE, 6, "d_3line" },
/*W*/	{ TIEEE, 6, "d_3space" },
/*X*/	{ TIEEE, 3, "d_3point" },
/*Y*/	{ 0, 0, 0 },
/*Z*/	{ 0, 0, 0 },
/*[*/	{ 0, 0, 0 },
/*\*/	{ 0, 0, 0 },
/*]*/	{ 0, 0, 0 },
/*^*/	{ 0, 0, 0 },
/*_*/	{ 0, 0, 0 },
/*`*/	{ 0, 0, 0 },
/*a*/	{ TSHORT, 6, "arc" },
/*b*/	{ 0, 0, 0 },
/*c*/	{ TSHORT, 3, "circle" },
/*d*/	{ 0, 0, 0 },
/*e*/	{ TNONE, 0, "erase" },
/*f*/	{ TSTRING, 1, "linmod" },
/*g*/	{ 0, 0, 0 },
/*h*/	{ 0, 0, 0 },
/*i*/	{ TIEEE, 3, "d_circle" },
/*j*/	{ 0, 0, 0 },
/*k*/	{ 0, 0, 0 },
/*l*/	{ TSHORT, 4, "line" },
/*m*/	{ TSHORT, 2, "move" },
/*n*/	{ TSHORT, 2, "cont" },
/*o*/	{ TIEEE, 2, "d_move" },
/*p*/	{ TSHORT, 2, "point" },
/*q*/	{ TIEEE, 2, "d_cont" },
/*r*/	{ TIEEE, 6, "d_arc" },
/*s*/	{ TSHORT, 4, "space" },
/*t*/	{ TSTRING, 1, "label" },
/*u*/	{ 0, 0, 0 },
/*v*/	{ TIEEE, 4, "d_line" },
/*w*/	{ TIEEE, 4, "d_space" },
/*x*/	{ TIEEE, 2, "d_point" },
/*y*/	{ 0, 0, 0 },
/*z*/	{ 0, 0, 0 }
};

double	getieee(void);
int	verbose;
double	cx, cy, cz;		/* current x, y, z, point */
double	arg[6];			/* parsed plot command arguments */
double	sp[6];			/* space command */
char	strarg[512];		/* string buffer */
int	width, height;

static char usage[] = "\
Usage: pl-X [-v] < unix_plot\n";

Display	*dpy;
Window	win;
GC	gc;
XFontStruct *fontstruct;

int
main(int argc, char **argv)
{
	register int	c;
	struct	uplot *up;
	int	i;

	while( argc > 1 ) {
		if( strcmp(argv[1], "-v") == 0 ) {
			verbose++;
		} else
			break;

		argc--;
		argv++;
	}
	if( isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}
	xsetup( argc, argv );

	while( (c = getchar()) != EOF ) {
		/* look it up */
		if( c < 'A' || c > 'z' ) {
			up = &uerror;
		} else {
			up = &letters[ c - 'A' ];
		}

		if( up->targ == TBAD ) {
			fprintf( stderr, "Bad command '%c' (0x%02x)\n", c, c );
			continue;
		}

		if( up->narg > 0 )
			getargs( up );

		switch( c ) {
		case 's':
		case 'w':
			sp[0] = arg[0];
			sp[1] = arg[1];
			sp[2] = 0;
			sp[3] = arg[2];
			sp[4] = arg[3];
			sp[5] = 0;
			break;
		case 'S':
		case 'W':
			sp[0] = arg[0];
			sp[1] = arg[1];
			sp[2] = arg[2];
			sp[3] = arg[3];
			sp[4] = arg[4];
			sp[5] = arg[5];
			break;
		case 'm':
		case 'o':
			cx = arg[0];
			cy = arg[1];
			cz = 0;
			break;
		case 'M':
		case 'O':
			cx = arg[0];
			cy = arg[1];
			cz = arg[2];
			break;
		case 'n':
		case 'q':
			draw( cx, cy, cz, arg[0], arg[1], 0.0 );
			break;
		case 'N':
		case 'Q':
			draw( cx, cy, cz, arg[0], arg[1], arg[2] );
			break;
		case 'l':
		case 'v':
			draw( arg[0], arg[1], 0.0, arg[2], arg[3], 0.0 );
			break;
		case 'L':
		case 'V':
			draw( arg[0], arg[1], arg[2], arg[3], arg[4], arg[5] );
			break;
		case 'p':
		case 'x':
			draw( arg[0], arg[1], 0.0, arg[0], arg[1], 0.0 );
			break;
		case 'P':
		case 'X':
			draw( arg[0], arg[1], arg[2], arg[0], arg[1], arg[2] );
			break;
		case 't':
			label( cx, cy, strarg );
			break;
		case 'F':
			XFlush( dpy );
			break;
		case 'e':
			XClearWindow( dpy, win );
			break;
		}

		if( verbose )
			printf( "%s\n", up->desc );
	}

	XFlush( dpy );
	sleep( 1 );
	return 0;
}

getargs(struct uplot *up)
{
	int	i;

	for( i = 0; i < up->narg; i++ ) {
		switch( up->targ ){
			case TSHORT:
				arg[i] = getshort();
				break;
			case TIEEE:
				arg[i] = getieee();
				break;
			case TSTRING:
				getstring();
				break;
			case TCHAR:
				arg[i] = getchar();
				break;
			case TNONE:
			default:
				arg[i] = 0;	/* ? */
				break;
		}
	}
}

getstring(void)
{
	int	c;
	char	*cp;

	cp = strarg;
	while( (c = getchar()) != '\n' && c != EOF )
		*cp++ = c;
	*cp = 0;
}

getshort(void)
{
	register long	v, w;

	v = getchar();
	v |= (getchar()<<8);	/* order is important! */

	/* worry about sign extension - sigh */
	if( v <= 0x7FFF )  return(v);
	w = -1;
	w &= ~0x7FFF;
	return( w | v );
}

double
getieee(void)
{
	char	in[8];
	double	d;

	fread( in, 8, 1, stdin );
	ntohd( &d, in, 1 );
	return	d;
}

draw(double x1, double y1, double z1, double x2, double y2, double z2)
      	           	/* from point */
      	           	/* to point */
{
	int	sx1, sy1, sx2, sy2;

	sx1 = (x1 - sp[0]) / (sp[3] - sp[0]) * width;
	sy1 = height - (y1 - sp[1]) / (sp[4] - sp[1]) * height;
	sx2 = (x2 - sp[0]) / (sp[3] - sp[0]) * width;
	sy2 = height - (y2 - sp[1]) / (sp[4] - sp[1]) * height;

	if( sx1 == sx2 && sy1 == sy2 )
		XDrawPoint( dpy, win, gc, sx1, sy1 );
	else
		XDrawLine( dpy, win, gc, sx1, sy1, sx2, sy2 );

	cx = x2;
	cy = y2;
	cz = z2;
}

label(double x, double y, char *str)
{
	int	sx, sy;

	sx = (x - sp[0]) / (sp[3] - sp[0]) * width;
	sy = height - (y - sp[1]) / (sp[4] - sp[1]) * height;
	/*sy -= fontstruct->height;	/* point is lower left of text XXXXXX*/

	XDrawString( dpy, win, gc, sx, sy, str, strlen(str) );
}

#define	FONT	"vtsingle"

XWMHints xwmh = {
	(InputHint|StateHint),		/* flags */
	False,				/* input */
	NormalState,			/* initial_state */
	0,				/* icon pixmap */
	0,				/* icon window */
	0, 0,				/* icon location */
	0,				/* icon mask */
	0				/* Window group */
};

xsetup(int argc, char **argv)
{
	char	hostname[80];
	char	display[80];
	char	*envp;
	unsigned long	bd, bg, fg, bw;
	XSizeHints xsh;
	XEvent	event;
	XGCValues gcv;

	width = height = 512;

	if( (envp = getenv("DISPLAY")) == NULL ) {
		/* Env not set, use local host */
		gethostname( hostname, 80 );
		sprintf( display, "%s:0", hostname );
		envp = display;
	}

	/* Open the display - XXX see what NULL does now */
	if( (dpy = XOpenDisplay( envp )) == NULL ) {
		fprintf( stderr, "pl-X: Can't open X display\n" );
		exit( 2 );
	}

	/* Load the font to use */
	if( (fontstruct = XLoadQueryFont(dpy, FONT)) == NULL ) {
		fprintf( stderr, "pl-X: Can't open font\n" );
		exit( 4 );
	}

	/* Select border, background, foreground colors,
	 * and border width.
	 */
	bd = WhitePixel( dpy, DefaultScreen(dpy) );
	bg = BlackPixel( dpy, DefaultScreen(dpy) );
	fg = WhitePixel( dpy, DefaultScreen(dpy) );
	bw = 1;

	/* Fill in XSizeHints struct to inform window
	 * manager about initial size and location.
	 */
	xsh.flags = (PSize);
	xsh.height = height + 10;
	xsh.width = width + 10;
	xsh.x = xsh.y = 0;

	win = XCreateSimpleWindow( dpy, DefaultRootWindow(dpy),
		xsh.x, xsh.y, xsh.width, xsh.height,
		bw, bd, bg );
	if( win == 0 ) {
		fprintf( stderr, "pl-X: Can't create window\n" );
		exit( 3 );
	}

	/* Set standard properties for Window Managers */
	XSetStandardProperties( dpy, win, "Unix Plot", "Unix Plot", None, argv, argc, &xsh );
	XSetWMHints( dpy, win, &xwmh );

	/* Create a Graphics Context for drawing */
	gcv.font = fontstruct->fid;
	gcv.foreground = fg;
	gcv.background = bg;
	gc = XCreateGC( dpy, win, (GCFont|GCForeground|GCBackground), &gcv );

	XSelectInput( dpy, win, ExposureMask );
	XMapWindow( dpy, win );

	while( 1 ) {
		XNextEvent( dpy, &event );
		if( event.type == Expose && event.xexpose.count == 0 ) {
			XWindowAttributes xwa;
			int	x, y;

			/* remove other exposure events */
			while( XCheckTypedEvent(dpy, Expose, &event) ) ;

			if( XGetWindowAttributes( dpy, win, &xwa ) == 0 )
				break;

			width = xwa.width;
			height = xwa.height;
			break;
		}
	}
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
