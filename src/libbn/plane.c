/*                         P L A N E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup plane */
/** @{ */
/** @file libbn/plane.c
 *
 * @brief
 * Some useful routines for dealing with planes and lines.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"

#define UNIT_SQ_TOL 1.0e-13

/**
 * B N _ D I S T _ P T 3 _ P T 3
 * @brief
 * Returns distance between two points.
 */
double
bn_dist_pt3_pt3(const fastf_t *a, const fastf_t *b)
{
    vect_t diff;

    VSUB2(diff, a, b);
    return MAGNITUDE(diff);
}


/**
 * B N _ P T 3 _ P T 3 _ E Q U A L
 *
 * @return 1	if the two points are equal, within the tolerance
 * @return 0	if the two points are not "the same"
 */
int
bn_pt3_pt3_equal(const fastf_t *a, const fastf_t *b, const struct bn_tol *tol)
{
    vect_t diff;

    BN_CK_TOL(tol);
    VSUB2(diff, b, a);
    if (MAGSQ(diff) < tol->dist_sq) return 1;
    return 0;
}


/**
 * B N _ P T 2 _ P T 2 _ E Q U A L
 *
 * @return 1	if the two points are equal, within the tolerance
 * @return 0	if the two points are not "the same"
 */
int
bn_pt2_pt2_equal(const fastf_t *a, const fastf_t *b, const struct bn_tol *tol)
{
    vect_t diff;

    BN_CK_TOL(tol);
    VSUB2_2D(diff, b, a);
    if (MAGSQ_2D(diff) < tol->dist_sq) return 1;
    return 0;
}


/**
 * B N _ 3 P T S _ C O L L I N E A R
 * @brief
 * Check to see if three points are collinear.
 *
 * The algorithm is designed to work properly regardless of the order
 * in which the points are provided.
 *
 * @return 1	If 3 points are collinear
 * @return 0	If they are not
 */
int
bn_3pts_collinear(fastf_t *a, fastf_t *b, fastf_t *c, const struct bn_tol *tol)
{
    fastf_t mag_ab, mag_bc, mag_ca, max_len, dist_sq;
    fastf_t cos_a, cos_b, cos_c;
    vect_t ab, bc, ca;
    int max_edge_no;

    VSUB2(ab, b, a);
    VSUB2(bc, c, b);
    VSUB2(ca, a, c);
    mag_ab = MAGNITUDE(ab);
    mag_bc = MAGNITUDE(bc);
    mag_ca = MAGNITUDE(ca);

    /* find longest edge */
    max_len = mag_ab;
    max_edge_no = 1;

    if (mag_bc > max_len) {
	max_len = mag_bc;
	max_edge_no = 2;
    }

    if (mag_ca > max_len) {
	max_len = mag_ca;
	max_edge_no = 3;
    }

    switch (max_edge_no) {
	default:
	case 1:
	    cos_b = (-VDOT(ab, bc))/(mag_ab * mag_bc);
	    dist_sq = mag_bc*mag_bc*(1.0 - cos_b*cos_b);
	    break;
	case 2:
	    cos_c = (-VDOT(bc, ca))/(mag_bc * mag_ca);
	    dist_sq = mag_ca*mag_ca*(1.0 - cos_c*cos_c);
	    break;
	case 3:
	    cos_a = (-VDOT(ca, ab))/(mag_ca * mag_ab);
	    dist_sq = mag_ab*mag_ab*(1.0 - cos_a*cos_a);
	    break;
    }

    if (dist_sq <= tol->dist_sq)
	return 1;
    else
	return 0;
}


/**
 * B N _ 3 P T S _ D I S T I N C T
 *
 * Check to see if three points are all distinct, i.e., ensure that
 * there is at least sqrt(dist_tol_sq) distance between every pair of
 * points.
 *
 * @return 1 If all three points are distinct
 * @return 0 If two or more points are closer together than dist_tol_sq
 */
int
bn_3pts_distinct(const fastf_t *a, const fastf_t *b, const fastf_t *c, const struct bn_tol *tol)
{
    vect_t B_A;
    vect_t C_A;
    vect_t C_B;

    BN_CK_TOL(tol);
    VSUB2(B_A, b, a);
    if (MAGSQ(B_A) <= tol->dist_sq) return 0;
    VSUB2(C_A, c, a);
    if (MAGSQ(C_A) <= tol->dist_sq) return 0;
    VSUB2(C_B, c, b);
    if (MAGSQ(C_B) <= tol->dist_sq) return 0;
    return 1;
}


/**
 * B N _ N P T S _ D I S T I N C T
 *
 * Check to see if the points are all distinct, i.e., ensure that
 * there is at least sqrt(dist_tol_sq) distance between every pair of
 * points.
 *
 * @return 1 If all the points are distinct
 * @return 0 If two or more points are closer together than dist_tol_sq
 */
int
bn_npts_distinct(const int npt, const point_t *pts, const struct bn_tol *tol)
{
    int i, j;
    point_t r;

    BN_CK_TOL(tol);

    for (i=0;i<npt;i++)
	for (j=i+1;j<npt;j++) {
	    VSUB2(r, pts[i], pts[j]);
	    if (MAGSQ(r) <= tol->dist_sq)
		return 0;
	}
    return 1;
}


/**
 * B N _ M K _ P L A N E _ 3 P T S
 *
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
 *		or all three were colinear.
 *
 * @param[out]	plane	The plane equation is stored here.
 * @param[in]	a	point 1
 * @param[in]	b	point 2
 * @param[in]	c	point 3
 * @param[in]	tol	Tolerance values for doing calcualtion
 */
int
bn_mk_plane_3pts(fastf_t *plane,
		 const fastf_t *a,
		 const fastf_t *b,
		 const fastf_t *c,
		 const struct bn_tol *tol)
{
    vect_t B_A;
    vect_t C_A;
    vect_t C_B;
    register fastf_t mag;

    BN_CK_TOL(tol);

    VSUB2(B_A, b, a);
    if (MAGSQ(B_A) <= tol->dist_sq) return -1;
    VSUB2(C_A, c, a);
    if (MAGSQ(C_A) <= tol->dist_sq) return -1;
    VSUB2(C_B, c, b);
    if (MAGSQ(C_B) <= tol->dist_sq) return -1;

    VCROSS(plane, B_A, C_A);

    /* Ensure unit length normal */
    if ((mag = MAGNITUDE(plane)) <= SMALL_FASTF)
	return -1;	/* FAIL */
    mag = 1/mag;
    VSCALE(plane, plane, mag);

    /* Find distance from the origin to the plane */
    /* XXX Should do with pt that has smallest magnitude (closest to origin) */
    plane[3] = VDOT(plane, a);

    return 0;		/* OK */
}


/**
 * B N _ M K P O I N T _ 3 P L A N E S
 *@brief
 * Given the description of three planes, compute the point of
 * intersection, if any.
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
 *
 * @return 0	OK
 * @return -1	Failure.  Intersection is a line or plane.
 *
 * @param[out] pt	The point of intersection is stored here.
 * @param	a	plane 1
 * @param	b	plane 2
 * @param	c	plane 3
 */
int
bn_mkpoint_3planes(fastf_t *pt, const fastf_t *a, const fastf_t *b, const fastf_t *c)
{
    vect_t v1, v2, v3;
    register fastf_t det;

    /* Find a vector perpendicular to both planes B and C */
    VCROSS(v1, b, c);

    /* If that vector is perpendicular to A, then A is parallel to
     * either B or C, and no intersection exists.  This dot&cross
     * product is the determinant of the matrix M.  (I suspect there
     * is some deep significance to this!)
     */
    det = VDOT(a, v1);
    if (ZERO(det)) return -1;

    VCROSS(v2, a, c);
    VCROSS(v3, a, b);

    det = 1/det;
    pt[X] = det*(a[3]*v1[X] - b[3]*v2[X] + c[3]*v3[X]);
    pt[Y] = det*(a[3]*v1[Y] - b[3]*v2[Y] + c[3]*v3[Y]);
    pt[Z] = det*(a[3]*v1[Z] - b[3]*v2[Z] + c[3]*v3[Z]);
    return 0;
}


/**
 * B N _ 2 L I N E 3 _ C O L I N E A R
 * @brief
 * Returns non-zero if the 3 lines are colinear to within tol->dist
 * over the given distance range.
 *
 * Range should be at least one model diameter for most applications.
 * 1e5 might be OK for a default for "vehicle sized" models.
 *
 * The direction vectors do not need to be unit length.
 */
int
bn_2line3_colinear(const fastf_t *p1,
		   const fastf_t *d1,
		   const fastf_t *p2,
		   const fastf_t *d2,
		   double range,
		   const struct bn_tol *tol)
{
    fastf_t mag1;
    fastf_t mag2;
    point_t tail;

    BN_CK_TOL(tol);

    if (!p1 || !d1 || !p2 || !d2) {
	goto fail;
    }

    if ((mag1 = MAGNITUDE(d1)) < SMALL_FASTF) bu_bomb("bn_2line3_colinear() mag1 zero\n");
    if ((mag2 = MAGNITUDE(d2)) < SMALL_FASTF) bu_bomb("bn_2line3_colinear() mag2 zero\n");

    /* Impose a general angular tolerance to reject "obviously" non-parallel lines */
    /* tol->para and RT_DOT_TOL are too tight a tolerance.  0.1 is 5 degrees */
    if (fabs(VDOT(d1, d2)) < 0.9 * mag1 * mag2) goto fail;

    /* See if start points are within tolerance of other line */
    if (bn_distsq_line3_pt3(p1, d1, p2) > tol->dist_sq) goto fail;
    if (bn_distsq_line3_pt3(p2, d2, p1) > tol->dist_sq) goto fail;

    VJOIN1(tail, p1, range/mag1, d1);
    if (bn_distsq_line3_pt3(p2, d2, tail) > tol->dist_sq) goto fail;

    VJOIN1(tail, p2, range/mag2, d2);
    if (bn_distsq_line3_pt3(p1, d1, tail) > tol->dist_sq) goto fail;

    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_2line3colinear(range=%g) ret=1\n", range);
    }
    return 1;
fail:
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_2line3colinear(range=%g) ret=0\n", range);
    }
    return 0;
}


/**
 * B N _ I S E C T _ L I N E 3 _ P L A N E
 *
 * Intersect an infinite line (specified in point and direction vector
 * form) with a plane that has an outward pointing normal.  The
 * direction vector need not have unit length.  The first three
 * elements of the plane equation must form a unit lengh vector.
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
int
bn_isect_line3_plane(fastf_t *dist,
		     const fastf_t *pt,
		     const fastf_t *dir,
		     const fastf_t *plane,
		     const struct bn_tol *tol)
{
    register fastf_t slant_factor;
    register fastf_t norm_dist;
    register fastf_t dot;
    vect_t local_dir;

    BN_CK_TOL(tol);

    norm_dist = plane[3] - VDOT(plane, pt);
    slant_factor = VDOT(plane, dir);
    VMOVE(local_dir, dir)
	VUNITIZE(local_dir)
	dot = VDOT(plane, local_dir);

    if (slant_factor < -SMALL_FASTF && dot < -tol->perp) {
	*dist = norm_dist/slant_factor;
	return 1;			/* HIT, entering */
    } else if (slant_factor > SMALL_FASTF && dot > tol->perp) {
	*dist = norm_dist/slant_factor;
	return 2;			/* HIT, leaving */
    }

    /*
     * Ray is parallel to plane when dir.N == 0.
     */
    *dist = 0;		/* sanity */
    if (norm_dist < -tol->dist)
	return -2;	/* missed, outside */
    if (norm_dist > tol->dist)
	return -1;	/* missed, inside */
    return 0;		/* Ray lies in the plane */
}


/**
 * B N _ I S E C T _ 2 P L A N E S
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
 * @return 0	OK, line of intersection stored in `pt' and `dir'.
 * @return -1	FAIL, planes are identical (co-planar)
 * @return -2	FAIL, planes are parallel and distinct
 * @return -3	FAIL, unable to find line of intersection
 *
 * @param[out]	pt	Starting point of line of intersection
 * @param[out]	dir	Direction vector of line of intersection (unit length)
 * @param[in]	a	plane 1
 * @param[in]	b	plane 2
 * @param[in]	rpp_min	minimum poit of model RPP
 * @param[in]	tol	tolerance values
 */
