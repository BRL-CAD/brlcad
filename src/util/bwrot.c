/*                         B W R O T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file bwrot.c
 *
 *  Rotate, Invert, and/or Reverse the pixels in a Black
 *  and White (.bw) file.
 *
 *  The rotation logic was worked out for data ordered with
 *  "upper left" first.  It is being used on files in first
 *  quadrant order (lower left first).  Thus the "forward",
 *  "backward" flags are reversed.
 *
 *  The code was designed to never need to seek on the input,
 *  while it *may* need to seek on output (if the max buffer
 *  is too small).  It would be nice if we could handle the
 *  reverse case also (e.g. pipe on stdout).
 *
 *  Note that this program can be applied to any collection
 *  of single byte entities.
 *
 *  Author -
 *	Phillip Dykstra
 *	24 Sep 1986
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"


#define	MAXBUFBYTES	(1280*1024)

int	buflines, scanbytes;
int	firsty = -1;	/* first "y" scanline in buffer */
int	lasty = -1;	/* last "y" scanline in buffer */
unsigned char *buffer;
unsigned char *bp;
unsigned char *obuf;
unsigned char *obp;

int	nxin = 512;
int	nyin = 512;
int	yin, xout, yout;
int	plus90, minus90, reverse, invert;
double	angle;

static	char usage[] = "\
Usage: bwrot [-f -b -r -i] [-s squaresize]\n\
	[-w width] [-n height] [file.bw] > file.bw\n\
  or   bwrot -a angle [-s squaresize]\n\
	[-w width] [-n height] [file.bw] > file.bw\n";

void	fill_buffer(void), reverse_buffer(void), arbrot(double a);

