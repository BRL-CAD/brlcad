/*
	SCCS id:	@(#) fbgrid.c	1.6
	Last edit: 	3/29/85 at 15:38:54	G S M
	Retrieved: 	8/13/86 at 03:14:17
	SCCS archive:	/m/cad/fb_utils/RCS/s.fbgrid.c

*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fbgrid.c	1.6	last edit 3/29/85 at 15:38:54";
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
	static Pixel	black, white;
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
	fbioinit();
	for( y = 0; y < fb_sz; y++ )
		{
		for( x = 0; x < fb_sz; x++ )
			{
			if( x == y || x == fb_sz - y )
				(void) fbwpixel( &white );
			else
			if( (x & 0x7) && (y & 0x7) )
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

