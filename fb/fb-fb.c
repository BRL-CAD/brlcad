/*
 *  			F B - F B . C
 *  
 *  Program to copy the entire image on a framebuffer to another framebuffer.
 *  
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1991 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "externs.h"		/* For getopt() */
#include "fb.h"

static unsigned char *scanline;		/* 1 scanline pixel buffer */
static int	scanbytes;		/* # of bytes of scanline */
static int	scanpix;		/* # of pixels of scanline */
static int	streamline;		/* # scanlines to do at once */

static char	*in_fb_name;
static char	*out_fb_name;

static int	scr_width = 0;		/* screen tracks file if not given */
static int	scr_height = 0;

static char usage[] = "\
Usage: fb-fb [-F output_framebuffer]\n\
	input_framebuffer [output_framebuffer]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "ahiczF:s:w:n:x:y:X:Y:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'F':
			out_fb_name = optarg;
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		return(0);		/* missing input framebuffer */
	}
	in_fb_name = argv[optind++];

	if( optind >= argc )  {
		out_fb_name = NULL;
		return(1);	/* OK */
	}
	out_fb_name = argv[optind++];

	if ( argc > optind )
		(void)fprintf( stderr, "fb-fb: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{
	register int y;
	register FBIO *in_fbp, *out_fbp;
	int	n, m;
	int	height;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}


	if( (in_fbp = fb_open( in_fb_name, 0, 0 )) == NULL )  {
		if( in_fb_name )
			fprintf(stderr,"fb-fb: unable to open input '%s'\n", in_fb_name );
		exit(12);
	}

	/* Get the screen size we were given */
	scr_width = fb_getwidth(in_fbp);
	scr_height = fb_getheight(in_fbp);

	if( (out_fbp = fb_open( out_fb_name, scr_width, scr_height )) == FBIO_NULL )  {
		if( out_fb_name )
			fprintf(stderr,"fb-fb: unable to open output '%s'\n", out_fb_name );
		exit(12);
	}

	scanpix = scr_width;			/* # pixels on scanline */
	streamline = 8;
	scanbytes = scanpix * streamline * sizeof(RGBpixel);
	if( (scanline = (unsigned char *)malloc(scanbytes)) == RGBPIXEL_NULL )  {
		fprintf(stderr,
			"fb-fb:  malloc(%d) failure for scanline buffer\n",
			scanbytes);
		exit(2);
	}

	/* Bottom to top with multi-line reads & writes */
	for( y = 0; y < scr_height; y += streamline )  {
		n = fb_readrect( in_fbp, 0, y, scr_width, streamline,
			scanline );
		if( n <= 0 ) break;
		height = streamline;
		if( n != scr_width * streamline )  {
			height = (n+scr_width-1)/scr_width;
			if( height <= 0 )  break;
		}
		m = fb_writerect( out_fbp, 0, y, scr_width, height,
			scanline );
		if( m != scr_width*height )
			fprintf(stderr,
				"fb-fb: fb_writerect(x=0, y=%d, w=%d, h=%d) failure, ret=%d, s/b=%d\n",
				y, scr_width, height, m, scanbytes );
	}
	fb_close( in_fbp );
	fb_close( out_fbp );
	exit(0);
}