int
bn_isect_2planes(fastf_t *pt,
		 fastf_t *dir,
		 const fastf_t *a,
		 const fastf_t *b,
		 const fastf_t *rpp_min,
		 const struct bn_tol *tol)
{
    vect_t abs_dir;
    plane_t pl;
    int i;

    if ((i = bn_coplanar(a, b, tol)) != 0) {
	if (i > 0)
	    return -1;	/* FAIL -- coplanar */
	return -2;		/* FAIL -- parallel & distinct */
    }

    /* Direction vector for ray is perpendicular to both plane
     * normals.
     */
    VCROSS(dir, a, b);
    VUNITIZE(dir);		/* safety */

    /* Select an axis-aligned plane which has its normal pointing
     * along the same axis as the largest magnitude component of the
     * direction vector.  If the largest magnitude component is
     * negative, reverse the direction vector, so that model is "in
     * front" of start point.
     */
    abs_dir[X] = (dir[X] >= 0) ? dir[X] : (-dir[X]);
    abs_dir[Y] = (dir[Y] >= 0) ? dir[Y] : (-dir[Y]);
    abs_dir[Z] = (dir[Z] >= 0) ? dir[Z] : (-dir[Z]);

    if (abs_dir[X] >= abs_dir[Y]) {
	if (abs_dir[X] >= abs_dir[Z]) {
	    VSET(pl, 1, 0, 0);	/* X */
	    pl[W] = rpp_min[X];
	    if (dir[X] < 0) {
		VREVERSE(dir, dir);
	    }
	} else {
	    VSET(pl, 0, 0, 1);	/* Z */
	    pl[W] = rpp_min[Z];
	    if (dir[Z] < 0) {
		VREVERSE(dir, dir);
	    }
	}
    } else {
	if (abs_dir[Y] >= abs_dir[Z]) {
	    VSET(pl, 0, 1, 0);	/* Y */
	    pl[W] = rpp_min[Y];
	    if (dir[Y] < 0) {
		VREVERSE(dir, dir);
	    }
	} else {
	    VSET(pl, 0, 0, 1);	/* Z */
	    pl[W] = rpp_min[Z];
	    if (dir[Z] < 0) {
		VREVERSE(dir, dir);
	    }
	}
    }

    /* Intersection of the 3 planes defines ray start point */
    if (bn_mkpoint_3planes(pt, pl, a, b) < 0)
	return -3;	/* FAIL -- no intersection */

    return 0;		/* OK */
}


/**
 * B N _ I S E C T _ L I N E 2 _ L I N E 2
 *
 * Intersect two lines, each in given in parametric form:
 @verbatim

 X = P + t * D
 and
 X = A + u * C

 @endverbatim
 *
 * While the parametric form is usually used to denote a ray (ie,
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
int
bn_isect_line2_line2(fastf_t *dist, const fastf_t *p, const fastf_t *d, const fastf_t *a, const fastf_t *c, const struct bn_tol *tol)
/* dist[2] */


{
    fastf_t hx, hy;		/* A - P */
    register fastf_t det;
    register fastf_t det1;
    vect_t unit_d;
    vect_t unit_c;
    vect_t unit_h;
    int parallel;
    int parallel1;

    BN_CK_TOL(tol);
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_isect_line2_line2() p=(%g, %g), d=(%g, %g)\n\t\t\ta=(%g, %g), c=(%g, %g)\n",
	       V2ARGS(p), V2ARGS(d), V2ARGS(a), V2ARGS(c));
    }

    /*
     * From the two components q and r, form a system of 2 equations
     * in 2 unknowns.  Solve for t and u in the system:
     *
     * Px + t * Dx = Ax + u * Cx
     * Py + t * Dy = Ay + u * Cy
     * or
     * t * Dx - u * Cx = Ax - Px
     * t * Dy - u * Cy = Ay - Py
     *
     * Let H = A - P, resulting in:
     *
     * t * Dx - u * Cx = Hx
     * t * Dy - u * Cy = Hy
     *
     * or
     *
     * [ Dx  -Cx ]   [ t ]   [ Hx ]
     * [         ] * [   ] = [    ]
     * [ Dy  -Cy ]   [ u ]   [ Hy ]
     *
     * This system can be solved by direct substitution, or by finding
     * the determinants by Cramers rule:
     *
     *	             [ Dx  -Cx ]
     *	det(M) = det [         ] = -Dx * Cy + Cx * Dy
     *	             [ Dy  -Cy ]
     *
     * If det(M) is zero, then the lines are parallel (perhaps
     * colinear).  Otherwise, exactly one solution exists.
     */
    det = c[X] * d[Y] - d[X] * c[Y];

    /*
     * det(M) is non-zero, so there is exactly one solution.  Using
     * Cramer's rule, det1(M) replaces the first column of M with the
     * constant column vector, in this case H.  Similarly, det2(M)
     * replaces the second column.  Computation of the determinant is
     * done as before.
     *
     * Now,
     *
     *	                  [ Hx  -Cx ]
     *	              det [         ]
     *	    det1(M)       [ Hy  -Cy ]   -Hx * Cy + Cx * Hy
     *	t = ------- = --------------- = ------------------
     *	     det(M) det(M)        -Dx * Cy + Cx * Dy
     *
     * and
     *
     *	                  [ Dx   Hx ]
     *	              det [         ]
     *	    det2(M)       [ Dy   Hy ]    Dx * Hy - Hx * Dy
     *	u = ------- = --------------- = ------------------
     *	     det(M) det(M)        -Dx * Cy + Cx * Dy
     */
    hx = a[X] - p[X];
    hy = a[Y] - p[Y];
    det1 = (c[X] * hy - hx * c[Y]);

    unit_d[0] = d[0];
    unit_d[1] = d[1];
    unit_d[2] = 0.0;
    VUNITIZE(unit_d);
    unit_c[0] = c[0];
    unit_c[1] = c[1];
    unit_c[2] = 0.0;
    VUNITIZE(unit_c);
    unit_h[0] = hx;
    unit_h[1] = hy;
    unit_h[2] = 0.0;
    VUNITIZE(unit_h);

    if (fabs(VDOT(unit_d, unit_c)) >= tol->para)
	parallel = 1;
    else
	parallel = 0;

    if (fabs(VDOT(unit_h, unit_c)) >= tol->para)
	parallel1 = 1;
    else
	parallel1 = 0;

    /* XXX This zero tolerance here should actually be
     * XXX determined by something like
     * XXX max(c[X], c[Y], d[X], d[Y]) / MAX_FASTF_DYNAMIC_RANGE
     * XXX In any case, nothing smaller than 1e-16
     */
#define DETERMINANT_TOL 1.0e-14		/* XXX caution on non-IEEE machines */
    if (parallel || NEAR_ZERO(det, DETERMINANT_TOL)) {
	/* Lines are parallel */
	if (!parallel1 && !NEAR_ZERO(det1, DETERMINANT_TOL)) {
	    /* Lines are NOT co-linear, just parallel */
	    if (bu_debug & BU_DEBUG_MATH) {
		bu_log("\tparallel, not co-linear.  det=%e, det1=%g\n", det, det1);
	    }
	    return -1;	/* parallel, no intersection */
	}

	/*
	 * Lines are co-linear.
	 * Determine t as distance from P to A.
	 * Determine u as distance from P to (A+C).  [special!]
	 * Use largest direction component, for numeric stability
	 * (and avoiding division by zero).
	 */
	if (fabs(d[X]) >= fabs(d[Y])) {
	    dist[0] = hx/d[X];
	    dist[1] = (hx + c[X]) / d[X];
	} else {
	    dist[0] = hy/d[Y];
	    dist[1] = (hy + c[Y]) / d[Y];
	}
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("\tcolinear, t = %g, u = %g\n", dist[0], dist[1]);
	}
	return 0;	/* Lines co-linear */
    }
    if (bu_debug & BU_DEBUG_MATH) {
	/* XXX This print is temporary */
	bu_log("\thx=%g, hy=%g, det=%g, det1=%g, det2=%g\n", hx, hy, det, det1, (d[X] * hy - hx * d[Y]));
    }
    det = 1/det;
    dist[0] = det * det1;
    dist[1] = det * (d[X] * hy - hx * d[Y]);
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("\tintersection, t = %g, u = %g\n", dist[0], dist[1]);
    }

    return 1;		/* Intersection found */
}


/**
 * B N _ I S E C T _ L I N E 2 _ L S E G 2
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
 * @return -2	Intersection exists, but outside segemnt, < A
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
int
bn_isect_line2_lseg2(fastf_t *dist,
		     const fastf_t *p,
		     const fastf_t *d,
		     const fastf_t *a,
		     const fastf_t *c,
		     const struct bn_tol *tol)
{
    register fastf_t f;
    fastf_t ctol;
    int ret;
    point_t b;

    BN_CK_TOL(tol);
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_isect_line2_lseg2() p=(%g, %g), pdir=(%g, %g)\n\t\t\ta=(%g, %g), adir=(%g, %g)\n",
	       V2ARGS(p), V2ARGS(d), V2ARGS(a), V2ARGS(c));
    }

    /* To keep the values of u between 0 and 1.  C should NOT be
     * scaled to have unit length.  However, it is a good idea to make
     * sure that C is a non-zero vector, (ie, that A and B are
     * distinct).
     */
    if ((ctol = MAGSQ_2D(c)) <= tol->dist_sq) {
	ret = -4;		/* points A and B are not distinct */
	goto out;
    }

    /* Detecting colinearity is difficult, and very very important.
     * As a first step, check to see if both points A and B lie within
     * tolerance of the line.  If so, then the line segment AC is ON
     * the line.
     */
    VADD2_2D(b, a, c);
    if (bn_distsq_line2_point2(p, d, a) <= tol->dist_sq  &&
	(ctol=bn_distsq_line2_point2(p, d, b)) <= tol->dist_sq) {
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("b=(%g, %g), b_dist_sq=%g\n", V2ARGS(b), ctol);
	    bu_log("bn_isect_line2_lseg2() pts A and B within tol of line\n");
	}
	/* Find the parametric distance along the ray */
	dist[0] = bn_dist_pt2_along_line2(p, d, a);
	dist[1] = bn_dist_pt2_along_line2(p, d, b);
	ret = 0;		/* Colinear */
	goto out;
    }

    if ((ret = bn_isect_line2_line2(dist, p, d, a, c, tol)) < 0) {
	/* Lines are parallel, non-colinear */
	ret = -3;		/* No intersection found */
	goto out;
    }
    if (ret == 0) {
	fastf_t dtol;
	/* Lines are colinear */
	/* If P within tol of either endpoint (0, 1), make exact. */
	dtol = tol->dist / sqrt(MAGSQ_2D(d));
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("bn_isect_line2_lseg2() dtol=%g, dist[0]=%g, dist[1]=%g\n",
		   dtol, dist[0], dist[1]);
	}
	if (dist[0] > -dtol && dist[0] < dtol) dist[0] = 0;
	else if (dist[0] > 1-dtol && dist[0] < 1+dtol) dist[0] = 1;

	if (dist[1] > -dtol && dist[1] < dtol) dist[1] = 0;
	else if (dist[1] > 1-dtol && dist[1] < 1+dtol) dist[1] = 1;
	ret = 0;		/* Colinear */
	goto out;
    }

    /* The two lines are claimed to intersect at a point.  First,
     * validate that hit point represented by dist[0] is in fact on
     * and between A--B.  (Nearly parallel lines can result in odd
     * situations here).  The performance hit of doing this is vastly
     * preferable to returning wrong answers.  Know a faster
     * algorithm?
     */
    {
	fastf_t ab_dist = 0;
	point_t hit_pt;
	point_t hit2;

	VJOIN1_2D(hit_pt, p, dist[0], d);
	VJOIN1_2D(hit2, a, dist[1], c);
	/* Check both hit point value calculations */
	if (bn_pt2_pt2_equal(a, hit_pt, tol) ||
	    bn_pt2_pt2_equal(a, hit2, tol)) {
	    dist[1] = 0;
	    ret = 1;	/* Intersect is at A */
	}
	if (bn_pt2_pt2_equal(b, hit_pt, tol) ||
	    bn_pt2_pt2_equal(b, hit_pt, tol)) {
	    dist[1] = 1;
	    ret = 2;	/* Intersect is at B */
	}

	ret = bn_isect_pt2_lseg2(&ab_dist, a, b, hit_pt, tol);
	if (bu_debug & BU_DEBUG_MATH) {
	    /* XXX This is temporary */
	    V2PRINT("a", a);
	    V2PRINT("hit", hit_pt);
	    V2PRINT("b", b);
	    bu_log("bn_isect_pt2_lseg2() hit2d=(%g, %g) ab_dist=%g, ret=%d\n", hit_pt[X], hit_pt[Y], ab_dist, ret);
	    bu_log("\tother hit2d=(%g, %g)\n", hit2[X], hit2[Y]);
	}
	if (ret <= 0) {
	    if (ab_dist < 0) {
		ret = -2;	/* Intersection < A */
	    } else {
		ret = -1;	/* Intersection >B */
	    }
	    goto out;
	}
	if (ret == 1) {
	    dist[1] = 0;
	    ret = 1;	/* Intersect is at A */
	    goto out;
	}
	if (ret == 2) {
	    dist[1] = 1;
	    ret = 2;	/* Intersect is at B */
	    goto out;
	}
	/* ret == 3, hit_pt is between A and B */

	if (!bn_between(a[X], hit_pt[X], b[X], tol) ||
	    !bn_between(a[Y], hit_pt[Y], b[Y], tol)) {
	    bu_bomb("bn_isect_line2_lseg2() hit_pt not between A and B!\n");
	}
    }

    /* If the dist[1] parameter is outside the range (0..1), reject
     * the intersection, because it falls outside the line segment
     * A--B.
     *
     * Convert the tol->dist into allowable deviation in terms of
     * (0..1) range of the parameters.
     */
    ctol = tol->dist / sqrt(ctol);
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_isect_line2_lseg2() ctol=%g, dist[1]=%g\n", ctol, dist[1]);
    }
    if (dist[1] < -ctol) {
	ret = -2;		/* Intersection < A */
	goto out;
    }
    if ((f=(dist[1]-1)) > ctol) {
	ret = -1;		/* Intersection > B */
	goto out;
    }

    /* Check for ctoly intersection with one of the verticies */
    if (dist[1] < ctol) {
	dist[1] = 0;
	ret = 1;		/* Intersection at A */
	goto out;
    }
    if (f >= -ctol) {
	dist[1] = 1;
	ret = 2;		/* Intersection at B */
	goto out;
    }
    ret = 3;			/* Intersection between A and B */
