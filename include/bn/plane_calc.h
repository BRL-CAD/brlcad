/*                        P L A N E _ C A L C . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file plane_calc.h */
/** @addtogroup plane */
/** @{ */

/**
 *  @brief Plane/line/point calculations
 */

#ifndef BN_PLANE_CALC_H
#define BN_PLANE_CALC_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"
#include "bn/plane_struct.h"
#include "bn/tol.h"

__BEGIN_DECLS

/**
 *@brief
 * Calculate the square of the distance of closest approach for two
 * lines.
 *
 * The lines are specified as a point and a vector each.  The vectors
 * need not be unit length.  P and d define one line; Q and e define
 * the other.
 *
 * @return 0 - normal return
 * @return 1 - lines are parallel, dist[0] is set to 0.0
 *
 * Output values:
 * dist[0] is the parametric distance along the first line P + dist[0] * d of the PCA
 * dist[1] is the parametric distance along the second line Q + dist[1] * e of the PCA
 * dist[3] is the square of the distance between the points of closest approach
 * pt1 is the point of closest approach on the first line
 * pt2 is the point of closest approach on the second line
 *
 * This algorithm is based on expressing the distance squared, taking
 * partials with respect to the two unknown parameters (dist[0] and
 * dist[1]), setting the two partials equal to 0, and solving the two
 * simultaneous equations
 */
BN_EXPORT extern int bn_distsq_line3_line3(fastf_t dist[3],
					   point_t P,
					   vect_t d,
					   point_t Q,
					   vect_t e,
					   point_t pt1,
					   point_t pt2);

/**
 * Find the distance from a point P to a line described by the
 * endpoint A and direction dir, and the point of closest approach
 * (PCA).
 *
 @code
			P
		       *
		      /.
		     / .
		    /  .
		   /   . (dist)
		  /    .
		 /     .
		*------*-------->
		A      PCA    dir
 @endcode
 * There are three distinct cases, with these return codes -
 *   0 => P is within tolerance of point A.  *dist = 0, pca=A.
 *   1 => P is within tolerance of line.  *dist = 0, pca=computed.
 *   2 => P is "above/below" line.  *dist=|PCA-P|, pca=computed.
 *
 * TODO: For efficiency, a version of this routine that provides the
 * distance squared would be faster.
 */
BN_EXPORT extern int bn_dist_pt3_line3(fastf_t *dist,
				       point_t pca,
				       const point_t a,
				       const point_t p,
				       const vect_t dir,
				       const struct bn_tol *tol);

/**
 * calculate intersection or closest approach of a line and a line
 * segment.
 *
 * returns:
 *	-2 -> line and line segment are parallel and collinear.
 *	-1 -> line and line segment are parallel, not collinear.
 *	 0 -> intersection between points a and b.
 *	 1 -> intersection outside a and b.
 *	 2 -> closest approach is between a and b.
 *	 3 -> closest approach is outside a and b.
 *
 * dist[0] is actual distance from p in d direction to
 * closest portion of segment.
 * dist[1] is ratio of distance from a to b (0.0 at a, and 1.0 at b),
 * dist[1] may be less than 0 or greater than 1.
 * For return values less than 0, closest approach is defined as
 * closest to point p.
 * Direction vector, d, must be unit length.
 *
 */
BN_EXPORT extern int bn_dist_line3_lseg3(fastf_t *dist,
					 const fastf_t *p,
					 const fastf_t *d,
					 const fastf_t *a,
					 const fastf_t *b,
					 const struct bn_tol *tol);

/**
 * Calculate closest approach of two lines
 *
 * returns:
 *	-2 -> lines are parallel and do not intersect
 *	-1 -> lines are parallel and collinear
 *	 0 -> lines intersect
 *	 1 -> lines do not intersect
 *
 * For return values less than zero, dist is not set.  For return
 * values of 0 or 1, dist[0] is the distance from p1 in the d1
 * direction to the point of closest approach for that line.  Similar
 * for the second line.  d1 and d2 must be unit direction vectors.
 *
 * XXX How is this different from bn_isect_line3_line3() ?
 * XXX Why are the calling sequences just slightly different?
 * XXX Can we pick the better one, and get rid of the other one?
 * XXX If not, can we document how they differ?
 */
