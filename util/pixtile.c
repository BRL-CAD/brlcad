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

#include <stdio.h>

extern char *malloc();

int pix_line;		/* number of pixels/line (512, 1024) */
int scanbytes;		/* bytes per input line */
int w;			/* width of sub-images in pixels */
int im_line;		/* number of images across output scanline */
int image;		/* current sub-image number */

char usage[] = "Usage: pixtile [-h] basename width [startframe] >file.pix\n";

main( argc, argv )
char **argv;
{
	register int i;
	char *obuf;
	char *basename;
	int framenumber;
	int swathbytes;
	int rel;		/* Relative image # within swath */
	char name[128];

	if( argc < 2 || isatty(fileno(stdout)) )  {
		fprintf(stderr, "%s", usage);
		exit(12);
	}

	if( strcmp( argv[1], "-h" ) == 0 )  {
		argc--; argv++;
		pix_line = 1024;
	}  else
		pix_line = 512;

	basename = argv[1];
	w = atoi( argv[2] );
	if( w < 1 ) {
		fprintf(stderr,"pixtile: width of %d out of range\n", w);
		exit(12);
	}
	if( argc == 4 )
		framenumber = atoi(argv[3]);
	else	framenumber = 0;

	scanbytes = w * 3;
	im_line = pix_line/w;	/* number of images across line */
	swathbytes = pix_line * w * 3;	/* One swath of images */
	if( (obuf = (char *)malloc( swathbytes )) == (char *)0 )  {
		fprintf(stderr,"pixtile:  malloc %d failure\n", swathbytes );
		exit(10);
	}
	image = 0;
	while( 1 )  {
		/*
		 * Collect together one swath
		 */
		for( rel = 0; rel<im_line; rel++, image++, framenumber++ )  {
			register char *out;
			int fd;

			fprintf(stderr,"%d ", framenumber);  fflush(stdout);
			if(image >= im_line*im_line )  {
				fprintf(stderr,"\npixtile: frame full\n");
				/* All swaths already written out */
				exit(0);
			}
			sprintf(name,"%s.%d", basename, framenumber);
			if( (fd=open(name,0))<0 )  {
				perror(name);
				goto done;
			}
			/* Read in .pix file.  Bottom to top */
			for( i=0; i<w; i++ )  {
				register int j;

				/* virtual image l/r offset */
				j = (rel*w);

				/* select proper scanline within image */
				j += i*pix_line;

				if( read( fd, &obuf[j*3], scanbytes ) != scanbytes )
					break;
			}
			close(fd);
		}
		(void)write( 1, obuf, swathbytes );
	}
	/* NOTREACHED */
done:
	(void)write( 1, obuf, swathbytes );
	exit(0);
}
