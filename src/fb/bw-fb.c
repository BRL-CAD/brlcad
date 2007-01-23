/*                         B W - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file bw-fb.c
 *
 * Write a black and white (.bw) image to the framebuffer.
 * From an 8-bit/pixel, pix order file (i.e. Bottom UP, left to right).
 *
 * This allows an offset into both the display and source file.
 * The color planes to be loaded are also selectable.
 *
 *  Author -
 *	Phillip Dykstra
 *	15 Aug 1985
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"


int skipbytes(int fd, off_t num);

#define	MAX_LINE	(16*1024)	/* Largest output scan line length */

static char	ibuf[MAX_LINE];
static RGBpixel obuf[MAX_LINE];

static int	fileinput = 0;		/* file of pipe on input? */
static int	autosize = 0;		/* !0 to autosize input */

static unsigned long int	file_width = 512;	/* default input width */
static unsigned long int	file_height = 512;	/* default input height */
static int	scr_width = 0;		/* screen tracks file if not given */
static int	scr_height = 0;
static int	file_xoff, file_yoff;
static int	scr_xoff, scr_yoff;
static int	clear = 0;
static int	zoom = 0;
static int	inverse = 0;
static int	redflag   = 0;
static int	greenflag = 0;
static int	blueflag  = 0;

static char	*framebuffer = NULL;
static char	*file_name;
static int	infd;
static FBIO	*fbp;