out:
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_isect_line2_lseg2() dist[0]=%g, dist[1]=%g, ret=%d\n",
	       dist[0], dist[1], ret);
    }
    return ret;
}


/**
 * B N _ I S E C T _ L S E G 2 _ L S E G 2
 *@brief
 * Intersect two 2D line segments, defined by two points and two
 * vectors.  The vectors are unlikely to be unit length.
 *
 * @return -2	missed (line segments are parallel)
 * @return -1	missed (colinear and non-overlapping)
 * @return 0	hit (line segments colinear and overlapping)
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
int
bn_isect_lseg2_lseg2(fastf_t *dist,
		     const fastf_t *p,
		     const fastf_t *pdir,
		     const fastf_t *q,
		     const fastf_t *qdir,
		     const struct bn_tol *tol)
{
    fastf_t ptol, qtol;	/* length in parameter space == tol->dist */
    int status;

    BN_CK_TOL(tol);
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_isect_lseg2_lseg2() p=(%g, %g), pdir=(%g, %g)\n\t\tq=(%g, %g), qdir=(%g, %g)\n",
	       V2ARGS(p), V2ARGS(pdir), V2ARGS(q), V2ARGS(qdir));
    }

    status = bn_isect_line2_line2(dist, p, pdir, q, qdir, tol);
    if (status < 0) {
	/* Lines are parallel, non-colinear */
	return -1;	/* No intersection */
    }
    if (status == 0) {
	int nogood = 0;
	/* Lines are colinear */
	/* If P within tol of either endpoint (0, 1), make exact. */
	ptol = tol->dist / sqrt(MAGSQ_2D(pdir));
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("ptol=%g\n", ptol);
	}
	if (dist[0] > -ptol && dist[0] < ptol) dist[0] = 0;
	else if (dist[0] > 1-ptol && dist[0] < 1+ptol) dist[0] = 1;

	if (dist[1] > -ptol && dist[1] < ptol) dist[1] = 0;
	else if (dist[1] > 1-ptol && dist[1] < 1+ptol) dist[1] = 1;

	if (dist[1] < 0 || dist[1] > 1) nogood = 1;
	if (dist[0] < 0 || dist[0] > 1) nogood++;
	if (nogood >= 2)
	    return -1;	/* colinear, but not overlapping */
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("  HIT colinear!\n");
	}
	return 0;		/* colinear and overlapping */
    }
    /* Lines intersect */
    /* If within tolerance of an endpoint (0, 1), make exact. */
    ptol = tol->dist / sqrt(MAGSQ_2D(pdir));
    if (dist[0] > -ptol && dist[0] < ptol) dist[0] = 0;
    else if (dist[0] > 1-ptol && dist[0] < 1+ptol) dist[0] = 1;

    qtol = tol->dist / sqrt(MAGSQ_2D(qdir));
    if (dist[1] > -qtol && dist[1] < qtol) dist[1] = 0;
    else if (dist[1] > 1-qtol && dist[1] < 1+qtol) dist[1] = 1;

    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("ptol=%g, qtol=%g\n", ptol, qtol);
    }
    if (dist[0] < 0 || dist[0] > 1 || dist[1] < 0 || dist[1] > 1) {
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("  MISS\n");
	}
	return -1;		/* missed */
    }
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("  HIT!\n");
    }
    return 1;			/* hit, normal intersection */
}


#ifdef TRI_PROTOTYPE
/**
 * B N _ I S E C T _ L S E G 3 _ L S E G 3 _ N E W
 *@brief
 * Intersect two 3D line segments, defined by two points and two
 * vectors.  The vectors are unlikely to be unit length.
 *
 *
 * @return -3	missed
 * @return -2	missed (line segments are parallel)
 * @return -1	missed (colinear and non-overlapping)
 * @return 0	hit (line segments colinear and overlapping)
 * @return 1	hit (normal intersection)
 *
 * @param[out] dist
 *	The value at dist[] is set to the parametric distance of the
 *	intercept dist[0] is parameter along p, range 0 to 1, to
 *	intercept.  dist[1] is parameter along q, range 0 to 1, to
 *	intercept.  If within distance tolerance of the endpoints,
 *	these will be exactly 0.0 or 1.0, to ease the job of caller.
 *
 *      CLARIFICATION: This function 'bn_isect_lseg3_lseg3_new'
 *      returns distance values scaled where an intersect at the start
 *      point of the line segement (within tol->dist) results in 0.0
 *      and when the intersect is at the end point of the line
 *      segement (within tol->dist), the result is 1.0.  Intersects
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
int
bn_isect_lseg3_lseg3_new(fastf_t *dist,
		         const fastf_t *p,
		         const fastf_t *pdir,
		         const fastf_t *q,
		         const fastf_t *qdir,
		         const struct bn_tol *tol)
{
    fastf_t ptol, qtol;	/* length in parameter space == tol->dist */
    fastf_t pmag, qmag;
    int status;

    BN_CK_TOL(tol);
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_isect_lseg3_lseg3_new() p=(%g, %g, %g), pdir=(%g, %g, %g)\n\t\tq=(%g, %g, %g), qdir=(%g, %g, %g)\n",
	       V3ARGS(p), V3ARGS(pdir), V3ARGS(q), V3ARGS(qdir));
    }

    status = bn_isect_line3_line3_new(&dist[0], &dist[1], p, pdir, q, qdir, tol);

    /* It is expected that dist[0] and dist[1] returned from
     * 'bn_isect_line3_line3_new' are the actual distance to the
     * intersect, i.e. not scaled. Distances in the opposite
     * of the line direction vector result in a negative distance.
     */

    /* sanity check */
    if (status < -2 || status > 1) {
        bu_bomb("bn_isect_lseg3_lseg3_new() function 'bn_isect_line3_line3_new' returned an invalid status\n");
    }

    if (status == -1) {
        /* Infinite lines do not intersect and are not parallel
         * therefore line segments do not intersect and are not
         * parallel.
         */
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("bn_isect_lseg3_lseg3_new(): MISS, line segments do not intersect and are not parallel\n");
	}
	return -3; /* missed */
    }

    if (status == -2) {
        /* infinite lines do not intersect, they are parallel */
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("bn_isect_lseg3_lseg3_new(): MISS, line segments are parallel, i.e. do not intersect\n");
	}
	return -2; /* missed (line segments are parallel) */
    }

    pmag = MAGNITUDE(pdir);
    qmag = MAGNITUDE(qdir);

    if (pmag < SMALL_FASTF)
	bu_bomb("bn_isect_lseg3_lseg3_new(): |p|=0\n");

    if (qmag < SMALL_FASTF)
	bu_bomb("bn_isect_lseg3_lseg3_new(): |q|=0\n");

    ptol = tol->dist / pmag;
    qtol = tol->dist / qmag;
    dist[0] = dist[0] / pmag;
    if (status == 0) {  /* infinite lines are colinear */
        /* When line segments are colinear, dist[1] has an alternate
         * interpretation: it's the parameter along p (not q)
         * therefore dist[1] must be scaled by pmag not qmag.
         */
        dist[1] = dist[1] / pmag;
    } else {
        dist[1] = dist[1] / qmag;
    }

    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("ptol=%g, qtol=%g\n", ptol, qtol);
    }

    /* If 'p' within tol of either endpoint (0.0, 1.0), make exact. */
    if (dist[0] > -ptol && dist[0] < ptol) {
        dist[0] = 0.0;
    } else if (dist[0] > 1.0-ptol && dist[0] < 1.0+ptol) {
        dist[0] = 1.0;
    }

    /* If 'q' within tol of either endpoint (0.0, 1.0), make exact. */
    if (dist[1] > -qtol && dist[1] < qtol) {
        dist[1] = 0.0;
    } else if (dist[1] > 1.0-qtol && dist[1] < 1.0+qtol) {
        dist[1] = 1.0;
    }

    if (status == 0) {  /* infinite lines are colinear */
	/* Lines are colinear */
        if ((dist[0] > 1.0+ptol && dist[1] > 1.0+ptol) || (dist[0] < -ptol && dist[1] < -ptol)) {
	    if (bu_debug & BU_DEBUG_MATH) {
	        bu_log("bn_isect_lseg3_lseg3_new(): MISS, line segments are colinear but not overlapping!\n");
	    }
            return -1;   /* line segments are colinear but not overlapping */
        }

	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("bn_isect_lseg3_lseg3_new(): HIT, line segments are colinear and overlapping!\n");
	}

	return 0; /* line segments are colinear and overlapping */
    }

    /* At this point we know the infinite lines intersect and are not colinear */


    if (dist[0] <= -ptol || dist[0] >= 1.0+ptol || dist[1] <= -qtol || dist[1] >= 1.0+qtol) {
        if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("bn_isect_lseg3_lseg3_new(): MISS, infinite lines intersect but line segments do not!\n");
        }
        return -3;  /* missed, infinite lines intersect but line segments do not */
    }

    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_isect_lseg3_lseg3_new(): HIT, line segments intersect!\n");
    }

    /* sanity check */
    if (dist[0] < 0.0 || dist[0] > 1.0 || dist[1] < 0.0 || dist[1] > 1.0) {
        bu_bomb("bn_isect_lseg3_lseg3_new(): INTERNAL ERROR, intersect distance values must be in the range 0-1\n");
    }

    return 1; /* hit, line segments intersect */
}
#endif


/**
 * B N _ I S E C T _ L S E G 3 _ L S E G 3
 *@brief
 * Intersect two 3D line segments, defined by two points and two
 * vectors.  The vectors are unlikely to be unit length.
 *
 *
 * @return -2	missed (line segments are parallel)
 * @return -1	missed (colinear and non-overlapping)
 * @return 0	hit (line segments colinear and overlapping)
 * @return 1	hit (normal intersection)
 *
 * @param[out] dist
 *	The value at dist[] is set to the parametric distance of the
 *	intercept dist[0] is parameter along p, range 0 to 1, to
 *	intercept.  dist[1] is parameter along q, range 0 to 1, to
 *	intercept.  If within distance tolerance of the endpoints,
 *	these will be exactly 0.0 or 1.0, to ease the job of caller.
 *
 * Special note: when return code is "0" for co-linearity, dist[1] has
 * an alternate interpretation: it's the parameter along p (not q)
 * which takes you from point p to the point (q + qdir), i.e., it's
 * the endpoint of the q linesegment, since in this case there may be
 * *two* intersections, if q is contained within span p to (p + pdir).
 * And either may be -10 if the point is outside the span.
 *
 * @param p	point 1
 * @param pdir	direction-1
 * @param q	point 2
 * @param qdir	direction-2
 * @param tol	tolerance values
 */
