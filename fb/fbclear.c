/*
	SCCS id:	@(#) fbclear.c	1.3
	Last edit: 	3/13/85 at 19:03:10	G S M
	Retrieved: 	8/13/86 at 03:12:03
	SCCS archive:	/m/cad/fb_utils/RCS/s.fbclear.c

*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fbclear.c	1.3	last edit 3/13/85 at 19:03:10";
#endif
#include <stdio.h>
#include <fb.h>
typedef unsigned char	u_char;
/*
 *			I K C L E A R . C
 *
 * This program is intended to be used to clear the screen of
 * the Ikonas display, quickly.
 *
 * The special FBC clear-screen command is used, for speed.
 *
 * Mike Muuss, BRL, 10/27/83.
 * Gary S. Moss, BRL. 03/12/85	Modified to use libfb(3).
 */
static int	hires = 0;

main(argc, argv)
char	**argv;
int	argc;
{
	if( argc > 1 && strcmp (argv[1], "-h") == 0 )
		{
		hires++;
		setfbsize( 1024 );
		argv++;
		argc--;
		}
	if( argc != 4 && argc > 1 )
		{
		(void) fprintf( stderr, "Usage:  ikclear [r g b]\n" );
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
		pixel.blue = (u_char) atoi( argv[2] );
		pixel.green = (u_char) atoi( argv[3] );
		setbackground( &pixel );
		}
	return	fbclear() == -1;
	}