BN_EXPORT extern int bn_dist_line3_line3(fastf_t dist[2],
					 const point_t p1,
					 const point_t p2,
					 const vect_t d1,
					 const vect_t d2,
					 const struct bn_tol *tol);

/**
 *@brief
 * Find the distance from a point P to a line segment described by the
 * two endpoints A and B, and the point of closest approach (PCA).
 @verbatim
 *
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*--------*
 *		A      PCA	B
 @endverbatim
 *
 * @return 0	P is within tolerance of lseg AB.  *dist isn't 0: (SPECIAL!!!)
 *		  *dist = parametric dist = |PCA-A| / |B-A|.  pca=computed.
 * @return 1	P is within tolerance of point A.  *dist = 0, pca=A.
 * @return 2	P is within tolerance of point B.  *dist = 0, pca=B.
 * @return 3	P is to the "left" of point A.  *dist=|P-A|, pca=A.
 * @return 4	P is to the "right" of point B.  *dist=|P-B|, pca=B.
 * @return 5	P is "above/below" lseg AB.  *dist=|PCA-P|, pca=computed.
 *
 * This routine was formerly called bn_dist_pt_lseg().
 *
 * XXX For efficiency, a version of this routine that provides the
 * XXX distance squared would be faster.
 */
BN_EXPORT extern int bn_dist_pt3_lseg3(fastf_t *dist,
				       point_t pca,
				       const point_t a,
				       const point_t b,
				       const point_t p,
				       const struct bn_tol *tol);

/**
 * PRIVATE: This is a new API and should be considered unpublished.
 *
 * Find the square of the distance from a point P to a line segment described
 * by the two endpoints A and B.
 *
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*--------*
 *		A      PCA	B
 *
 * There are six distinct cases, with these return codes -
 * Return code precedence: 1, 2, 0, 3, 4, 5
 *
 *	0	P is within tolerance of lseg AB.  *dist =  0.
 *	1	P is within tolerance of point A.  *dist = 0.
 *	2	P is within tolerance of point B.  *dist = 0.
 *	3	PCA is within tolerance of A. *dist = |P-A|**2.
 *	4	PCA is within tolerance of B. *dist = |P-B|**2.
 *	5	P is "above/below" lseg AB.   *dist=|PCA-P|**2.
 *
 * If both P and PCA and not within tolerance of lseg AB use
 * these return codes -
 *
 *	3	PCA is to the left of A.  *dist = |P-A|**2.
 *	4	PCA is to the right of B. *dist = |P-B|**2.
 *
 * This function is a test version of "bn_distsq_pt3_lseg3".
 *
 */
BN_EXPORT extern int bn_distsq_pt3_lseg3_v2(fastf_t *distsq,
					    const fastf_t *a,
					    const fastf_t *b,
					    const fastf_t *p,
					    const struct bn_tol *tol);

/**
 * @brief
 * Check to see if three points are collinear.
 *
 * The algorithm is designed to work properly regardless of the order
 * in which the points are provided.
 *
 * @return 1	If 3 points are collinear
 * @return 0	If they are not
 */
BN_EXPORT extern int bn_3pts_collinear(point_t a,
				       point_t b,
				       point_t c,
				       const struct bn_tol *tol);

/**
 * @return 1	if the two points are equal, within the tolerance
 * @return 0	if the two points are not "the same"
 */
BN_EXPORT extern int bn_pt3_pt3_equal(const point_t a,
				      const point_t b,
				      const struct bn_tol *tol);

/**
 *@brief
 * Find the distance from a point P to a line segment described by the
 * two endpoints A and B, and the point of closest approach (PCA).
 @verbatim
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*--------*
 *		A      PCA	B
 @endverbatim
 * There are six distinct cases, with these return codes -
 * @return 0	P is within tolerance of lseg AB.  *dist isn't 0: (SPECIAL!!!)
 *		  *dist = parametric dist = |PCA-A| / |B-A|.  pca=computed.
 * @return 1	P is within tolerance of point A.  *dist = 0, pca=A.
 * @return 2	P is within tolerance of point B.  *dist = 0, pca=B.
 * @return 3	P is to the "left" of point A.  *dist=|P-A|**2, pca=A.
 * @return 4	P is to the "right" of point B.  *dist=|P-B|**2, pca=B.
 * @return 5	P is "above/below" lseg AB.  *dist=|PCA-P|**2, pca=computed.
 *
 *
 * Patterned after bn_dist_pt3_lseg3().
 */