int
bn_isect_lseg3_lseg3(fastf_t *dist,
		     const fastf_t *p,
		     const fastf_t *pdir,
		     const fastf_t *q,
		     const fastf_t *qdir,
		     const struct bn_tol *tol)
{
    fastf_t ptol, qtol;	/* length in parameter space == tol->dist */
    fastf_t pmag, qmag;
    int status;

    BN_CK_TOL(tol);
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_isect_lseg3_lseg3() p=(%g, %g), pdir=(%g, %g)\n\t\tq=(%g, %g), qdir=(%g, %g)\n",
	       V2ARGS(p), V2ARGS(pdir), V2ARGS(q), V2ARGS(qdir));
    }

    status = bn_isect_line3_line3(&dist[0], &dist[1], p, pdir, q, qdir, tol);
    if (status < 0) {
	/* Lines are parallel, non-colinear */
	return -1;	/* No intersection */
    }
    pmag = MAGNITUDE(pdir);
    if (pmag < SMALL_FASTF)
	bu_bomb("bn_isect_lseg3_lseg3: |p|=0\n");
    if (status == 0) {
	int nogood = 0;
	/* Lines are colinear */
	/* If P within tol of either endpoint (0, 1), make exact. */
	ptol = tol->dist / pmag;
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("ptol=%g\n", ptol);
	}
	if (dist[0] > -ptol && dist[0] < ptol) dist[0] = 0;
	else if (dist[0] > 1-ptol && dist[0] < 1+ptol) dist[0] = 1;

	if (dist[1] > -ptol && dist[1] < ptol) dist[1] = 0;
	else if (dist[1] > 1-ptol && dist[1] < 1+ptol) dist[1] = 1;

	if (dist[1] < 0 || dist[1] > 1) nogood = 1;
	if (dist[0] < 0 || dist[0] > 1) nogood++;
	if (nogood >= 2)
	    return -1;	/* colinear, but not overlapping */
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("  HIT colinear!\n");
	}
	return 0;		/* colinear and overlapping */
    }
    /* Lines intersect */
    /* If within tolerance of an endpoint (0, 1), make exact. */
    ptol = tol->dist / pmag;
    if (dist[0] > -ptol && dist[0] < ptol) dist[0] = 0;
    else if (dist[0] > 1-ptol && dist[0] < 1+ptol) dist[0] = 1;

    qmag = MAGNITUDE(qdir);
    if (qmag < SMALL_FASTF)
	bu_bomb("bn_isect_lseg3_lseg3: |q|=0\n");
    qtol = tol->dist / qmag;
    if (dist[1] > -qtol && dist[1] < qtol) dist[1] = 0;
    else if (dist[1] > 1-qtol && dist[1] < 1+qtol) dist[1] = 1;

    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("ptol=%g, qtol=%g\n", ptol, qtol);
    }
    if (dist[0] < 0 || dist[0] > 1 || dist[1] < 0 || dist[1] > 1) {
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("  MISS\n");
	}
	return -1;		/* missed */
    }
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("  HIT!\n");
    }
    return 1;			/* hit, normal intersection */
}


#ifdef TRI_PROTOTYPE
/**
 * B N _ I S E C T _ L I N E 3 _ L I N E 3 _ N E W
 *
 * Intersect two lines, each in given in parametric form:
 *
 * X = p + s * u
 * and
 * X = q + t * v
 *
 * While the parametric form is usually used to denote a ray (ie,
 * positive values of the parameter only), in this case the full line
 * is considered.
 *
 * The direction vectors u and v need not have unit length.
 *
 * @return -2	no intersection, lines are parallel.
 * @return -1	no intersection
 * @return 0	lines are co-linear (s returned for t=0 to give distance to q0)
 * @return 1	intersection found (s and t returned)
 *
 * @param[out]	s, t	line parameter of interseciton
 *		When explicit return >= 0, s and t are the
 *		line parameters of the intersection point on the 2 rays.
 *		The actual intersection coordinates can be found by
 *		substituting either of these into the original ray equations.
 *
 * @param	p0	point 1
 * @param	u	direction 1
 * @param	q0	point 2
 * @param	v	direction 2
 * @param tol	tolerance values
 *
 *	s, t	When explicit return >= 0, s and t are the
 *		line parameters of the intersection point on the 2 rays.
 *		The actual intersection coordinates can be found by
 *		substituting either of these into the original ray equations.
 *
 * XXX It would be sensible to change the s, t pair to dist[2].
 */
