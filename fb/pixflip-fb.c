/*
 *  			P I X F L I P - F B . C
 *  
 *  Given multiple .pix files with ordinary lines of pixels,
 *  sequence through them on the current framebuffer.
 *  A window-system version of "pixtile and fbanim".
 *  This program depends heavily on having lots of virtual memory
 *  in which to buffer all the images.
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
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <sys/time.h>		/* For struct timeval */

#include "machine.h"
#include "externs.h"		/* For getopt and malloc */
#include "fb.h"

int	file_width = 512;	/* width of input sub-images in pixels */
int	file_height = 512;	/* height of input sub-images in scanlines */
int	screen_width = 0;	/* number of screen pixels/line */
int	screen_height = 0;	/* number of screen lines */
char	*basename;		/* basename of input file(s) */
int	framenumber = 0;	/* starting frame number (default is 0) */
int	fps = 8;		/* frames/second */

void		showframe();

FBIO	*fbp;
int	verbose = 0;
int	rocking = 0;
int	passes = 100;		/* limit on number of passes */
int	zoom = 1;

#define MAXFRAMES	1000
unsigned char	*frames[MAXFRAMES];	/* Pointers to pixel arrays */
int	maxframe = 0;		/* Index of first unused slot in frames[] */

char usage[] = "\
Usage: pixflip-fb [-h]\n\
	[-s square_file_size] [-w file_width] [-n file_height]\n\
	[-S square_scr_size] [-W scr_width] [-N scr_height]\n\
	[-f frames/sec] [-p passes] [-r] [-v] [-z]\n\
	[-o startframe] basename [file2 ... fileN]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hs:w:n:S:W:N:o:f:p:rzv" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			screen_height = screen_width = 1024;
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
			screen_height = screen_width = atoi(optarg);
			break;
		case 'W':
			screen_width = atoi(optarg);
			break;
		case 'N':
			screen_height = atoi(optarg);
			break;
		case 'o':
			framenumber = atoi(optarg);
			break;
		case 'f':
			fps = atoi(optarg);
			break;
		case 'p':
			passes = atoi(optarg);
			if(passes<1)  passes=1;
			break;
		case 'r':
			rocking = 1;
			break;
		case 'z':
			zoom = 0;
			break;
		case 'v':
			verbose = 1;
			break;
		default:		/* '?' */
			return(0);	/* Bad */
		}
	}

	if( optind >= argc )  {
		fprintf(stderr, "pixflip-fb: basename or filename(s) missing\n");
		return(0);	/* Bad */
	}

	return(1);		/* OK */
}

main( argc, argv )
char **argv;
{
	register int i;
	unsigned char	*obuf;
	int	scanbytes;		/* bytes per input image */
	int	islist = 0;		/* set if a list, zero if basename */
	char	name[256];
	int	fd;
	struct timeval tv;
	fd_set readfds;

	FD_ZERO(&readfds);
	FD_SET(fileno(stdin), &readfds);

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

	if( file_width < 1 ) {
		fprintf(stderr,"pixflip-fb: width of %d out of range\n", file_width);
		exit(12);
	}

	if( (fbp = fb_open( NULL, screen_width, screen_height )) == FBIO_NULL )  {
		fprintf(stderr,"pixflip-fb: fb_open failed\n");
		exit(12);
	}
	screen_width = fb_getwidth(fbp);
	screen_height = fb_getheight(fbp);

	if (zoom) {
		if (verbose) fprintf(stderr,
			"pixflip-fb: zoom = %d x %d\n",
			screen_width/file_width,
			screen_height/file_height);
		fb_view(fbp, file_width/2, file_height/2,
			screen_width/file_width,
			screen_height/file_height);
	}

	if( fps <= 1 )  {
		tv.tv_sec = fps ? 1L : 4L;
		tv.tv_usec = 0L;
	} else {
		tv.tv_sec = 0L;
		tv.tv_usec = (long) (1000000/fps);
	}

	scanbytes = file_width * file_height * sizeof(RGBpixel);

	for( maxframe = 0; maxframe < MAXFRAMES;  )  {

		if( (obuf = (unsigned char *)malloc( scanbytes )) == (unsigned char *)0 )  {
			(void)fprintf(stderr,"pixflip-fb:  malloc %d failure\n", scanbytes );
			break;
		}
		bzero( (char *)obuf, scanbytes );
		frames[maxframe] = obuf;

		fprintf(stderr,"%d ", framenumber);  fflush(stdout);
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

		/* Read in .pix file.  Bottom to top */
		i = read( fd, obuf, scanbytes );
		if( i <= 0 )  {
			perror(name);
		} else if( i != scanbytes ) {
			fprintf(stderr,"\npixflip-fb:  %s wanted %d got %d\n",
				name, scanbytes, i );
		}
		close(fd);

		/* Show user the frame that was just read */
		showframe( maxframe++ );
		framenumber++;
	}
done:
	while(passes-- > 0)  {
		if( !rocking )  {
			/* Play from start to finish, over and over */
			for( i=0; i<maxframe; i++ )  {
				showframe(i);
				select( fileno(stdin)+1, &readfds, 
					(fd_set *)0, (fd_set *)0, &tv );
			}
		} else {
			/* Play from start to finish and back */
			for( i=0; i<maxframe; i++ )  {
				showframe(i);
				select( fileno(stdin)+1, &readfds, 
					(fd_set *)0, (fd_set *)0, &tv );
			}
			while(i-->0)  {
				showframe(i);
				select( fileno(stdin)+1, &readfds, 
					(fd_set *)0, (fd_set *)0, &tv );
			}
		}
	}
	fb_close( fbp );

	fprintf(stderr,"\n");
	exit(0);
}

void
showframe(i)
register int i;
{
	if( verbose )  {
		fprintf(stderr, " %d", i);
		fflush( stderr );
	}

	if( i < 0 || i > maxframe )  {
		fprintf(stderr,"pixflip-fb:  %d out of range\n", i);
		return;
	}

	fb_writerect( fbp, 0, 0, file_width, file_height, frames[i] );

	if( verbose )  {
		fprintf(stderr, ",");
		fflush( stderr );
	}
}
