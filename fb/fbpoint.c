/*
 *			F B P O I N T . C
 *
 *  Tool to identify X,Y coordinates on the screen.
 *
 *  Authors -
 *	Michael Johns Muuss
 *	Bob Suckling
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "../h/fb.h"

FBIO *fbp;

#define	JumpSpeed 30 /* number of pixels skiped with UPPERCASE commands. */

RGBpixel flashPix;		/* Different pixel */
RGBpixel savePix; 		/* Old Pixel for restoring the image.	*/

int curX, curY;			/* current (new) position */
int markX, markY;		/* Save the mark.	*/
int oldX, oldY;			/* saved (previous) position */

int	Run = 1;	/* Tells when to stop the main loop.	*/

#define PrtLast	fprintf( stderr, "xy=(%d, %d)      \r",\
				 curX, curY ); fflush( stderr )

static char *help = "\
Char:   Command:                                                \r\n\
h B	Left (1)\r\n\
j N	Down (1)\r\n\
k P	Up (1)\r\n\
l F	Right (1)\r\n\
H ^B	Left (many)\r\n\
J ^N	Down (many)\r\n\
K ^P	Up (many)\r\n\
L ^F	Right (many)\r\n\
q Q	QUIT\r\n\
";

SimpleInput()	/* ==== get keyboard input.	*/
{ 
	char ch;

	if( read( 0, &ch, 1) <= 0 ) {
		Run = 0;
		return;
	}
	switch( ch ) {
	default:	
		fprintf( stderr,
		"Unknown command(%c:%o). Type '?' for help!           \r\n",
			ch, ch );
		PrtLast;
		break;

	case '?':	
		fprintf( stderr, "%s", help );
		PrtLast;
		return;

	case 'q':
	case 'Q':	
		Run = 0;
		return;
#define ctl(x)	('x'&037)

	case 'B':
	case 'h':
		--curX;		/* Go left.	*/
		return;
	case 'N':
	case 'j':		
		--curY;		/* Go down.	*/
		return;
	case 'P':
	case 'k':		
		++curY;		/* Go up.	*/
		return;
	case 'F':
	case 'l':		
		++curX;		/* Go right.	*/
		return;
	case ctl(b):
	case 'H':		
		curX -= JumpSpeed;	/* Go LEFT.	*/
		return;
	case ctl(n):
	case 'J':		
		curY -= JumpSpeed;	/* Go DOWN.	*/
		return;
	case ctl(p):
	case 'K':		
		curY += JumpSpeed;	/* Go UP.	*/
		return;
	case ctl(f):
	case 'L':
		curX += JumpSpeed;	/* Go RIGHT.	*/
		return;
/**	case 'Z':		driver(); **/
	}
}

main(argc, argv)
char **argv;
{ 
	int width, height;
	char *malloc();

	setbuf( stderr, malloc( BUFSIZ ) );

	if( argc > 1 && strcmp( argv[1], "-h" ) == 0 )
		width = height = 1024;
	else
		width = height = 512;

	if( (fbp = fb_open( NULL, width, height )) == NULL )
		exit(12);

	curX = markX = oldX = fb_getwidth(fbp)/2;
	curY = markY = oldY = fb_getheight(fbp)/2;

	/* Set RAW mode */
	save_Tty( 0 );
	set_Raw( 0 );
	clr_Echo( 0 );

	/* save : old pixel.	*/
	fb_read( fbp, oldX, oldY, savePix, 1 );
	fb_cursor( fbp, 1, curX, curY );
	PrtLast;

	while( Run )  {
		SimpleInput();			/* read and do keybord	*/
		if( oldX == curX && oldY == curY )
			continue;

		/* replace saved pixel.	*/
		fb_write( fbp, oldX, oldY, savePix, 1 );
		if( curX < 0 )
			curX = 0;	
		if( curX >= fb_getwidth(fbp) )
			curX = fb_getwidth(fbp) -1;
		if( curY < 0 )
			curY = 0;		
		if( curY >= fb_getheight(fbp) )
			curY = fb_getheight(fbp) -1;

		/* save : new pixel.	*/
		fb_read( fbp, curX, curY, savePix, 1 );
		oldX = curX;
		oldY = curY;
		fb_cursor( fbp, 1, curX, curY );
		PrtLast;

		flashPix[RED] = (255 - savePix[RED]) + 32;
		flashPix[GRN] = (255 - savePix[GRN]) + 32;
		flashPix[BLU] = (255 - savePix[BLU]) + 32;
		fb_write( fbp, curX, curY, flashPix, 1 );
	}

	fb_cursor( fbp, 0, curX, curY );	/* turn off */

	/* replace saved pixel.	*/
	fb_write( fbp, oldX, oldY, savePix, 1 );
	fprintf( stderr, "\n\r" );

	reset_Tty( 0 );

	/* write final location on stdout */
	fprintf( stdout, "%d %d\n", curX, curY );
	exit( 0 );    
}
