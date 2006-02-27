/*                          C L I P . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file clip.c
 *
 *  Functions -
 *	clip	clip a 2-D integer line seg against the size of the display
 *	vclip	clip a 3-D floating line segment against a bounding RPP.
 *
 *  Authors -
 *	clip() was written by Doug Kingston, 14 October 81
 *	Based on the clipping routine in "Principles of Computer
 *	Graphics" by Newman and Sproull, 1973, McGraw/Hill.
 *
 *	vclip() was adapted from RT by Mike Muuss, 17 January 1985.
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

#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "dm.h"


static int	code();

int
clip (fastf_t *xp1, fastf_t *yp1, fastf_t *xp2, fastf_t *yp2)
{
	char code1, code2;

	code1 = code (*xp1, *yp1);
	code2 = code (*xp2, *yp2);

	while (code1 || code2) {
		if (code1 & code2)
			return (-1);	/* No part is visible */

		/* SWAP codes, X's, and Y's */
		if (code1 == 0) {
			char ctemp;
			fastf_t temp;

			ctemp = code1;
			code1 = code2;
			code2 = ctemp;

			temp = *xp1;
			*xp1 = *xp2;
			*xp2 = temp;

			temp = *yp1;
			*yp1 = *yp2;
			*yp2 = temp;
		}

		if (code1 & 01)  {	/* Push toward left edge */
			*yp1 = *yp1 + (*yp2-*yp1)*(GED_MIN-*xp1)/(*xp2-*xp1);
			*xp1 = GED_MIN;
		}
		else if (code1 & 02)  {	/* Push toward right edge */
			*yp1 = *yp1 + (*yp2-*yp1)*(GED_MAX-*xp1)/(*xp2-*xp1);
			*xp1 = GED_MAX;
		}
		else if (code1 & 04)  {	/* Push toward bottom edge */
			*xp1 = *xp1 + (*xp2-*xp1)*(GED_MIN-*yp1)/(*yp2-*yp1);
			*yp1 = GED_MIN;
		}
		else if (code1 & 010)  {	/* Push toward top edge */
			*xp1 = *xp1 + (*xp2-*xp1)*(GED_MAX-*yp1)/(*yp2-*yp1);
			*yp1 = GED_MAX;
		}

		code1 = code (*xp1, *yp1);
	}

	return (0);
}

static int
code (fastf_t x, fastf_t y)
{
	int cval;

	cval = 0;
	if (x < GED_MIN)
		cval |= 01;
	else if (x > GED_MAX)
		cval |= 02;

	if (y < GED_MIN)
		cval |= 04;
	else if (y > GED_MAX)
		cval |= 010;

	return (cval);
}

/* XXX need to test more thoroughly
#define ANGLE_EPSILON 0.0001
#define CLIP_DISTANCE 1000000000.0
*/
#define EPSILON 0.0001
#define INFINITY 100000000.0

/*
 *			V C L I P
 *
 *  Clip a ray against a rectangular parallelpiped (RPP)
 *  that has faces parallel to the coordinate planes (a clipping RPP).
 *  The RPP is defined by a minimum point and a maximum point.
 *
 *  Returns -
 *	 0  if ray does not hit RPP,
 *	!0  if ray hits RPP.
 *
 *  Implicit Return -
 *	if !0 was returned, "a" and "b" have been clipped to the RPP.
 */
int vclip( vect_t a, vect_t b, register fastf_t *min, register fastf_t *max )
{
	static vect_t diff;
	static double sv;
	static double st;
	static double mindist, maxdist;
	register fastf_t *pt = &a[0];
	register fastf_t *dir = &diff[0];
	register int i;

	mindist = -INFINITY;
	maxdist = INFINITY;
	VSUB2( diff, b, a );

	for( i=0; i < 3; i++, pt++, dir++, max++, min++ )  {
		if( *dir < -EPSILON )  {
			if( (sv = (*min - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(maxdist > sv)
				maxdist = sv;
			if( mindist < (st = (*max - *pt) / *dir) )
				mindist = st;
		}  else if( *dir > EPSILON )  {
			if( (st = (*max - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(maxdist > st)
				maxdist = st;
			if( mindist < ((sv = (*min - *pt) / *dir)) )
				mindist = sv;
		}  else  {
			/*
			 *  If direction component along this axis is NEAR 0,
			 *  (ie, this ray is aligned with this axis),
			 *  merely check against the boundaries.
			 */
			if( (*min > *pt) || (*max < *pt) )
				return(0);	/* MISS */;
		}
	}
	if( mindist >= maxdist )
		return(0);	/* MISS */

	if( mindist > 1 || maxdist < 0 )
		return(0);	/* MISS */

	if( mindist <= 0 && maxdist >= 1 )
		return(1);	/* HIT, no clipping needed */

	/* Don't grow one end of a contained segment */
	if( mindist < 0 )
		mindist = 0;
	if( maxdist > 1 )
		maxdist = 1;

	/* Compute actual intercept points */
	VJOIN1( b, a, maxdist, diff );		/* b must go first */
	VJOIN1( a, a, mindist, diff );
	return(1);		/* HIT */
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
