/*
	SCCS id:	@(#) fbgrid.c	1.7
	Last edit: 	4/3/85 at 09:24:36	G S M
	Retrieved: 	8/13/86 at 03:14:22
	SCCS archive:	/m/cad/fb_utils/RCS/s.fbgrid.c

*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fbgrid.c	1.7	last edit 4/3/85 at 09:24:36";
#endif
/*
			F B G R I D

	This program computes an image for the frame buffer.
	Mike Muuss, 7/19/82

	Conversion to generic frame buffer utility using libfb(3).
	In the process, the name has been changed to fbgrid from ikgrid.
	Gary S. Moss, BRL. 03/12/85
 */
#include <stdio.h>
#include <fb.h>
main( argc, argv )
int	argc;
char	**argv;
	{
	register int	x, y;
	register int	fb_sz;
	register int	middle;
	static Pixel	black, white, red;
	static int	val;

	if( ! pars_Argv( argc, argv ) )
		{
		(void) fprintf( stderr, "Usage : fbgrid	[-h]\n" );
		return	1;
		}
	if( fbopen( NULL, APPEND ) == -1 )
		{
		return	1;
		}
	fb_sz = fbgetsize();
	white.red = white.green = white.blue = 255;
	black.red = black.green = black.blue = 0;
	red.red = 255;
	middle = fb_sz/2;
	fbioinit();
	if( fb_sz == 512 )
	for( y = 0; y < fb_sz; y++ )
		{
		for( x = 0; x < fb_sz; x++ )
			{
			if( x == y || x == fb_sz - y )
				(void) fbwpixel( &white );
			else
			if( x == middle || y == middle )
				(void) fbwpixel( &red );
			else
			if( (x & 0x7) && (y & 0x7) )
				(void) fbwpixel( &black );
			else
				(void) fbwpixel( &white );
			}
		}
	else
	for( y = 0; y < fb_sz; y++ )
		{
		for( x = 0; x < fb_sz; x++ )
			{
			if( x == y || x == fb_sz - y )
				(void) fbwpixel( &white );
			else
			if( x == middle || y == middle )
				(void) fbwpixel( &red );
			else
			if( (x & 0xf) && (y & 0xf) )
				(void) fbwpixel( &black );
			else
				(void) fbwpixel( &white );
			}
		}
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
				setfbsize( 1024 );
				break;
			case '?' :
				return	0;
			}
		}
	return	1;
	}

