/*
 *			S U B R O U T I N E S . C
 *
 *  Author -
 *	S.Coates - 18 November 1991
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990-2004 by the United States Army.
 *	All rights reserved.
 */

/*	18 November 1991 - Put /n/walrus/usr/brlcad/include into includes.  */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "externs.h"

#if !defined(PI)
#define PI 3.14159265358979323846264		/*  Pi.  */
#endif


/*  Subroutine to rotate a point, given a point (3 coordinates)  */
/*  and three angles (in radians).  */

void rotate( p, a, np )

double p[3];
double a[3];
double np[3];

{

	/*  p[3]  - The point brought in where p[0] is the x-coordinate,  */
	/*	    p[1] is the y-coordinate, and p[2] is the z-  */
	/*	    coordinate.  */
	/*  a[3]  - The angle (in degrees) for rotation brought in,  */
	/*	    where a[0] is the rotation about the x-axis,  */
	/*	    a[1] is the rotation about the y-axis, and a[2]  */
	/*	    is the rotation about the z-axis.  */
	/*  np[3] - The rotated point that is passed back, where np[0]  */
	/*	    is the x-coordinate, np[1] is the y-coordinate,  */
	/*	    and np[2] is the z-coordinate.  */

	double sa[3],ca[3];	/*  Sine and cosine of each angle.  */

	/*  Find sine and cosine of each angle.  */
	sa[0] = sin(a[0]);
	sa[1] = sin(a[1]);
	sa[2] = sin(a[2]);

	ca[0] = cos(a[0]);
	ca[1] = cos(a[1]);
	ca[2] = cos(a[2]);

	/*  Do rotation.  The rotation is as follows.  */
	/*      R[z] * R[y] * R[x] * P  */
	np[0] = p[0]         * ca[1] * ca[2]
	      + p[1] * sa[0] * sa[1] * ca[2]
	      + p[2] * ca[0] * sa[1] * ca[2]
	      - p[1] * ca[0]         * sa[2]
	      + p[2] * sa[0]         * sa[2];
	np[1] = p[0]         * ca[1] * sa[2]
	      + p[1] * sa[0] * sa[1] * sa[2]
	      + p[2] * ca[0] * sa[1] * sa[2]
	      + p[1] * ca[0]         * ca[2]
	      - p[2] * sa[0]         * ca[2];
	np[2] = (-p[0])         * sa[1]
	      +   p[1]  * sa[0] * ca[1]
	      +   p[2]  * ca[0] * ca[1];

/*
 *	(void)printf("End of subroutine\n");
 *	(void)fflush(stdout);
 */

	return;

}



/*  Subroutine to receive an angle in degrees and return  */
/*  the angle in radians.  */

double radians(a)
double a;
{

	/*  a - Angle in degrees.  */

	double b;	/*  Angle in radians.  */

	b = a * PI / 180.;

	return(b);

}
