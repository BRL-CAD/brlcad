/*
 *			I K Z O O M . C
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

#include <stdio.h>	
#ifdef vax
#include <sys/ioctl.h>
#else
#include <sgtty.h>
#endif
struct sgttyb ttyold, ttynew;

int ikfd;	/* Ikonas FD */
		
#define	IKsize 512

/* Zoom rate and limits.	*/
#define	Zfactor		(2)
#define MaxZoom		(15)
#define MinZoom		(1)

/* Pan limits.	*/
#define MaxPan		( IKsize )
#define MinPan		(0)

int PanFactor;			/* Speed with whitch to pan.	*/
int zoom;			/* Zoom Factor.			*/
int xPan, yPan;			/* Pan Location.		*/

main(argc, argv )
char **argv;
{
	ikopen();

	zoom = 2;
	xPan = yPan = 0;

	/* Set RAW mode */
	gtty( 0, &ttyold);
	ttynew = ttyold;
	ttynew.sg_flags |= RAW;
	stty( 0, &ttynew);

	do {
		PanFactor = 128 / zoom;
		ikzoom( zoom-1, zoom-1 );
		ikwindow( (xPan-1) * 4, 4063+yPan );
		printf( "zoom=%d, Upper Left Pixel is %d,%d            \r",
			zoom, xPan, yPan );
		fflush( stdout );
	} while( doKeyPad() );

	stty( 0, &ttyold);
	printf( "\n");		/* Move off of the output line.	*/
	ikzoom( zoom-1, zoom-1 );
	ikwindow( (xPan-1) * 4, 4063+yPan );
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
		return(0);		/* done */

	switch( ch )  {

	default:
		printf("\r\n'%c' bad -- Type ? for help\r\n", ch);
	case '?':
		printf("\r\n%s", help);
		break;

	case '\r':    
	case '\n':				/* Done, return to normal */
	case 'q':
		zoom = 1;
		xPan = yPan = 0;
		return(0);
	case 'Q':				/* Done, leave "as is" */
		return(0);

	case 'c':	
	case 'C':				/* Center */
		xPan = yPan = 0;
		break;

	case 'r':	
	case 'R':				/* Reset */
		zoom = 1;
		xPan = yPan = 0;
		break;

	case ctl(v):
	case 'b':				/* zoom BIG.	*/
		if(  ++zoom > MaxZoom )
			zoom = MaxZoom;
		break;

	case 's':				/* zoom small.	*/
		if(  --zoom < MinZoom )
			zoom = MinZoom;
		break;

	case 'F':
	case 'l':				/* move LEFT.	*/
		if( ++xPan > MaxPan )
			xPan = MaxPan;
		break;

	case ctl(f):
	case 'L':
		if( (xPan += PanFactor) > MaxPan )
			xPan = MaxPan;
		break;

	case 'N':
	case 'k':				/* move DOWN.	*/
		if( ++yPan > MaxPan )
			yPan = MaxPan;
		break;
	case ctl(n):
	case 'K':
		if( (yPan += PanFactor) > MaxPan )
			yPan = MaxPan;
		break;

	case 'P':
	case 'j':				/* move UP.	*/
		if( --yPan < MinPan )
			yPan = MinPan;
		break;

	case ctl(p):
	case 'J':
		if( (yPan -= PanFactor) < MinPan )
			yPan = MinPan;
		break;

	case 'B':
	case 'h':				/* move RIGHT.	*/
		if( --xPan < MinPan )
			xPan = MinPan;
		break;

	case ctl(b):
	case 'H':
		if( (xPan -= PanFactor) < MinPan )
			xPan = MinPan;
		break;
	}
	return(1);		/* keep going */
}