int
bn_isect_line3_line3_new(fastf_t *s,
		         fastf_t *t,
		         const fastf_t *p0,
		         const fastf_t *u,
		         const fastf_t *q0,
		         const fastf_t *v,
		         const struct bn_tol *tol)
{
    fastf_t a, b, c, d, e, sc, tc, sc_numerator, tc_numerator, denominator;
    vect_t w0, qc_to_pc, u_scaled, v_scaled, v_scaled_to_u_scaled, tmp_vec, p0_to_q1;
    point_t q_intersect, p0_to_q_intersect, p1, q1;

    int parallel1 = 0;
    int parallel2 = 0;
    int colinear = 0;
    vect_t u_unit, v_unit;
    fastf_t p_dot, d1,d2,d3,d4;

    VMOVE(u_unit,u);
    VMOVE(v_unit,v);
    VUNITIZE(u_unit);
    VUNITIZE(v_unit);

    p_dot = VDOT(u_unit,v_unit);
    parallel1 = BN_VECT_ARE_PARALLEL(p_dot,tol);

    a = MAGSQ(u);
    c = MAGSQ(v);

    if (NEAR_ZERO(a, SMALL_FASTF) || NEAR_ZERO(c, SMALL_FASTF)) {
        bu_log("p0 = %g %g %g\n", V3ARGS(p0));
        bu_log("u = %g %g %g\n", V3ARGS(u));
        bu_log("q0 = %g %g %g\n", V3ARGS(q0));
        bu_log("v = %g %g %g\n", V3ARGS(v));
        bu_bomb("bn_isect_line3_line3_new(): input vector(s) 'u' and/or 'v' is zero magnitude.\n");
    }

    *s = 0.0;
    *t = 0.0;

    VADD2(p1, p0, u);
    VADD2(q1, q0, v);
    VSUB2(p0_to_q1, q1, p0);

#if 0
    bu_log("p0 = %g %g %g\n", V3ARGS(p0));
    bu_log("p1 = %g %g %g\n", V3ARGS(p1));
    bu_log("u = %g %g %g\n", V3ARGS(u));
    bu_log("q0 = %g %g %g\n", V3ARGS(q0));
    bu_log("q1 = %g %g %g\n", V3ARGS(q1));
    bu_log("v = %g %g %g\n", V3ARGS(v));
#endif

    d1 = bn_dist_line3_pt3(q0,v,p0);
    d2 = bn_dist_line3_pt3(q0,v,p1);
    d3 = bn_dist_line3_pt3(p0,u,q0);
    d4 = bn_dist_line3_pt3(p0,u,q1);

    /* if all distances are within distance tolerance of each
     * other then they a parallel 
     */
    if (NEAR_EQUAL(d1, d2, tol->dist) &&
        NEAR_EQUAL(d1, d3, tol->dist) &&
        NEAR_EQUAL(d1, d4, tol->dist) &&
        NEAR_EQUAL(d2, d3, tol->dist) &&
        NEAR_EQUAL(d2, d4, tol->dist) &&
        NEAR_EQUAL(d3, d4, tol->dist)) {
#if 0
        bu_log("all values within distance tolerance of each other\n");
#endif
        parallel2 = 1;
    }

    if (NEAR_ZERO(d1, tol->dist) &&
        NEAR_ZERO(d2, tol->dist) &&
        NEAR_ZERO(d3, tol->dist) &&
        NEAR_ZERO(d4, tol->dist)) {
#if 0
        bu_log("all values within distance tolerance of each other\n");
#endif
        colinear = 1;
    }

#if 0
    if (parallel1 != parallel2) {
    bu_log("p0 = %g %g %g\n", V3ARGS(p0));
    bu_log("p1 = %g %g %g\n", V3ARGS(p1));
    bu_log("u = %g %g %g\n", V3ARGS(u));
    bu_log("q0 = %g %g %g\n", V3ARGS(q0));
    bu_log("q1 = %g %g %g\n", V3ARGS(q1));
    bu_log("v = %g %g %g\n", V3ARGS(v));
    bu_log("p_dot = %.15f\n", p_dot);
    bu_log("dist p0 to line q0-v = %g\n", d1);
    bu_log("dist p1 to line q0-v = %g\n", d2);
    bu_log("dist q0 to line p0-u = %g\n", d3);
    bu_log("dist q1 to line p0-u = %g\n", d4);
    bu_log("parallel1 = %d parallel2 = %d\n", parallel1, parallel2);
        bu_bomb("parallel1 != parallel2\n");
    }
#endif
#if 0
    bu_log("dist p0 to line q0-v = %g\n", d1);
    bu_log("dist p1 to line q0-v = %g\n", d2);
    bu_log("dist q0 to line p0-u = %g\n", d3);
    bu_log("dist q1 to line p0-u = %g\n", d4);
#endif

    VSUB2(w0, p0, q0);
    b = VDOT(u, v);
    d = VDOT(u, w0);
    e = VDOT(v, w0);
    denominator = a * c - b * b;

#if 0
    if (NEAR_ZERO(denominator, VUNITIZE_TOL)) {
#endif

#if 0
    if ((denominator > SMALL_FASTF && denominator < VUNITIZE_TOL) ||
        (denominator < -SMALL_FASTF && denominator > -VUNITIZE_TOL)) {
        bu_log("denominator btwn SMALL_FASTF VUNITIZE_TOL == %g\n", denominator);
        bu_log("a = %.15f c = %.15f b = %.15f\n", a, c, b);
        bu_log("a * c = %.15f b * b = %.15f\n", a * c, b * b);
    }
#endif

#if 0
    if (NEAR_ZERO(denominator, SMALL_FASTF)) {
#endif
    if (parallel2) {
#if 0
        if (!parallel2) {
    bu_log("p0 = %g %g %g\n", V3ARGS(p0));
    bu_log("p1 = %g %g %g\n", V3ARGS(p1));
    bu_log("u = %g %g %g\n", V3ARGS(u));
    bu_log("q0 = %g %g %g\n", V3ARGS(q0));
    bu_log("q1 = %g %g %g\n", V3ARGS(q1));
    bu_log("v = %g %g %g\n", V3ARGS(v));
            bu_log("new true, old false; denominator = %g p_dot = %g\n", denominator, p_dot);
            bu_bomb("stop\n");
        }
#endif
        /* lines are parallel */
#if 0
        tc = d/b; /* or tc = e/c */
#endif
        sc = d/a;
        tc = 0.0;
        VSCALE(q_intersect, v, tc);
        VADD2(q_intersect, q_intersect, q0);
        VSUB2(p0_to_q_intersect, q_intersect, p0);

#if 0
        if (MAGSQ(p0_to_q_intersect) <= (tol->dist_sq)) {
#endif
        if (colinear) {
            if (!colinear) {
    bu_log("dist p0 to line q0-v = %g\n", d1);
    bu_log("dist p1 to line q0-v = %g\n", d2);
    bu_log("dist q0 to line p0-u = %g\n", d3);
    bu_log("dist q1 to line p0-u = %g\n", d4);
                bu_bomb("colinear new true, old false\n");
            }
            *s = sc * sqrt(a);
            *t = 0.0;

            *t = MAGNITUDE(p0_to_q1) / sqrt(a);

            p_dot = VDOT(u, p0_to_q1);
            if (p_dot < -SMALL_FASTF) {
                *t = *t * -1.0;
            }

#if 0
            bu_log("colinear, distance = %.15f s = %.15f denom = %.15f\n", MAGNITUDE(p0_to_q_intersect), *s, denominator);
            bu_log("a = %.15f c = %.15f b = %.15f\n", a, c, b);
            bu_log("p0 = %g %g %g\n", V3ARGS(p0));
            bu_log("u = %g %g %g\n", V3ARGS(u));
            bu_log("q0 = %g %g %g\n", V3ARGS(q0));
            bu_log("v = %g %g %g\n", V3ARGS(v));
#endif
            return 0;  /* lines are co-linear (s returned for t=0 to give distance to q0) */
        } else {
#if 0
            *s = tc * sqrt(a);
            *t = 0.0;
            bu_log("parallel not-colinear, distance = %.15f dist p0-2-q0 = %.15f denom = %.15f\n", MAGNITUDE(p0_to_q_intersect), *s, denominator);
            bu_log("a = %.15f c = %.15f b = %.15f\n", a, c, b);
            bu_log("p0 = %g %g %g\n", V3ARGS(p0));
            bu_log("u = %g %g %g\n", V3ARGS(u));
            bu_log("q0 = %g %g %g\n", V3ARGS(q0));
            bu_log("v = %g %g %g\n", V3ARGS(v));
#endif
            return -2; /* no intersection, lines are parallel */
        }
    }

    if (parallel2) {
        bu_log("new false, old true; denominator = %g p_dot = %g\n", denominator, p_dot);
    }

    sc_numerator = (b * e - c * d);
    tc_numerator = (a * e - b * d);
    sc = sc_numerator / denominator;
    tc = tc_numerator / denominator;

    VSCALE(u_scaled, u, sc_numerator);
    VSCALE(v_scaled, v, tc_numerator);
    VSUB2(v_scaled_to_u_scaled, u_scaled, v_scaled);
    VSCALE(tmp_vec, v_scaled_to_u_scaled, 1.0/denominator);
    VADD2(qc_to_pc, w0, tmp_vec);

    if (MAGSQ(qc_to_pc) <= tol->dist_sq) {
        *s = sc * sqrt(a);
        *t = tc * sqrt(c);
#if 0
            bu_log("intersect, min dist btwn lines = %.15f s = %.15f t = %.15f denom = %.15f\n", MAGNITUDE(qc_to_pc), *s, *t, denominator);
            bu_log("a = %.15f c = %.15f b = %.15f\n", a, c, b);
            bu_log("p0_to_q1 mag = %.15f\n", MAGNITUDE(p0_to_q1));
            bu_log("p0 = %g %g %g\n", V3ARGS(p0));
            bu_log("p1 = %g %g %g\n", V3ARGS(p1));
            bu_log("u = %g %g %g\n", V3ARGS(u));
            bu_log("q0 = %g %g %g\n", V3ARGS(q0));
            bu_log("q1 = %g %g %g\n", V3ARGS(q1));
            bu_log("v = %g %g %g\n", V3ARGS(v));
#endif
        return 1; /* intersection found (s and t returned) */
    } else {
        return -1; /* no intersection */
    }
}
#endif


/**
 * B N _ I S E C T _ L I N E 3 _ L I N E 3
 *
 * Intersect two lines, each in given in parametric form:
 *
 * X = P + t * D
 * and
 * X = A + u * C
 *
 * While the parametric form is usually used to denote a ray (ie,
 * positive values of the parameter only), in this case the full line
 * is considered.
 *
 * The direction vectors C and D need not have unit length.
 *
 * @return -2	no intersection, lines are parallel.
 * @return -1	no intersection
 * @return 0	lines are co-linear (t returned for u=0 to give distance to A)
 * @return 1	intersection found (t and u returned)
 *
 * @param[out]	t, u	line parameter of interseciton
 *		When explicit return >= 0, t and u are the
 *		line parameters of the intersection point on the 2 rays.
 *		The actual intersection coordinates can be found by
 *		substituting either of these into the original ray equations.

 * @param	p	point 1
 * @param	d	direction 1
 * @param	a	point 2
 * @param	c	direction 2
 * @param tol	tolerance values
 *
 *	t, u	When explicit return >= 0, t and u are the
 *		line parameters of the intersection point on the 2 rays.
 *		The actual intersection coordinates can be found by
 *		substituting either of these into the original ray equations.
 *
 * XXX It would be sensible to change the t, u pair to dist[2].
 */
int
bn_isect_line3_line3(fastf_t *t,
		     fastf_t *u,
		     const fastf_t *p,
		     const fastf_t *d,
		     const fastf_t *a,
		     const fastf_t *c,
		     const struct bn_tol *tol)
{
    vect_t n;
    vect_t abs_n;
    vect_t h;
    register fastf_t det;
    register fastf_t det1;
    register short int q, r, s;
    int parallel = 0;
    int colinear = 0;


    BN_CK_TOL(tol);

    if (NEAR_ZERO(MAGSQ(c), VUNITIZE_TOL) || NEAR_ZERO(MAGSQ(d), VUNITIZE_TOL)) {
        bu_bomb("bn_isect_line3_line3(): input vector(s) 'c' and/or 'd' is zero magnitude.\n");
    }

    /* Any intersection will occur in the plane with surface normal D
     * cross C, which may not have unit length.  The plane containing
     * the two lines will be a constant distance from a plane with the
     * same normal that contains the origin.  Therfore, the projection
     * of any point on the plane along N has the same length.  Verify
     * that this holds for P and A.  If N dot P != N dot A, there is
     * no intersection, because P and A must lie on parallel planes
     * that are different distances from the origin.
     */

    VCROSS(n, d, c);
    det = VDOT(n, p) - VDOT(n, a);
    if (!NEAR_ZERO(det, tol->perp)) {
	return -1; /* no intersection, lines not in same plane */
    }

    if (NEAR_ZERO(MAGSQ(n), VUNITIZE_TOL)) {
	/* lines are parallel, must find another way to get normal vector */
	vect_t a_to_p;

	parallel = 1;
	VSUB2(a_to_p, p, a);
	VCROSS(n, a_to_p, d);

	if (NEAR_ZERO(MAGSQ(n), VUNITIZE_TOL)) {
	    /* lines are parallel and colinear */

            colinear = 1;
	    bn_vec_ortho(n, d);
	}
    }

    if (NEAR_ZERO(MAGSQ(n), VUNITIZE_TOL)) {
	bu_bomb("bn_isect_line3_line3(): ortho vector zero magnitude\n"); 
    }

    /* Solve for t and u in the system:
     *
     * Px + t * Dx = Ax + u * Cx
     * Py + t * Dy = Ay + u * Cy
     * Pz + t * Dz = Az + u * Cz
     *
     * This system is over-determined, having 3 equations in 2
     * unknowns.  However, the intersection problem is really only a
     * 2-dimensional problem, being located in the surface of a plane.
     * Therefore, the "least important" of these equations can be
     * initially ignored, leaving a system of 2 equations in 2
     * unknowns.
     *
     * Find the component of N with the largest magnitude.  This
     * component will have the least effect on the parameters in the
     * system, being most nearly perpendicular to the plane.  Denote
     * the two remaining components by the subscripts q and r, rather
     * than x, y, z.  Subscript s is the smallest component, used for
     * checking later.
     */
    if (ZERO(n[X])) {
        n[X] = 0.0;
    }
    if (ZERO(n[Y])) {
        n[Y] = 0.0;
    }
    if (ZERO(n[Z])) {
        n[Z] = 0.0;
    }
    abs_n[X] = (n[X] >= 0) ? n[X] : (-n[X]);
    abs_n[Y] = (n[Y] >= 0) ? n[Y] : (-n[Y]);
    abs_n[Z] = (n[Z] >= 0) ? n[Z] : (-n[Z]);

    if (abs_n[X] >= abs_n[Y]) {
	if (abs_n[X] >= abs_n[Z]) {
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
	if (abs_n[Y] >= abs_n[Z]) {
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
     * From the two components q and r, form a system of 2 equations
     * in 2 unknowns:
     *
     * Pq + t * Dq = Aq + u * Cq
     * Pr + t * Dr = Ar + u * Cr
     * or
     * t * Dq - u * Cq = Aq - Pq
     * t * Dr - u * Cr = Ar - Pr
     *
     * Let H = A - P, resulting in:
     *
     * t * Dq - u * Cq = Hq
     * t * Dr - u * Cr = Hr
     *
     * or
     *
     * [ Dq  -Cq ]   [ t ]   [ Hq ]
     * [         ] * [   ] = [    ]
     * [ Dr  -Cr ]   [ u ]   [ Hr ]
     *
     * This system can be solved by direct substitution, or by finding
     * the determinants by Cramers rule:
     *
     *	             [ Dq  -Cq ]
     *	det(M) = det [         ] = -Dq * Cr + Cq * Dr
     *	             [ Dr  -Cr ]
     *
     * If det(M) is zero, then the lines are parallel (perhaps
     * colinear).  Otherwise, exactly one solution exists.
     */
    VSUB2(h, a, p);
    det = c[q] * d[r] - d[q] * c[r];
    det1 = (c[q] * h[r] - h[q] * c[r]);		/* see below */
    /* XXX This should be no smaller than 1e-16.  See
     * bn_isect_line2_line2 for details.
     */
    if (NEAR_ZERO(det, VUNITIZE_TOL)) {
	/* Lines are parallel */
	if (!colinear || !NEAR_ZERO(det1, VUNITIZE_TOL)) {
	    return -2;	/* parallel, not colinear, no intersection */
	}

	/* Lines are co-linear */
	/* Compute t for u=0 as a convenience to caller */
	*u = 0;
	/* Use largest direction component */
	if (fabs(d[q]) >= fabs(d[r])) {
	    *t = h[q]/d[q];
	} else {
	    *t = h[r]/d[r];
	}
	return 0;	/* Lines co-linear */
    }

    /* det(M) is non-zero, so there is exactly one solution.  Using
     * Cramer's rule, det1(M) replaces the first column of M with the
     * constant column vector, in this case H.  Similarly, det2(M)
     * replaces the second column.  Computation of the determinant is
     * done as before.
     *
     * Now,
     *
     *	                  [ Hq  -Cq ]
     *	              det [         ]
     *	    det1(M)       [ Hr  -Cr ]   -Hq * Cr + Cq * Hr
     *	t = ------- = --------------- = ------------------
     *	     det(M) det(M)        -Dq * Cr + Cq * Dr
     *
     * and
     *
     *	                  [ Dq   Hq ]
     *	              det [         ]
     *	    det2(M)       [ Dr   Hr ]    Dq * Hr - Hq * Dr
     *	u = ------- = --------------- = ------------------
     *	     det(M) det(M)        -Dq * Cr + Cq * Dr
     */
    det = 1/det;
    *t = det * det1;
    *u = det * (d[q] * h[r] - h[q] * d[r]);

    /* Check that these values of t and u satisfy the 3rd equation as
     * well!
     *
     * XXX It isn't clear that "det" is exactly a model-space
     * distance.
     */
    det = *t * d[s] - *u * c[s] - h[s];
    if (!NEAR_ZERO(det, VUNITIZE_TOL)) {
	/* XXX This tolerance needs to be much less loose than
	 * SQRT_SMALL_FASTF.  What about DETERMINANT_TOL?
	 */
	/* Inconsistent solution, lines miss each other */
	return -1;
    }

    /* To prevent errors, check the answer.  Not returning bogus
     * results to our caller is worth the extra time.
     */
    {
	point_t hit1, hit2;

	VJOIN1(hit1, p, *t, d);
	VJOIN1(hit2, a, *u, c);
	if (!bn_pt3_pt3_equal(hit1, hit2, tol)) {
/* bu_log("bn_isect_line3_line3(): BOGUS RESULT, hit1=(%g, %g, %g), hit2=(%g, %g, %g)\n",
   hit1[X], hit1[Y], hit1[Z], hit2[X], hit2[Y], hit2[Z]); */
	    return -1;
	}
    }

    return 1;		/* Intersection found */
}


/**
 * B N _ I S E C T _ L I N E _ L S E G
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
int
bn_isect_line_lseg(fastf_t *t, const fastf_t *p, const fastf_t *d, const fastf_t *a, const fastf_t *b, const struct bn_tol *tol)
{
    vect_t c;		/* Direction vector from A to B */
    auto fastf_t u;		/* As in, A + u * C = X */
    register fastf_t f;
    register int ret;
    fastf_t fuzz;

    BN_CK_TOL(tol);

    VSUB2(c, b, a);
    /* To keep the values of u between 0 and 1, C should NOT be scaled
     * to have unit length.  However, it is a good idea to make sure
     * that C is a non-zero vector, (ie, that A and B are distinct).
     */
    if ((fuzz = MAGSQ(c)) < tol->dist_sq) {
	return -4;		/* points A and B are not distinct */
    }

    /* Detecting colinearity is difficult, and very very important.
     * As a first step, check to see if both points A and B lie within
     * tolerance of the line.  If so, then the line segment AC is ON
     * the line.
     */
    if (bn_distsq_line3_pt3(p, d, a) <= tol->dist_sq  &&
	bn_distsq_line3_pt3(p, d, b) <= tol->dist_sq) {
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("bn_isect_line3_lseg3() pts A and B within tol of line\n");
	}
	/* Find the parametric distance along the ray */
	*t = bn_dist_pt3_along_line3(p, d, a);
	/*** dist[1] = bn_dist_pt3_along_line3(p, d, b); ***/
	return 0;		/* Colinear */
    }

    if ((ret = bn_isect_line3_line3(t, &u, p, d, a, c, tol)) < 0) {
	/* No intersection found */
	return -1;
    }
    if (ret == 0) {
	/* co-linear (t was computed for point A, u=0) */
	return 0;
    }

    /* The two lines intersect at a point.  If the u parameter is
     * outside the range (0..1), reject the intersection, because it
     * falls outside the line segment A--B.
     *
     * Convert the tol->dist into allowable deviation in terms of
     * (0..1) range of the parameters.
     */
    fuzz = tol->dist / sqrt(fuzz);
    if (u < -fuzz)
	return -3;		/* Intersection < A */
    if ((f=(u-1)) > fuzz)
	return -2;		/* Intersection > B */

    /* Check for fuzzy intersection with one of the verticies */
    if (u < fuzz)
	return 1;		/* Intersection at A */
    if (f >= -fuzz)
	return 2;		/* Intersection at B */

    return 3;			/* Intersection between A and B */
}


/**
 * B N _ D I S T _ L I N E 3_ P T 3
 *@brief
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
double
bn_dist_line3_pt3(const fastf_t *pt, const fastf_t *dir, const fastf_t *a)
{
    vect_t f;
    register fastf_t FdotD;

    if ((FdotD = MAGNITUDE(dir)) <= SMALL_FASTF) {
	FdotD = 0.0;
	goto out;
    }
    VSUB2(f, a, pt);
    FdotD = VDOT(f, dir) / FdotD;
    FdotD = MAGSQ(f) - FdotD * FdotD;
    if (FdotD <= SMALL_FASTF) {
	FdotD = 0.0;
	goto out;
    }
    FdotD = sqrt(FdotD);
out:
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_dist_line3_pt3() ret=%g\n", FdotD);
    }
    return FdotD;
}


/**
 * B N _ D I S T S Q _ L I N E 3 _ P T 3
 *
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the square of the closest distance between the line and the
 * point.
 *
 * 'dir' need not have unit length.
 *
 * Return -
 * Distance squared
 */
double
bn_distsq_line3_pt3(const fastf_t *pt, const fastf_t *dir, const fastf_t *a)
{
    vect_t f;
    register fastf_t FdotD;

    VSUB2(f, pt, a);
    if ((FdotD = MAGNITUDE(dir)) <= SMALL_FASTF) {
	FdotD = 0.0;
	goto out;
    }
    FdotD = VDOT(f, dir) / FdotD;
    if ((FdotD = VDOT(f, f) - FdotD * FdotD) <= SMALL_FASTF) {
	FdotD = 0.0;
    }
out:
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_distsq_line3_pt3() ret=%g\n", FdotD);
    }
    return FdotD;
}


/**
 * B N _ D I S T _ L I N E _ O R I G I N
 *@brief
 * Given a parametric line defined by PT + t * DIR, return the closest
 * distance between the line and the origin.
 *
 * 'dir' need not have unit length.
 *
 * @return Distance
 */
double
bn_dist_line_origin(const fastf_t *pt, const fastf_t *dir)
{
    register fastf_t PTdotD;

    if ((PTdotD = MAGNITUDE(dir)) <= SMALL_FASTF)
	return 0.0;
    PTdotD = VDOT(pt, dir) / PTdotD;
    if ((PTdotD = VDOT(pt, pt) - PTdotD * PTdotD) <= SMALL_FASTF)
	return 0.0;
    return sqrt(PTdotD);
}


/**
 * B N _ D I S T _ L I N E 2 _ P O I N T 2
 *@brief
 * Given a parametric line defined by PT + t * DIR and a point A,
 * return the closest distance between the line and the point.
 *
 * 'dir' need not have unit length.
 *
 * @return Distance
 */
double
bn_dist_line2_point2(const fastf_t *pt, const fastf_t *dir, const fastf_t *a)
{
    vect_t f;
    register fastf_t FdotD;

    VSUB2_2D(f, pt, a);
    if ((FdotD = sqrt(MAGSQ_2D(dir))) <= SMALL_FASTF)
	return 0.0;
    FdotD = VDOT_2D(f, dir) / FdotD;
    if ((FdotD = VDOT_2D(f, f) - FdotD * FdotD) <= SMALL_FASTF)
	return 0.0;
    return sqrt(FdotD);
}


/**
 * B N _ D I S T S Q _ L I N E 2 _ P O I N T 2
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
double
bn_distsq_line2_point2(const fastf_t *pt, const fastf_t *dir, const fastf_t *a)
{
    vect_t f;
    register fastf_t FdotD;

    VSUB2_2D(f, pt, a);
    if ((FdotD = sqrt(MAGSQ_2D(dir))) <= SMALL_FASTF)
	return 0.0;
    FdotD = VDOT_2D(f, dir) / FdotD;
    if ((FdotD = VDOT_2D(f, f) - FdotD * FdotD) <= SMALL_FASTF)
	return 0.0;
    return FdotD;
}


/**
 * B N _ A R E A _ O F _ T R I A N G L E
 *@brief
 * Returns the area of a triangle. Algorithm by Jon Leech 3/24/89.
 */
double
bn_area_of_triangle(register const fastf_t *a, register const fastf_t *b, register const fastf_t *c)
{
    register double t;
    register double area;

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

    return 0.5 * sqrt(area);
}


/**
 * B N _ I S E C T _ P T _ L S E G
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
int bn_isect_pt_lseg(fastf_t *dist,
		     const fastf_t *a,
		     const fastf_t *b,
		     const fastf_t *p,
		     const struct bn_tol *tol)
/* distance along line from A to P */
/* points for line and intersect */

{
    vect_t AtoP,
	BtoP,
	AtoB,
	ABunit;	/* unit vector from A to B */
    fastf_t APprABunit;	/* Mag of projection of AtoP onto ABunit */
    fastf_t distsq;

    BN_CK_TOL(tol);

    VSUB2(AtoP, p, a);
    if (MAGSQ(AtoP) < tol->dist_sq)
	return 1;	/* P at A */

    VSUB2(BtoP, p, b);
    if (MAGSQ(BtoP) < tol->dist_sq)
	return 2;	/* P at B */

    VSUB2(AtoB, b, a);
    VMOVE(ABunit, AtoB);
    distsq = MAGSQ(ABunit);
    if (distsq < tol->dist_sq)
	return -1;	/* A equals B, and P isn't there */
    distsq = 1/sqrt(distsq);
    VSCALE(ABunit, ABunit, distsq);

    /* Similar to bn_dist_line_pt, except we never actually have to do
     * the sqrt that the other routine does.
     */

    /* find dist as a function of ABunit, actually the projection of
     * AtoP onto ABunit
     */
    APprABunit = VDOT(AtoP, ABunit);

    /* because of pythgorean theorem ... */
    distsq = MAGSQ(AtoP) - APprABunit * APprABunit;
    if (distsq > tol->dist_sq)
	return -1;	/* dist pt to line too large */

    /* Distance from the point to the line is within tolerance. */
    *dist = VDOT(AtoP, AtoB) / MAGSQ(AtoB);

    if (*dist > 1.0 || *dist < 0.0)	/* P outside AtoB */
	return -2;

    return 3;	/* P on AtoB */
}


/**
 * B N _ I S E C T _ P T 2 _ L S E G 2
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
int
bn_isect_pt2_lseg2(fastf_t *dist, const fastf_t *a, const fastf_t *b, const fastf_t *p, const struct bn_tol *tol)
/* distance along line from A to P */
/* points for line and intersect */

{
    vect_t AtoP,
	BtoP,
	AtoB,
	ABunit;	/* unit vector from A to B */
    fastf_t APprABunit;	/* Mag of projection of AtoP onto ABunit */
    fastf_t distsq;

    BN_CK_TOL(tol);

    VSUB2_2D(AtoP, p, a);
    if (MAGSQ_2D(AtoP) < tol->dist_sq)
	return 1;	/* P at A */

    VSUB2_2D(BtoP, p, b);
    if (MAGSQ_2D(BtoP) < tol->dist_sq)
	return 2;	/* P at B */

    VSUB2_2D(AtoB, b, a);
    VMOVE_2D(ABunit, AtoB);
    distsq = MAGSQ_2D(ABunit);
    if (distsq < tol->dist_sq) {
	if (bu_debug & BU_DEBUG_MATH) {
	    bu_log("distsq A=%g\n", distsq);
	}
	return -1;	/* A equals B, and P isn't there */
    }
    distsq = 1/sqrt(distsq);
    VSCALE_2D(ABunit, ABunit, distsq);

    /* Similar to bn_dist_line_pt, except we never actually have to do
     * the sqrt that the other routine does.
     */

    /* find dist as a function of ABunit, actually the projection of
     * AtoP onto ABunit
     */
    APprABunit = VDOT_2D(AtoP, ABunit);

    /* because of pythgorean theorem ... */
    distsq = MAGSQ_2D(AtoP) - APprABunit * APprABunit;
    if (distsq > tol->dist_sq) {
	if (bu_debug & BU_DEBUG_MATH) {
	    V2PRINT("ABunit", ABunit);
	    bu_log("distsq B=%g\n", distsq);
	}
	return -1;	/* dist pt to line too large */
    }

    /* Distance from the point to the line is within tolerance. */
    *dist = VDOT_2D(AtoP, AtoB) / MAGSQ_2D(AtoB);

    if (*dist > 1.0 || *dist < 0.0)	/* P outside AtoB */
	return -2;

    return 3;	/* P on AtoB */
}


/**
 * B N _ D I S T _ P T 3 _ L S E G 3
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
int
bn_dist_pt3_lseg3(fastf_t *dist,
		  fastf_t *pca,
		  const fastf_t *a,
		  const fastf_t *b,
		  const fastf_t *p,
		  const struct bn_tol *tol)
{
    vect_t PtoA;		/* P-A */
    vect_t PtoB;		/* P-B */
    vect_t AtoB;		/* B-A */
    fastf_t P_A_sq;		/* |P-A|**2 */
    fastf_t P_B_sq;		/* |P-B|**2 */
    fastf_t B_A;		/* |B-A| */
    fastf_t t;		/* distance along ray of projection of P */

    BN_CK_TOL(tol);

    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_dist_pt3_lseg3() a=(%g, %g, %g) b=(%g, %g, %g)\n\tp=(%g, %g, %g), tol->dist=%g sq=%g\n",
	       V3ARGS(a),
	       V3ARGS(b),
	       V3ARGS(p),
	       tol->dist, tol->dist_sq);
    }

    /* Check proximity to endpoint A */
    VSUB2(PtoA, p, a);
    if ((P_A_sq = MAGSQ(PtoA)) < tol->dist_sq) {
	/* P is within the tol->dist radius circle around A */
	VMOVE(pca, a);
	if (bu_debug & BU_DEBUG_MATH) bu_log("  at A\n");
	*dist = 0.0;
	return 1;
    }

    /* Check proximity to endpoint B */
    VSUB2(PtoB, p, b);
    if ((P_B_sq = MAGSQ(PtoB)) < tol->dist_sq) {
	/* P is within the tol->dist radius circle around B */
	VMOVE(pca, b);
	if (bu_debug & BU_DEBUG_MATH) bu_log("  at B\n");
	*dist = 0.0;
	return 2;
    }

    VSUB2(AtoB, b, a);
    B_A = sqrt(MAGSQ(AtoB));

    /* compute distance (in actual units) along line to PROJECTION of
     * point p onto the line: point pca
     */
    t = VDOT(PtoA, AtoB) / B_A;
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_dist_pt3_lseg3() B_A=%g, t=%g\n",
	       B_A, t);
    }

    if (t <= 0) {
	/* P is "left" of A */
	if (bu_debug & BU_DEBUG_MATH) bu_log("  left of A\n");
	VMOVE(pca, a);
	*dist = sqrt(P_A_sq);
	return 3;
    }
    if (t < B_A) {
	/* PCA falls between A and B */
	register fastf_t dsq;
	fastf_t param_dist;	/* parametric dist */

	/* Find PCA */
	param_dist = t / B_A;		/* Range 0..1 */
	VJOIN1(pca, a, param_dist, AtoB);

	/* Find distance from PCA to line segment (Pythagorus) */
	if ((dsq = P_A_sq - t * t) <= tol->dist_sq) {
	    if (bu_debug & BU_DEBUG_MATH) bu_log("  ON lseg\n");
	    /* Distance from PCA to lseg is zero, give param instead */
	    *dist = param_dist;	/* special! */
	    return 0;
	}
	if (bu_debug & BU_DEBUG_MATH) bu_log("  closest to lseg\n");
	*dist = sqrt(dsq);
	return 5;
    }
    /* P is "right" of B */
    if (bu_debug & BU_DEBUG_MATH) bu_log("  right of B\n");
    VMOVE(pca, b);
    *dist = sqrt(P_B_sq);
    return 4;
}