static char	usage[] = "\
Usage: bw-fb [-a -h -i -c -z -R -G -B] [-F framebuffer]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height] [file.bw]\n";
int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "ahiczRGBF:s:w:n:x:y:X:Y:S:W:N:" )) != EOF )  {
		switch( c )  {
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
		case 'R':
			redflag = 1;
			break;
		case 'G':
			greenflag = 1;
			break;
		case 'B':
			blueflag = 1;
			break;
		case 'F':
			framebuffer = bu_optarg;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(bu_optarg);
			autosize = 0;
			break;
		case 'w':
			file_width = atoi(bu_optarg);
			autosize = 0;
			break;
		case 'n':
			file_height = atoi(bu_optarg);
			autosize = 0;
			break;
		case 'x':
			file_xoff = atoi(bu_optarg);
			break;
		case 'y':
			file_yoff = atoi(bu_optarg);
			break;
		case 'X':
			scr_xoff = atoi(bu_optarg);
			break;
		case 'Y':
			scr_yoff = atoi(bu_optarg);
			break;
		case 'S':
			scr_height = scr_width = atoi(bu_optarg);
			break;
		case 'W':
			scr_width = atoi(bu_optarg);
			break;
		case 'N':
			scr_height = atoi(bu_optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		infd = 0;
	} else {
		file_name = argv[bu_optind];
		if( (infd = open(file_name, 0)) < 0 )  {
			(void)fprintf( stderr,
				"bw-fb: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
		fileinput++;
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "bw-fb: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	register int	x, y, n;
	int	xout, yout;		/* number of sceen output lines */
	int	xstart, xskip;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* autosize input? */
	if( fileinput && autosize ) {
		unsigned long int	w, h;
		if( fb_common_file_size(&w, &h, file_name, 1) ) {
			file_width = w;
			file_height = h;
		} else {
			fprintf(stderr, "bw-fb: unable to autosize\n");
		}
	}

	/* If no color planes were selected, load them all */
	if( redflag == 0 && greenflag == 0 && blueflag == 0 )
		redflag = greenflag = blueflag = 1;

	/* If screen size was not set, track the file size */
	if( scr_width == 0 )
		scr_width = file_width;
	if( scr_height == 0 )
		scr_height = file_height;

	/* Open Display Device */
	if ((fbp = fb_open( framebuffer, scr_width, scr_height )) == NULL ) {
		fprintf( stderr, "fb_open failed\n");
		exit( 3 );
	}

	/* Get the screen size we were given */
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	/* compute pixels output to screen */
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
	if( xout < 0 ) xout = 0;
	if( xout > (file_width-file_xoff) ) xout = (file_width-file_xoff);

	if( inverse )
		scr_yoff = (-scr_yoff);

	yout = scr_height - scr_yoff;
	if( yout < 0 ) yout = 0;
	if( yout > (file_height-file_yoff) ) yout = (file_height-file_yoff);
	if( xout > MAX_LINE ) {
		fprintf( stderr, "bw-fb: can't output %d pixel lines.\n", xout );
		exit( 2 );
	}

	if( clear ) {
		fb_clear( fbp, PIXEL_NULL );
	}
	if( zoom ) {
		/* Zoom in, and center the file */
		fb_zoom( fbp, scr_width/xout, scr_height/yout );
		if( inverse )
			fb_window( fbp, scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2) );
		else
			fb_window( fbp, scr_xoff+xout/2, scr_yoff+yout/2 );
	}

	/* Test for simplest case */
	if( inverse == 0 &&
		file_xoff == 0 && file_yoff == 0 &&
		scr_xoff+file_width <= fb_getwidth(fbp)
	)  {
		unsigned char	*buf;
		int		npix = file_width * yout;

		if( (buf = malloc(npix)) == NULL )  {
			perror("bw-fb malloc");
			goto general;
		}
		n = bu_mread( infd, (char *)buf, npix );
		if( n != npix )  {
			fprintf(stderr, "bw-fb: read got %d, s/b %d\n", n, npix );
			if( n <= 0 )  exit(7);
			npix = n;	/* show what we got */
		}
		n = (npix+file_width-1)/file_width;	/* num lines got */
		n = fb_bwwriterect(fbp, scr_xoff, scr_yoff, file_width, n, buf);
		if( npix != n )  {
			fprintf(stderr, "bw-fb: fb_bwwriterect() got %d, s/b %d\n", n, npix );
			exit(8);
		}
		fb_close( fbp );
		exit(0);
	}

	/* Begin general case */
general:
	if( file_yoff != 0 ) skipbytes( infd, file_yoff*file_width );

	for( y = scr_yoff; y < scr_yoff + yout; y++ ) {
		if( y < 0 || y >= scr_height )
		{
			skipbytes( infd , file_width );
			continue;
		}
		if( file_xoff+xskip != 0 )
			skipbytes( infd, file_xoff+xskip );
		n = bu_mread( infd, &ibuf[0], xout );
		if( n <= 0 ) break;
		/*
		 * If we are not loading all color planes, we have
		 * to do a pre-read.
		 */
		if( redflag == 0 || greenflag == 0 || blueflag == 0 ) {
			if( inverse )
				n = fb_read( fbp, scr_xoff, scr_height-1-y,
					(unsigned char *)obuf, xout );
			else
				n = fb_read( fbp, scr_xoff, y,
					(unsigned char *)obuf, xout );
			if( n < 0 )  break;
		}
		for( x = 0; x < xout; x++ ) {
			if( redflag )
				obuf[x][RED] = ibuf[x];
			if( greenflag )
				obuf[x][GRN] = ibuf[x];
			if( blueflag )
				obuf[x][BLU] = ibuf[x];
		}
		if( inverse )
			fb_write( fbp, xstart, scr_height-1-y, (unsigned char *)obuf, xout );
		else
			fb_write( fbp, xstart, y, (unsigned char *)obuf, xout );

		/* slop at the end of the line? */
		if( file_xoff+xskip+xout < file_width )
			skipbytes( infd, file_width-file_xoff-xskip-xout );
	}

	fb_close( fbp );
	exit( 0 );
}

/*
 * Throw bytes away.  Use reads into ibuf buffer if a pipe, else seek.
 */
int
skipbytes(int fd, off_t num)
{
	int	n, try;

	if( fileinput ) {
		(void)lseek( fd, num, 1 );
		return 0;
	}

	while( num > 0 ) {
		try = num > MAX_LINE ? MAX_LINE : num;
		n = read( fd, ibuf, try );
		if( n <= 0 ){
			return -1;
		}
		num -= n;
	}
	return	0;
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
