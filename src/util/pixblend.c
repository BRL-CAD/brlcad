/*                      P I X B L E N D . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2007 United States Government as represented by
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
/** @file pixblend.c
 *
 *  Given two streams of data, typically pix(5) or bw(5) images,
 *  generate an output stream of the same size, where the value of
 *  each pixel in the output is determined by either:
 *    1) a linear interpolation between the two corresponding pixels in the
 *       input streams; or,
 *    2) the pixel of either the first stream or the second stream, chosen
 *       randomly.
 *
 *  This routine operates on a pixel-by-pixel basis, and thus
 *  is independent of the resolution of the image.
 *
 *  Authors -
 *	Paul Randal Stay
 *      Glenn Durfee
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"


static char	*f1_name;
static char	*f2_name;
static FILE	*f1;
static FILE	*f2;

/* CHUNK should be a multiple of 3 (the pixel size) */
#define CHUNK	3*1024
static char	*b1;			/* fg input buffer */
static char	*b2;			/* bg input buffer */
static char	*b3;			/* output buffer */

double		value, gvalue;
int             iflg, rflg, gflg;
int             seed;

static char usage[] = "\
Usage: pixblend [-{r|i} value] [-s [seed]] file1.pix file2.pix > out.pix\n";

int
timeseed(void)
{
    struct timeval tv;
    gettimeofday(&tv, (struct timezone *)NULL);
    return (int)tv.tv_usec;
}

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "r:i:Ss:g:" )) != EOF )  {
		switch( c )  {
		case 'r':
		    if (iflg)
			return 0;
		    else {
			value = atof( bu_optarg );
			++rflg;
		    }
		    break;
		case 'i':
		    if (rflg)
			return 0;
		    else {
			if (gflg) {
			    fprintf(stderr, "The -g and -i options do not make sense together.\n");
			    return 0;
			}
			value = atof( bu_optarg );
			++iflg;
		    }
		    break;
		case 'S':
		    seed = timeseed();
		    break;
		case 's':
		    seed = atoi( bu_optarg );
		    break;
		case 'g':
		    if (iflg) {
			fprintf(stderr, "The -g and -i options do not make sense together.\n");
			return 0;
		    }
		    ++gflg;
		    gvalue = atof( bu_optarg );
		    break;
		default:		/* '?' */
		    return(0);
		}
	}

	if( bu_optind+2 > argc )
		return(0);

	f1_name = argv[bu_optind++];
	if( strcmp( f1_name, "-" ) == 0 )
		f1 = stdin;
	else if( (f1 = fopen(f1_name, "r")) == NULL )  {
		perror(f1_name);
		(void)fprintf( stderr,
			"pixblend: cannot open \"%s\" for reading\n",
			f1_name );
		return(0);
	}

	f2_name = argv[bu_optind++];
	if( strcmp( f2_name, "-" ) == 0 )
		f2 = stdin;
	else if( (f2 = fopen(f2_name, "r")) == NULL )  {
		perror(f2_name);
		(void)fprintf( stderr,
			"pixblend: cannot open \"%s\" for reading\n",
			f2_name );
		return(0);
	}

	if ( argc > bu_optind )
		(void)fprintf( stderr, "pixblend: excess argument(s) ignored\n" );

	/* Adjust value upwards if glitterize option is used */
	value += gvalue * (1 - value);

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
#ifndef HAVE_DRAND48
	int threshold = 0;
	int gthreshold = 0;
#endif
        int c = 0;

	if ( !get_args( argc, argv ) || isatty(fileno(stdout)) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	gvalue = 0.0;
	if (!iflg && !rflg) {   /* Default action: interpolate by 50% */
	    iflg = 1;
	    value = 0.5;
	}

	if( value < 0.0 || value > 1.0)
	{
		fprintf(stderr,"pixblend: Blend value must be between 0.0 and 1.0\n");
		exit(0);

	}

	if (rflg) {
#ifdef HAVE_SRAND48
	    srand48((long)seed);
#else
	    threshold = (int) (value * 65536.0);
	    gthreshold = (int) (gvalue * 65536.0);
	    srandom(seed);
#endif
	}

	if( (b1 = (char *)malloc( CHUNK )) == (char *)0 ||
	    (b2 = (char *)malloc( CHUNK )) == (char *)0 ||
	    (b3 = (char *)malloc( CHUNK )) == (char *)0 ) {
	    	fprintf(stderr, "pixblend:  malloc failure\n");
	    	exit(3);
	}

	while(1)  {
		unsigned char	*cb1, *cb2;	/* current input buf ptrs */
		register unsigned char	*cb3; 	/* current output buf ptr */
		int r1, r2, len, todo;

	        ++c;
		r1 = fread( b1, 1, CHUNK, f1 );
		r2 = fread( b2, 1, CHUNK, f2 );
		len = r1;
		if( r2 < len )
			len = r2;
		if( len <= 0 )
			break;

		cb1 = (unsigned char *)b1;
		cb2 = (unsigned char *)b2;
		cb3 = (unsigned char *)b3;
		todo = len;
		if (iflg) {
		    while( todo-- ) {
			*cb3++ = (char) ((1.0 - value) * (*cb1++) +
					 value * (*cb2++));
		    }
		} else {
		    while( todo > 0 ) {
			if (cb1[0] == cb2[0] &&
			    cb1[1] == cb2[1] &&
			    cb1[2] == cb2[2]) {
			    cb3[0] = cb1[0];
			    cb3[1] = cb1[1];
			    cb3[2] = cb1[2];
			    cb1 += 3;
			    cb2 += 3;
			    cb3 += 3;
			    todo -= 3;
			} else {
#ifdef HAVE_DRAND48
			    double d;
			    extern double drand48(void);
			    d = drand48();
			    if (d >= value) {
#else
			    int r;
			    r = random() & 0xffff;
 			    if (r >= threshold) {
#endif
				cb3[0] = cb1[0];
				cb3[1] = cb1[1];
				cb3[2] = cb1[2];
			    } else {
#ifdef HAVE_DRAND48
				if (d >= gvalue) {
#else
				if (r >= gthreshold) {
#endif
				    cb3[0] = cb2[0];
				    cb3[1] = cb2[1];
				    cb3[2] = cb2[2];
				} else {
				    cb3[0] = cb3[1] = cb3[2] = 255;
				}
			    }
			    cb1 += 3;
			    cb2 += 3;
			    cb3 += 3;
			    todo -= 3;
			}
		    }
		}
		fwrite( b3, 1, len, stdout );
	}
	exit(0);
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