BN_EXPORT extern int bn_dist_pt2_lseg2(fastf_t *dist_sq,
				       fastf_t pca[2],
				       const point_t a,
				       const point_t b,
				       const point_t p,
				       const struct bn_tol *tol);

/**
 *@brief
 * Intersect two 3D line segments, defined by two points and two
 * vectors.  The vectors are unlikely to be unit length.
 *
 *
 * @return -3	missed
 * @return -2	missed (line segments are parallel)
 * @return -1	missed (collinear and non-overlapping)
 * @return 0	hit (line segments collinear and overlapping)
 * @return 1	hit (normal intersection)
 *
 * @param[out] dist
 *	The value at dist[] is set to the parametric distance of the
 *	intercept dist[0] is parameter along p, range 0 to 1, to
 *	intercept.  dist[1] is parameter along q, range 0 to 1, to
 *	intercept.  If within distance tolerance of the endpoints,
 *	these will be exactly 0.0 or 1.0, to ease the job of caller.
 *
 *      CLARIFICATION: This function 'bn_isect_lseg3_lseg3'
 *      returns distance values scaled where an intersect at the start
 *      point of the line segment (within tol->dist) results in 0.0
 *      and when the intersect is at the end point of the line
 *      segment (within tol->dist), the result is 1.0.  Intersects
 *      before the start point return a negative distance.  Intersects
 *      after the end point result in a return value > 1.0.
 *
 * Special note: when return code is "0" for co-linearity, dist[1] has
 * an alternate interpretation: it's the parameter along p (not q)
 * which takes you from point p to the point (q + qdir), i.e., it's
 * the endpoint of the q linesegment, since in this case there may be
 * *two* intersections, if q is contained within span p to (p + pdir).
 *
 * @param p	point 1
 * @param pdir	direction-1
 * @param q	point 2
 * @param qdir	direction-2
 * @param tol	tolerance values
 */
BN_EXPORT extern int bn_isect_lseg3_lseg3(fastf_t *dist,
					      const point_t p, const vect_t pdir,
					      const point_t q, const vect_t qdir,
					      const struct bn_tol *tol);

BN_EXPORT extern int bn_lseg3_lseg3_parallel(const point_t sg1pt1, const point_t sg1pt2,
					     const point_t sg2pt1, const point_t sg2pt2,
					     const struct bn_tol *tol);

/**
 * Intersect two line segments, each in given in parametric form:
 *
 * X = p0 + pdist * pdir_i   (i.e. line p0->p1)
 * and
 * X = q0 + qdist * qdir_i   (i.e. line q0->q1)
 *
 * The input vectors 'pdir_i' and 'qdir_i' must NOT be unit vectors
 * for this function to work correctly. The magnitude of the direction
 * vectors indicates line segment length.
 *
 * The 'pdist' and 'qdist' values returned from this function are the
 * actual distance to the intersect (i.e. not scaled). Distances may
 * be negative, see below.
 *
 * @return -2	no intersection, lines are parallel.
 * @return -1	no intersection
 * @return 0	lines are co-linear (pdist and qdist returned) (see below)
 * @return 1	intersection found  (pdist and qdist returned) (see below)
 *
 * @param	p0	point 1
 * @param	u       direction 1
 * @param	q0	point 2
 * @param	v       direction 2
 * @param       tol	tolerance values
 * @param[out]	s       (distances to intersection) (see below)
 * @param[out]  t       (distances to intersection) (see below)
 *
 *		When return = 1, pdist is the distance along line p0->p1 to the
 *		intersect with line q0->q1. If the intersect is along p0->p1 but
 *		in the opposite direction of vector pdir_i (i.e. occurring before
 *		p0 on line p0->p1) then the distance will be negative. The value
 *		if qdist is the same as pdist except it is the distance along line
 *		q0->q1 to the intersect with line p0->p1.
 *
 *		When return code = 0 for co-linearity, pdist and qdist have a
 *		different meaning. pdist is the distance from point p0 to point q0
 *		and qdist is the distance from point p0 to point q1. If point q0
 *		occurs before point p0 on line segment p0->p1 then pdist will be
 *		negative. The same occurs for the distance to point q1.
 */
BN_EXPORT extern int bn_isect_line3_line3(fastf_t *s, fastf_t *t,
					      const point_t p0,
					      const vect_t u,
					      const point_t q0,
					      const vect_t v,
					      const struct bn_tol *tol);