/**
 * B N _ D I S T _ P T 2 _ L S E G 2
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
int
bn_dist_pt2_lseg2(fastf_t *dist_sq, fastf_t *pca, const fastf_t *a, const fastf_t *b, const fastf_t *p, const struct bn_tol *tol)
{
    vect_t PtoA;		/* P-A */
    vect_t PtoB;		/* P-B */
    vect_t AtoB;		/* B-A */
    fastf_t P_A_sq;		/* |P-A|**2 */
    fastf_t P_B_sq;		/* |P-B|**2 */
    fastf_t B_A;		/* |B-A| */
    fastf_t t;		/* distance along ray of projection of P */

    BN_CK_TOL(tol);

    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_dist_pt3_lseg3() a=(%g, %g, %g) b=(%g, %g, %g)\n\tp=(%g, %g, %g), tol->dist=%g sq=%g\n",
	       V3ARGS(a),
	       V3ARGS(b),
	       V3ARGS(p),
	       tol->dist, tol->dist_sq);
    }


    /* Check proximity to endpoint A */
    VSUB2_2D(PtoA, p, a);
    if ((P_A_sq = MAGSQ_2D(PtoA)) < tol->dist_sq) {
	/* P is within the tol->dist radius circle around A */
	V2MOVE(pca, a);
	if (bu_debug & BU_DEBUG_MATH) bu_log("  at A\n");
	*dist_sq = 0.0;
	return 1;
    }

    /* Check proximity to endpoint B */
    VSUB2_2D(PtoB, p, b);
    if ((P_B_sq = MAGSQ_2D(PtoB)) < tol->dist_sq) {
	/* P is within the tol->dist radius circle around B */
	V2MOVE(pca, b);
	if (bu_debug & BU_DEBUG_MATH) bu_log("  at B\n");
	*dist_sq = 0.0;
	return 2;
    }

    VSUB2_2D(AtoB, b, a);
    B_A = sqrt(MAGSQ_2D(AtoB));

    /* compute distance (in actual units) along line to PROJECTION of
     * point p onto the line: point pca
     */
    t = VDOT_2D(PtoA, AtoB) / B_A;
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_dist_pt3_lseg3() B_A=%g, t=%g\n",
	       B_A, t);
    }

    if (t <= 0) {
	/* P is "left" of A */
	if (bu_debug & BU_DEBUG_MATH) bu_log("  left of A\n");
	V2MOVE(pca, a);
	*dist_sq = P_A_sq;
	return 3;
    }
    if (t < B_A) {
	/* PCA falls between A and B */
	register fastf_t dsq;
	fastf_t param_dist;	/* parametric dist */

	/* Find PCA */
	param_dist = t / B_A;		/* Range 0..1 */
	V2JOIN1(pca, a, param_dist, AtoB);

	/* Find distance from PCA to line segment (Pythagorus) */
	if ((dsq = P_A_sq - t * t) <= tol->dist_sq) {
	    if (bu_debug & BU_DEBUG_MATH) bu_log("  ON lseg\n");
	    /* Distance from PCA to lseg is zero, give param instead */
	    *dist_sq = param_dist;	/* special! Not squared. */
	    return 0;
	}
	if (bu_debug & BU_DEBUG_MATH) bu_log("  closest to lseg\n");
	*dist_sq = dsq;
	return 5;
    }
    /* P is "right" of B */
    if (bu_debug & BU_DEBUG_MATH) bu_log("  right of B\n");
    V2MOVE(pca, b);
    *dist_sq = P_B_sq;
    return 4;
}


