/*                        F B A N I M . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
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
 *
 */
/** @file fbanim.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <sys/time.h>		/* For struct timeval */

#include "machine.h"
#include "bu.h"
#include "fb.h"


int		sec;
int		usec;

void		newframe(register int i);

FBIO	*fbp;
int	screen_width;		/* Number of pixels/line in frame buffer */
int	screen_height;
int	verbose = 0;
int	rocking = 0;

int	subimage_width;		/* subimage width */
int	subimage_height;		/* subimage height */
int nframes;			/* number of frames */
int im_line;			/* Number of images across the screen */
int fps;			/* frames/sec */
int passes = 100;		/* limit on number of passes */
int inverse;			/* for old 4th quadrant sequences */

char Usage[] = "\
Usage: fbanim [-h -i -r -v] [-p passes]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height]\n\
	[-s square_subimage_size] [-w subimage_width] [-n subimage_height]\n\
	subimage_width nframes [fps]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "s:w:n:hirvp:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 's':
			subimage_width = subimage_height = atoi(bu_optarg);
			break;
		case 'w':
			subimage_width = atoi(bu_optarg);
			break;
		case 'n':
			subimage_height = atoi(bu_optarg);
			break;
		case 'i':
			inverse = 1;
			break;
		case 'p':
			passes = atoi(bu_optarg);
			if(passes<1)  passes=1;
			break;
		case 'r':
			rocking = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
			/* high-res screen */
			screen_width = screen_height = 1024;
			break;
		case 'S':
			screen_height = screen_width = atoi(bu_optarg);
			break;
		case 'W':
			screen_width = atoi(bu_optarg);
			break;
		case 'N':
			screen_height = atoi(bu_optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind+1 >= argc )	/* two mandatory positional args */
		return(0);
	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	register int i;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(Usage, stderr);
		exit( 1 );
	}

	/* If not given with -s & -n, use (old) positional param (compat) */
	if( subimage_width <= 0 || subimage_height <= 0 )  {
		subimage_width = subimage_height = atoi(argv[bu_optind]);
		if( subimage_width == 0 ) {
			fprintf(stderr,"fbanim: must specify image size\n");
			exit( 2 );
		}
	}
	nframes = atoi(argv[bu_optind+1]);
	if( bu_optind+2 >= argc )
		fps = 8;
	else
		fps = atoi(argv[bu_optind+2]);

	if( fps <= 1 )  {
		sec = fps ? 1 : 4;
		usec = 0;
	} else {
		sec = 0;
		usec = 1000000/fps;
	}

	if( (fbp = fb_open( NULL, screen_width, screen_height )) == NULL )  {
		fprintf(stderr,"fbanim: fb_open failed\n");
		exit(12);
	}
	screen_width = fb_getwidth(fbp);
	screen_height = fb_getheight(fbp);

	im_line = screen_width/subimage_width;	/* number of images across line */

	fb_zoom( fbp, screen_width/subimage_width, screen_height/subimage_height );

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
	return(0);
}

void
newframe(register int i)
{
	register int	xPan, yPan;		/* Pan Location */
	struct timeval tv;
	fd_set fds;

	xPan = (i%im_line)*subimage_width+subimage_width/2;
	yPan = (i/im_line)*subimage_height+subimage_height/2;
	if( inverse )
		yPan = screen_width - yPan;
	if( verbose )  {
		printf("%3d: %3d %3d\n", i, xPan, yPan);
		fflush( stdout );
	}
	fb_window( fbp, xPan, yPan );

	FD_ZERO(&fds);
	FD_SET(fileno(stdin), &fds);

	tv.tv_sec = sec;
	tv.tv_usec = usec;

	select(fileno(stdin)+1, &fds, (fd_set *)0, (fd_set *)0, &tv);
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