/**
 * @brief
 * Returns non-zero if the 3 lines are collinear to within tol->dist
 * over the given distance range.
 *
 * Range should be at least one model diameter for most applications.
 * 1e5 might be OK for a default for "vehicle sized" models.
 *
 * The direction vectors do not need to be unit length.
 */
BN_EXPORT extern int bn_2line3_colinear(const point_t p1,
					const vect_t d1,
					const point_t p2,
					const vect_t d2,
					double range,
					const struct bn_tol *tol);

/**
 * @brief
 * Intersect a point P with the line segment defined by two distinct
 * points A and B.
 *
 * @return -2	P on line AB but outside range of AB,
 *			dist = distance from A to P on line.
 * @return -1	P not on line of AB within tolerance
 * @return 1	P is at A
 * @return 2	P is at B
 * @return 3	P is on AB, dist = distance from A to P on line.
 @verbatim
 B *
 |
 P'*-tol-*P
 |    /  _
 dist  /   /|
 |  /   /
 | /   / AtoP
 |/   /
 A *   /

 tol = distance limit from line to pt P;
 dist = distance from A to P'
 @endverbatim
*/
BN_EXPORT extern int bn_isect_pt2_lseg2(fastf_t *dist,
					const point_t a,
					const point_t b,
					const point_t p,
					const struct bn_tol *tol);

/**
 *@brief
 * Intersect a line in parametric form:
 *
 * X = P + t * D
 *
 * with a line segment defined by two distinct points A and B=(A+C).
 *
 * XXX probably should take point B, not vector C.  Sigh.
 *
 * @return -4	A and B are not distinct points
 * @return -3	Lines do not intersect
 * @return -2	Intersection exists, but outside segment, < A
 * @return -1	Intersection exists, but outside segment, > B
 * @return 0	Lines are co-linear (special meaning of dist[1])
 * @return 1	Intersection at vertex A
 * @return 2	Intersection at vertex B (A+C)
 * @return 3	Intersection between A and B
 *
 * Implicit Returns -
 * @param dist	When explicit return >= 0, t is the parameter that describes
 *		the intersection of the line and the line segment.
 *		The actual intersection coordinates can be found by
 *		solving P + t * D.  However, note that for return codes
 *		1 and 2 (intersection exactly at a vertex), it is
 *		strongly recommended that the original values passed in
 *		A or B are used instead of solving P + t * D, to prevent
 *		numeric error from creeping into the position of
 *		the endpoints.
 *
 * @param p	point of first line
 * @param d	direction of first line
 * @param a	point of second line
 * @param c	direction of second line
 * @param tol	tolerance values
 */
BN_EXPORT extern int bn_isect_line2_lseg2(fastf_t *dist,
					  const point_t p,
					  const vect_t d,
					  const point_t a,
					  const vect_t c,
					  const struct bn_tol *tol);

/**
 *@brief
 * Intersect two 2D line segments, defined by two points and two
 * vectors.  The vectors are unlikely to be unit length.
 *
 * @return -2	missed (line segments are parallel)
 * @return -1	missed (collinear and non-overlapping)
 * @return 0	hit (line segments collinear and overlapping)
 * @return 1	hit (normal intersection)
 *
 * @param dist  The value at dist[] is set to the parametric distance of the
 *		intercept.
 *@n	dist[0] is parameter along p, range 0 to 1, to intercept.
 *@n	dist[1] is parameter along q, range 0 to 1, to intercept.
 *@n	If within distance tolerance of the endpoints, these will be
 *	exactly 0.0 or 1.0, to ease the job of caller.
 *
 * Special note: when return code is "0" for co-linearity, dist[1] has
 * an alternate interpretation: it's the parameter along p (not q)
 * which takes you from point p to the point (q + qdir), i.e., its
 * the endpoint of the q linesegment, since in this case there may be
 * *two* intersections, if q is contained within span p to (p + pdir).
 * And either may be -10 if the point is outside the span.
 *
 * @param p	point 1
 * @param pdir	direction1
 * @param q	point 2
 * @param qdir	direction2
 * @param tol	tolerance values
 */
BN_EXPORT extern int bn_isect_lseg2_lseg2(fastf_t *dist,
					  const point_t p,
					  const vect_t pdir,
					  const point_t q,
					  const vect_t qdir,
					  const struct bn_tol *tol);

