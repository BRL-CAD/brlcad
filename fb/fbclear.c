/*
	SCCS id:	@(#) fbclear.c	1.4
	Last edit: 	3/13/85 at 22:13:39	G S M
	Retrieved: 	8/13/86 at 03:12:09
	SCCS archive:	/m/cad/fb_utils/RCS/s.fbclear.c

*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fbclear.c	1.4	last edit 3/13/85 at 22:13:39";
#endif
2#include <stdio.h>
#include <std.h>
#include <fb.h>
/*
			F B C L E A R . C

	This program is intended to be used to clear a frame buffer, quickly.
	The special FBC clear-screen command is used, for speed (unless a
	non-black color is specified).

	Mike Muuss, BRL, 10/27/83.

	Conversion to generic frame buffer utility using libfb(3).
	In the process, the name has been changed to fbclear from ikclear.
	Gary S. Moss, BRL. 03/12/85
 */
main(argc, argv)
char	**argv;
int	argc;
{
	if( argc > 1 && strcmp (argv[1], "-h") == 0 )
		{
		setfbsize( 1024 );
		argv++;
		argc--;
		}
	if( argc != 4 && argc > 1 )
		{
		(void) fprintf( stderr, "Usage:  fbclear [r g b]\n" );
		return	1;
		}
	if(	fbopen( NULL, APPEND ) == -1
	    ||	fb_wmap( (ColorMap *) NULL ) == -1
		)
		{
		return	1;
		}
	if( argc == 4 )
		{ static Pixel	pixel;
		pixel.red = (u_char) atoi( argv[1] );
		pixel.green = (u_char) atoi( argv[2] );
		pixel.blue = (u_char) atoi( argv[3] );
		setbackground( &pixel );
		}
	return	fbclear() == -1;
	}
