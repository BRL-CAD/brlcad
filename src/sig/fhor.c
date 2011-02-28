/*                          F H O R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
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

#include <string.h>
#include <stdlib.h>
#include <math.h>		/* XXX - temp debug */
#include "bio.h"

#include "fb.h"

#define	MYMETHOD	on

#define	MAX(x, y)	(((x)>(y))?(x):(y))
#define	MIN(x, y)	(((x)<(y))?(x):(y))

#define	HSCREEN	1024	/* Max Horizontal screen resolution */
#define	VSCREEN	1024	/* Max Vertical screen resolution   */

#define	INVISIBLE	0
#define	ABOVE		1
#define	BELOW		-1

/* Max and Min horizon holders */
static	int	upper[HSCREEN], lower[HSCREEN];

FBIO	*fbp;	/* XXX - debug */

void	Efill(void);
void	Horizon(int sx1, int sy1, int sx2, int sy2);
void	Intersect(int sx1, int sy1, int x2, int y2, int *hor, int *xi, int *yi);
void	Draw(int sx1, int sy1, int x2, int y2);
int	fhvis(int x, int y);
int	sign(int i);

void
fhinit(void)
{
    int	i;

    /* Set initial horizons */
    for ( i = 0; i < HSCREEN; i++ ) {
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
    Xprev = Yi = Xi = y = x = 0;
    Yprev = f[ 0 ];
    /* VIEWING XFORM */

    /* Fill left side */
    Efill( );
/*	Previously = fhvis( x, y );		<<< WHAT ARE X AND Y? */
    Previously = fhvis( Xprev, Yprev );		/* <<< WHAT ARE X AND Y? */

    /* Do each point in Z plane */
    for ( x = 0; x < num; x++ ) {
	y = f[x];
	/* VIEWING XFORM */

	/* Check visibility and fill horizon */
	Currently = fhvis( x, y );
	if ( Currently == Previously ) {
	    if ( Currently != INVISIBLE ) {
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
	    switch ( Currently ) {
		case INVISIBLE:
		    if ( Previously == ABOVE )
			Intersect( Xprev, Yprev, x, y, upper, &Xi, &Yi );
		    else /* previously BELOW */
			Intersect( Xprev, Yprev, x, y, lower, &Xi, &Yi );
		    Draw( Xprev, Yprev, Xi, Yi );
		    Horizon( Xprev, Yprev, Xi, Yi );
		    break;

		case ABOVE:
		    if ( Previously == INVISIBLE ) {
			Intersect( Xprev, Yprev, x, y, lower, &Xi, &Yi );
			Draw( Xi, Yi, x, y );
			Horizon( Xi, Yi, x, y );
		    } else {
			/* previously BELOW */
			Intersect( Xprev, Yprev, x, y, lower, &Xi, &Yi );
			Draw( Xprev, Yprev, Xi, Yi );
			Horizon( Xprev, Yprev, Xi, Yi );
			Intersect( Xprev, Yprev, x, y, upper, &Xi, &Yi );
			Draw( Xi, Yi, x, y );
			Horizon( Xi, Yi, x, y );
		    }
		    break;

		case BELOW:
		    if ( Previously == INVISIBLE ) {
			Intersect( Xprev, Yprev, x, y, lower, &Xi, &Yi );
			Draw( Xi, Yi, x, y );
			Horizon( Xi, Yi, x, y );
		    } else {
			/* previously ABOVE */
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
    if ( y < upper[x] && y > lower[x] )
	return INVISIBLE;

    if ( y >= upper[x] )
	return ABOVE;

    return BELOW;
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
 * Fill the upper and lower horizon arrays from x_1 to x_2
 * with a line spanning (x_1, y_1) to (x_2, y_2).
 */
void
Horizon(int x_1, int y_1, int x_2, int y_2)
{
    int	xinc, x, y;
    double	slope;

    xinc = sign( x_2 - x_1 );
    if ( xinc == 0 ) {
	/* Vertical line */
	upper[x_2] = MAX( upper[x_2], y_2 );
	lower[x_2] = MIN( lower[x_2], y_2 );
    } else {
	slope = (y_2 - y_1) / (x_2 - x_1);
	for ( x = x_1; x <= x_2; x += xinc ) {
	    y = slope * (x - x_1) + y_1;
	    upper[x] = MAX( upper[x], y );
	    lower[x] = MIN( lower[x], y );
	}
    }
}

/*
 * Find the intersection (xi, yi) between the line (x_1, y_1)->(x_2, y_2)
 *  and the horizon hor[].
 */
void
Intersect(int x_1, int y_1, int x_2, int y_2, int *hor, int *xi, int *yi)
{
    int	xinc, ysign;
    int	slope;

/*
  printf("Intersect( (%3d,%3d)->(%3d,%3d) & (%3d,%3d)->(%3d,%3d) ) = ", x_1, y_1, x_2, y_2, x_1, hor[x_1], x_2, hor[x_2] );
  fflush( stdout );
*/
    xinc = sign( x_2 - x_1 );
    if ( xinc == 0 ) {
	/* Vertical line */
	*xi = x_2;
	*yi = hor[x_2];
/*printf("(vert x_2=%d) ", x_2);*/
    } else {
#ifdef FOOBARBAZ
	denom = (hor[x_2]-hor[x_1])-(y_2-y_1);
	if ( denom == 0 ) {
	    /* same line! */
	    *xi = x_1;
	} else
	    *xi = x_1 + ((x_2-x_1)*(hor[x_1]-y_1))/denom;
	*yi = y_1 + (*xi-x_1)*((y_2-y_1)/(x_2-x_1)) + 0.5;
/*printf("(%3d,%3d)\n", *xi, *yi );*/
	return;
#endif /* FOOBARBAZ */

	slope = (y_2 - y_1) / (x_2 - x_1);
	ysign = sign( y_1 - hor[x_1 + xinc] );
#ifdef MYMETHOD
	for ( *xi = x_1; *xi <= x_2; *xi += xinc ) {
	    *yi = y_1 + (*xi-x_1)*slope;	/* XXX */
	    if ( sign( *yi - hor[*xi + xinc] ) != ysign )
		break;
	}
	if ( xinc == 1 && *xi > x_2 ) *xi = x_2;
	if ( xinc == -1 && *xi < x_2 ) *xi = x_2;
#else
	*yi = y_1;
	*xi = x_1;
	while ( sign( *yi - hor[*xi + xinc] ) == ysign ) {
	    for ( *xi = x_1; *xi <= x_2; *xi += xinc )
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
    if ( i > 0 )
	return 1;
    else if ( i < 0 )
	return -1;
    else
	return 0;
}

/*
 * DRAW - plot a line from (x_1, y_1) to (x_2, y_2)
 *  An integer Bresenham algorithm for any quadrant.
 */
void
Draw(int x_1, int y_1, int x_2, int y_2)
{
    int	x, y, deltx, delty, error, i;
    int	temp, s1, s2, interchange;
    static	RGBpixel white = { 255, 255, 255 };	/* XXX - debug */

/*printf("Draw (%d %d) -> (%d %d)\n", x_1, y_1, x_2, y_2 );*/
    x = x_1;
    y = y_1;
    deltx = (x_2 > x_1 ? x_2 - x_1 : x_1 - x_2);
    delty = (y_2 > y_1 ? y_2 - y_1 : y_1 - y_2);
    s1 = sign(x_2 - x_1);
    s2 = sign(y_2 - y_1);

    /* check for swap of deltx and delty */
    if ( delty > deltx ) {
	temp = deltx;
	deltx = delty;
	delty = temp;
	interchange = 1;
    } else
	interchange = 0;

    /* init error term */
    error = 2 * delty - deltx;

    for ( i = 0; i < deltx; i++ ) {
/*		plotxy( x, y );*/
/*		printf( "(%3d,%3d)\n", x, y );*/
	fb_write( fbp, x, y, white, 1 );
	while ( error >= 0 ) {
	    if ( interchange == 1 )
		x += s1;
	    else
		y += s2;
	    error -= 2 * deltx;
	}
	if ( interchange == 1 )
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
    for ( z = 500; z > 0; z-- ) {
	/* Left to Right */
	for ( x = 0; x < 500; x++ ) {
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

static const char usage[] = "\
Usage: fhor [width] < doubles\n";

int main(int argc, char **argv)
{
    double	inbuf[512];
    int	f[512];
    int	i, x, z;
    int	size = 512;

    if ( argc > 1 ) {
	size = atoi( argv[1] );
    }

    if ( isatty(fileno(stdin)) ) {
	bu_exit(1, "%s", usage );
    }

    fhinit();

    fbp = fb_open( NULL, 0, 0 );
    fb_clear( fbp, PIXEL_NULL );

    memset((char *)f, 0, 512*sizeof(*f));
    fhnewz( f, 512 );

    /*
     *  Nearest to Farthest
     *  Here we reverse the sense of Z
     *  (it now goes into the screen).
     */
    z = 0;
    while ( fread( inbuf, sizeof(*inbuf), size, stdin ) != 0 ) {
	/* Left to Right */
	/*memset((char *)f, 0, 512*sizeof(*f));*/
	for ( i = 0; i < 512; i++ ) {
	    f[i] = 4*z;	/* up 4 for every z back */
	}
	for ( i = 0; i < size; i++ ) {
	    x = i + 2*z;	/* right 2 for every z back */
	    if ( x >= 0 && x < 512 ) {
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
