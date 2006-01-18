/*                          F H O R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file fhor.c
 * Floating horizon 3D plotting routines.
 *
 * The terminology throughout is X across, Y up, Z toward you.
 *
 * Ref: "Procedural Elements for Computer Graphics,"
 *       D. F. Rogers.
 */
#include "common.h"



#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdlib.h>
#include <unistd.h>
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

FBIO	*fbp;	/* XXX - debug */

void	Efill(void);
void	Horizon(int x1, int y1, int x2, int y2);
void	Intersect(int x1, int y1, int x2, int y2, int *hor, int *xi, int *yi);
void	Draw(int x1, int y1, int x2, int y2);
int	fhvis(int x, int y);
int	sign(int i);

void
fhinit(void)
{
	int	i;

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
void
fhnewz(int *f, int num)
{
	int	x, y, Xprev, Yprev, Xi, Yi;
	int	Previously, Currently;

	/* Init previous X and Y values */
	Xprev = 0;
	Yprev = f[ 0 ];
	/* VIEWING XFORM */

	/* Fill left side */
	Efill( );
/*	Previously = fhvis( x, y );		<<< WHAT ARE X AND Y? */
	Previously = fhvis( Xprev, Yprev );		/* <<< WHAT ARE X AND Y? */

	/* Do each point in Z plane */
	for( x = 0; x < num; x++ ) {
		y = f[x];
		/* VIEWING XFORM */

		/* Check visibility and fill horizon */
		Currently = fhvis( x, y );
		if( Currently == Previously ) {
			if( Currently != INVISIBLE ) {
				/*
				 * Current and Previous point both
				 *  visible on same side of horizon.
				 */
				Draw( Xprev, Yprev, x, y );
				Horizon( Xprev, Yprev, x, y );
			}
			/* else both invisible */
		} else {
			/*
			 * Visibility has changed.
			 * Calculate intersection and fill horizon.
			 */
			switch( Currently ) {
			case INVISIBLE:
				if( Previously == ABOVE )
					Intersect( Xprev, Yprev, x, y, upper, &Xi, &Yi );
				else /* previously BELOW */
					Intersect( Xprev, Yprev, x, y, lower, &Xi, &Yi );
				Draw( Xprev, Yprev, Xi, Yi );
				Horizon( Xprev, Yprev, Xi, Yi );
				break;

			case ABOVE:
				if( Previously == INVISIBLE ) {
					Intersect( Xprev, Yprev, x, y, lower, &Xi, &Yi );
					Draw( Xi, Yi, x, y );
					Horizon( Xi, Yi, x, y );
				} else { /* previously BELOW */
					Intersect( Xprev, Yprev, x, y, lower, &Xi, &Yi );
					Draw( Xprev, Yprev, Xi, Yi );
					Horizon( Xprev, Yprev, Xi, Yi );
					Intersect( Xprev, Yprev, x, y, upper, &Xi, &Yi );
					Draw( Xi, Yi, x, y );
					Horizon( Xi, Yi, x, y );
				}
				break;

			case BELOW:
				if( Previously == INVISIBLE ) {
					Intersect( Xprev, Yprev, x, y, lower, &Xi, &Yi );
					Draw( Xi, Yi, x, y );
					Horizon( Xi, Yi, x, y );
				} else { /* previously ABOVE */
					Intersect( Xprev, Yprev, x, y, upper, &Xi, &Yi );
					Draw( Xprev, Yprev, Xi, Yi );
					Horizon( Xprev, Yprev, Xi, Yi );
					Intersect( Xprev, Yprev, x, y, lower, &Xi, &Yi );
					Draw( Xi, Yi, x, y );
					Horizon( Xi, Yi, x, y );
				}
				break;
			}
		} /* end changed visibility */

		/*
		 * Reset "previous" point values for next iteration.
		 */
		Previously = Currently;
		Xprev = x;
		Yprev = y;
	}

	/* Fill Right Side */
	Efill( );
}

/*
 * INTERNAL Visibility routine.
 *  Answers, Is Y visible at point X?
 *
 * Returns: 0 if invisible
 *	    1 if visible above upper horizon.
 *	   -1 if visible below lower horizon.
 */
int
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
void
Efill(void)
{
}

/*
 * Fill the upper and lower horizon arrays from x1 to x2
 *  with a line spanning (x1,y1) to (x2,y2).
 */
void
Horizon(int x1, int y1, int x2, int y2)
{
	int	xinc, x, y;
	double	slope;

	xinc = sign( x2 - x1 );
	if( xinc == 0 ) {
		/* Vertical line */
		upper[x2] = MAX( upper[x2], y2 );
		lower[x2] = MIN( lower[x2], y2 );
	} else {
		slope = (y2 - y1) / (x2 - x1);
		for( x = x1; x <= x2; x += xinc ) {
			y = slope * (x - x1) + y1;
			upper[x] = MAX( upper[x], y );
			lower[x] = MIN( lower[x], y );
		}
	}
}

/*
 * Find the intersection (xi,yi) between the line (x1,y1)->(x2,y2)
 *  and the horizon hor[].
 */
void
Intersect(int x1, int y1, int x2, int y2, int *hor, int *xi, int *yi)
{
	int	xinc, ysign;
	int	slope;

/*
printf("Intersect( (%3d,%3d)->(%3d,%3d) & (%3d,%3d)->(%3d,%3d) ) = ", x1, y1, x2, y2, x1, hor[x1], x2, hor[x2] );
fflush( stdout );
*/
	xinc = sign( x2 - x1 );
	if( xinc == 0 ) {
		/* Vertical line */
		*xi = x2;
		*yi = hor[x2];
/*printf("(vert x2=%d) ", x2);*/
	} else {
#ifdef FOOBARBAZ
		denom = (hor[x2]-hor[x1])-(y2-y1);
		if( denom == 0 ) {
			/* same line! */
			*xi = x1;
		} else
			*xi = x1 + ((x2-x1)*(hor[x1]-y1))/denom;
		*yi = y1 + (*xi-x1)*((y2-y1)/(x2-x1)) + 0.5;
/*printf("(%3d,%3d)\n", *xi, *yi );*/
		return;
#endif /* FOOBARBAZ */

		slope = (y2 - y1) / (x2 - x1);
		ysign = sign( y1 - hor[x1 + xinc] );
#ifdef MYMETHOD
		for( *xi = x1; *xi <= x2; *xi += xinc ) {
			*yi = y1 + (*xi-x1)*slope;	/* XXX */
			if( sign( *yi - hor[*xi + xinc] ) != ysign )
				break;
		}
		if( xinc == 1 && *xi > x2 ) *xi = x2;
		if( xinc == -1 && *xi < x2 ) *xi = x2;
#else
		*yi = y1;
		*xi = x1;
		while( sign( *yi - hor[*xi + xinc] ) == ysign ) {
			for( *xi = x1; *xi <= x2; *xi += xinc )
				*yi = *yi + slope;	/* XXX */
/*printf("[%3d,%3d]", *xi, *yi );*/
		}
		*xi = *xi + xinc;
#endif /* MYMETHOD */
	}
/*printf("(%3d,%3d)\n", *xi, *yi );*/
}

int
sign(int i)
{
	if( i > 0 )
		return( 1 );
	else if( i < 0 )
		return( -1 );
	else
		return( 0 );
}

/*
 * DRAW - plot a line from (x1,y1) to (x2,y2)
 *  An integer Bresenham algorithm for any quadrant.
 */
void
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
int main()
{
	int	f[500];
	int	x, y, z;
	double	r;

	fhinit();

	fbp = fb_open( NULL, 512, 512 );
	fb_clear( fbp, PIXEL_NULL );

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

	return 0;
}
#endif

static char usage[] = "\
Usage: fhor [width] < doubles\n";

int main(int argc, char **argv)
{
	double	inbuf[512];
	int	f[512];
	int	i, x, z;
	int	size = 512;

	if( argc > 1 ) {
		size = atoi( argv[1] );
	}

	if( isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	fhinit();

	fbp = fb_open( NULL, 0, 0 );
	fb_clear( fbp, PIXEL_NULL );

	bzero( (char *)f, 512*sizeof(*f) );
	fhnewz( f, 512 );

	/*
	 *  Nearest to Farthest
	 *  Here we reverse the sense of Z
	 *  (it now goes into the screen).
	 */
	z = 0;
	while( fread( inbuf, sizeof(*inbuf), size, stdin ) != 0 ) {
		/* Left to Right */
		/*bzero( (char *)f, 512*sizeof(*f) );*/
		for( i = 0; i < 512; i++ ) {
			f[i] = 4*z;	/* up 4 for every z back */
		}
		for( i = 0; i < size; i++ ) {
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
