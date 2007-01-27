/*                        F B Z O O M . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file fbzoom.c
 *
 * Function -
 *	Dynamicly modify Ikonas Zoom and Window parameters,
 *	using VI and/or EMACS-like keystrokes on a regular terminal.
 *
 *  Authors -
 *	Bob Suckling
 *	Michael John Muuss
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"
#include "libtermio.h"


int pars_Argv(int argc, register char **argv);
int doKeyPad(void);

/* Zoom rate and limits */
#define MinZoom		(1)

/* Pan limits */
#define MaxXPan		(fb_getwidth(fbp)-1)
#define MaxYPan		(fb_getheight(fbp)-1)
#define MinPan		(0)

static int PanFactor;			/* Speed with whitch to pan.	*/
static int xPan, yPan;			/* Pan Location.		*/
static int xZoom, yZoom;		/* Zoom Factor.			*/
static int new_xPan, new_yPan;
static int new_xZoom, new_yZoom;

static int	scr_width = 0;		/* screen size */
static int	scr_height = 0;
static int	toggle_pan = 0;		/* Reverse sense of pan commands? */
static char	*framebuffer = NULL;
static FBIO	*fbp;

static char usage[] = "\
Usage: fbzoom [-hT] [-F framebuffer]\n\
	[-{sS} squarescrsize] [-{wW} scr_width] [-{nN} scr_height]\n";

int
main(int argc, char **argv)
{
	if( ! pars_Argv( argc, argv ) ) {
		(void)fputs(usage, stderr);
		exit(1);
	}
	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == NULL )
		exit(1);

	if( bu_optind+4 == argc ) {
		xPan = atoi( argv[bu_optind+0] );
		yPan = atoi( argv[bu_optind+1] );
		xZoom = atoi( argv[bu_optind+2] );
		yZoom = atoi( argv[bu_optind+3] );
		fb_view(fbp, xPan, yPan, xZoom, yZoom);
	}

#if 0
	xZoom = 1;
	yZoom = 1;
	xPan = fb_getwidth(fbp)/2;
	yPan = fb_getheight(fbp)/2;
#else
	fb_getview(fbp, &xPan, &yPan, &xZoom, &yZoom);
#endif

	/* Set RAW mode */
	save_Tty( 0 );
	set_Raw( 0 );
	clr_Echo( 0 );

	PanFactor = fb_getwidth(fbp)/16;
	if( PanFactor < 2 )  PanFactor = 2;

	new_xPan = xPan;
	new_yPan = yPan;
	new_xZoom = xZoom;
	new_yZoom = yZoom;
	do  {
		/* Clip values against Min/Max */
		if (new_xPan > MaxXPan) new_xPan = MaxXPan;
		if (new_xPan < MinPan) new_xPan = MinPan;
		if (new_yPan > MaxYPan) new_yPan = MaxYPan;
		if (new_yPan < MinPan) new_yPan = MinPan;
		if (new_xZoom < MinZoom) new_xZoom = MinZoom;
		if (new_yZoom < MinZoom) new_yZoom = MinZoom;

		if( new_xPan != xPan || new_yPan != yPan
		  || new_xZoom != xZoom || new_yZoom != yZoom ) {
			/* values have changed, write them */
			if( fb_view(fbp, new_xPan, new_yPan,
			    new_xZoom, new_yZoom) >= 0 ) {
				/* good values, save them */
				xPan = new_xPan;
				yPan = new_yPan;
				xZoom = new_xZoom;
				yZoom = new_yZoom;
			} else {
				/* bad values, reset to old ones */
				new_xPan = xPan;
				new_yPan = yPan;
				new_xZoom = xZoom;
				new_yZoom = yZoom;
			}
		}
#if 0
		(void) fprintf( stdout,
				"Zoom: %2d %2d  Center Pixel: %4d %4d            \r",
				xZoom, yZoom, xPan, yPan );
#else
		(void) fprintf( stdout,
				"Center Pixel: %4d %4d   Zoom: %2d %2d   %s\r",
				xPan, yPan, xZoom, yZoom,
				toggle_pan ? "Pan: image "
					   : "Pan: window");
#endif
		(void) fflush( stdout );
	}  while( doKeyPad() );

	reset_Tty( 0 );
	(void) fb_view( fbp, xPan, yPan, xZoom, yZoom );
	(void) fb_close( fbp );
	(void) fprintf( stdout,  "\n");	/* Move off of the output line.	*/
	return	0;
}

