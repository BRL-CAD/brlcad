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
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "externs.h"			/* For getopt */
#include "fb.h"

/* defined in libbn/asize.c */
extern int bn_common_file_size();

int mread();
int skipbytes();

static unsigned char *scanline;		/* 1 scanline pixel buffer */
static int	scanbytes;		/* # of bytes of scanline */
static int	scanpix;		/* # of pixels of scanline */

static int	multiple_lines = 0;	/* Streamlined operation */

static char	*framebuffer = NULL;
static char	*file_name;
static int	infd;

static int	fileinput = 0;		/* file of pipe on input? */
static int	autosize = 0;		/* !0 to autosize input */

static int	file_width = 512;	/* default input width */
static int	file_height = 512;	/* default input height */
static int	scr_width = 0;		/* screen tracks file if not given */
static int	scr_height = 0;
static int	file_xoff, file_yoff;
static int	scr_xoff, scr_yoff;
static int	clear = 0;
static int	zoom = 0;
static int	inverse = 0;		/* Draw upside-down */
static int	one_line_only = 0;	/* insist on 1-line writes */

static char usage[] = "\
Usage: pix-fb [-a -h -i -c -z -1] [-m #lines] [-F framebuffer]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height] [file.pix]\n";

int
get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "1m:ahiczF:s:w:n:x:y:X:Y:S:W:N:" )) != EOF )  {
		switch( c )  {
		case '1':
			one_line_only = 1;
			break;
		case 'm':
			multiple_lines = atoi(optarg);
			break;
		case 'a':
			autosize = 1;
			break;
		case 'h':
			/* high-res */
			file_height = file_width = 1024;
			scr_height = scr_width = 1024;
			autosize = 0;
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
		case 'F':
			framebuffer = optarg;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(optarg);
			autosize = 0;
			break;
		case 'w':
			file_width = atoi(optarg);
			autosize = 0;
			break;
		case 'n':
			file_height = atoi(optarg);
			autosize = 0;
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
		if( (infd = open(file_name, 0)) < 0 )  {
			perror(file_name);
			(void)fprintf( stderr,
				"pix-fb: cannot open \"%s\" for reading\n",
				file_name );
			exit(1);
		}
		fileinput++;
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pix-fb: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(argc, argv)
int argc;
char **argv;
{
	register int y;
	register FBIO *fbp;
	int	xout, yout, n, m, xstart, xskip;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* autosize input? */
	if( fileinput && autosize ) {
		int	w, h;
		if( bn_common_file_size(&w, &h, file_name, 3) ) {
			file_width = w;
			file_height = h;
		} else {
			fprintf(stderr,"pix-fb: unable to autosize\n");
		}
	}

	/* If screen size was not set, track the file size */
	if( scr_width == 0 )
		scr_width = file_width;
	if( scr_height == 0 )
		scr_height = file_height;

	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == NULL )
		exit(12);

	/* Get the screen size we were given */
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	/* compute number of pixels to be output to screen */
	if( scr_xoff < 0 )
	{
		xout = scr_width + scr_xoff;
		xskip = (-scr_xoff);
		xstart = 0;
	}
	else
	{
		xout = scr_width - scr_xoff;
		xskip = 0;
		xstart = scr_xoff;
	}

	if( xout < 0 )
		exit(0);			/* off screen */
	if( xout > (file_width-file_xoff) )
		xout = (file_width-file_xoff);
	scanpix = xout;				/* # pixels on scanline */

	if( inverse )
		scr_yoff = (-scr_yoff);

	yout = scr_height - scr_yoff;
	if( yout < 0 )
		exit(0);			/* off screen */
	if( yout > (file_height-file_yoff) )
		yout = (file_height-file_yoff);

	/* Only in the simplest case use multi-line writes */
	if( !one_line_only && multiple_lines > 0 && !inverse && !zoom &&
	    xout == file_width &&
	    file_width <= scr_width )  {
	    	scanpix *= multiple_lines;
	}

	scanbytes = scanpix * sizeof(RGBpixel);
	if( (scanline = (unsigned char *)malloc(scanbytes)) == RGBPIXEL_NULL )  {
		fprintf(stderr,
			"pix-fb:  malloc(%d) failure for scanline buffer\n",
			scanbytes);
		exit(2);
	}

	if( clear )  {
		fb_clear( fbp, PIXEL_NULL );
	}
	if( zoom ) {
		/* Zoom in, and center the display.  Use square zoom. */
		int	zoom;
		zoom = scr_width/xout;
		if( scr_height/yout < zoom )  zoom = scr_height/yout;
		if( inverse )  {
			fb_view( fbp,
				scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2),
				zoom, zoom );
		} else {
			fb_view( fbp,
				scr_xoff+xout/2, scr_yoff+yout/2,
				zoom, zoom );
		}
	}

	if( file_yoff != 0 ) skipbytes( infd, (off_t)file_yoff*(off_t)file_width*sizeof(RGBpixel) );

	if( multiple_lines )  {
		/* Bottom to top with multi-line reads & writes */
		int	height;
		for( y = scr_yoff; y < scr_yoff + yout; y += multiple_lines )  {
			n = mread( infd, (char *)scanline, scanbytes );
			if( n <= 0 ) break;
			height = multiple_lines;
			if( n != scanbytes )  {
				height = (n/sizeof(RGBpixel)+xout-1)/xout;
				if( height <= 0 )  break;
			}
			/* Don't over-write */
			if( y + height > scr_yoff + yout )
				height = scr_yoff + yout - y;
			if( height <= 0 )  break;
			m = fb_writerect( fbp, scr_xoff, y,
				file_width, height,
				scanline );
			if( m != file_width*height )  {
				fprintf(stderr,
					"pix-fb: fb_writerect(x=%d, y=%d, w=%d, h=%d) failure, ret=%d, s/b=%d\n",
					scr_xoff, y,
					file_width, height, m, scanbytes );
			}
		}
	} else if( !inverse )  {
		/* Normal way -- bottom to top */
		for( y = scr_yoff; y < scr_yoff + yout; y++ )  {
			if( y < 0 || y > scr_height )
			{
				skipbytes( infd , (off_t)file_width*sizeof(RGBpixel) );
				continue;
			}
			if( file_xoff+xskip != 0 )
				skipbytes( infd, (off_t)(file_xoff+xskip)*sizeof(RGBpixel) );
			n = mread( infd, (char *)scanline, scanbytes );
			if( n <= 0 ) break;
			m = fb_write( fbp, xstart, y, scanline, xout );
			if( m != xout )  {
				fprintf(stderr,
					"pix-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
					scr_xoff, y, xout,
					m, xout );
			}
			/* slop at the end of the line? */
			if( file_xoff+xskip+scanpix < file_width )
				skipbytes( infd, (off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel) );
		}
	}  else  {
		/* Inverse -- top to bottom */
		for( y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y-- )  {
			if( y < 0 || y >= scr_height )
			{
				skipbytes( infd , (off_t)file_width*sizeof(RGBpixel) );
				continue;
			}
			if( file_xoff+xskip != 0 )
				skipbytes( infd, (off_t)(file_xoff+xskip)*sizeof(RGBpixel) );
			n = mread( infd, (char *)scanline, scanbytes );
			if( n <= 0 ) break;
			m = fb_write( fbp, xstart, y, scanline, xout );
			if( m != xout )  {
				fprintf(stderr,
					"pix-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
					scr_xoff, y, xout,
					m, xout );
			}
			/* slop at the end of the line? */
			if( file_xoff+xskip+scanpix < file_width )
				skipbytes( infd, (off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel) );
		}
	}
	if( fb_close( fbp ) < 0 )  {
		fprintf(stderr, "pix-fb: Warning: fb_close() error\n");
	}
	exit(0);
}

/*
 * Throw bytes away.  Use reads into scanline buffer if a pipe, else seek.
 */
int
skipbytes( fd, num )
int	fd;
off_t	num;
{
	int	n, try;

	if( fileinput ) {
		(void)lseek( fd, (off_t)num, 1 );
		return 0;
	}
	
	while( num > 0 ) {
		try = num > scanbytes ? scanbytes : num;
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
int
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