static char	*file_name;
FILE	*ifp, *ofp;

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "fbrihs:w:n:S:W:N:a:" )) != EOF )  {
		switch( c )  {
		case 'f':
			minus90++;
			break;
		case 'b':
			plus90++;
			break;
		case 'r':
			reverse++;
			break;
		case 'i':
			invert++;
			break;
		case 'h':
			/* high-res */
			nxin = nyin = 1024;
			break;
		case 'S':
		case 's':
			/* square size */
			nxin = nyin = atoi(bu_optarg);
			break;
		case 'W':
		case 'w':
			nxin = atoi(bu_optarg);
			break;
		case 'N':
		case 'n':
			nyin = atoi(bu_optarg);
			break;
		case 'a':
			angle = atof(bu_optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	/* XXX - backward compatability hack */
	if( bu_optind+2 == argc ) {
		nxin = atoi(argv[bu_optind++]);
		nyin = atoi(argv[bu_optind++]);
	}
	if( bu_optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		ifp = stdin;
	} else {
		file_name = argv[bu_optind];
		if( (ifp = fopen(file_name, "r")) == NULL )  {
			(void)fprintf( stderr,
				"bwrot: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "bwrot: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	int	x, y;
	long	outbyte, outplace;

	if ( !get_args( argc, argv ) || isatty(fileno(stdout)) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	ofp = stdout;

	scanbytes = nxin;
	buflines = MAXBUFBYTES / scanbytes;
	if( buflines <= 0 ) {
		fprintf( stderr, "bwrot: I'm not compiled to do a scanline that long!\n" );
		exit( 1 );
	}
	if( buflines > nyin ) buflines = nyin;
	buffer = (unsigned char *)malloc( buflines * scanbytes );
	obuf = (unsigned char *)malloc( (nyin > nxin) ? nyin : nxin );
	if( buffer == (unsigned char *)0 || obuf == (unsigned char *)0 ) {
		fprintf( stderr, "bwrot: malloc failed\n" );
		exit( 3 );
	}

	/*
	 * Break out to added arbitrary angle routine
	 */
	if( angle ) {
		arbrot( angle );
		exit( 0 );
	}

	/*
	 * Clear our "file pointer."  We need to maintain this
	 * In order to tell if seeking is required.  ftell() always
	 * fails on pipes, so we can't use it.
	 */
	outplace = 0;

	yin = 0;
	while( yin < nyin ) {
		/* Fill buffer */
		fill_buffer();
		if( !buflines )
			break;
		if( reverse )
			reverse_buffer();
		if( plus90 ) {
			for( x = 0; x < nxin; x++ ) {
				obp = obuf;
				bp = &buffer[ (lasty-firsty)*scanbytes + x ];
				for( y = lasty; y >= yin; y-- ) { /* firsty? */
					*obp++ = *bp;
					bp -= scanbytes;
				}
				yout = x;
				xout = (nyin - 1) - lasty;
				outbyte = ((yout * nyin) + xout);
				if( outplace != outbyte ) {
					if( fseek( ofp, outbyte, 0 ) < 0 ) {
						fprintf( stderr, "bwrot: Can't seek on output, yet I need to!\n" );
						exit( 3 );
					}
					outplace = outbyte;
				}
				fwrite( obuf, 1, buflines, ofp );
				outplace += buflines;
			}
		} else if( minus90 ) {
			for( x = nxin-1; x >= 0; x-- ) {
				obp = obuf;
				bp = &buffer[ x ];
				for( y = firsty; y <= lasty; y++ ) {
					*obp++ = *bp;
					bp += scanbytes;
				}
				yout = (nxin - 1) - x;
				xout = yin;
				outbyte = ((yout * nyin) + xout);
				if( outplace != outbyte ) {
					if( fseek( ofp, outbyte, 0 ) < 0 ) {
						fprintf( stderr, "bwrot: Can't seek on output, yet I need to!\n" );
						exit( 3 );
					}
					outplace = outbyte;
				}
				fwrite( obuf, 1, buflines, ofp );
				outplace += buflines;
			}
		} else if( invert ) {
			for( y = lasty; y >= firsty; y-- ) {
				yout = (nyin - 1) - y;
				outbyte = yout * scanbytes;
				if( outplace != outbyte ) {
					if( fseek( ofp, outbyte, 0 ) < 0 ) {
						fprintf( stderr, "bwrot: Can't seek on output, yet I need to!\n" );
						exit( 3 );
					}
					outplace = outbyte;
				}
				fwrite( &buffer[(y-firsty)*scanbytes], 1, scanbytes, ofp );
				outplace += scanbytes;
			}
		} else {
			/* Reverse only */
			for( y = 0; y < buflines; y++ ) {
				fwrite( &buffer[y*scanbytes], 1, scanbytes, ofp );
			}
		}

		yin += buflines;
	}
	return 0;
}

void
fill_buffer(void)
{
	buflines = fread( buffer, scanbytes, buflines, ifp );

	firsty = lasty + 1;
	lasty = firsty + (buflines - 1);
}

void
reverse_buffer(void)
{
	int	i;
	unsigned char *p1, *p2, temp;

	for( i = 0; i < buflines; i++ ) {
		p1 = &buffer[ i * scanbytes ];
		p2 = p1 + (scanbytes - 1);
		while( p1 < p2 ) {
			temp = *p1;
			*p1++ = *p2;
			*p2-- = temp;
		}
	}
}

/*
 *  Arbitrary angle rotation.
 *  Currently this needs to be able to buffer the entire image
 *  in memory at one time.
 *
 *  To rotate a point (x,y) CCW about the origin:
 *    x' = x cos(a) - y sin(a)
 *    y' = x sin(a) + y cos(a)
 *  To rotate it about a point (xc,yc):
 *    x' = (x-xc) cos(a) - (y-yc) sin(a) + xc
 *       = x cos(a) - y sin(a) + [xc - xc cos(a) + yc sin(a)]
 *    y' = (x-xc) sin(a) + (y-yc) cos(a) + yc
 *	 = x sin(a) + y cos(a) + [yc - yc cos(a) - xc sin(a)]
 *  So, to take one step in x:
 *    dx' = cos(a)
 *    dy' = sin(a)
 *  or one step in y:
 *    dx' = -sin(a)
 *    dy' = cos(a)
 */
#ifndef M_PI
#define	PI	3.1415926535898
#else
#define PI M_PI
#endif
#define	DtoR(x)	((x)*PI/180.0)

void
arbrot(double a)
      	  	/* rotation angle */
{
	int	x, y;				/* working coord */
	double	x2, y2;				/* its rotated position */
	double	xc, yc;				/* rotation origin */
	int	x_min, y_min, x_max, y_max;	/* area to rotate */
	double	x_goop, y_goop;
	double	sina, cosa;

	if( buflines != nyin ) {
		/* I won't all fit in the buffer */
		fprintf(stderr,"bwrot: Sorry but I can't do an arbitrary rotate of an image this large\n");
		exit(1);
	}
	if( buflines > nyin ) buflines = nyin;
	fill_buffer();

	/*
	 * Convert rotation angle to radians.
	 * Because we "pull down" the pixel from their rotated positions
	 * to their standard ones, the sign of the rotation is reversed.
	 */
	a = -DtoR(a);
	sina = sin(a);
	cosa = cos(a);

	/* XXX - Let the user pick the rotation origin? */
	xc = nxin / 2.0;
	yc = nyin / 2.0;

	x_goop = xc - xc * cosa + yc * sina;
	y_goop = yc - yc * cosa - xc * sina;

	x_min = 0;
	y_min = 0;
	x_max = nxin;
	y_max = nyin;

	for( y = y_min; y < y_max; y++ ) {
		x2 = x_min * cosa - y * sina + x_goop;
		y2 = x_min * sina + y * cosa + y_goop;
		for( x = x_min; x < x_max; x++ ) {
			/* check for in bounds */
			if( x2 >= 0 && x2 < nxin && y2 >= 0 && y2 < nyin )
				putchar(buffer[(int)y2*nyin + (int)x2]);
			else
				putchar(0);	/* XXX - setable color? */
			/* "forward difference" our coordinates */
			x2 += cosa;
			y2 += sina;
		}
	}
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