char help[] = "\r\n\
Both VI and EMACS motions work.\r\n\
b ^V	zoom Bigger (*2)		s	zoom Smaller (*0.5)\r\n\
+ =	zoom Bigger (+1)		-	zoom Smaller (-1)\r\n\
(	zoom Y Bigger (*2)		)	zoom Y Smaller (*0.5)\r\n\
0	zoom Y Bigger (+1)		9	zoom Y Smaller (-1)\r\n\
<	zoom X Bigger (*2)		>	zoom X Smaller (*0.5)\r\n\
,	zoom X Bigger (+1)		.	zoom X Smaller (-1)\r\n\
h B 	pan Left (1)			l F	pan Right (1)\r\n\
H ^B	pan Left (many)			L ^F	pan Right (many)\r\n\
k P	pan Up (1)			j N	pan Down (1)\r\n\
K ^P	pan Up (many)			J ^N	pan Down (many)\r\n\
T	toggle sense of pan commands\r\n\
c	goto Center\r\n\
z	zoom 1 1\r\n\
r	Reset to normal\r\n\
q	Exit\r\n\
RETURN	Exit\r\n";

#define ctl(x)	(x&037)

int
doKeyPad(void)
{
	register int ch;

	if( (ch = getchar()) == EOF )
		return	0;		/* done */
	ch &= ~0x80;			/* strip off parity bit */
	switch( ch ) {
	default :
		(void) fprintf( stdout,
				"\r\n'%c' bad -- Type ? for help\r\n",
				ch );
	case '?' :
		(void) fprintf( stdout, "\r\n%s", help );
		break;

	case 'T' :
		toggle_pan = 1 - toggle_pan;
		break;
	case '\r' :				/* Done, leave "as is" */
	case '\n' :
	case 'q' :
	case 'Q' :
		return	0;

	case 'c' :				/* Reset Pan (Center) */
	case 'C' :
		new_xPan = fb_getwidth(fbp)/2;
		new_yPan = fb_getheight(fbp)/2;
		break;
	case 'z' :				/* Reset Zoom */
	case 'Z' :
		new_xZoom = 1;
		new_yZoom = 1;
		break;
	case 'r' :				/* Reset Pan and Zoom */
	case 'R' :
		new_xZoom = 1;
		new_yZoom = 1;
		new_xPan = fb_getwidth(fbp)/2;
		new_yPan = fb_getheight(fbp)/2;
		break;

	case ctl('v') :
	case 'b' :				/* zoom BIG binary */
		new_xZoom *= 2;
		new_yZoom *= 2;
		break;
	case '=' :
	case '+' :				/* zoom BIG incr */
		new_xZoom++;
		new_yZoom++;
		break;
	case 's' :				/* zoom small binary */
		new_xZoom /= 2;
		new_yZoom /= 2;
		break;
	case '-' :				/* zoom small incr */
		--new_xZoom;
		--new_yZoom;
		break;

	case '>' :				/* X Zoom */
		new_xZoom *= 2;
		break;
	case '.' :
		++new_xZoom;
		break;
	case '<' :
		new_xZoom /= 2;
		break;
	case ',' :
		--new_xZoom;
		break;

	case ')' :				/* Y Zoom */
		new_yZoom *= 2;
		break;
	case '0' :
		++new_yZoom;
		break;
	case '(' :
		new_yZoom /= 2;
		break;
	case '9' :
		--new_yZoom;
		break;

	case 'h' :				/* pan LEFT.	*/
	case 'B' :
		new_xPan -= 1 - 2 * toggle_pan;
		break;
	case 'H' :
	case ctl('b') :
		new_xPan -= PanFactor * (1 - 2 * toggle_pan);
		break;
	case 'j' :				/* pan DOWN.	*/
	case 'N' :
		new_yPan -= 1 - 2 * toggle_pan;
		break;
	case 'J' :
	case ctl('n') :
		new_yPan -= PanFactor * (1 - 2 * toggle_pan);
		break;
	case 'k' :				/* pan UP.	*/
	case 'P' :
		new_yPan += 1 - 2 * toggle_pan;
		break;
	case 'K' :
	case ctl('p') :
		new_yPan += PanFactor * (1 - 2 * toggle_pan);
		break;
	case 'l' :				/* pan RIGHT.	*/
	case 'F' :
		new_xPan += 1 - 2 * toggle_pan;
		break;
	case 'L' :
	case ctl('f') :
		new_xPan += PanFactor * (1 - 2 * toggle_pan);
		break;
	}
	return	1;		/* keep going */
}

/*	p a r s _ A r g v ( )
 */
int
pars_Argv(int argc, register char **argv)
{
	register int	c;

	while( (c = bu_getopt( argc, argv, "hTF:s:S:w:W:n:N:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			scr_height = scr_width = 1024;
			break;
		case 'T':
			/* reverse the sense of pan commands */
			toggle_pan = 1;
			break;
		case 'F':
			framebuffer = bu_optarg;
			break;
		case 's':
		case 'S':
			scr_height = scr_width = atoi(bu_optarg);
			break;
		case 'w':
		case 'W':
			scr_width = atoi(bu_optarg);
			break;
		case 'n':
		case 'N':
			scr_height = atoi(bu_optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}
	return	1;
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