/**
 * Intersect two lines, each in given in parametric form:
 @verbatim

 X = P + t * D
 and
 X = A + u * C

 @endverbatim
 *
 * While the parametric form is usually used to denote a ray (i.e.,
 * positive values of the parameter only), in this case the full line
 * is considered.
 *
 * The direction vectors C and D need not have unit length.
 *
 * @return -1	no intersection, lines are parallel.
 * @return 0	lines are co-linear
 *@n			dist[0] gives distance from P to A,
 *@n			dist[1] gives distance from P to (A+C) [not same as below]
 * @return 1	intersection found (t and u returned)
 *@n			dist[0] gives distance from P to isect,
 *@n			dist[1] gives distance from A to isect.
 *
 * @param dist When explicit return > 0, dist[0] and dist[1] are the
 * line parameters of the intersection point on the 2 rays.  The
 * actual intersection coordinates can be found by substituting either
 * of these into the original ray equations.
 *
 * @param p	point of first line
 * @param d	direction of first line
 * @param a	point of second line
 * @param c	direction of second line
 * @param tol	tolerance values
 *
 * Note that for lines which are very nearly parallel, but not quite
 * parallel enough to have the determinant go to "zero", the
 * intersection can turn up in surprising places.  (e.g. when
 * det=1e-15 and det1=5.5e-17, t=0.5)
 */
BN_EXPORT extern int bn_isect_line2_line2(fastf_t *dist,
					  const point_t p,
					  const vect_t d,
					  const point_t a,
					  const vect_t c,
					  const struct bn_tol *tol);

/**
 * @brief
 * Returns distance between two points.
 */
BN_EXPORT extern double bn_dist_pt3_pt3(const point_t a,
					const point_t b);

/**
 * Check to see if three points are all distinct, i.e., ensure that
 * there is at least sqrt(dist_tol_sq) distance between every pair of
 * points.
 *
 * @return 1 If all three points are distinct
 * @return 0 If two or more points are closer together than dist_tol_sq
 */
BN_EXPORT extern int bn_3pts_distinct(const point_t a,
				      const point_t b,
				      const point_t c,
				      const struct bn_tol *tol);

/**
 * Check to see if the points are all distinct, i.e., ensure that
 * there is at least sqrt(dist_tol_sq) distance between every pair of
 * points.
 *
 * @return 1 If all the points are distinct
 * @return 0 If two or more points are closer together than dist_tol_sq
 */
BN_EXPORT extern int bn_npts_distinct(const int npts,
				      const point_t *pts,
				      const struct bn_tol *tol);

/**
 * Find the equation of a plane that contains three points.  Note that
 * normal vector created is expected to point out (see vmath.h), so
 * the vector from A to C had better be counter-clockwise (about the
 * point A) from the vector from A to B.  This follows the BRL-CAD
 * outward-pointing normal convention, and the right-hand rule for
 * cross products.
 *
 @verbatim
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
 @endverbatim
 *
 * If the points are given in the order A B C (e.g.,
 * *counter*-clockwise), then the outward pointing surface normal:
 *
 * N = (B-A) x (C-A).
 *
 *  @return 0	OK
 *  @return -1	Failure.  At least two of the points were not distinct,
 *		or all three were collinear.
 *
 * @param[out]	plane	The plane equation is stored here.
 * @param[in]	a	point 1
 * @param[in]	b	point 2
 * @param[in]	c	point 3
 * @param[in]	tol	Tolerance values for doing calculation
 */
BN_EXPORT extern int bn_mk_plane_3pts(plane_t plane,
				      const point_t a,
				      const point_t b,
				      const point_t c,
				      const struct bn_tol *tol);

/**
 *@brief
 * Given the description of three planes, compute the point of intersection, if
 * any. The direction vectors of the planes need not be of unit length.
 *
 * Find the solution to a system of three equations in three unknowns:
 @verbatim
 * Px * Ax + Py * Ay + Pz * Az = -A3;
 * Px * Bx + Py * By + Pz * Bz = -B3;
 * Px * Cx + Py * Cy + Pz * Cz = -C3;
 *
 * OR
 *
 * [ Ax  Ay  Az ]   [ Px ]   [ -A3 ]
 * [ Bx  By  Bz ] * [ Py ] = [ -B3 ]
 * [ Cx  Cy  Cz ]   [ Pz ]   [ -C3 ]
 *
 @endverbatim
 *
 * @return 0	OK
 * @return -1	Failure.  Intersection is a line or plane.
 *
 * @param[out] pt	The point of intersection is stored here.
 * @param	a	plane 1
 * @param	b	plane 2
 * @param	c	plane 3
 */

