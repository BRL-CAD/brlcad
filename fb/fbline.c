/*
 *			F B L I N E . C
 *
 *  Draw a single 2-D line on the framebuffer, in a given color
 *
 *  Author -
 *	Michael John Muuss
 *
 * Acknowledgment:
 * 	Based rather heavily on plot-fb.c
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include "machine.h"
#include "externs.h"			/* For getopt() */
#include "fb.h"

typedef int	bool;			/* boolean data type */
#define false	0
#define true	1

struct  coords {
	short		x;
	short		y;
}; 		/* Cartesian coordinates */

struct stroke {
	struct stroke	*next;		/* next in list, or NULL */
	struct coords	pixel;		/* starting scan, nib */
	short		xsign;		/* 0 or +1 */
	short		ysign;		/* -1, 0, or +1 */
	bool		ymajor; 	/* true iff Y is major dir. */
#undef major
#undef minor
	short		major;		/* major dir delta (nonneg) */
	short		minor;		/* minor dir delta (nonneg) */
	short		e;		/* DDA error accumulator */
	short		de;		/* increment for `e' */
}; 		/* rasterization descriptor */

static char	*framebuffer = NULL;
FBIO		*fbp;			/* Current framebuffer */

static int	screen_width = 512;	/* default input width */
static int	screen_height = 512;	/* default input height */
static int	clear = 0;

RGBpixel	pixcolor = { 255, 255, 255 };

static int	x1, y1, x2, y2;

void		edgelimit(), BuildStr(), Raster();

static char usage[] = "\
Usage: fbline [-h -c ] [-F framebuffer]\n\
	[-W screen_width] [-N screen_height]\n\
	[-r red] [-g green] [-b blue] x1 y1 x2 y2\n";

/*
 *			G E T_ A R G S
 */
int
get_args( argc, argv )
register char **argv;
{

	register int c;

	while ( (c = getopt( argc, argv, "hW:w:N:n:cF:r:g:b:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			screen_height = screen_width = 1024;
			break;
		case 'W':
		case 'w':
			screen_width = atoi(optarg);
			break;
		case 'N':
		case 'n':
			screen_height = atoi(optarg);
			break;
		case 'c':
			clear = 1;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 'r':
			pixcolor[RED] = atoi( optarg );
			break;
		case 'g':
			pixcolor[GRN] = atoi( optarg );
			break;
		case 'b':
			pixcolor[BLU] = atoi( optarg );
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( optind+4 > argc )
		return(0);		/* BAD */
	x1 = atoi( argv[optind++]);
	y1 = atoi( argv[optind++]);
	x2 = atoi( argv[optind++]);
	y2 = atoi( argv[optind++]);

	if ( argc > optind )
		(void)fprintf( stderr, "fbline: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

/*
 *			M A I N
 */
int
main(argc, argv)
int argc;
char **argv;
{
	struct coords	start, end;

	if ( !get_args( argc, argv ) ) {
		fputs( usage, stderr);
		exit(1);
	}

	if( (fbp = fb_open( framebuffer, screen_width, screen_height )) == NULL )
		exit(12);

	if( clear ) {
		fb_clear( fbp, PIXEL_NULL);
	}
	screen_width = fb_getwidth(fbp);
	screen_height = fb_getheight(fbp);

	start.x = x1;
	start.y = y1;
	end.x = x2;
	end.y = y2;
	edgelimit( &start );
	edgelimit( &end );
	BuildStr( &start, &end );	/* pixels */

	fb_close( fbp );
	exit(0);
}

/*
 *			E D G E L I M I T
 *
 *	Limit generated positions to edges of screen
 *	Really should clip to screen, instead.
 */
void
edgelimit( ppos )
register struct coords *ppos;
{
	if( ppos->x >= screen_width )
		ppos->x = screen_width -1;

	if( ppos->y >= screen_height )
		ppos->y = screen_height -1;
}

/*
 *			B U I L D S T R
 *
 *  set up DDA parameters
 */
void
BuildStr( pt1, pt2 )
struct coords		*pt1, *pt2;
{
	struct stroke cur_stroke;
	register struct stroke *vp = &cur_stroke;

	/* arrange for pt1 to have the smaller Y-coordinate: */
	if ( pt1->y > pt2->y )  {
		register struct coords *temp;	/* temporary for swap */

		temp = pt1;		/* swap pointers */
		pt1 = pt2;
		pt2 = temp;
	}

	/* set up DDA parameters for stroke */
	vp->pixel = *pt1;		/* initial pixel */
	vp->major = pt2->y - vp->pixel.y;	/* always nonnegative */
	vp->ysign = vp->major ? 1 : 0;
	vp->minor = pt2->x - vp->pixel.x;
	if ( (vp->xsign = vp->minor ? (vp->minor > 0 ? 1 : -1) : 0) < 0 )
		vp->minor = -vp->minor;

	/* if Y is not really major, correct the assignments */
	if ( !(vp->ymajor = vp->minor <= vp->major) )  {
		register short	temp;	/* temporary for swap */

		temp = vp->minor;
		vp->minor = vp->major;
		vp->major = temp;
	}

	vp->e = vp->major / 2 - vp->minor;	/* initial DDA error */
	vp->de = vp->major - vp->minor;

	Raster( vp );
}

/*
 *			R A S T E R
 *
 *	Raster - rasterize stroke. 
 *
 * Method:
 *	Modified Bresenham algorithm.  Guaranteed to mark a dot for
 *	a zero-length stroke.
 */
void
Raster( vp )
register struct stroke *vp;
{
	register short	dy;		/* raster within active band */

	/*
	 *  Write the color of this vector on all pixels.
	 */
	for ( dy = vp->pixel.y; dy < screen_height; )  {

		/* set the appropriate pixel in the buffer */
		fb_write( fbp, vp->pixel.x, dy, pixcolor, 1 );

		if ( vp->major-- == 0 )
			return;		/* Done */

		if ( vp->e < 0 )  {
			/* advance major & minor */
			dy += vp->ysign;
			vp->pixel.x += vp->xsign;
			vp->e += vp->de;
		}  else	{
			/* advance major only */
			if ( vp->ymajor )	/* Y is major dir */
				++dy;
			else			/* X is major dir */
				vp->pixel.x += vp->xsign;
			vp->e -= vp->minor;
		}
	}
	fprintf(stderr,"line left screen?\n");
}
