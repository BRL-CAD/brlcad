/*
 *			P L A N E . C
 *
 *  Some useful routines for dealing with planes and lines.
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
#include "raytrace.h"

/*
 *			R T _ 3 P T S _ D I S T I N C T
 *
 *  Check to see if three points are all distinct, i.e.,
 *  ensure that there is at least sqrt(dist_tol_sq) distance
 *  between every pair of points.
 *
 *  Returns (boolean) -
 *	1	If all three points are distinct
 *	0	If two or more points are closer together than dist_tol_sq
 */
int
rt_3pts_distinct( a, b, c, tol )
CONST point_t		a, b, c;
CONST struct rt_tol	*tol;
{
	vect_t	B_A;
	vect_t	C_A;
	vect_t	C_B;

	VSUB2( B_A, b, a );
	if( MAGSQ( B_A ) <= tol->dist_sq )  return(0);
	VSUB2( C_A, c, a );
	if( MAGSQ( C_A ) <= tol->dist_sq )  return(0);
	VSUB2( C_B, c, b );
	if( MAGSQ( C_B ) <= tol->dist_sq )  return(0);
	return(1);
}

/*
 *			R T _ M K _ P L A N E _ 3 P T S
 *
 *  Find the equation of a plane that contains three points.
 *  Note that normal vector created is expected to point out (see vmath.h),
 *  so the vector from A to C had better be counter-clockwise
 *  (about the point A) from the vector from A to B.
 *  This follows the BRL-CAD outward-pointing normal convention, and the
 *  right-hand rule for cross products.
 *
 *
 *			C
 *	                *
 *	                |\
 *	                | \
 *	   ^     N      |  \
 *	   |      \     |   \
 *	   |       \    |    \
 *	   |C-A     \   |     \
 *	   |         \  |      \
 *	   |          \ |       \
 *	               \|        \
 *	                *---------*
 *	                A         B
 *			   ----->
 *		            B-A
 *
 *  If the points are given in the order A B C (eg, *counter*-clockwise),
 *  then the outward pointing surface normal N = (B-A) x (C-A).
 *
 *  Explicit Return -
 *	 0	OK
 *	-1	Failure.  At least two of the points were not distinct,
 *		or all three were colinear.
 *
 *  Implicit Return -
 *	plane	The plane equation is stored here.
 */