BN_EXPORT extern int bn_mkpoint_3planes(point_t pt,
					const plane_t a,
					const plane_t b,
					const plane_t c);

/**
 * Intersect an infinite line (specified in point and direction vector
 * form) with a plane that has an outward pointing normal.  The
 * direction vector need not have unit length.  The first three
 * elements of the plane equation must form a unit length vector.
 *
 * @return -2	missed (ray is outside halfspace)
 * @return -1	missed (ray is inside)
 * @return 0	line lies on plane
 * @return 1	hit (ray is entering halfspace)
 * @return 2	hit (ray is leaving)
 *
 * @param[out]	dist	set to the parametric distance of the intercept
 * @param[in]	pt	origin of ray
 * @param[in]	dir	direction of ray
 * @param[in]	plane	equation of plane
 * @param[in]	tol	tolerance values
 */
BN_EXPORT extern int bn_isect_line3_plane(fastf_t *dist,
					  const point_t pt,
					  const vect_t dir,
					  const plane_t plane,
					  const struct bn_tol *tol);

/**
 *@brief
 * Given two planes, find the line of intersection between them, if
 * one exists.  The line of intersection is returned in parametric
 * line (point & direction vector) form.
 *
 * In order that all the geometry under consideration be in "front" of
 * the ray, it is necessary to pass the minimum point of the model
 * RPP.  If this convention is unnecessary, just pass (0, 0, 0) as
 * rpp_min.
 *
 * @return 0	success, line of intersection stored in 'pt' and 'dir'
 * @return -1	planes are coplanar
 * @return -2	planes are parallel but not coplanar
 * @return -3	error, should be intersection but unable to find
 *
 * @param[out]	pt	Starting point of line of intersection
 * @param[out]	dir	Direction vector of line of intersection (unit length)
 * @param[in]	a	plane 1 (normal must be unit length)
 * @param[in]	b	plane 2 (normal must be unit length)
 * @param[in]	rpp_min	minimum point of model RPP
 * @param[in]	tol	tolerance values
 */
BN_EXPORT extern int bn_isect_2planes(point_t pt,
				      vect_t dir,
				      const plane_t a,
				      const plane_t b,
				      const vect_t rpp_min,
				      const struct bn_tol *tol);
BN_EXPORT extern int bn_isect_2lines(fastf_t *t,
				     fastf_t *u,
				     const point_t p,
				     const vect_t d,
				     const point_t a,
				     const vect_t c,
				     const struct bn_tol *tol);

/**
 *@brief
 * Intersect a line in parametric form:
 *
 * X = P + t * D
 *
 * with a line segment defined by two distinct points A and B.
 *
 *
 * @return -4	A and B are not distinct points
 * @return -3	Intersection exists, < A (t is returned)
 * @return -2	Intersection exists, > B (t is returned)
 * @return -1	Lines do not intersect
 * @return 0	Lines are co-linear (t for A is returned)
 * @return 1	Intersection at vertex A
 * @return 2	Intersection at vertex B
 * @return 3	Intersection between A and B
 *
 * @par Implicit Returns -
 *
 * t When explicit return >= 0, t is the parameter that describes the
 * intersection of the line and the line segment.  The actual
 * intersection coordinates can be found by solving P + t * D.
 * However, note that for return codes 1 and 2 (intersection exactly
 * at a vertex), it is strongly recommended that the original values
 * passed in A or B are used instead of solving P + t * D, to prevent
 * numeric error from creeping into the position of the endpoints.
 *
 * XXX should probably be called bn_isect_line3_lseg3()
 * XXX should probably be changed to return dist[2]
 */
BN_EXPORT extern int bn_isect_line_lseg(fastf_t *t, const point_t p,
					const vect_t d,
					const point_t a,
					const point_t b,
					const struct bn_tol *tol);

/**
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the closest distance between the line and the point.
 *
 *  'dir' need not have unit length.
 *
 * Find parameter for PCA along line with unitized DIR:
 * d = VDOT(f, dir) / MAGNITUDE(dir);
 * Find distance g from PCA to A using Pythagoras:
 * g = sqrt(MAGSQ(f) - d**2)
 *
 * Return -
 * Distance
 */
