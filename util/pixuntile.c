/*
 *  			P I X U N T I L E . C
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
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"			/* For malloc and getopt */

int out_width = 64;	/* width of input sub-images in pixels (64) */
int out_height = 64;	/* height of input sub-images in scanlines (64) */
int in_width = 512;	/* number of output pixels/line (512, 1024) */
int in_height = 512;	/* number of output lines (512, 1024) */
char *basename;		/* basename of input file(s) */
int framenumber = 0;	/* starting frame number (default is 0) */
int islist = 0;

char usage[] = "\
Usage: pixuntile [-h] [-s squareinsize] [-w in_width] [-n in_height]\n\
	[-S squareoutsize] [-W out_width] [-N out_height]\n\
	[-o startframe] basename [file2 ... fileN] <file.pix\n";

int
get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hs:w:n:S:W:N:o:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			in_height = in_width = 1024;
			break;
		case 's':
			/* square input file size */
			in_height = in_width = atoi(optarg);
			break;
		case 'w':
			in_width = atoi(optarg);
			break;
		case 'n':
			in_height = atoi(optarg);
			break;
		case 'S':
			out_height = out_width = atoi(optarg);
			break;
		case 'W':
			out_width = atoi(optarg);
			break;
		case 'N':
			out_height = atoi(optarg);
			break;
		case 'o':
			framenumber = atoi(optarg);
			break;
		default:		/* '?' */
			return(0);	/* Bad */
		}
	}

	if( isatty(fileno(stdin)) )  {
		return(0);	/* Bad */
	}

	if( optind >= argc )  {
		fprintf(stderr, "pixuntile: basename or filename(s) missing\n");
		return(0);	/* Bad */
	}

	return(1);		/* OK */
}

int	numx;
int	numy;
int	pixsize = 3;

int
main( argc, argv )
int	argc;
char	**argv;
{
	int	i, y;
	char	ibuf[1024*3], name[80];
	FILE	*f[8];

	if( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1);
	}

	if( optind+1 == argc )  {
		basename = argv[optind];
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
					if( optind >= argc )
						goto done;
					strcpy( name, argv[optind++] );
				} else {
					sprintf( name, "%s.%d", basename, framenumber );
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