int
rt_mk_plane_3pts( plane, a, b, c, tol )
plane_t			plane;
CONST point_t		a, b, c;
CONST struct rt_tol	*tol;
{
	vect_t	B_A;
	vect_t	C_A;
	vect_t	C_B;
	register fastf_t mag;

	VSUB2( B_A, b, a );
	if( MAGSQ( B_A ) <= tol->dist_sq )  return(-1);
	VSUB2( C_A, c, a );
	if( MAGSQ( C_A ) <= tol->dist_sq )  return(-1);
	VSUB2( C_B, c, b );
	if( MAGSQ( C_B ) <= tol->dist_sq )  return(-1);

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
 *  Find the solution to a system of three equations in three unknowns:
 *
 *	Px * Ax + Py * Ay + Pz * Az = -A3;
 *	Px * Bx + Py * By + Pz * Bz = -B3;
 *	Px * Cx + Py * Cy + Pz * Cz = -C3;
 *
 *  or
 *
 *	[ Ax  Ay  Az ]   [ Px ]   [ -A3 ]
 *	[ Bx  By  Bz ] * [ Py ] = [ -B3 ]
 *	[ Cx  Cy  Cz ]   [ Pz ]   [ -C3 ]
 *
 *
 *  Explitic Return -
 *	 0	OK
 *	-1	Failure.  Intersection is a line or plane.
 *
 *  Implicit Return -
 *	pt	The point of intersection is stored here.
 */
int
rt_mkpoint_3planes( pt, a, b, c )
point_t		pt;
CONST plane_t	a, b, c;
{
	vect_t	v1, v2, v3;
	register fastf_t det;

	/* Find a vector perpendicular to both planes B and C */
	VCROSS( v1, b, c );

	/*  If that vector is perpendicular to A,
	 *  then A is parallel to either B or C, and no intersection exists.
	 *  This dot&cross product is the determinant of the matrix M.
	 *  (I suspect there is some deep significance to this!)
	 */
	det = VDOT( a, v1 );
	if( NEAR_ZERO( det, SQRT_SMALL_FASTF ) )  return(-1);

	VCROSS( v2, a, c );
	VCROSS( v3, a, b );

	det = 1/det;
	pt[X] = det*(a[3]*v1[X] - b[3]*v2[X] + c[3]*v3[X]);
	pt[Y] = det*(a[3]*v1[Y] - b[3]*v2[Y] + c[3]*v3[Y]);
	pt[Z] = det*(a[3]*v1[Z] - b[3]*v2[Z] + c[3]*v3[Z]);
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
fastf_t		*dist;
CONST point_t	pt;
CONST vect_t	dir;
CONST plane_t	plane;
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

/*
 *			R T _ I S E C T _ 2 P L A N E S
 *
 *  Given two planes, find the line of intersection between them,
 *  if one exists.
 *  The line of intersection is returned in parametric line
 *  (point & direction vector) form.
 *
 *  In order that all the geometry under consideration be in "front"
 *  of the ray, it is necessary to pass the minimum point of the model
 *  RPP.  If this convention is unnecessary, just pass (0,0,0) as rpp_min.
 *
 *  Explicit Return -
 *	 0	OK, line of intersection stored in `pt' and `dir'.
 *	-1	FAIL, planes are identical (co-planar)
 *	-2	FAIL, planes are parallel and distinct
 *	-3	FAIL, unable to find line of intersection
 *
 *  Implicit Returns -
 *	pt	Starting point of line of intersection
 *	dir	Direction vector of line of intersection (unit length)
 */
int
rt_isect_2planes( pt, dir, a, b, rpp_min, tol )
point_t		pt;
vect_t		dir;
CONST plane_t	a;
CONST plane_t	b;
CONST vect_t	rpp_min;
CONST struct rt_tol	*tol;
{
	register fastf_t	d;
	LOCAL vect_t		abs_dir;
	LOCAL plane_t		pl;
	int			i;

	if( (i = rt_coplanar( a, b, tol )) != 0 )  {
		if( i > 0 )
			return(-1);	/* FAIL -- coplanar */
		return(-2);		/* FAIL -- parallel & distinct */
	}

	/* Direction vector for ray is perpendicular to both plane normals */
	VCROSS( dir, a, b );
	VUNITIZE( dir );		/* safety */

	/*
	 *  Select an axis-aligned plane which has it's normal pointing
	 *  along the same axis as the largest magnitude component of
	 *  the direction vector.
	 *  If the largest magnitude component is negative, reverse the
	 *  direction vector, so that model is "in front" of start point.
	 */
	abs_dir[X] = (dir[X] >= 0) ? dir[X] : (-dir[X]);
	abs_dir[Y] = (dir[Y] >= 0) ? dir[Y] : (-dir[Y]);
	abs_dir[Z] = (dir[Z] >= 0) ? dir[Z] : (-dir[Z]);

	if( abs_dir[X] >= abs_dir[Y] )  {
		if( abs_dir[X] >= abs_dir[Z] )  {
			VSET( pl, 1, 0, 0 );	/* X */
			pl[3] = rpp_min[X];
			if( dir[X] < 0 )  {
				VREVERSE( dir, dir );
			}
		} else {
			VSET( pl, 0, 0, 1 );	/* Z */
			pl[3] = rpp_min[Z];
			if( dir[Z] < 0 )  {
				VREVERSE( dir, dir );
			}
		}
	} else {
		if( abs_dir[Y] >= abs_dir[Z] )  {
			VSET( pl, 0, 1, 0 );	/* Y */
			pl[3] = rpp_min[Y];
			if( dir[Y] < 0 )  {
				VREVERSE( dir, dir );
			}
		} else {
			VSET( pl, 0, 0, 1 );	/* Z */
			pl[3] = rpp_min[Z];
			if( dir[Z] < 0 )  {
				VREVERSE( dir, dir );
			}
		}
	}

	/* Intersection of the 3 planes defines ray start point */
	if( rt_mkpoint_3planes( pt, pl, a, b ) < 0 )
		return(-3);	/* FAIL -- no intersection */

	return(0);		/* OK */
}

/*
 *			R T _ I S E C T _ 2 L I N E S
 *
 *  Intersect two lines, each in given in parametric form:
 *
 *	X = P + t * D
 *  and
 *	X = A + u * C
 *
 *  While the parametric form is usually used to denote a ray
 *  (ie, positive values of the parameter only), in this case
 *  the full line is considered.
 *
 *  The direction vectors C and D need not have unit length.
 *
 *  Explicit Return -
 *	-1	no intersection
 *	 0	lines are co-linear (t returned for u=0 to give distance to A)
 *	 1	intersection found (t and u returned)
 *
 *  Implicit Returns -
 *
 *	t,u	When explicit return >= 0, t and u are the
 *		line parameters of the intersection point on the 2 rays.
 *		The actual intersection coordinates can be found by
 *		substituting either of these into the original ray equations.
 */
int
rt_isect_2lines( t, u, p, d, a, c, tol )
fastf_t			*t;
fastf_t			*u;
CONST point_t		p;
CONST vect_t		d;
CONST point_t		a;
CONST vect_t		c;
CONST struct rt_tol	*tol;
{
	LOCAL vect_t		n;
	LOCAL vect_t		abs_n;
	LOCAL vect_t		h;
	register fastf_t	det;
	register short int	q,r,s;

	/*
	 *  Any intersection will occur in the plane with surface
	 *  normal D cross C, which may not have unit length.
	 *  The plane containing the two lines will be a constant
	 *  distance from a plane with the same normal that contains
	 *  the origin.  Therfore, the projection of any point on the
	 *  plane along N has the same length.
	 *  Verify that this holds for P and A.
	 *  If N dot P != N dot A, there is no intersection, because
	 *  P and A must lie on parallel planes that are different
	 *  distances from the origin.
	 */
	VCROSS( n, d, c );
	det = VDOT( n, p ) - VDOT( n, a );
	if( !NEAR_ZERO( det, tol->dist ) )  {
		return(-1);		/* No intersection */
	}

	/*
	 *  Solve for t and u in the system:
	 *
	 *	Px + t * Dx = Ax + u * Cx
	 *	Py + t * Dy = Ay + u * Cy
	 *	Pz + t * Dz = Az + u * Cz
	 *
	 *  This system is over-determined, having 3 equations in 2 unknowns.
	 *  However, the intersection problem is really only a 2-dimensional
	 *  problem, being located in the surface of a plane.
	 *  Therefore, the "least important" of these equations can
	 *  be initially ignored, leaving a system of 2 equations in
	 *  2 unknowns.
	 *
	 *  Find the component of N with the largest magnitude.
	 *  This component will have the least effect on the parameters
	 *  in the system, being most nearly perpendicular to the plane.
	 *  Denote the two remaining components by the
	 *  subscripts q and r, rather than x,y,z.
	 *  Subscript s is the smallest component, used for checking later.
	 */
	abs_n[X] = (n[X] >= 0) ? n[X] : (-n[X]);
	abs_n[Y] = (n[Y] >= 0) ? n[Y] : (-n[Y]);
	abs_n[Z] = (n[Z] >= 0) ? n[Z] : (-n[Z]);
	if( abs_n[X] >= abs_n[Y] )  {
		if( abs_n[X] >= abs_n[Z] )  {
			/* X is largest in magnitude */
			q = Y;
			r = Z;
			s = X;
		} else {
			/* Z is largest in magnitude */
			q = X;
			r = Y;
			s = Z;
		}
	} else {
		if( abs_n[Y] >= abs_n[Z] )  {
			/* Y is largest in magnitude */
			q = X;
			r = Z;
			s = Y;
		} else {
			/* Z is largest in magnitude */
			q = X;
			r = Y;
			s = Z;
		}
	}

	/*
	 *  From the two components q and r, form a system
	 *  of 2 equations in 2 unknowns:
	 *
	 *	Pq + t * Dq = Aq + u * Cq
	 *	Pr + t * Dr = Ar + u * Cr
	 *  or
	 *	t * Dq - u * Cq = Aq - Pq
	 *	t * Dr - u * Cr = Ar - Pr
	 *
	 *  Let H = A - P, resulting in:
	 *
	 *	t * Dq - u * Cq = Hq
	 *	t * Dr - u * Cr = Hr
	 *
	 *  or
	 *
	 *	[ Dq  -Cq ]   [ t ]   [ Hq ]
	 *	[         ] * [   ] = [    ]
	 *	[ Dr  -Cr ]   [ u ]   [ Hr ]
	 *
	 *  This system can be solved by direct substitution, or by
	 *  finding the determinants by Cramers rule:
	 *
	 *	             [ Dq  -Cq ]
	 *	det(M) = det [         ] = -Dq * Cr + Cq * Dr
	 *	             [ Dr  -Cr ]
	 *
	 *  If det(M) is zero, then there is no solution; otherwise,
	 *  exactly one solution exists.
	 */
	VSUB2( h, a, p );
	det = c[q] * d[r] - d[q] * c[r];
	if( NEAR_ZERO( det, SQRT_SMALL_FASTF ) )  {
		/* Lines are co-linear */
		/* Compute t for u=0 as a convenience to caller */
		*u = 0;
		/* Use largest direction component */
		if( d[q] >= d[r] )  {
			*t = h[q]/d[q];
		} else {
			*t = h[r]/d[r];
		}
		return(0);	/* Lines co-linear */
	}

	/*
	 *  det(M) is non-zero, so there is exactly one solution.
	 *  Using Cramer's rule, det1(M) replaces the first column
	 *  of M with the constant column vector, in this case H.
	 *  Similarly, det2(M) replaces the second column.
	 *  Computation of the determinant is done as before.
	 *
	 *  Now,
	 *
	 *	                  [ Hq  -Cq ]
	 *	              det [         ]
	 *	    det1(M)       [ Hr  -Cr ]   -Hq * Cr + Cq * Hr
	 *	t = ------- = --------------- = ------------------
	 *	     det(M)       det(M)        -Dq * Cr + Cq * Dr
	 *
	 *  and
	 *
	 *	                  [ Dq   Hq ]
	 *	              det [         ]
	 *	    det2(M)       [ Dr   Hr ]    Dq * Hr - Hq * Dr
	 *	u = ------- = --------------- = ------------------
	 *	     det(M)       det(M)        -Dq * Cr + Cq * Dr
	 */
	det = 1/det;
	*t = det * (c[q] * h[r] - h[q] * c[r]);
	*u = det * (d[q] * h[r] - h[q] * d[r]);

	/*
	 *  Check that these values of t and u satisfy the 3rd equation
	 *  as well!
	 *  XXX It isn't clear that "det" is exactly a model-space distance.
	 */
	det = *t * d[s] - *u * c[s] - h[s];
	if( !NEAR_ZERO( det, tol->dist ) )  {
		/* This tolerance needs to be much less loose than
		 * SQRT_SMALL_FASTF.
		 */
		/* Inconsistent solution, lines miss each other */
		return(-1);
	}

	return(1);		/* Intersection found */
}

/*
 *			R T _ I S E C T _ L I N E _ L S E G
 *
 *  Intersect a line in parametric form:
 *
 *	X = P + t * D
 *
 *  with a line segment defined by two distinct points A and B.
 *
 *  Explicit Return -
 *	-3	A and B are not distinct points
 *	-2	Intersection exists, but is outside of A--B
 *	-1	Lines do not intersect
 *	 0	Lines are co-linear (t for A is returned)
 *	 1	Intersection at vertex A
 *	 2	Intersection at vertex B
 *	 3	Intersection between A and B
 *
 *  Implicit Returns -
 *	t	When explicit return >= 0, t is the parameter that describes
 *		the intersection of the line and the line segment.
 *		The actual intersection coordinates can be found by
 *		solving P + t * D.  However, note that for return codes
 *		1 and 2 (intersection exactly at a vertex), it is
 *		strongly recommended that the original values passed in
 *		A or B are used instead of solving P + t * D, to prevent
 *		numeric error from creeping into the position of
 *		the endpoints.
 */
int
rt_isect_line_lseg( t, p, d, a, b, tol )
fastf_t			*t;
CONST point_t		p;
CONST vect_t		d;
CONST point_t		a;
CONST point_t		b;
CONST struct rt_tol	*tol;
{
	LOCAL vect_t	c;		/* Direction vector from A to B */
	auto fastf_t	u;		/* As in, A + u * C = X */
	register fastf_t f;
	register int	ret;
	fastf_t		fuzz;

	VSUB2( c, b, a );
	/*
	 *  To keep the values of u between 0 and 1,
	 *  C should NOT be scaled to have unit length.
	 *  However, it is a good idea to make sure that
	 *  C is a non-zero vector, (ie, that A and B are distinct).
	 */
	if( (fuzz = MAGSQ(c)) < tol->dist_sq )  {
		return(-3);		/* points A and B are not distinct */
	}

	if( (ret = rt_isect_2lines( t, &u, p, d, a, c, tol )) < 0 )  {
		/* No intersection found */
		return( -1 );
	}
	if( ret == 0 )  {
		/* co-linear (t was computed for point A, u=0) */
		return( 0 );
	}

	/*
	 *  The two lines intersect at a point.
	 *  If the u parameter is outside the range (0..1),
	 *  reject the intersection, because it falls outside
	 *  the line segment A--B.
	 *
	 *  Convert the tol->dist into allowable deviation in terms of
	 *  (0..1) range of the parameters.
	 */
	fuzz = tol->dist / sqrt(fuzz);
	if( u < -fuzz )
		return(-2);		/* Intersection outside of A--B */
	if( (f=(u-1)) > fuzz )
		return(-2);		/* Intersection outside of A--B */

	/* Check for fuzzy intersection with one of the verticies */
	if( u < fuzz )
		return( 1 );		/* Intersection at A */
	if( f >= -fuzz )
		return( 2 );		/* Intersection at B */

	return(3);			/* Intersection between A and B */
}

/*
 *			R T _ D I S T _ L I N E _ P O I N T
 *
 *  Given a parametric line defined by PT + t * DIR and a point A,
 *  return the closest distance between the line and the point.
 *  It is necessary that DIR have unit length.
 *
 *  Return -
 *	Distance
 */
double
rt_dist_line_point( pt, dir, a )
CONST point_t	pt;
CONST vect_t	dir;
CONST point_t	a;
{
	LOCAL vect_t		f;
	register fastf_t	FdotD;

	VSUB2( f, pt, a );
	FdotD = VDOT( f, dir );
	if( (FdotD = VDOT( f, f ) - FdotD * FdotD ) <= SMALL_FASTF )
		return(0.0);
	return( sqrt(FdotD) );
}

/*
 *			R T _ D I S T _ L I N E _ O R I G I N
 *
 *  Given a parametric line defined by PT + t * DIR,
 *  return the closest distance between the line and the origin.
 *  It is necessary that DIR have unit length.
 *
 *  Return -
 *	Distance
 */
double
rt_dist_line_origin( pt, dir )
CONST point_t	pt;
CONST vect_t	dir;
{
	register fastf_t	PTdotD;

	PTdotD = VDOT( pt, dir );
	if( (PTdotD = VDOT( pt, pt ) - PTdotD * PTdotD ) <= SMALL_FASTF )
		return(0.0);
	return( sqrt(PTdotD) );
}

/*
 *			R T _ A R E A _ O F _ T R I A N G L E
 *
 *  Returns the area of a triangle.
 *  Algorithm by Jon Leech 3/24/89.
 */
double
rt_area_of_triangle( a, b, c )
register CONST point_t a, b, c;
{
	register double	t;
	register double	area;

	t =	a[Y] * (b[Z] - c[Z]) -
		b[Y] * (a[Z] - c[Z]) +
		c[Y] * (a[Z] - b[Z]);
	area  = t*t;
	t =	a[Z] * (b[X] - c[X]) -
		b[Z] * (a[X] - c[X]) +
		c[Z] * (a[X] - b[X]);
	area += t*t;
	t = 	a[X] * (b[Y] - c[Y]) -
		b[X] * (a[Y] - c[Y]) +
		c[X] * (a[Y] - b[Y]);
	area += t*t;

	return( 0.5 * sqrt(area) );
}


/*
 *			R T _ I S E C T _ P T _ L S E G
 *
 * Intersect a point P with the line segment defined by two distinct
 * points A and B.
 *	
 * Explicit Return
 *	-2	P on line AB but outside range of AB,
 *			dist = distance from A to P on line.
 *	-1	P not on line of AB within tolerance
 *	1	P is at A
 *	2	P is at B
 *	3	P is on AB, dist = distance from A to P on line.
 *	
 *    B *
 *	|  
 *    P'*-tol-*P 
 *	|    /  _
 *    dist  /   /|
 *	|  /   /
 *	| /   / AtoP
 *	|/   /
 *    A *   /
 *	
 *	tol = distance limit from line to pt P;
 *	dist = distance from A to P'
 */
int rt_isect_pt_lseg(dist, a, b, p, tol)
fastf_t			*dist;		/* distance along line from A to P */
CONST point_t		a, b, p;	/* points for line and intersect */
CONST struct rt_tol	*tol;
{
	vect_t	AtoP,
		BtoP,
		AtoB,
		ABunit;	/* unit vector from A to B */
	fastf_t	APprABunit;	/* Mag of projection of AtoP onto ABunit */
	fastf_t	distsq;

	VSUB2(AtoP, p, a);
	if (MAGSQ(AtoP) < tol->dist_sq)
		return(1);	/* P at A */

	VSUB2(BtoP, p, b);
	if (MAGSQ(BtoP) < tol->dist_sq)
		return(2);	/* P at B */

	VSUB2(AtoB, b, a);
	VMOVE(ABunit, AtoB);
	VUNITIZE(ABunit);

	/* Similar to rt_dist_line_pt, except we
	 * never actually have to do the sqrt that the other routine does.
	 */

	/* find dist as a function of ABunit, actually the projection
	 * of AtoP onto ABunit
	 */
	APprABunit = VDOT(AtoP, ABunit);

	/* because of pythgorean theorem ... */
	distsq = MAGSQ(AtoP) - APprABunit * APprABunit;
	if (distsq > tol->dist_sq)
		return(-1);	/* dist pt to line too large */

	/* Distance from the point to the line is within tolerance. */
	*dist = VDOT(AtoP, AtoB) / MAGSQ(AtoB);

	if (*dist > 1.0 || *dist < 0.0)	/* P outside AtoB */
		return(-2);

	return(3);	/* P on AtoB */
}

/*
 *			R T _ D I S T _ P T _ L S E G
 *
 *	Find the distance from a point P to a line segment described
 *	by the two endpoints A and B.
 *
 *	Explicit return
 *	    distance from the point of closest approach on lseg to point
 *	    0.0 if within tol->dist of A or B or the line segment.
 *
 *	Implicit return
 *	    pca 	the point of closest approach
 *
 * XXX For efficiency, a version of this routine that provides the
 * XXX distance squared would be faster.
 */
double rt_dist_pt_lseg(pca, a, b, p, tol)
point_t		pca;
CONST point_t	a, b, p;
CONST struct rt_tol *tol;
{
	vect_t	PtoA;		/* P-A */
	vect_t	AtoB;		/* B-A */
	fastf_t	seglen;		/* |B-A| */
	fastf_t	seglen_sq;
	fastf_t	t;		/* distance along ray of projection of P */
	
	VSUB2(PtoA, p, a);
	VSUB2(AtoB, b, a);
	seglen_sq = MAGSQ(AtoB);
	seglen = sqrt(seglen_sq);

	/* compute distance (in actual units) along line to pca */
	t = VDOT(PtoA, AtoB) / seglen;

	if( t < -tol->dist )  {
		/* P is "below" A */
		VMOVE( pca, a );
		return MAGNITUDE(PtoA);
	}
	if( t < tol->dist )  {
		/* P is within the tol->dist radius circle around A */
		VMOVE( pca, a );
		return 0.0;
	}
	if( t < seglen - tol->dist )  {
		/* P falls between A and B */
		register fastf_t	dsq;
		fastf_t			param_dist;	/* parametric dist */

		/* Find PCA */
		param_dist = VDOT(PtoA, AtoB) / seglen_sq;
		VJOIN1(pca, a, param_dist, AtoB);

		/* Find distance from PCA to line segment */
		if( (dsq = VDOT( PtoA, PtoA ) - t * t ) <= tol->dist_sq )  {
			return 0.0;
		}
		return sqrt(dsq);
	}
	if( t < seglen + tol->dist )  {
		/* P is within the tol->dist radius circle around B */
		VMOVE( pca, b );
		return 0.0;
	}
	/* P is "above" B */
	VMOVE(pca, b);
	VSUB2(PtoA, p, b);
	return MAGNITUDE(PtoA);
}

/*
 *			R T _ R O T A T E _ B B O X
 *
 *  Transform a bounding box (RPP) by the given 4x4 matrix.
 *  There are 8 corners to the bounding RPP.
 *  Each one needs to be transformed and min/max'ed.
 *  This is not minimal, but does fully contain any internal object,
 *  using an axis-aligned RPP.
 */
void
rt_rotate_bbox( omin, omax, mat, imin, imax )
point_t		omin;
point_t		omax;
CONST mat_t	mat;
CONST point_t	imin;
CONST point_t	imax;
{
	point_t	local;		/* vertex point in local coordinates */
	point_t	model;		/* vertex point in model coordinates */

#define ROT_VERT( a, b, c )  \
	VSET( local, a[X], b[Y], c[Z] ); \
	MAT4X3PNT( model, mat, local ); \
	VMINMAX( omin, omax, model ) \

	ROT_VERT( imin, imin, imin );
	ROT_VERT( imin, imin, imax );
	ROT_VERT( imin, imax, imin );
	ROT_VERT( imin, imax, imax );
	ROT_VERT( imax, imin, imin );
	ROT_VERT( imax, imin, imax );
	ROT_VERT( imax, imax, imin );
	ROT_VERT( imax, imax, imax );
#undef ROT_VERT
}

/*
 *			R T _ R O T A T E _ P L A N E
 *
 *  Transform a plane equation by the given 4x4 matrix.
 */
void
rt_rotate_plane( oplane, mat, iplane )
plane_t		oplane;
CONST mat_t	mat;
CONST plane_t	iplane;
{
	point_t		orig_pt;
	point_t		new_pt;

	/* First, pick a point that lies on the original halfspace */
	VSCALE( orig_pt, iplane, iplane[3] );

	/* Transform the surface normal */
	MAT4X3VEC( oplane, mat, iplane );

	/* Transform the point from original to new halfspace */
	MAT4X3PNT( new_pt, mat, orig_pt );

	/*
	 *  The transformed normal is all that is required.
	 *  The new distance is found from the transformed point on the plane.
	 */
	oplane[3] = VDOT( new_pt, oplane );
}

/*
 *			R T _ C O P L A N A R
 *
 *  Test if two planes are identical.  If so, their dot products will be
 *  either +1 or -1, with the distance from the origin equal in magnitude.
 *
 *  Returns -
 *	-1	not coplanar, parallel but distinct
 *	 0	not coplanar, not parallel.  Planes intersect.
 *	+1	coplanar, same normal direction
 *	+2	coplanar, opposite normal direction
 */
int
rt_coplanar( a, b, tol )
CONST plane_t		a;
CONST plane_t		b;
CONST struct rt_tol	*tol;
{
	register fastf_t	f;
	register fastf_t	dot;

	/* Check to see if the planes are parallel */
	dot = VDOT( a, b );
	if( dot >= 0 )  {
		/* Normals head in generally the same directions */
		if( dot < tol->para )
			return(0);	/* Planes intersect */

		/* Planes have "exactly" the same normal vector */
		f = a[3] - b[3];
		if( NEAR_ZERO( f, tol->dist ) )  {
			return(1);	/* Coplanar, same direction */
		}
		return(-1);		/* Parallel but distinct */
	}
	/* Normals head in generally opposite directions */
	if( -dot < tol->para )
		return(0);		/* Planes intersect */

	/* Planes have "exactly" opposite normal vectors */
	f = a[3] + b[3];
	if( NEAR_ZERO( f, tol->dist ) )  {
		return(2);		/* Coplanar, opposite directions */
	}
	return(-1);			/* Parallel but distinct */
}