BN_EXPORT extern double bn_dist_line3_pt3(const point_t pt,
					  const vect_t dir,
					  const point_t a);

/**
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the square of the closest distance between the line and the
 * point.
 *
 * 'dir' need not have unit length.
 *
 * Return -
 * Distance squared
 */
BN_EXPORT extern double bn_distsq_line3_pt3(const point_t pt,
					    const vect_t dir,
					    const point_t a);

/**
 *@brief
 * Given a parametric line defined by PT + t * DIR, return the closest
 * distance between the line and the origin.
 *
 * 'dir' need not have unit length.
 *
 * @return Distance
 */
BN_EXPORT extern double bn_dist_line_origin(const point_t pt,
					    const vect_t dir);

/**
 *@brief
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the closest distance between the line and the point.
 *
 * 'dir' need not have unit length.
 *
 * @return Distance
 */
BN_EXPORT extern double bn_dist_line2_point2(const point_t pt,
					     const vect_t dir,
					     const point_t a);

/**
 *@brief
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the closest distance between the line and the point,
 * squared.
 *
 * 'dir' need not have unit length.
 *
 * @return
 * Distance squared
 */
BN_EXPORT extern double bn_distsq_line2_point2(const point_t pt,
					       const vect_t dir,
					       const point_t a);

/**
 *@brief
 * Returns the area of a triangle. Algorithm by Jon Leech 3/24/89.
 */
BN_EXPORT extern double bn_area_of_triangle(const point_t a,
					    const point_t b,
					    const point_t c);

/**
 *@brief
 * Intersect a point P with the line segment defined by two distinct
 * points A and B.
 *
 * @return -2	P on line AB but outside range of AB,
 * 			dist = distance from A to P on line.
 * @return -1	P not on line of AB within tolerance
 * @return 1	P is at A
 * @return 2	P is at B
 * @return 3	P is on AB, dist = distance from A to P on line.
 @verbatim
 B *
 |
 P'*-tol-*P
 |    /   _
 dist  /   /|
 |  /   /
 | /   / AtoP
 |/   /
 A *   /

 tol = distance limit from line to pt P;
 dist = parametric distance from A to P' (in terms of A to B)
 @endverbatim
 *
 * @param p	point
 * @param a	start of lseg
 * @param b	end of lseg
 * @param tol	tolerance values
 * @param[out] dist	parametric distance from A to P' (in terms of A to B)
 */
BN_EXPORT extern int bn_isect_pt_lseg(fastf_t *dist,
				      const point_t a,
				      const point_t b,
				      const point_t p,
				      const struct bn_tol *tol);

BN_EXPORT extern double bn_dist_pt_lseg(point_t pca,
					const point_t a,
					const point_t b,
					const point_t p,
					const struct bn_tol *tol);

/**
 *@brief
 * Transform a bounding box (RPP) by the given 4x4 matrix.  There are
 * 8 corners to the bounding RPP.  Each one needs to be transformed
 * and min/max'ed.  This is not minimal, but does fully contain any
 * internal object, using an axis-aligned RPP.
 */
BN_EXPORT extern void bn_rotate_bbox(point_t omin,
				     point_t omax,
				     const mat_t mat,
				     const point_t imin,
				     const point_t imax);

/**
 *@brief
 * Transform a plane equation by the given 4x4 matrix.
 */
BN_EXPORT extern void bn_rotate_plane(plane_t oplane,
				      const mat_t mat,
				      const plane_t iplane);

/**
 *@brief
 * Test if two planes are identical.  If so, their dot products will
 * be either +1 or -1, with the distance from the origin equal in
 * magnitude.
 *
 * @return -1	not coplanar, parallel but distinct
 * @return 0	not coplanar, not parallel.  Planes intersect.
 * @return 1	coplanar, same normal direction
 * @return 2	coplanar, opposite normal direction
 */
BN_EXPORT extern int bn_coplanar(const plane_t a,
				 const plane_t b,
				 const struct bn_tol *tol);

