/*
 *  			P I X T I L E . C
 *  
 *  Given multiple .pix files with ordinary lines of pixels,
 *  produce a single image with each image side-by-side,
 *  right to left, bottom to top on STDOUT.
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

#include "conf.h"

#include <stdio.h>

#include "machine.h"			/* For bzero */
#include "externs.h"			/* For getopt and malloc */

int file_width = 64;	/* width of input sub-images in pixels (64) */
int file_height = 64;	/* height of input sub-images in scanlines (64) */
int scr_width = 512;	/* number of output pixels/line (512, 1024) */
int scr_height = 512;	/* number of output lines (512, 1024) */
char *basename;		/* basename of input file(s) */
int framenumber = 0;	/* starting frame number (default is 0) */

char usage[] = "\
Usage: pixtile [-h] [-s squareinsize] [-w file_width] [-n file_height]\n\
	[-S squareoutsize] [-W out_width] [-N out_height]\n\
	[-o startframe] basename [file2 ... fileN] >file.pix\n";

/*
 *			M R E A D
 *
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because pipes
 * and network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 */
int
mread(fd, bufp, n)
int	fd;
register char	*bufp;
int	n;
{
	register int	count = 0;
	register int	nread;

	do {
		nread = read(fd, bufp, (unsigned)n-count);
		if(nread < 0)  {
			perror("buffer: mread");
			return(-1);
		}
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
}

/*
 *			G E T _ A R G S
 */
get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hs:w:n:S:W:N:o:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			scr_height = scr_width = 1024;
			break;
		case 's':
			/* square input file size */
			file_height = file_width = atoi(optarg);
			break;
		case 'w':
			file_width = atoi(optarg);
			break;
		case 'n':
			file_height = atoi(optarg);
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
		case 'o':
			framenumber = atoi(optarg);
			break;
		default:		/* '?' */
			return(0);	/* Bad */
		}
	}

	if( isatty(fileno(stdout)) )  {
		return(0);	/* Bad */
	}

	if( optind >= argc )  {
		fprintf(stderr, "pixtile: basename or filename(s) missing\n");
		return(0);	/* Bad */
	}

	return(1);		/* OK */
}

main( argc, argv )
char **argv;
{
	register int i;
	char *obuf;
	int im_line;		/* number of images across output scanline */
	int im_high;		/* number of images (swaths) high */
	int scanbytes;		/* bytes per input line */
	int swathbytes;		/* bytes per swath of images */
	int image;		/* current sub-image number */
	int rel = 0;		/* Relative image # within swath */
	int maximage;		/* Maximum # of images that will fit */
	int islist = 0;		/* set if a list, zero if basename */
	int is_stream = 0;	/* set if input is stream on stdin */
	char name[256];

	if( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1);
	}

	if( optind+1 == argc )  {
		basename = argv[optind];
		islist = 0;
		if( basename[0] == '-' && basename[1] == '\0' )
			is_stream = 1;
	} else {
		islist = 1;
	}

	if( file_width < 1 ) {
		fprintf(stderr,"pixtile: width of %d out of range\n", file_width);
		exit(12);
	}

	scanbytes = file_width * 3;

	/* number of images across line */
	im_line = scr_width/file_width;

	/* number of images high */
	im_high = scr_height/file_height;

	/* One swath of images */
	swathbytes = scr_width * file_height * 3;

	maximage = im_line * im_high;

	if( (obuf = (char *)malloc( swathbytes )) == (char *)0 )  {
		(void)fprintf(stderr,"pixtile:  malloc %d failure\n", swathbytes );
		exit(10);
	}

	image = 0;
	while( image < maximage )  {
		bzero( obuf, swathbytes );
		/*
		 * Collect together one swath
		 */
		for( rel = 0; rel<im_line; rel++, image++, framenumber++ )  {
			int fd;

			if(image >= maximage )  {
				fprintf(stderr,"\npixtile: frame full\n");
				/* All swaths already written out */
				exit(0);
			}
			fprintf(stderr,"%d ", framenumber);  fflush(stdout);
			if( is_stream )  {
				fd = 0;		/* stdin */
			} else {
				if( islist )  {
					/* See if we read all the files */
					if( optind >= argc )
						goto done;
					strcpy(name, argv[optind++]);
				} else {
					sprintf(name,"%s.%d", basename, framenumber);
				}
				if( (fd=open(name,0))<0 )  {
					perror(name);
					goto done;
				}
			}
			/* Read in .pix file.  Bottom to top */
			for( i=0; i<file_height; i++ )  {
				register int j;

				/* virtual image l/r offset */
				j = (rel*file_width);

				/* select proper scanline within image */
				j += i*scr_width;

				if( mread( fd, &obuf[j*3], scanbytes ) != scanbytes )
					break;
			}
			if( fd > 0 )  close(fd);
		}
		(void)write( 1, obuf, swathbytes );
		rel = 0;	/* in case we fall through */
	}
done:
	/* Flush partial frame? */
	if( rel != 0 )
		(void)write( 1, obuf, swathbytes );
	fprintf(stderr,"\n");
	exit(0);
}

