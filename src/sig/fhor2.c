/*                         F H O R 2 . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file fhor2.c
 * Floating horizon 3D plotting routines.
 *  This is a super simple version where the data is
 *  given at the same resolution as the screen.
 *  No lines need to be drawn, nor intersection computed!
 */
#include "common.h"



#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <math.h>		/* XXX - temp debug */
#include "machine.h"
#include "fb.h"

#define	MYMETHOD	on

#define	MAX(x,y)	(((x)>(y))?(x):(y))
#define	MIN(x,y)	(((x)<(y))?(x):(y))

#define	HSCREEN	1024	/* Max Horizontal screen resolution */
#define	VSCREEN	1024	/* Max Vertical screen resolution   */

#define	INVISIBLE	0
#define	ABOVE		1
#define	BELOW		-1

/* Max and Min horizon holders */
static	int	upper[HSCREEN], lower[HSCREEN];
static	int	Xleft, Yleft, Xright, Yright;		/* XXX */

FBIO	*fbp;	/* XXX - debug */

fhinit(void)
{
	int	i;

	Xleft = Yleft = Xright = Yright = -1;

	/* Set initial horizons */
	for( i = 0; i < HSCREEN; i++ ) {
		upper[ i ] = 0;
		lower[ i ] = VSCREEN;
	}
}

/*
 * Add another Z cut to the display.
 *  This one goes "behind" the last one.
 */
fhnewz(int *f, int num)
{
	int	x, y, Xprev, Yprev, Xi, Yi;

	/* Do each point in Z plane */
	for( x = 0; x < num; x++ ) {
		y = f[x];
		/* VIEWING XFORM */

		/* Check visibility and fill horizon */
		if( fhvis( x, y ) ) {
			/*
			 * Current and Previous point both
			 *  visible on same side of horizon.
			 */
			Point( x, y );
			Horizon( x, y );
		}
	}
}

/*
 * INTERNAL Visibility routine.
 *  Answers, Is Y visible at point X?
 *
 * Returns: 0 if invisible
 *	    1 if visible above upper horizon.
 *	   -1 if visible below lower horizon.
 */
fhvis(int x, int y)
{
	/* See if hidden behind horizons */
	if( y < upper[x] && y > lower[x] )
		return( INVISIBLE );

	if( y >= upper[x] )
		return( ABOVE );

	return( BELOW );
}

/*
 * INTERNAL Edge fill routine.
 * NOT DONE YET.
 */
Efill(void)
{
}

/*
 * Fill the upper and lower horizon arrays
 */
Horizon(int x, int y)
{
	/* Vertical line */
	upper[x] = MAX( upper[x], y );
	lower[x] = MIN( lower[x], y );
}

sign(int i)
{
	if( i > 0 )
		return( 1 );
	else if( i < 0 )
		return( -1 );
	else
		return( 0 );
}

Point(int x, int y)
{
	static	RGBpixel white = { 255, 255, 255 };
	fb_write( fbp, x, y, white, 1 );
}

/*
 * DRAW - plot a line from (x1,y1) to (x2,y2)
 *  An integer Bresenham algorithm for any quadrant.
 */
Draw(int x1, int y1, int x2, int y2)
{
	int	x, y, deltx, delty, error, i;
	int	temp, s1, s2, interchange;
	static	RGBpixel white = { 255, 255, 255 };	/* XXX - debug */

/*printf("Draw (%d %d) -> (%d %d)\n", x1, y1, x2, y2 );*/
	x = x1;
	y = y1;
	deltx = (x2 > x1 ? x2 - x1 : x1 - x2);
	delty = (y2 > y1 ? y2 - y1 : y1 - y2);
	s1 = sign(x2 - x1);
	s2 = sign(y2 - y1);

	/* check for swap of deltx and delty */
	if( delty > deltx ) {
		temp = deltx;
		deltx = delty;
		delty = temp;
		interchange = 1;
	} else
		interchange = 0;

	/* init error term */
	error = 2 * delty - deltx;

	for( i = 0; i < deltx; i++ ) {
/*		plotxy( x, y );*/
/*		printf( "(%3d,%3d)\n", x, y );*/
		fb_write( fbp, x, y, white, 1 );
		while( error >= 0 ) {
			if( interchange == 1 )
				x += s1;
			else
				y += s2;
			error -= 2 * deltx;
		}
		if( interchange == 1 )
			y += s2;
		else
			x += s1;
		error += 2 * delty;
	}
}

#ifdef SOMBRERO
main()
{
	int	f[500];
	int	x, y, z;
	double	r;

	fhinit();

	fbp = fb_open( NULL, 512, 512 );
	/*fb_clear( fbp, PIXEL_NULL );*/

	/* Nearest to Farthest */
	for( z = 500; z > 0; z-- ) {
		/* Left to Right */
		for( x = 0; x < 500; x++ ) {
			r = (x - 250) * (x - 250) + (z - 250) * (z - 250);
			r = 0.10*sqrt( r ) + 0.00001;
			y = 250.0 * sin( r ) / r + 100.0 + (500-z)/3;
			f[x] = y;
/*			printf( "f[%3d] = %d\n", x, y );*/
		}
		fhnewz( f, 500 );
	}
}
#endif

static char usage[] = "\
Usage: fhor [width] < doubles\n";

int main(int argc, char **argv)
{
	double	inbuf[512];
	int	f[512];
	int	i, x, y, z;

	if( isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	fhinit();

	fbp = fb_open( NULL, 0, 0 );
	/*fb_clear( fbp, PIXEL_NULL );*/

	bzero( f, 512*sizeof(*f) );
	fhnewz( f, 512 );

	/*
	 *  Nearest to Farthest
	 *  Here we reverse the sense of Z
	 *  (it now goes into the screen).
	 */
	z = 0;
	while( fread( inbuf, sizeof(*inbuf), 512, stdin ) > 0 ) {
		/* Left to Right */
		/*bzero( f, 512*sizeof(*f) );*/
		for( i = 0; i < 512; i++ ) {
			f[i] = 4*z;	/* up 4 for every z back */
		}
		for( i = 0; i < 512; i++ ) {
			x = i + 2*z;	/* right 2 for every z back */
			if( x >= 0 && x < 512 ) {
				f[x] += 128 * inbuf[i];
			}
			/*printf( "f[%3d] = %d\n", x, y );*/
		}
		fhnewz( f, 512 );
		z++;
	}

	return 0;
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
