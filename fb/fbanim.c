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
#include <time.h>		/* for struct timeval */
struct timeval tv;
#endif

int pix_line;		/* Number of pixels/line */
int zoom;		/* Zoom Factor.			*/
int xPan, yPan;		/* Pan Location.		*/
int xoff, yoff;		/* Ikonas farbling */
int verbose = 0;
int rocking = 0;

int w, n;
int im_line;
int fps;			/* frames/sec */
int passes = 100;		/* limit on number of passes */

char Usage[] = "Usage: fbanim [-h] [-v] [-r] [-p#pass] width nframes [fps]\n";

main(argc, argv )
char **argv;
{
	register int i;

	if( argc < 3 )  {
		printf(Usage);
		exit(12);
	}

	pix_line = 512;
	while( argv[1][0] == '-' )  {
		switch( argv[1][1] )  {
		case 'h':
			pix_line = 1024;
			break;
		case 'p':
			passes = atoi(&argv[1][2]);
			if(passes<1)  passes=1;
			break;
		case 'r':
			rocking = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			printf(Usage);
			exit(12);
		}
		argc--;
		argv++;
	}

	w = atoi(argv[1]);
	if( w < 4 || w > 512 )  {
		printf("w=%d out of range\n");
		exit(12);
	}
	n = atoi(argv[2]);
	if( argc == 4 )
		fps = atoi(argv[3]);
	else
		fps = 8;
#ifdef BSD
	if( fps <= 1 )  {
		tv.tv_sec = fps ? 1 : 4;
		tv.tv_usec = 0;
	} else {
		tv.tv_sec = 0;
		tv.tv_usec = 1000000/fps;
	}
#endif
	if( pix_line > 512 )
		fbsetsize(pix_line);

	if( fbopen( NULL, APPEND ) < 0 )  {
		fprintf(stderr,"fbopen failed\n");
		exit(12);
	}

	zoom = pix_line/w;
	im_line = pix_line/w;	/* number of images across line */
	xPan = yPan = 0;

	fbzoom( pix_line==w? 0 : pix_line/w,
		pix_line==w? 0 : pix_line/w );

	while(passes-- > 0)  {
		if( !rocking )  {
			/* Play from start to finish, over and over */
			for( i=0; i<n; i++ )
				newframe(i);
		} else {
			/* Play from start to finish and back */
			for( i=0; i<n; i++ )
				newframe(i);
			while(i-->0)
				newframe(i);
		}
	}
}

newframe(i)
register int i;
{
	xPan = (i%im_line)*w+w/2;
	yPan = (i/im_line)*w+w/2;
	if( verbose )  {
		printf("%3d: %3d %3d\n", i, xPan, yPan);
		fflush( stdout );
	}
	fbwindow( xPan, yPan );
#ifdef BSD
	(void)select( 0, 0, 0, 0, &tv );
#endif
#ifdef SYSV
	(void)sleep(1);	/* best I can do, sorry */
#endif
}
