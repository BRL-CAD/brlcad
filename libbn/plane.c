/*
 *			P L A N E . C
 *
 *  Some useful routines for dealing with planes.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited
 */
#ifndef lint
static char RCSplane[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"

/*
 *			R T _ M K _ P L A N E _ 3 P T S
 *
 *  Find the equation of a plane that contains three points.
 *  Note that normal vector created is expected to point out (see vmath.h),
 *  so C had better be counter-clockwise from B to follow the
 *  BRL-CAD outward-pointing normal convention.
 *
 *  XXX The tolerance here should be relative to the model diameter, not abs.
 *
 *  Returns -
 *	 0	OK
 *	-1	Failure.  At least two of the points were not distinct.
 */
int
rt_mk_plane_3pts( plane, a, b, c )
plane_t	plane;
point_t	a, b, c;
{
	vect_t	B_A;
	vect_t	C_A;
	vect_t	C_B;
	register fastf_t mag;

	VSUB2( B_A, b, a );
	if( VNEAR_ZERO( B_A, 0.005 ) )  return(-1);
	VSUB2( C_A, c, a );
	if( VNEAR_ZERO( C_A, 0.005 ) )  return(-1);
	VSUB2( C_B, c, b );
	if( VNEAR_ZERO( C_B, 0.005 ) )  return(-1);

	VCROSS( plane, B_A, C_A );

	/* Ensure unit length normal */
	if( (mag = MAGNITUDE(plane)) <= SQRT_SMALL_FASTF )
		return(-1);	/* FAIL */
	mag = 1/mag;
	VSCALE( plane, plane, mag );

	/* Find distance from the origin to the plane */
	plane[3] = VDOT( plane, a );
	return(0);		/* OK */
}

/*
 *			R T _ M K P O I N T _ 3 P L A N E S
 *
 *  Given the description of three planes, compute the point of intersection,
 *  if any.
 *
 *  Returns -
 *	 0	OK
 *	-1	Failure.  Intersection is a line or plane.
 */
int
rt_mkpoint_3planes( pt, a, b, c )
point_t	pt;
plane_t	a, b, c;
{
	vect_t	v1, v2, v3;
	register fastf_t d;

	VCROSS( v1, b, c );
	d = VDOT( a, v1 );
	if( NEAR_ZERO( d, SQRT_SMALL_FASTF ) )  return(-1);
	d = 1/d;
	VCROSS( v2, a, c );
	VCROSS( v3, a, b );

	pt[X] = a[3]*v1[X] - b[3]*v2[X] + c[3]*v3[X];
	pt[Y] = a[3]*v1[Y] - b[3]*v2[Y] + c[3]*v3[Y];
	pt[Z] = a[3]*v1[Z] - b[3]*v2[Z] + c[3]*v3[Z];
	VSCALE( pt, pt, d );
	return(0);
}

/*
 *			R T _ I S E C T _ R A Y _ P L A N E
 *
 *  Intersect a ray with a plane that has an outward pointing normal.
 *  The ray direction vector need not have unit length.
 *
 *  Explicit Return -
 *	-2	missed (ray is outside halfspace)
 *	-1	missed (ray is inside)
 *	 0	hit (ray is entering halfspace)
 *	 1	hit (ray is leaving)
 *
 *  Implicit Return -
 *	The value at *dist is set to the parametric distance of the intercept
 */
int
rt_isect_ray_plane( dist, pt, dir, plane )
fastf_t	*dist;
point_t	pt;
vect_t	dir;
plane_t	plane;
{
	register fastf_t	slant_factor;
	register fastf_t	norm_dist;

	norm_dist = plane[3] - VDOT( plane, pt );
	slant_factor = VDOT( plane, dir );

	if( slant_factor < -SQRT_SMALL_FASTF )  {
		*dist = norm_dist/slant_factor;
		return(0);			/* HIT, entering */
	} else if( slant_factor > SQRT_SMALL_FASTF )  {
		*dist = norm_dist/slant_factor;
		return(1);			/* HIT, leaving */
	}

	/*
	 *  Ray is parallel to plane when dir.N == 0.
	 */
	*dist = 0;		/* sanity */
	if( norm_dist < 0.0 )
		return(-2);	/* missed, outside */
	return(-1);		/* missed, inside */
}
