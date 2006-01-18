/*                     P I X U N T I L E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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
/** @file pixuntile.c
 *
 *  Given a single .pix file with multiple images, each
 *  side-by-side, right to left, bottom to top, break them
 *  up into separate .pix files.
 *
 *  Author -
 *	Phillip Dykstra
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"


int out_width = 64;	/* width of input sub-images in pixels (64) */
int out_height = 64;	/* height of input sub-images in scanlines (64) */
int in_width = 512;	/* number of output pixels/line (512, 1024) */
int in_height = 512;	/* number of output lines (512, 1024) */
char *base_name;		/* basename of input file(s) */
int framenumber = 0;	/* starting frame number (default is 0) */
int islist = 0;

char usage[] = "\
Usage: pixuntile [-h] [-s squareinsize] [-w in_width] [-n in_height]\n\
	[-S squareoutsize] [-W out_width] [-N out_height]\n\
	[-o startframe] basename [file2 ... fileN] <file.pix\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "hs:w:n:S:W:N:o:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			in_height = in_width = 1024;
			break;
		case 's':
			/* square input file size */
			in_height = in_width = atoi(bu_optarg);
			break;
		case 'w':
			in_width = atoi(bu_optarg);
			break;
		case 'n':
			in_height = atoi(bu_optarg);
			break;
		case 'S':
			out_height = out_width = atoi(bu_optarg);
			break;
		case 'W':
			out_width = atoi(bu_optarg);
			break;
		case 'N':
			out_height = atoi(bu_optarg);
			break;
		case 'o':
			framenumber = atoi(bu_optarg);
			break;
		default:		/* '?' */
			return(0);	/* Bad */
		}
	}

	if( isatty(fileno(stdin)) )  {
		return(0);	/* Bad */
	}

	if( bu_optind >= argc )  {
		fprintf(stderr, "pixuntile: basename or filename(s) missing\n");
		return(0);	/* Bad */
	}

	return(1);		/* OK */
}

int	numx;
int	numy;
int	pixsize = 3;

int
main(int argc, char **argv)
{
	int	i, y;
	char	ibuf[1024*3], name[80];
	FILE	*f[8];

	if( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1);
	}

	if( bu_optind+1 == argc )  {
		base_name = argv[bu_optind];
		islist = 0;
	} else {
		islist = 1;
	}

	if( in_width < 1 ) {
		fprintf(stderr,"pixuntile: width of %d out of range\n", in_width);
		exit(12);
	}

	numx = in_width / out_width;
	numy = in_height / out_height;

	y = 0;
	while( fread(ibuf, pixsize, in_width, stdin) == in_width ) {
		if( y == 0 ) {
			/* open a new set of output files */
			for( i = 0; i < numx; i++ ) {
				if( f[i] != 0 )
					fclose(f[i]);
				fprintf(stderr,"%d ", framenumber);  fflush(stdout);
				if( islist )  {
					/* See if we read all the files */
					if( bu_optind >= argc )
						goto done;
					strcpy( name, argv[bu_optind++] );
				} else {
					sprintf( name, "%s.%d", base_name, framenumber );
				}
				if( (f[i] = fopen(name,"w")) == NULL ) {
					perror( name );
					goto done;
				}
				framenumber++;
			}
		}
		/* split this scanline up into the output files */
		for( i = 0; i < numx; i++ ) {
			fwrite(&ibuf[i*out_width*pixsize], pixsize, out_width, f[i]);
		}
		y = (y + 1) % out_height;
	}
done:
	fprintf( stderr,"\n" );
	exit( 0 );
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