/**
 * B N _ R O T A T E _ B B O X
 *@brief
 * Transform a bounding box (RPP) by the given 4x4 matrix.  There are
 * 8 corners to the bounding RPP.  Each one needs to be transformed
 * and min/max'ed.  This is not minimal, but does fully contain any
 * internal object, using an axis-aligned RPP.
 */
void
bn_rotate_bbox(fastf_t *omin, fastf_t *omax, const fastf_t *mat, const fastf_t *imin, const fastf_t *imax)
{
    point_t local;		/* vertex point in local coordinates */
    point_t model;		/* vertex point in model coordinates */

#define ROT_VERT(a, b, c) {			\
	VSET(local, a[X], b[Y], c[Z]);		\
	MAT4X3PNT(model, mat, local);		\
	VMINMAX(omin, omax, model);		\
    }

    ROT_VERT(imin, imin, imin);
    ROT_VERT(imin, imin, imax);
    ROT_VERT(imin, imax, imin);
    ROT_VERT(imin, imax, imax);
    ROT_VERT(imax, imin, imin);
    ROT_VERT(imax, imin, imax);
    ROT_VERT(imax, imax, imin);
    ROT_VERT(imax, imax, imax);
#undef ROT_VERT
}


/**
 * B N _ R O T A T E _ P L A N E
 *@brief
 * Transform a plane equation by the given 4x4 matrix.
 */
void
bn_rotate_plane(fastf_t *oplane, const fastf_t *mat, const fastf_t *iplane)
{
    point_t orig_pt;
    point_t new_pt;

    /* First, pick a point that lies on the original halfspace */
    VSCALE(orig_pt, iplane, iplane[3]);

    /* Transform the surface normal */
    MAT4X3VEC(oplane, mat, iplane);

    /* Transform the point from original to new halfspace */
    MAT4X3PNT(new_pt, mat, orig_pt);

    /*
     * The transformed normal is all that is required.
     * The new distance is found from the transformed point on the plane.
     */
    oplane[3] = VDOT(new_pt, oplane);
}


/**
 * B N _ C O P L A N A R
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
int
bn_coplanar(const fastf_t *a, const fastf_t *b, const struct bn_tol *tol)
{
    register fastf_t dot;
    vect_t pt_a, pt_b;
    BN_CK_TOL(tol);

    if (!NEAR_EQUAL(MAGSQ(a), 1.0, VUNITIZE_TOL) || !NEAR_EQUAL(MAGSQ(b), 1.0, VUNITIZE_TOL)) {
	bu_bomb("bn_coplanar(): input vector(s) 'a' and/or 'b' is not a unit vector.\n");
    }

    dot = VDOT(a, b);
    VSCALE(pt_a, a, a[3]);
    VSCALE(pt_b, b, b[3]);

    if (NEAR_ZERO(dot, tol->perp)) {
	return 0; /* planes are perpendicular */
    }

    /* parallel is when dot is within tol->perp of either -1 or 1 */
    if ((dot <= -SMALL_FASTF) ? (NEAR_EQUAL(dot, -1.0, tol->perp)) : (NEAR_EQUAL(dot, 1.0, tol->perp))) {
	if (bn_pt3_pt3_equal(pt_a, pt_b, tol)) {
	    /* test for coplanar */
	    if (dot >= SMALL_FASTF) {
		/* test normals in same direction */
		return 1;
            } else {
		return 2;
	    }
	} else {
	    return -1;
	}
    }
    return 0;
}


/**
 * B N _ A N G L E _ M E A S U R E
 *
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
double
bn_angle_measure(fastf_t *vec, const fastf_t *x_dir, const fastf_t *y_dir)
{
    fastf_t xproj, yproj;
    fastf_t gam;
    fastf_t ang;

    xproj = -VDOT(vec, x_dir);
    yproj = -VDOT(vec, y_dir);
    gam = atan2(yproj, xproj);	/* -pi..+pi */
    ang = bn_pi + gam;		/* 0..+2pi */
    if (ang < 0) {
	do {
	    ang += bn_twopi;
	} while (ang < 0);
    } else if (ang > bn_twopi) {
	do {
	    ang -= bn_twopi;
	} while (ang > bn_twopi);
    }
    if (ang < 0 || ang > bn_twopi)
	bu_bomb("bn_angle_measure() angle out of range\n");

    return ang;
}


/**
 * B N _ D I S T _ P T 3 _ A L O N G _ L I N E 3
 *@brief
 * Return the parametric distance t of a point X along a line defined
 * as a ray, i.e. solve X = P + t * D.  If the point X does not lie on
 * the line, then t is the distance of the perpendicular projection of
 * point X onto the line.
 */
double
bn_dist_pt3_along_line3(const fastf_t *p, const fastf_t *d, const fastf_t *x)
{
    vect_t x_p;

    VSUB2(x_p, x, p);
    return VDOT(x_p, d);
}


/**
 * B N _ D I S T _ P T 2 _ A L O N G _ L I N E 2
 *@brief
 * Return the parametric distance t of a point X along a line defined
 * as a ray, i.e. solve X = P + t * D.  If the point X does not lie on
 * the line, then t is the distance of the perpendicular projection of
 * point X onto the line.
 */
double
bn_dist_pt2_along_line2(const fastf_t *p, const fastf_t *d, const fastf_t *x)
{
    vect_t x_p;
    double ret;

    VSUB2_2D(x_p, x, p);
    ret = VDOT_2D(x_p, d);
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_dist_pt2_along_line2() p=(%g, %g), d=(%g, %g), x=(%g, %g) ret=%g\n",
	       V2ARGS(p),
	       V2ARGS(d),
	       V2ARGS(x),
	       ret);
    }
    return ret;
}


/**
 *
 * @return 1	if left <= mid <= right
 * @return 0	if mid is not in the range.
 */
int
bn_between(double left, double mid, double right, const struct bn_tol *tol)
{
    BN_CK_TOL(tol);

    if (left < right) {
	if (NEAR_EQUAL(left, right, tol->dist*0.1)) {
	    left -= tol->dist*0.1;
	    right += tol->dist*0.1;
	}
	if (mid < left || mid > right) goto fail;
	return 1;
    }
    /* The 'right' value is lowest */
    if (NEAR_EQUAL(left, right, tol->dist*0.1)) {
	right -= tol->dist*0.1;
	left += tol->dist*0.1;
    }
    if (mid < right || mid > left) goto fail;
    return 1;
fail:
    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_between(%.17e, %.17e, %.17e) ret=0 FAIL\n",
	       left, mid, right);
    }
    return 0;
}


/**
 * B N _ D O E S _ R A Y _ I S E C T _ T R I
 *
 * @return 0	No intersection
 * @return 1	Intersection, 'inter' has intersect point.
 */
int
bn_does_ray_isect_tri(
    const point_t pt,
    const vect_t dir,
    const point_t V,
    const point_t A,
    const point_t B,
    point_t inter)			/* output variable */
{
    vect_t VP, VA, VB, AB, AP, N;
    fastf_t NdotDir;
    plane_t pl;
    fastf_t dist;

    /* insersect with plane */

    VSUB2(VA, A, V);
    VSUB2(VB, B, V);
    VCROSS(pl, VA, VB);
    VUNITIZE(pl);

    NdotDir = VDOT(pl, dir);
    if (ZERO(NdotDir))
	return 0;

    pl[W] = VDOT(pl, V);

    dist = (pl[W] - VDOT(pl, pt))/NdotDir;
    VJOIN1(inter, pt, dist, dir);

    /* determine if point is within triangle */
    VSUB2(VP, inter, V);
    VCROSS(N, VA, VP);
    if (VDOT(N, pl) < 0.0)
	return 0;

    VCROSS(N, VP, VB);
    if (VDOT(N, pl) < 0.0)
	return 0;

    VSUB2(AB, B, A);
    VSUB2(AP, inter, A);
    VCROSS(N, AB, AP);
    if (VDOT(N, pl) < 0.0)
	return 0;

    return 1;
}


