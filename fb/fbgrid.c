/*
 *			F B G R I D
 *
 *  This program displays a grid pattern on a framebuffer.
 *  Useful for convergance alignment, etc.
 *
 *  Authors -
 *	Michael John Muuss
 *	Gary S. Moss
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
#include "fb.h"

FBIO	*fbp;
int	fbsize = 512;

main( argc, argv )
int	argc;
char	**argv;
	{
	register int	x, y;
	register int	fb_sz;
	register int	middle;
	static Pixel	black, white, red;
	static int	val;
	static int	mask;

	if( ! pars_Argv( argc, argv ) )
		{
		(void) fprintf( stderr, "Usage : fbgrid	[-h]\n" );
		return	1;
		}
	if( (fbp = fb_open( NULL, fbsize, fbsize )) == NULL )
		{
		return	1;
		}
	fb_sz = fb_getwidth(fbp);
	white.red = white.green = white.blue = 255;
	black.red = black.green = black.blue = 0;
	red.red = 255;
	middle = fb_sz/2;
	fb_ioinit(fbp);
	if( fb_sz <= 512 )
		mask = 0x7;
	else
		mask = 0xf;

	for( y = fb_sz-1; y >= 0; y-- )  {
		for( x = 0; x < fb_sz; x++ )
			{
			if( x == y || x == fb_sz - y )
				(void) fb_wpixel( fbp, &white );
			else
			if( x == middle || y == middle )
				(void) fb_wpixel( fbp, &red );
			else
			if( (x & mask) && (y & mask) )
				(void) fb_wpixel( fbp, &black );
			else
				(void) fb_wpixel( fbp, &white );
			}
		}
	fb_close( fbp );
	return	0;
	}

/*	p a r s _ A r g v ( )
 */
int
pars_Argv( argc, argv )
register char	**argv;
	{
	register int	c;
	extern int	optind;

	while( (c = getopt( argc, argv, "h" )) != EOF )
		{
		switch( c )
			{
			case 'h' : /* High resolution frame buffer.	*/
				fbsize = 1024;
				break;
			case '?' :
				return	0;
			}
		}
	return	1;
	}

