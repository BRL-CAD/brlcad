/*
 *		C L O U D T E X T . C
 *
 * An attempt at 2D Geoffrey Gardner style cloud texture map
 *
 *  Notes -
 *	Uses sin() table for speed.
 *
 *  Author -
 *	Philip Dykstra
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
#include <math.h>
#include "../h/machine.h"
#include "../h/vmath.h"

#define	PI	3.1415926535898
#define	TWOPI	6.283185307179
#define	NUMSINES	4

/*
 *	S I N E
 *
 * Table lookup sine function.
 */
double
sine(angle)
double angle;
{
#define	TABSIZE	512

	static	int	init = 0;
	static	double	table[TABSIZE];
	register int	i;

	if (init == 0) {
		for( i = 0; i < TABSIZE; i++ )
			table[i] = sin( TWOPI * i / TABSIZE );
		init++;
	}

	if (angle > 0)
		return( table[(int)((angle / TWOPI) * TABSIZE + 0.5) % TABSIZE] );
	else
		return( -table[(int)((-angle / TWOPI) * TABSIZE + 0.5) % TABSIZE] );
}

/*
 *	T E X T U R E
 *
 * Returns the texture value for a plane point
 */
double
texture(x,y,Contrast,initFx,initFy)
double x, y;
float Contrast, initFx, initFy;
{
	LOCAL float	t1, t2, k;
	LOCAL double	Px, Py, Fx, Fy, C;
	register int	i;

	t1 = t2 = 0;

	/*
	 * Compute initial Phases and Frequencies
	 * Freq "1" goes through 2Pi as x or y go thru 0.0 -> 1.0
	 */
	Fx = TWOPI * initFx;
	Fy = TWOPI * initFy;
	Px = PI * 0.5 * sine( 0.5 * Fy * y );
	Py = PI * 0.5 * sine( 0.5 * Fx * x );
	C = 1.0;	/* ??? */

	for( i = 0; i < NUMSINES; i++ ) {
		/*
		 * Compute one term of each summation.
		 */
		t1 += C * sine( Fx * x + Px ) + Contrast;
		t2 += C * sine( Fy * y + Py ) + Contrast;

		/*
		 * Compute the new phases and frequencies.
		 * N.B. The phases shouldn't vary the same way!
		 */
		Px = PI / 2.0 * sine( Fy * y );
		Py = PI / 2.0 * sine( Fx * x );
		Fx *= 2.0;
		Fy *= 2.0;
		C  *= 0.707;
	}

	/* Choose a magic k! */
	/* Compute max possible summation */
	k =  NUMSINES * 2 * NUMSINES;

	return( t1 * t2 / k );
}

/*
 *	S K Y C O L O R
 *
 * Return a sky color with translucency control.
 *  Threshold is the intensity below which it is completely translucent.
 *  Range in the range on intensities over which translucence varies
 *   from 0 to 1.
 */
long
skycolor( intensity, color, thresh, range )
double intensity;
vect_t color;
double thresh, range;
{
	FAST fastf_t	TR;

	/* Intensity is normalized - check bounds */
	if( intensity > 1.0 )
		intensity = 1.0;
	else if( intensity < 0.0 )
		intensity = 0.0;

	/* Compute Translucency Function */
	TR = 1.0 - ( intensity - thresh ) / range;
	if (TR < 0.0)
		TR = 0.0;
	else if (TR > 1.0)
		TR = 1.0;

	color[0] = ((1-TR) * intensity + (TR * .31));		/* Red */
	color[1] = ((1-TR) * intensity + (TR * .31));		/* Green */
	color[2] = ((1-TR) * intensity + (TR * .78));		/* Blue */
}
