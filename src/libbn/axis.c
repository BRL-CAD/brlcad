/*                          A X I S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup axis */
/** @{ */
/** @file axis.c
 *
 * @brief
 *	This routine is used to generate an axis for a graph.
 * It draws an axis with a linear scale, places tic marks every inch,
 * labels the tics, and uses the supplied title for the axis.
 *
 *	The strategy behind this routine is to split the axis
 * into SEGMENTS, which run from one tick to the next.  The
 * origin of the first segment (x,y), the origin of the bottom
 * of the first tick (xbott,ybott), and the origin of the first
 * tick label (xnum,ynum) are computed along with the delta x
 * and delta y (xincr,yincr) which describes the interval to
 * the start of the next tick.
 *
 *  @author
 *	Michael John Muuss
 *@n	August 01, 1978
 *
 *  @par Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *@n	The U. S. Army Ballistic Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/** @} */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "plot3.h"

#define	TICK_YLEN	(char_width)	/**< @brief tick is 1 character height */
#define	NUM_YOFF	(3*char_width)	/**< @brief numbers offset from line */
#define	TITLE_YOFF	(5*char_width)	/**< @brief title offset from line */

void
/**
 *
 */
tp_3axis(FILE *fp,
	 char *string,		/**< label for axis */
	 fastf_t *origin,
	 fastf_t *rot,
	 double length,		/**< length of axis */
	 int ccw,    		/**< 0=clockwise, !0=counter clockwise (ccw) */
	 int ndigits,		/**< # digits wide */
	 double label_start,	/**< label starting value */
	 double label_incr,	/**< label increment between ticks */
	 double tick_separation,/**< plot distance between ticks */
	 double char_width)	/**< character scale (size) */
{
	register int i;
	int	nticks;
	point_t	tick_bottom;			/* -Y point of tick */
	vect_t	axis_incr;			/* +X vect between ticks */
	vect_t	axis_dir;
	point_t	title_left;			/* left edge of title */
	point_t	cur_point;
	point_t	num_start;
	point_t	num_center;			/* center point of number */
	point_t	num_last_end;			/* end of last number */
	vect_t	temp;
	vect_t	diff;
	mat_t	xlate_to_0;
	mat_t	mat;				/* combined transform */
	char	fmt[32];
	char	str[64];

	/* Determine direction for ticks */
	if( ccw )
		ccw = -1;			/* counter clockwise */
	else
		ccw = 1;			/* clockwise */

	if( NEAR_ZERO(tick_separation, SMALL) )  tick_separation = 1;

	/*
	 *  The point "origin" will be the center of the axis rotation.
	 *  On the assumption that this origin point is not at (0,0,0),
	 *  translate origin to (0,0,0) & apply the provided rotation matrix.
	 *  If the user provides translation or
	 *  scaling in his matrix, it will also be applied, but in most
	 *  cases that would not be useful.
	 */
	MAT_IDN( xlate_to_0 );
	MAT_DELTAS_VEC( xlate_to_0, origin);
	bn_mat_mul( mat, rot, xlate_to_0 );
	VMOVE( cur_point, origin );

	/* Compute the bottom of the first tick */
	VSET( temp, 0, -TICK_YLEN * ccw, 0 );
	MAT4X3PNT( tick_bottom, mat, temp );

	/* Compute the start of this tick's label */
	VSET( temp, 0, -NUM_YOFF * ccw, 0 );
	MAT4X3PNT( num_center, mat, temp );
	temp[X] = -char_width*ndigits;
	MAT4X3PNT( num_last_end, mat, temp );

	/* Determine the increment between ticks */
	VSET( temp, 1, 0, 0 );
	MAT4X3VEC( axis_dir, mat, temp );
	VSCALE( axis_incr, axis_dir, tick_separation );

	/* Center the title, and find left edge */
	VSET( temp, 0.5*(length - strlen(string)*char_width), -TITLE_YOFF*ccw, 0 );
	MAT4X3PNT( title_left, mat, temp );
	tp_3symbol(fp, string, title_left, rot, char_width );

	nticks = length/tick_separation+0.5;
	pdv_3move( fp, cur_point );
	for( i=0; i<=nticks; i++) {
		/*
		 *  First, draw a tick.
		 *  Then, if room, draw a numeric label.
		 *  If last tick, done.
		 *  Otherwise, advance in axis_dir direction.
		 */
		pdv_3cont( fp, tick_bottom );

		if( ndigits > 0 )  {
			double f;
			sprintf( fmt, "%%%dg", ndigits);
			sprintf( str, fmt, label_start );
			f = strlen(str) * char_width * 0.5;
			VJOIN1( num_start, num_center, -f, axis_dir );

			/* Only label this tick if the number will not
			 * overlap with the previous number.
			 */
			VSUB2( diff, num_start, num_last_end );
			if( VDOT( diff, axis_dir ) >= 0 )  {
				tp_3symbol( fp, str, num_start, rot, char_width );
				VJOIN1( num_last_end, num_center, f, axis_dir );
			}
		}

		if( i == nticks )  break;

		/* Advance, and draw next axis segment */
		pdv_3move( fp, cur_point );
		VADD2( cur_point, cur_point, axis_incr );
		VADD2( tick_bottom, tick_bottom, axis_incr );
		VADD2( num_center, num_center, axis_incr );

		label_start += label_incr;

		pdv_3cont( fp, cur_point);		/* draw axis */
	}
}

void
PL_FORTRAN(f3axis, F3AXIS)(fp, string, x, y, z, length, theta, ccw,
	ndigits, label_start, label_incr, tick_separation, char_width )
FILE		**fp;
char		*string;	/* label for axis */
float		*x,*y,*z;		/* start coordinates for axis */
float		*length;	/* length of axis */
float		*theta;		/* rotation off X-axis, in degrees */
int		*ccw;
int		*ndigits;	/* # digits wide */
float		*label_start;	/* minimum value on axis */
float		*label_incr;		/* increment for each tick */
float		*tick_separation;		/* distance between ticks */
float		*char_width;	/* character scale (size) */
{
	char buf[128];
	mat_t	mat;
	vect_t	pnt;

	VSET( pnt, *x, *y, *z );
	MAT_IDN(mat);
	bn_mat_angles( mat, 0.0, 0.0, *theta );
	strncpy( buf, string, sizeof(buf)-1 );
	buf[sizeof(buf)-1] = '\0';
	tp_3axis( *fp, buf, pnt, mat, *length, *ccw,
		*ndigits, *label_start, *label_incr,
		*tick_separation, *char_width );
}
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
