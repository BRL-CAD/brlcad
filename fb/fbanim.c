/*
 *			F B A N I M . C
 *
 * Function -
 *	Dynamicly modify framebuffer Zoom and Window parameters,
 *	to flip betwen sub-images, giving an inexpensive animation
 *	effect.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "fb.h"

#ifdef BSD
#include <sys/time.h>		/* for struct timeval */
struct timeval tv;
#endif

extern int	getopt();
extern char	*optarg;
extern int	optind;

void		newframe();

FBIO *fbp;
int pix_line;		/* Number of pixels/line in frame buffer */
int zoom;		/* Zoom Factor */
int xPan, yPan;		/* Pan Location */
int xoff, yoff;		/* Ikonas farbling */
int verbose = 0;
int rocking = 0;

int w;				/* image width */
int n;				/* image height */
int nframes;			/* number of frames */
int im_line;
int fps;			/* frames/sec */
int passes = 100;		/* limit on number of passes */
int inverse;			/* for old 4th quadrant sequences */

char Usage[] = "\
Usage: fbanim [-h -i -r -v] [-s squaresize] [-w width] [-n height]\n\
        [-p passes] width nframes [fps]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "s:w:n:hirvp:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			pix_line = 1024;
			break;
		case 's':
			w = n = atoi(optarg);
			break;
		case 'w':
			w = atoi(optarg);
			break;
		case 'n':
			n = atoi(optarg);
			break;
		case 'i':
			inverse = 1;
			break;
		case 'p':
			passes = atoi(optarg);
			if(passes<1)  passes=1;
			break;
		case 'r':
			rocking = 1;
			break;
		case 'v':
			verbose = 1;
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind+1 >= argc )	/* two mandatory positional args */
		return(0);
	return(1);		/* OK */
}

main(argc, argv )
char **argv;
{
	register int i;

	pix_line = 512;
	if ( !get_args( argc, argv ) )  {
		(void)fputs(Usage, stderr);
		exit( 1 );
	}

	w = atoi(argv[optind]);
	if( w == 0 || n == 0 ) {
		fprintf(stderr,"fbanim: must specify image size\n");
		exit( 2 );
	}
	nframes = atoi(argv[optind+1]);
	if( optind+2 >= argc )
		fps = 8;
	else
		fps = atoi(argv[optind+2]);

#ifdef BSD
	if( fps <= 1 )  {
		tv.tv_sec = fps ? 1 : 4;
		tv.tv_usec = 0;
	} else {
		tv.tv_sec = 0;
		tv.tv_usec = 1000000/fps;
	}
#endif

	if( (fbp = fb_open( NULL, pix_line, pix_line )) == NULL )  {
		fprintf(stderr,"fb_open failed\n");
		exit(12);
	}

	zoom = pix_line/w;
	im_line = pix_line/w;	/* number of images across line */
	xPan = yPan = 0;

	fb_zoom( fbp, pix_line/w, pix_line/n );

	while(passes-- > 0)  {
		if( !rocking )  {
			/* Play from start to finish, over and over */
			for( i=0; i<nframes; i++ )
				newframe(i);
		} else {
			/* Play from start to finish and back */
			for( i=0; i<nframes; i++ )
				newframe(i);
			while(i-->0)
				newframe(i);
		}
	}
	fb_close( fbp );
}

void
newframe(i)
register int i;
{
	xPan = (i%im_line)*w+w/2;
	yPan = (i/im_line)*n+n/2;
	if( inverse )
		yPan = pix_line - yPan;
	if( verbose )  {
		printf("%3d: %3d %3d\n", i, xPan, yPan);
		fflush( stdout );
	}
	fb_window( fbp, xPan, yPan );
#ifdef BSD
	(void)select( 0, 0, 0, 0, &tv );
#endif
#ifdef SYSV
	(void)sleep(1);	/* best I can do, sorry */
#endif
}