#if 0
/*
 * B N _ I S E C T _ R A Y _ T R I
 *
 * Intersect a infinite ray with a triangle specified by 3 points.
 * From: "Graphics Gems" ed Andrew S. Glassner:
 *		"An Efficient Ray-Polygon Intersection"  P 390
 *
 *	      o A
 *	      |\
 *	      | \
 *	      |  \
 *	      |   \
 *	   _  |  o \
 *   alpha|   |  P  \
 *	  |   |      \
 *	  |_  o-------o
 *	       V       B
 *
 *	      |__|
 *		beta
 *
 * The intersection point P is computed by determining 2 quantities,
 * (alpha, beta) which indicate the parametric distance along VA and
 * VB respectively.  The intersection point is thus:
 *
 * P = V + (alpha * VA) + (beta * VB)
 *
 * Return:
 *	0	Miss
 *	1	Hit, incoming
 *	-1	Hit, outgoing
 *
 * If Hit, the following parameters are also set:
 *	dist	parametric distance to the triangle intercept.
 *	N	If non-null, surface normal of triangle
 *	Alpha	If non-null, parametric distance along VA
 *	Beta	If non-null, parametric distance along VB

 * The parameters Alpha and Beta may be null, otherwise they will be
 * set.
 */
int
bn_isect_ray_tri(dist_p, N_p, Alpha_p, Beta_p, pt, dir, V, A, B)
    fastf_t *dist_p;
    fastf_t *N_p;
    fastf_t *Alpha_p;
    fastf_t *Beta_p;
    const point_t pt;
    const vect_t dir;
    const point_t V;
    const point_t A;
    const point_t B;
{
    vect_t VA;	/* V -> A vector */
    vect_t VB;	/* V -> B vector */
    vect_t pt_V;	/* Ray_origin -> V vector */
    vect_t N;	/* Triangle Normal */
    vect_t VPP;	/* perpendicular to vector V -> P */
    fastf_t alpha;	/* parametric distance along VA */
    fastf_t beta;	/* parametric distance along VB */
    fastf_t NdotDir;
    fastf_t entleave;
    fastf_t k;

    /* form edge vectors of triangle */
    VSUB2(VB, B, V);
    VSUB2(VA, A, V);

    /* form triangle normal */
    VCROSS(N, VA, VB);

    NdotDir = VDOT(N, dir);
    if (fabs(NdotDir) < SQRT_SMALL_FASTF)
	return 0;	/* ray parallel to triangle */

    if (NdotDir >= 0.0) entleave = -1;	/* leaving */
    else entleave = 1;			/* entering */

    VSUB2(pt_V, V, pt);
    VCROSS(VPP, pt_V, dir);

    /* alpha is projection of VPP onto VA (not necessaily in plane)
     * If alpha < 0.0 then p is "before" point V on line V->A
     * No-one can figure out why alpha > NdotDir is important.
     */
    alpha = VDOT(VA, VPP) * entleave;
    if (alpha < 0.0 || alpha > fabs(NdotDir)) return 0;


    /* beta is projection of VPP onto VB (not necessaily in plane) */
    beta = VDOT(VB, VPP) * (-1 * entleave);
    if (beta < 0.0 || beta > fabs(NdotDir)) return 0;

    k = VDOT(VPP, N) / NdotDir;

    if (dist_p) *dist_p = k;
    if (N_p) {
	VUNITIZE(N);
	VMOVE(N_p, N);
    }
    if (Alpha_p) *Alpha_p = alpha;
    if (Beta_p) *Beta_p = beta;

    return entleave;
}
#endif

/**
 * B N _ H L F _ C L A S S
 *@brief
 * Classify a halfspace, specified by its plane equation, against a
 * bounding RPP.
 *
 * @return BN_CLASSIFY_INSIDE
 * @return BN_CLASSIFY_OVERLAPPING
 * @return BN_CLASSIFY_OUTSIDE
 */
int
bn_hlf_class(const fastf_t *half_eqn, const fastf_t *min, const fastf_t *max, const struct bn_tol *tol)
{
    int class;	/* current classification */
    fastf_t d;

#define CHECK_PT(x, y, z)						\
    d = (x)*half_eqn[0] + (y)*half_eqn[1] + (z)*half_eqn[2] - half_eqn[3]; \
    if (d < -tol->dist) {						\
	if (class == BN_CLASSIFY_OUTSIDE)				\
	    return BN_CLASSIFY_OVERLAPPING;				\
	else class = BN_CLASSIFY_INSIDE;				\
    } else if (d > tol->dist) {						\
	if (class == BN_CLASSIFY_INSIDE)				\
	    return BN_CLASSIFY_OVERLAPPING;				\
	else class = BN_CLASSIFY_OUTSIDE;				\
    } else return BN_CLASSIFY_OVERLAPPING

    class = BN_CLASSIFY_UNIMPLEMENTED;
    CHECK_PT(min[X], min[Y], min[Z]);
    CHECK_PT(min[X], min[Y], max[Z]);
    CHECK_PT(min[X], max[Y], min[Z]);
    CHECK_PT(min[X], max[Y], max[Z]);
    CHECK_PT(max[X], min[Y], min[Z]);
    CHECK_PT(max[X], min[Y], max[Z]);
    CHECK_PT(max[X], max[Y], min[Z]);
    CHECK_PT(max[X], max[Y], max[Z]);
    if (class == BN_CLASSIFY_UNIMPLEMENTED)
	bu_log("bn_hlf_class: error in implementation\
min = (%g, %g, %g), max = (%g, %g, %g), half_eqn = (%g, %g, %g, %g)\n",
	       V3ARGS(min), V3ARGS(max), V3ARGS(half_eqn),
	       half_eqn[3]);
    return class;
}


/** B N _ D I S T S Q _ L I N E 3 _ L I N E 3
 *@brief
 * Calculate the square of the distance of closest approach for two
 * lines.
 *
 * The lines are specifed as a point and a vector each.  The vectors
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
 * This algoritm is based on expressing the distance sqaured, taking
 * partials with respect to the two unknown parameters (dist[0] and
 * dist[1]), seeting the two partails equal to 0, and solving the two
 * simutaneous equations
 */
int
bn_distsq_line3_line3(fastf_t *dist, fastf_t *P, fastf_t *d_in, fastf_t *Q, fastf_t *e_in, fastf_t *pt1, fastf_t *pt2)
{
    fastf_t de, denom;
    vect_t diff, PmQ, tmp;
    vect_t d, e;
    fastf_t len_e, inv_len_e, len_d, inv_len_d;
    int ret=0;

    len_e = MAGNITUDE(e_in);
    if (ZERO(len_e))
	bu_bomb("bn_distsq_line3_line3() called with zero length vector\n");
    inv_len_e = 1.0 / len_e;

    len_d = MAGNITUDE(d_in);
    if (ZERO(len_d))
	bu_bomb("bn_distsq_line3_line3() called with zero length vector\n");
    inv_len_d = 1.0 / len_d;

    VSCALE(e, e_in, inv_len_e);
    VSCALE(d, d_in, inv_len_d);
    de = VDOT(d, e);

    if (ZERO(de)) {
	/* lines are perpendicular */
	dist[0] = VDOT(Q, d) - VDOT(P, d);
	dist[1] = VDOT(P, e) - VDOT(Q, e);
    } else {
	VSUB2(PmQ, P, Q);
	denom = 1.0 - de*de;
	if (ZERO(denom)) {
	    /* lines are parallel */
	    dist[0] = 0.0;
	    dist[1] = VDOT(PmQ, d);
	    ret = 1;
	} else {
	    VBLEND2(tmp, 1.0, e, -de, d);
	    dist[1] = VDOT(PmQ, tmp)/denom;
	    dist[0] = dist[1] * de - VDOT(PmQ, d);
	}
    }
    VJOIN1(pt1, P, dist[0], d);
    VJOIN1(pt2, Q, dist[1], e);
    VSUB2(diff, pt1, pt2);
    dist[0] *= inv_len_d;
    dist[1] *= inv_len_e;
    dist[2] =  MAGSQ(diff);
    return ret;
}


/**
 * B N _ I S E C T _ P L A N E S
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
int
bn_isect_planes(fastf_t *pt, const fastf_t (*planes)[4], const size_t pl_count)
{
    mat_t matrix;
    mat_t inverse;
    vect_t hpq;
    fastf_t det;
    size_t i;

    if (bu_debug & BU_DEBUG_MATH) {
	bu_log("bn_isect_planes:\n");
	for (i=0; i<pl_count; i++) {
	    bu_log("Plane #%zu (%f %f %f %f)\n", i, V4ARGS(planes[i]));
	}
    }

    MAT_ZERO(matrix);
    VSET(hpq, 0.0, 0.0, 0.0);

    for (i=0; i<pl_count; i++) {
	matrix[0] += planes[i][X] * planes[i][X];
	matrix[5] += planes[i][Y] * planes[i][Y];
	matrix[10] += planes[i][Z] * planes[i][Z];
	matrix[1] += planes[i][X] * planes[i][Y];
	matrix[2] += planes[i][X] * planes[i][Z];
	matrix[6] += planes[i][Y] * planes[i][Z];
	hpq[X] += planes[i][X] * planes[i][H];
	hpq[Y] += planes[i][Y] * planes[i][H];
	hpq[Z] += planes[i][Z] * planes[i][H];
    }

    matrix[4] = matrix[1];
    matrix[8] = matrix[2];
    matrix[9] = matrix[6];
    matrix[15] = 1.0;

    /* Check that we don't have a singular matrix */
    det = bn_mat_determinant(matrix);
    if (ZERO(det))
	return 1;

    bn_mat_inv(inverse, matrix);

    MAT4X3PNT(pt, inverse, hpq);

    return 0;

}


/**
 * B N _ I S E C T _ L S E G _ R P P
 *@brief
 * Intersect a line segment with a rectangular parallelpiped (RPP)
 * that has faces parallel to the coordinate planes (a clipping RPP).
 * The RPP is defined by a minimum point and a maximum point.  This is
 * a very close relative to rt_in_rpp() from librt/shoot.c
 *
 * @return 0   if ray does not hit RPP,
 * @return !0  if ray hits RPP.
 *
 * @param[in, out] a	Start point of lseg
 * @param[in, out] b	End point of lseg
 * @param[in] min	min point of RPP
 * @param[in] max	amx point of RPP
 *
 * if !0 was returned, "a" and "b" have been clipped to the RPP.
 */
int
bn_isect_lseg_rpp(fastf_t *a,
		  fastf_t *b,
		  register fastf_t *min,
		  register fastf_t *max)
{
    auto vect_t diff;
    register fastf_t *pt = &a[0];
    register fastf_t *dir = &diff[0];
    register int i;
    register double sv;
    register double st;
    register double mindist, maxdist;

    mindist = -MAX_FASTF;
    maxdist = MAX_FASTF;
    VSUB2(diff, b, a);

    for (i=0; i < 3; i++, pt++, dir++, max++, min++) {
	if (*dir < -SQRT_SMALL_FASTF) {
	    if ((sv = (*min - *pt) / *dir) < 0.0)
		return 0;	/* MISS */
	    if (maxdist > sv)
		maxdist = sv;
	    if (mindist < (st = (*max - *pt) / *dir))
		mindist = st;
	}  else if (*dir > SQRT_SMALL_FASTF) {
	    if ((st = (*max - *pt) / *dir) < 0.0)
		return 0;	/* MISS */
	    if (maxdist > st)
		maxdist = st;
	    if (mindist < ((sv = (*min - *pt) / *dir)))
		mindist = sv;
	} else {
	    /* If direction component along this axis is NEAR 0, (ie,
	     * this ray is aligned with this axis), merely check
	     * against the boundaries.
	     */
	    if ((*min > *pt) || (*max < *pt))
		return 0;	/* MISS */;
	}
    }
    if (mindist >= maxdist)
	return 0;	/* MISS */

    if (mindist > 1 || maxdist < 0)
	return 0;	/* MISS */

    if (mindist >= 0 && maxdist <= 1)
	return 1;	/* HIT within box, no clipping needed */

    /* Don't grow one end of a contained segment */
    if (mindist < 0)
	mindist = 0;
    if (maxdist > 1)
	maxdist = 1;

    /* Compute actual intercept points */
    VJOIN1(b, a, maxdist, diff);		/* b must go first */
    VJOIN1(a, a, mindist, diff);
    return 1;		/* HIT */
}


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






