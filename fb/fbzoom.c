/*
 *			F B Z O O M . C
 *
 * Function -
 *	Dynamicly modify Ikonas Zoom and Window parameters,
 *	using VI and/or EMACS-like keystrokes on a regular terminal.
 *
 * Original Version -
 *	Bob Suckling, BRL
 *
 * Enhancements -
 *	Ported to VAX, simplified internals somewhat.
 *	Mike Muuss, BRL.  03/22/84.
 *
 * $Revision$
 *
 * $Log$
 * Revision 1.2  84/05/02  02:04:13  mike
 * Converted to use library zoom and window functions.
 * 
 * Revision 1.1  84/03/22  23:59:43  mike
 * Initial revision
 * 
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
/*
	Conversion to generic frame buffer routine using 'libfb'.
	In the process, the name was changed to fbzoom from ikzoom.
	Gary S. Moss, BRL. 03/13/85.

	SCCS id:	@(#) fbzoom.c	1.7
	Last edit: 	6/6/85 at 16:51:04
	Retrieved: 	8/13/86 at 03:15:14
	SCCS archive:	/m/cad/fb_utils/RCS/s.fbzoom.c
*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fbzoom.c	1.7	last edit 6/6/85 at 16:51:04";
#endif
#include <stdio.h>	
#include <fb.h>

/* Zoom rate and limits.	*/
#define MaxZoom		(16)
#define MinZoom		(1)

/* Pan limits.	*/
#define MaxPan		_fbsize
#define MinPan		(0)

static int PanFactor;			/* Speed with whitch to pan.	*/
static int zoom;			/* Zoom Factor.			*/
static int xPan, yPan;			/* Pan Location.		*/

main(argc, argv )
char **argv;
	{
	if( ! pars_Argv( argc, argv ) )
		{
		(void) fprintf( stderr, "Usage : fbzoom	[-h]\n" );
		return	1;
		}
	if( fbopen( NULL, APPEND ) == -1 )
		return	1;
	zoom = 1;
	xPan = _fbsize/2;
	yPan = _fbsize/2 - 1;

	/* Set RAW mode */
	save_Tty( 0 );
	set_Raw( 0 );
	clr_Echo( 0 );

	do
		{
		PanFactor = 40 / zoom;
		(void) fbzoom( zoom, zoom );
		fbwindow( xPan, yPan );
		(void) fprintf( stdout,
			 "zoom=%d, Upper Left Pixel is %d,%d            \r",
				zoom, xPan, yPan
				);
		(void) fflush( stdout );
		}
	while( doKeyPad() );

	reset_Tty( 0 );
	(void) fprintf( stdout,  "\n");	/* Move off of the output line.	*/
	(void) fbzoom( zoom, zoom );
	(void) fbwindow( xPan, yPan );
	return	0;
	}

char help[] = "\r\n\
b ^V	zoom Bigger\r\n\
s	zoom Smaller\r\n\
h	move Right\r\n\
j	move Up\r\n\
k	move Down\r\n\
l	move Left\r\n\
c	goto Center\r\n\
r	Reset to normal\r\n\
^F ^B	Forward, Back\r\n\
^N ^P	Down, Up\r\n\
\\n	Exit\r\n\
Uppercase takes big steps.\r\n";

#define ctl(x)	('x'&037)

doKeyPad()
	{ 
	register ch;	

	if( (ch = getchar()) == EOF )
		return	0;		/* done */

	switch( ch )
		{
	default :
		(void) fprintf( stdout,
				"\r\n'%c' bad -- Type ? for help\r\n",
				ch
				);
	case '?' :
		(void) fprintf( stdout, "\r\n%s", help );
		break;
	case '\r' :    
	case '\n' :				/* Done, return to normal */
	case 'q' :
		return	0;
	case 'Q' :				/* Done, leave "as is" */
		return	0;
	case 'c' :	
	case 'C' :				/* Center */
		xPan = yPan = 0;
		break;
	case 'r' :	
	case 'R' :				/* Reset */
		zoom = 1;
		xPan = _fbsize/2;
		yPan = _fbsize/2 - 1;
		break;
	case ctl(v) :
	case 'b' :				/* zoom BIG.	*/
		if(  (zoom *= 2) > MaxZoom )
			zoom = MaxZoom;
		break;
	case 's' :				/* zoom small.	*/
		if(  (zoom /= 2) < MinZoom )
			zoom = MinZoom;
		break;
	case 'F' :
	case 'l' :				/* move LEFT.	*/
		if( ++xPan > MaxPan )
			xPan = MaxPan;
		break;
	case ctl(f) :
	case 'L' :
		if( (xPan += PanFactor) > MaxPan )
			xPan = MaxPan;
		break;
	case 'N' :
	case 'k' :				/* move DOWN.	*/
		if( ++yPan > MaxPan )
			yPan = MaxPan;
		break;
	case ctl(n) :
	case 'K' :
		if( (yPan += PanFactor) > MaxPan )
			yPan = MaxPan;
		break;
	case 'P' :
	case 'j' :				/* move UP.	*/
		if( --yPan < MinPan )
			yPan = MinPan;
		break;
	case ctl(p) :
	case 'J' :
		if( (yPan -= PanFactor) < MinPan )
			yPan = MinPan;
		break;
	case 'B' :
	case 'h' :				/* move RIGHT.	*/
		if( --xPan < MinPan )
			xPan = MinPan;
		break;
	case ctl(b) :
	case 'H' :
		if( (xPan -= PanFactor) < MinPan )
			xPan = MinPan;
		break;
		}
	return	1;		/* keep going */
	}

/*	p a r s _ A r g v ( )
 */
int
pars_Argv( argc, argv )
register char	**argv;
	{
	register int	c;
	while( (c = getopt( argc, argv, "h" )) != EOF )
		{
		switch( c )
			{
			case 'h' : /* High resolution frame buffer.	*/
				setfbsize( 1024 );
				break;
			case '?' :
				return	0;
			}
		}
	return	1;
	}

