/*
 *			F B C L E A R . C
 *
 *  This program is intended to be used to clear a frame buffer
 *  to black, or to the specified color
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
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
                                                                                                                                                                            
#include <stdio.h>

#include "machine.h"
#include "fb.h"

static char	*framebuffer = NULL;
static FBIO	*fbp;
static int	scr_width = 0;		/* use default size */
static int	scr_height = 0;
static int	clear_and_reset = 0;

#define u_char	unsigned char

static char usage[] = "\
Usage: fbclear [-h -c] [-F framebuffer]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height] [gray | r g b]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "hcF:s:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			scr_height = scr_width = 1024;
			break;
		case 'c':
			/* clear only, no cmap, pan, and zoom */
			clear_and_reset++;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 's':
		case 'S':
			scr_height = scr_width = atoi(optarg);
			break;
		case 'w':
		case 'W':
			scr_width = atoi(optarg);
			break;
		case 'n':
		case 'N':
			scr_height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}
	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == NULL )
		exit(2);

	/* Get the screen size we were given */
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	if( clear_and_reset ) {
		if( fb_wmap( fbp, COLORMAP_NULL ) < 0 )
			exit(3);
		(void)fb_view( fbp, scr_width/2, scr_height/2, 1, 1 );
	} else {
		ColorMap	cmap;
		int		xcent, ycent, xzoom, yzoom;
		if( fb_rmap( fbp, &cmap ) >= 0 )  {
			if( !fb_is_linear_cmap( &cmap ) )  {
				fprintf(stderr, "fbclear: NOTE: non-linear colormap in effect.  -c flag loads linear colormap.\n");
			}
		}
		(void)fb_getview( fbp, &xcent, &ycent, &xzoom, &yzoom );
		if( xzoom != 1 || yzoom != 1 )  {
			fprintf(stderr, "fbclear:  NOTE: framebuffer is zoomed.  -c will un-zoom.\n");
		}
	}

	if( optind+3 == argc ) {
		static RGBpixel	pixel;
		pixel[RED] = (u_char) atoi( argv[optind+0] );
		pixel[GRN] = (u_char) atoi( argv[optind+1] );
		pixel[BLU] = (u_char) atoi( argv[optind+2] );
		fb_clear( fbp, pixel );
	} else if( optind+1 == argc ) {
		static RGBpixel	pixel;
		pixel[RED] = pixel[GRN] = pixel[BLU]
			= (u_char) atoi( argv[optind+0] );
		fb_clear( fbp, pixel );
	} else {
		if( optind != argc )
			fprintf(stderr, "fbclear: extra arguments ignored\n");
		fb_clear( fbp, PIXEL_NULL );
	}
	(void)fb_close( fbp );
	return(0);
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
