/*
	SCCS id:	@(#) fbgrid.c	1.4
	Last edit: 	3/13/85 at 22:13:12	G S M
	Retrieved: 	8/13/86 at 03:14:04
	SCCS archive:	/m/cad/fb_utils/RCS/s.fbgrid.c

*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fbgrid.c	1.4	last edit 3/13/85 at 22:13:12";
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
Pixel	line[1024]; /* Room for high or low resolution.			*/

main( argc, argv )
int	argc;
char	**argv;
	{
	register int	x, y;
	register int	hf_fbsz, fb_sz;
	static int val;
	static Pixel	*lp;

	if( ! pars_Argv( argc, argv ) )
		{
		(void) fprintf( stderr, "Usage : fbgrid	[-h]\n" );
		return	1;
		}
	if( fbopen( NULL, APPEND ) == -1 )
		{
		return	1;
		}
	fb_sz = getfbsize();
	hf_fbsz = fb_sz / 2;
	for( y = hf_fbsz; y < fb_sz; y++ )
		{
		for( x = hf_fbsz; x < fb_sz; x++ )
			{
			if(	x == y
			    ||	(x % 8) == 0
			    ||	(y % 8) == 0
				)
				val = 255;
			else
				val = 0;
			lp = &line[x];
			lp->red = lp->green = lp->blue = val;
			lp = &line[fb_sz-x];
			lp->red = lp->green = lp->blue = val;
			}
		if(	fbwrite( 0, y, line, fb_sz )
		    ==	-1
			)
			{
			(void) fprintf( stderr, "Write failed!\n" );
			return	1;
			}
		if(	fbwrite( 0, fb_sz-y, line, fb_sz )
		    ==	-1
			)
			{
			(void) fprintf( stderr, "Write failed!\n" );
			return	1;
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