/**
 * Using two perpendicular vectors (x_dir and y_dir) which lie in the
 * same plane as 'vec', return the angle (in radians) of 'vec' from
 * x_dir, going CCW around the perpendicular x_dir CROSS y_dir.
 *
 * Trig note -
 *
 * theta = atan2(x, y) returns an angle in the range -pi to +pi.
 * Here, we need an angle in the range of 0 to 2pi.  This could be
 * implemented by adding 2pi to theta when theta is negative, but this
 * could have nasty numeric ambiguity right in the vicinity of theta =
 * +pi, which is a very critical angle for the applications using this
 * routine.
 *
 * So, an alternative formulation is to compute gamma = atan2(-x, -y),
 * and then theta = gamma + pi.  Now, any error will occur in the
 * vicinity of theta = 0, which can be handled much more readily.
 *
 * If theta is negative, or greater than two pi, wrap it around.
 * These conditions only occur if there are problems in atan2().
 *
 * @return vec == x_dir returns 0,
 * @return vec == y_dir returns pi/2,
 * @return vec == -x_dir returns pi,
 * @return vec == -y_dir returns 3*pi/2.
 *
 * In all cases, the returned value is between 0 and bn_twopi.
 */
BN_EXPORT extern double bn_angle_measure(vect_t vec,
					 const vect_t x_dir,
					 const vect_t y_dir);

/**
 *@brief
 * Return the parametric distance t of a point X along a line defined
 * as a ray, i.e. solve X = P + t * D.  If the point X does not lie on
 * the line, then t is the distance of the perpendicular projection of
 * point X onto the line.
 */
BN_EXPORT extern double bn_dist_pt3_along_line3(const point_t p,
						const vect_t d,
						const point_t x);

/**
 *@brief
 * Return the parametric distance t of a point X along a line defined
 * as a ray, i.e. solve X = P + t * D.  If the point X does not lie on
 * the line, then t is the distance of the perpendicular projection of
 * point X onto the line.
 */
BN_EXPORT extern double bn_dist_pt2_along_line2(const point_t p,
						const vect_t d,
						const point_t x);

/**
 *
 * @return 1	if left <= mid <= right
 * @return 0	if mid is not in the range.
 */
BN_EXPORT extern int bn_between(double left,
				double mid,
				double right,
				const struct bn_tol *tol);

/**
 * @return 0	No intersection
 * @return 1	Intersection, 'inter' has intersect point.
 */
BN_EXPORT extern int bn_does_ray_isect_tri(const point_t pt,
					   const vect_t dir,
					   const point_t V,
					   const point_t A,
					   const point_t B,
					   point_t inter);

/**
 *@brief
 * Classify a halfspace, specified by its plane equation, against a
 * bounding RPP.
 *
 * @return BN_CLASSIFY_INSIDE
 * @return BN_CLASSIFY_OVERLAPPING
 * @return BN_CLASSIFY_OUTSIDE
 */
BN_EXPORT extern int bn_hlf_class(const plane_t half_eqn,
				  const vect_t min, const vect_t max,
				  const struct bn_tol *tol);


#define BN_CLASSIFY_UNIMPLEMENTED 0x0000
#define BN_CLASSIFY_INSIDE        0x0001
#define BN_CLASSIFY_OVERLAPPING   0x0002
#define BN_CLASSIFY_OUTSIDE       0x0003


/**
 *@brief
 * Calculates the point that is the minimum distance from all the
 * planes in the "planes" array.  If the planes intersect at a single
 * point, that point is the solution.
 *
 * The method used here is based on:

 * An expression for the distance from a point to a plane is:
 * VDOT(pt, plane)-plane[H].
 * Square that distance and sum for all planes to get the "total"
 * distance.
 * For minimum total distance, the partial derivatives of this
 * expression (with respect to x, y, and z) must all be zero.
 * This produces a set of three equations in three unknowns (x, y, z).

 * This routine sets up the three equations as [matrix][pt] = [hpq]
 * and solves by inverting "matrix" into "inverse" and
 * [pt] = [inverse][hpq].
 *
 * There is likely a more economical solution rather than matrix
 * inversion, but bn_mat_inv was handy at the time.
 *
 * Checks if these planes form a singular matrix and returns.
 *
 * @return 0 - all is well
 * @return 1 - planes form a singular matrix (no solution)
 */
BN_EXPORT extern int bn_isect_planes(point_t pt,
				     const plane_t planes[],
				     const size_t pl_count);

__END_DECLS

#endif  /* BN_PLANE_CALC_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
