/*
 *  			P I X - F B . C
 *  
 *  Program to take bottom-up pixel files and send them to a framebuffer.
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
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "fb.h"

extern int	getopt();
extern char	*optarg;
extern int	optind;

#define	INFIN		999999		/* An "infinite" file size */
#define MAX_LINE	2048		/* Max pixels/line */
RGBpixel scanline[MAX_LINE];		/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

char	*file_name;
int	infd;

int	fileinput = 0;		/* file of pipe on input? */

int	file_width = 512;	/* default input width */
int	file_height = INFIN;	/* default input height */
int	scr_width = 0;		/* screen tracks file if not given */
int	scr_height = 0;
int	file_xoff, file_yoff;
int	scr_xoff, scr_yoff;
int	clear = 0;
int	zoom = 0;
int	inverse = 0;			/* Draw upside-down */

char usage[] = "\
Usage: pix-fb [-h -i -c -z]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height] [file.pix]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hiczs:w:n:x:y:X:Y:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			file_height = file_width = 1024;
			break;
		case 'i':
			inverse = 1;
			break;
		case 'c':
			clear = 1;
			break;
		case 'z':
			zoom = 1;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(optarg);
			break;
		case 'w':
			file_width = atoi(optarg);
			break;
		case 'n':
			file_height = atoi(optarg);
			break;
		case 'x':
			file_xoff = atoi(optarg);
			break;
		case 'y':
			file_yoff = atoi(optarg);
			break;
		case 'X':
			scr_xoff = atoi(optarg);
			break;
		case 'Y':
			scr_yoff = atoi(optarg);
			break;
		case 'S':
			scr_height = scr_width = atoi(optarg);
			break;
		case 'W':
			scr_width = atoi(optarg);
			break;
		case 'N':
			scr_height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		infd = 0;
	} else {
		file_name = argv[optind];
		if( (infd = open(file_name, 0)) == NULL )  {
			(void)fprintf( stderr,
				"pix-fb: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
		fileinput++;
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pix-fb: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{
	register int y;
	register FBIO *fbp;
	int	xout, yout, n;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* If screen size was not set, track the file size */
	if( scr_width == 0 )
		scr_width = file_width;
	if( scr_height == 0 && file_height != INFIN )
		scr_height = file_height;

	if( (fbp = fb_open( NULL, scr_width, scr_height )) == NULL )
		exit(12);

	/* Get the screen size we were given */
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	/* compute pixels output to screen */
	xout = scr_width - scr_xoff;
	if( xout < 0 ) xout = 0;
	if( xout > (file_width-file_xoff) ) xout = (file_width-file_xoff);
	yout = scr_height - scr_yoff;
	if( yout < 0 ) yout = 0;
	if( yout > (file_height-file_yoff) ) yout = (file_height-file_yoff);
	if( xout > MAX_LINE ) {
		fprintf( stderr, "pix-fb: can't output %d pixel lines.\n", xout );
		exit( 2 );
	}
	scanbytes = xout * sizeof(RGBpixel);

	if( clear )  {
		fb_clear( fbp, PIXEL_NULL );
		fb_wmap( fbp, COLORMAP_NULL );
	}
	/*
	 * We use Xout in the Y values if file_height == INFIN since we
	 * assume files have infinite height! (thus yout is always
	 * scr_height-scr_yoff)
	 */
	if( zoom ) {
		/* Zoom in, in the center of view */
		if( file_height == INFIN ) {
			fb_zoom( fbp, scr_width/xout, scr_height/xout );
			if( inverse )
				fb_window( fbp, scr_xoff+xout/2, scr_height-1-(scr_yoff+xout/2) );
			else
				fb_window( fbp, scr_xoff+xout/2, scr_yoff+xout/2 );
		} else {
			fb_zoom( fbp, scr_width/xout, scr_height/yout );
			if( inverse )
				fb_window( fbp, scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2) );
			else
				fb_window( fbp, scr_xoff+xout/2, scr_yoff+yout/2 );
		}
	}

	if( file_yoff != 0 ) skipbytes( infd, file_yoff*file_width*sizeof(RGBpixel) );

	if( !inverse )  {
		/* Normal way -- bottom to top */
		for( y = scr_yoff; y < scr_yoff + yout; y++ )  {
			if( file_xoff != 0 )
				skipbytes( infd, file_xoff*sizeof(RGBpixel) );
			n = mread( infd, (char *)scanline, scanbytes );
			if( n <= 0 ) break;
			fb_write( fbp, scr_xoff, y, scanline, xout );
			/* slop at the end of the line? */
			if( xout < file_width-file_xoff )
				skipbytes( infd, (file_width-file_xoff-xout)*sizeof(RGBpixel) );
		}
	}  else  {
		/* Inverse -- top to bottom */
		for( y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y-- )  {
			if( file_xoff != 0 )
				skipbytes( infd, file_xoff*sizeof(RGBpixel) );
			n = mread( infd, (char *)scanline, scanbytes );
			if( n <= 0 ) break;
			fb_write( fbp, scr_xoff, y, scanline, xout );
			/* slop at the end of the line? */
			if( xout < file_width-file_xoff )
				skipbytes( infd, (file_width-file_xoff-xout)*sizeof(RGBpixel) );
		}
	}
	fb_close( fbp );
	exit(0);
}

/*
 * Throw bytes away.  Use reads into scanline buffer if a pipe, else seek.
 */
skipbytes( fd, num )
int	fd;
long	num;
{
	int	n, try;

	if( fileinput ) {
		(void)lseek( fd, num, 1 );
		return 0;
	}
	
	while( num > 0 ) {
		try = num > MAX_LINE ? MAX_LINE : num;
		n = read( fd, scanline, try );
		if( n <= 0 ){
			return -1;
		}
		num -= n;
	}
	return	0;
}

/*
 * "Multiple try" read.
 *  Will keep reading until either an error occurs
 *  or the requested number of bytes is read.  This
 *  is important for pipes.
 */
mread( fd, bp, num )
int	fd;
register char	*bp;
register int	num;
{
	register int	n;
	int	count;

	count = 0;

	while( num > 0 ) {
		n = read( fd, bp, num );
		if( n < 0 )
			return	-1;
		if( n == 0 )
			return count;
		bp += n;
		count += n;
		num -= n;
	}
	return count;
}
