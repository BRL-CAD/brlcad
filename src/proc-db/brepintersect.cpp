/*                 B R E P I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @{ */
/** @file proc-db/brepintersect.cpp
 *
 * Until further notice this code is in a state of heavy flux as part of
 * GSoC 2009 as such it would be very foolish to write anything that
 * depends on it right now.
 *
 * This code is written and maintained by Joe Doliner: jdoliner@gmail.com
 */

#include "assert.h"
#include "brepintersect.h"


int PolylineBBox(
    const ON_Polyline& pline,
    ON_BoundingBox* bbox
    )
{
    ON_3dPoint min = pline[0], max = pline[0];
    for (int i = 1; i < pline.Count(); i++) {
	VMINMAX(min, max, pline[i]);
    }

    bbox->m_min = min;
    bbox->m_max = max;
    return 0;
}

/**
 * tests whether a point is inside of the triangle using vector math
 * the point has to be in the same plane as the triangle, otherwise
 * it returns false.
 */
bool PointInTriangle(
    const ON_3dPoint& a,
    const ON_3dPoint& b,
    const ON_3dPoint& c,
    const ON_3dPoint& P,
    double tol
    )
{
    /* First we check to make sure that the point is in the plane */
    double normal[3];
    VCROSS(normal, b - a, c - a);
    VUNITIZE(normal);

    if (!NEAR_ZERO(VDOT(normal, P - a), tol))
	return false;

    /* we have a point that we know is in the plane,
     * but we need to check that it's in the triangle
     * the cleanest way to check this is to check that
     * the crosses of edges and vectors from vertices to P are all parallel or 0
     * The reader could try to prove this if s/he were ambitious
     */
    double v1[3];
    VCROSS(v1, b - a, P - a);
    if (VNEAR_ZERO(v1, tol)) {
	VSETALL(v1, 0.0);
    } else
	VUNITIZE(v1);
    double v2[3];
    VCROSS(v2, c - b, P - b);
    if (VNEAR_ZERO(v2, tol)) {
	VSETALL(v2, 0.0);
    } else
	VUNITIZE(v2);
    double v3[3];
    VCROSS(v3, a - c, P - c);
    if (VNEAR_ZERO(v3, tol)) {
	VSETALL(v3, 0.0);
    } else
	VUNITIZE(v3);

    /* basically we need to check that v1 == v2 == v3, and 0 vectors get in for free
     * if 2 of them are 0 vectors, leaving the final vector with nothing to be equal too
     * then P is in the triangle (this actually means P is a vertex of our triangle)
     * I can't think of any slick way to do this, so it gets kinda ugly
     */

    if (VNEAR_ZERO(v1, tol)) {
	if (VNEAR_ZERO(v2, tol)) {
	    return true;
	} else if (VNEAR_ZERO(v3, tol)) {
	    return true;
	} else if (VNEAR_EQUAL(v2, v3, tol)) {
	    return true;
	} else
	    return false;
    } else if (VNEAR_ZERO(v2, tol)) {
	if (VNEAR_ZERO(v3, tol)) {
	    return true;
	} else if (VNEAR_EQUAL(v1, v3, tol)) {
	    return true;
	} else
	    return false;
    } else if (VNEAR_EQUAL(v1, v2, tol)) {
	if (VNEAR_ZERO(v3, tol)) {
	    return true;
	} else if (VNEAR_EQUAL(v2, v3, tol)) {
	    return true;
	} else
	    return false;
    } else
	return false;
}

bool PointInPolyline(
    const ON_3dPoint& P,
    const ON_Polyline pline,
    double tol
    )
{
    if (!pline.IsClosed(tol)) { /* no inside to speak of */
	return false;
    }
    /* First we need to find a point that's in the plane and outside the polyline */
    ON_BoundingBox bbox;
    PolylineBBox(pline, &bbox);
    ON_3dPoint adder;
    int i;
    for (i = 0; i < pline.Count(); i++) {
	adder = P - pline[i];
	if (!VNEAR_ZERO(adder, tol)) {
	    break;
	}
    }
    ON_3dPoint DistantPoint = P;
    int multiplier = 2;
    do {
	DistantPoint += adder*multiplier;
	multiplier = multiplier*multiplier;
    } while (bbox.IsPointIn(DistantPoint, false));
    bool inside = false;
    int rv;
    ON_3dPoint result[2];
    for (i = 0; i < pline.Count() - 1; i++) {
	rv = SegmentSegmentIntersect(P, DistantPoint, pline[i], pline[i + 1], result, tol);
	if (rv == 1) {
	    inside = !inside;
	} else if (rv == 2) {
	    bu_exit(-1, "This is very unlikely bug in PointInPolyline which needs to be fixed\n");
	}
    }
    return inside;
}

/**
 * determines whether or not a point is inside the given mesh
 */
bool PointInMesh(
    const ON_3dPoint UNUSED(P), /* heh */
    const ON_Mesh *mesh
    )
{
    ON_3dPoint triangles[2][3];
    int n_triangles = 0;
    int i, j;
    for (i = 0; i < mesh->m_F.Count(); i++) {
	ON_MeshFace face = mesh->m_F[i];
	if (face.IsTriangle()) {
	    for (j = 0; j < 3; j++) {
		triangles[0][j] = mesh->m_V[face.vi[j]];
	    }
	    n_triangles = 1;
	} else {
	    for (j= 0; j < 3; j++) {
		triangles[0][j] = mesh->m_V[face.vi[j]];
		triangles[1][j] = mesh->m_V[face.vi[(j + 2) % 4]];
	    }
	    n_triangles = 2;
	}
	for (j = 0; j < n_triangles; j++) {
	    /* XXX unimplemented */
	}
    }
    return 0;
}

/**
 * finds the intersection point between segments x1, x2 and x3, x4 and
 * stores the result in x we assume that the segments are coplanar.
 *
 * return values:
 *
 * 0: no intersection
 * 1: intersection in a point
 * 2: intersection in a line
 *
 *
 *            x1*                  *x3
 *                \               /
 *                 \             /
 *                  \           /
 *                   \         /
 *                    \       /
 *                     \     /
 *                      \   /
 *                       \ /
 *                        *   <----x
 *                       / \
 *                      /   \
 *                     /     \
 *                    /       \
 *                   /         \
 *                  /           \
 *               x4*             *x2
 *
 *
 *
 * the equations for the lines are:
 *
 * P(s) = x1 + s (x2 - x1) s in [0, 1]
 * Q(t) = x3 + t (x4 - x3) t in [0, 1]
 *
 * so we need to find s and t s.t. P(s) = Q(t)
 * So some vector calculus tells us that:
 *
 *       (CXB) dot (AXB)
 * s =   ---------------
 *           |AXB|^2
 *
 *      (-CXA) dot (BXA)
 * t =  ----------------
 *           |BXA|^2
 *
 *
 * Where we define:
 *
 * A = (x2-x1)
 * B = (x4-x3)
 * C = (x3-x1)
 *
 * This equation blows up if |AXB|^2 is 0 (in which case |BXA|^2 is
 * also 0), which indicates that the lines are parallel which is kind
 * of a pain.
 */
int SegmentSegmentIntersect(
    const ON_3dPoint& x1,
    const ON_3dPoint& x2,
    const ON_3dPoint& x3,
    const ON_3dPoint& x4,
    ON_3dPoint x[2],       /* segments could in degenerate cases intersect in another segment*/
    double tol
    )
{
    ON_3dPoint A = (x2 - x1);
    ON_3dPoint B = (x4 - x3);
    ON_3dPoint C = (x3 - x1);

    double AXB[3];
    VCROSS(AXB, A, B);
    double BXA[3];
    VCROSS(BXA, B, A);
    double CXB[3];
    VCROSS(CXB, C, B);
    double negC[3];
    VSCALE(negC, C, -1.0);
    double negCXA[3];
    VCROSS(negCXA, negC, A);

    if (VNEAR_ZERO(AXB, tol)) {/* the lines are parallel **commence sad music*/
	/* this is a potential bug if someone gets cheeky and passes us x2==x1*/
	double coincident_test[3];
	VCROSS(coincident_test, x4 - x2, x4 - x1);
	if (VNEAR_ZERO(coincident_test, tol)) {
	    /* the lines are coincident, meaning the segments lie on the same
	     * line but they could:
	     *	--not intersect at all
	     *  --intersect in a point
	     *  --intersect in a segment
	     * So here's the plan we. We're going to use dot products,
	     * The aspect of dot products that's important:
	     * A dot B is positive if A and B point the same way
	     * and negitive when they point in opposite directions
	     * so --> dot --> is positive, but <-- dot --> is negative
	     * so if (x3-x1) dot (x4-x1) is negative, then x1 lies on the segment (x3, x4)
	     * which means that x1 should be one of the points we return so we just go
	     * through and find which points are contained in the other segment
	     * and those are our return values
	     */
	    int points = 0;
	    if (x1 == x3 || x1 == x4) {
		x[points] = x1;
		points++;
	    }
	    if (x2 == x3 || x2 == x4) {
		x[points] = x2;
		points++;
	    }
	    if (VDOT((x3 - x1), (x4 - x1)) < 0) {
		x[points] = x1;
		points++;
	    }
	    if (VDOT((x3 - x2), (x4 - x2)) < 0) {
		x[points] = x2;
		points++;
	    }
	    if (VDOT((x1 - x3), (x2 - x3)) < 0) {
		x[points] = x3;
		points++;
	    }
	    if (VDOT((x1 - x4), (x2 - x4)) < 0) {
		x[points] = x4;
		points++;
	    }

	    assert(points <= 2);
	    return points;
	}
    } else {
	double s = VDOT(CXB, AXB)/MAGSQ(AXB);
	double t = VDOT(negCXA, BXA)/MAGSQ(BXA);
	/* now we need to perform some tests to make sure we're not
	 * outside these bounds by tiny little amounts */
	if (-tol <= s && s <= 1.0 + tol && -tol <= t && t <= 1.0 + tol) {
	    ON_3dPoint Ps = x1 + s * (x2 - x1); /* The answer according to equation P*/
	    ON_3dPoint Qt = x3 + t * (x4 - x3); /* The answer according to equation Q*/
	    assert(VNEAR_EQUAL(Ps, Qt, tol)); /* check to see if they agree, just a sanity check*/
	    x[0] = Ps;
	    return 1;
	} else {
	    /* this happens when the lines through x1, x2 and x3, x4 intersect but not the segments*/
	    return 0;
	}
    }
    return 0;
}

/**
 * uses iteration of SegmentSegmentIntersect.
 *
 * returns the number of intersections it finds
 */
int SegmentPolylineIntersect(
    const ON_3dPoint& P,
    const ON_3dPoint& Q,
    const ON_Polyline& pline,
    ON_SimpleArray<ON_3dPoint> out,
    double tol
    )
{
    int rv = 0;
    int my_rv = 0; /* what this function will return at the end */
    ON_3dPoint result[2];
    for (int i = 0; i < (pline.Count() - 1); i++) {
	rv = SegmentSegmentIntersect(P, Q, pline[i], pline[i+1], result, tol);
	if (out) { /* the output field can be made null to use the function to check for intersections but not return them */
	    for (int j = 0; j < rv; j++) {
		out.Append(result[j]);
	    }
	}
	my_rv += rv;
    }
    return my_rv;
}

/**
 * intersects a triangle ABC with a line PQ
 *
 * return values:
 * -1: error
 * 0: no intersection
 * 1: intersects in a point
 * 2: intersects in a line
 */
int SegmentTriangleIntersect(
    const ON_3dPoint& a,
    const ON_3dPoint& b,
    const ON_3dPoint& c,
    const ON_3dPoint& p,
    const ON_3dPoint& q,
    ON_3dPoint out[2],
    double tol
    )
{
    ON_3dPoint triangle[3] = {a, b, c}; /* it'll be nice to have this as an array too*/

    /* First we need to get our plane into point normal form (N \dot (P - P0) = 0)
     * Where N is a normal vector, and P0 is a point in the plane
     * P0 can be any of {a, b, c} so that's easy
     * Finding N
     */

    double normal[3];
    VCROSS(normal, b - a, c - a);
    VUNITIZE(normal);

    ON_3dPoint P0 = a; /* could be b or c*/

    /* Now we've got our plane in a manageable form (two of them actually)
     * So here's the rest of the plan:
     * Every point P on the line can be written as: P = p + u (q - p)
     * We just need to find u
     * We know that when u is correct:
     * normal dot (q + u * (q-p) = N dot P0
     *		   N dot (P0 - p)
     * so u =      --------------
     *		   N dot (q - p)
     */

    if (!NEAR_ZERO(VDOT(normal, (p-q)), tol)) {/* if this is 0 it indicates the line and plane are parallel*/
	double u = VDOT(normal, (P0 - p))/VDOT(normal, (q - p));
	if (u < 0.0 || u > 1.0)	/* this means we're on the line but not the line segment*/
	    return 0;		/* so we can return early*/
	ON_3dPoint P = p + u * (q - p);

	if (PointInTriangle(a, b, c, P, tol)) {
	    out[0] = P;
	    return 1;
	}
	return 0;

    } else {
	/* If we're here it means that the line and plane are parallel*/

	if (NEAR_ZERO(VDOT(normal, p-P0), tol)) {/* yahtzee!!*/
	    /* The line segment is in the same plane as the triangle*/
	    /* So first we check if the points are inside or outside the triangle*/
	    bool p_in = PointInTriangle(a, b, c, p, tol);
	    bool q_in = PointInTriangle(a, b , c , q , tol);
	    ON_3dPoint x[2]; /* a place to put our results*/

	    if (q_in && p_in) {
		out[0] = p;
		out[1] = q;
		return 2;
	    } else if (q_in || p_in) {
		if (q_in)
		    out[0] = q;
		else
		    out[0] = p;

		int i;
		int rv;
		for (i=0; i<3; i++) {
		    rv = SegmentSegmentIntersect(triangle[i], triangle[(i+1)%3], p, q, x, tol);
		    if (rv == 1) {
			out[1] = x[0];
			return 1;
		    } else if (rv == 2) {
			out[0] = x[0];
			out[1] = x[1];
			return 2;
		    }
		}
	    } else {
		/* neither q nor p is in the triangle*/

		int i;
		int points_found = 0;
		int rv;
		for (i = 0; i < 3; i++) {
		    rv = SegmentSegmentIntersect(triangle[i], triangle[(i+1)%3], p, q, x, tol);
		    if (rv == 1) {
			if (points_found == 0 || !VNEAR_EQUAL(out[0], x[0], tol)) { /* in rare cases we can get the same point twice*/
			    out[points_found] = x[0];
			    points_found++;
			}
		    } else if (rv == 2) {
			out[0] = x[0];
			out[1] = x[1];
			return 2;
		    }
		}
		return points_found;
	    }
	} else
	    return 0;
    }
    return -1;
}


/**
 * intersects triangle abc with triangle def.
 *
 * returns the number of intersections found [0-6].  more accurately
 * it returns the number of points needed to describe the intersection
 * so 6 points indicates intersection in a hexagon.
 *
 * Although it's never explicitly tested the points returned will be
 * within the tolerance of planar.
 *
 * TODO: make sure that the edges are outputted in an order
 * representative of the polygonal intersection
 *
 * When the triangles intersect in 2 points the order of the points is
 * meaningful as they indicate the ternality of the faces the
 * condition we want to meet is that the edge (result[1] - result[0])
 * is parallal to (Norm2 X Norm1)
 */

enum EdgeIndex {ab = 0, bc = 1, ca = 2, de = 3, ef = 4, fd = 5};

int TriangleTriangleIntersect(
    const ON_3dPoint a,
    const ON_3dPoint b,
    const ON_3dPoint c,
    const ON_3dPoint d,
    const ON_3dPoint e,
    const ON_3dPoint f,
    ON_3dPoint out[6], /* indicates the points of intersection */
    char edge[6], /* indicates which edge the intersection points lie on ab is 0 bc is 1 etc. */
    double tol
    )
{
    ON_3dPoint abc[3] = {a, b, c};
    ON_3dPoint def[3] = {d, e, f};
    ON_3dPoint result[2];
    int rv;
    ON_3dPoint p1, p2;
    int number_found = 0; /* number_found <= 2*/
    int i, j, k; /* iterators */
    /* intersect the edges of triangle abc with triangle def*/
    for (i = 0; i < 3; i++) {
	rv = SegmentTriangleIntersect(d, e, f, abc[i], abc[(i+1)%3], result, tol);
	for (j = 0; j < rv; j++) {
	    ON_3dPoint P = result[j];
	    bool dup = false;
	    for (k = 0; k < number_found; k++) {
		if (VNEAR_EQUAL(out[k], P, tol)) {
		    dup = true;
		    break;
		}
	    }
	    if (!dup) {
		out[number_found] = P;
		edge[number_found] = i;
		number_found++;
	    }
	}
    }

    /* intersect the edges of triangle def with triangle abc*/
    for (i = 0; i < 3; i++) {
	rv = SegmentTriangleIntersect(a, b, c, def[i], def[(i + 1) % 3], result, tol);
	for (j = 0; j < rv; j++) {
	    ON_3dPoint P = result[j];
	    bool dup = false;
	    for (k = 0; k < number_found; k++) {
		if (VNEAR_EQUAL(out[k], P, tol)) {
		    dup = true;
		    break;
		}
	    }
	    if (!dup) {
		out[number_found] = P;
		edge[number_found] = i+3;
		number_found++;
	    }
	}
    }

    /* now we check if the points need to be reordered to meet our
     * condition, and reorder them if necessary
     */
    if (number_found == 2) {

	double T1norm[3];
	VCROSS(T1norm, b - a, c - a);
	VUNITIZE(T1norm);
	double T2norm[3];
	VCROSS(T2norm, b - a, c - a);
	VUNITIZE(T2norm);
	double T2normXT1norm[3];
	VCROSS(T2normXT1norm, T1norm, T2norm);

	if (VDOT(T2normXT1norm, (out[1] - out[2])) < 0) {
	    /* the points are in the wrong order swap them */
	    ON_3dPoint tmpP = out[1];
	    out[1] = out[0];
	    out[0] = tmpP;

	    /* and swap the edges they came from */
	    char tmpC = edge[1];
	    edge[1] = edge[0];
	    edge[0] = tmpC;
	}
    }
    return number_found;
}

/**
 * checks whether two Polylines are inside one another.
 *
 * XXX It's the caller's responsibility to make sure the polylines are
 * coplanar.
 *
 * Returns:
 *  1 if the first is inside the second
 * -1 if the second is inside the first
 *  0 if the two intersect at some point or one of them isn't closed
 */
int PolylinePolylineInternal(
    ON_Polyline P,
    ON_Polyline Q,
    double tol
    )
{
    int i;
    for (i = 0; i < P.Count(); i++) {
	if (!PointInPolyline(P[i], Q, tol)) {
	    break;
	}
    }
    if (i == P.Count()) {
	return 1;
    }
    for (i = 0; i < Q.Count(); i++) {
	if (!PointInPolyline(Q[i], P, tol)) {
	    break;
	}
    }
    if (i == Q.Count()) {
	return -1;
    } else {
	return 0;
    }
}

/**
 * takes an array of PolyLines and gives the resulting polygon
 * triangulated.  there are a number of contraints on the input which
 * are the callers responsibility.  the points must all lie within the
 * same plane, none of the Polylines can intersect all polyline must
 * be closed.  one another all paths must be within the first path and
 * no path can be inside of 2 paths.
 */
int Triangulate(
    ON_ClassArray<ON_Polyline>& paths,
    ON_SimpleArray<ON_3dPoint[3]>& UNUSED(triangles)
    )
{
    /* first we need to link the paths together */
    int i, j, k, l;
    bool intersectfree;
    while (paths.Count() > 1) {
	/* we're going to merge paths[0] with some other path */
	for (i = 1; i < paths.Count(); i++) {
	    for (j = 0; j < paths[0].Count(); j++) { /* try connecting each point in paths[1] */
		for (k = 0; k < paths[i].Count(); k++) { /* with each point in paths[i] */
		    /* now we need to check that this line doesn't intersect any other polyline */
		    intersectfree = true;
		    for (l = 0; l < paths.Count(); l++) {
#if 0
			/* FIXME: compiler doesn't like that the first
			 * arguments is passing NULL as a non-pointer
			 * parameter. commented out until someone
			 * fixes it.
			 */
			if (SegmentPolylineIntersect(paths[0][j], paths[i][k], paths[l], NULL, 1E-9) != 0) {
			    intersectfree = false;
			    break;
			}
#endif
		    }
		    if (intersectfree) {
			for (l = 0; l < paths[0].Count(); l++) {
			    paths[i].Insert(k + l, paths[0][(j + l) % paths[0].Count()]);
			}
		    } else {
			break;
		    }
		}
	    }
	}
    }

#if 0
    /* Now we have one continous path and we need to triangulate it
     * I'm pretty sure it's true that given a planer path there exist
     * 3 consecutive points a, b, c s.t. ac doesn't intersect any line
     * in the polyline, perhaps I'll include a proof in a bit, but for
     * right now it seems pretty good.
     */
    while (paths[0].Count() > 4) { /* remember start pt is stored twice, so 4 indicates a triangle */
	for (i = 0; i < (paths[0].Count() - 1); i++) {
	    /* we check that the intersection is 4 because the segment shares end points with 4 different pline segments */
	    ON_3dPoint mid = (paths[0][i] + paths[0][i + 2])/2;
	    /* FIXME: compiler doesn't like that the first
	     * arguments is passing NULL as a non-pointer
	     * parameter. commented out until someone
	     * fixes it.
	     */
	    if (SegmentPolylineIntersect(paths[0][i], paths[0][(i + 2) % paths[0].Count()], paths[0], NULL, 1E-9) == 4 && PointInPolyline(mid, paths[0], 1E-9)) {
		/* ON_3dPoint tri[3] = {paths[0][i], paths[i + 1], paths[(i + 2) % paths[0].Count()]};
		   triangles.Append(tri);
		   paths[0].Remove(i + 1); */
	    }
	}
    }
#endif

    return 0;
}

/**
 * This class has the responsibility of keeping track of the points
 * that intersect the edges and eventually chopping the edges up into
 * little lines for the intersected mesh this is actually a somewhat
 * tricky proposition because the intersection functions never
 * actually return these edges, and there are a number of little
 * things to keep track of:
 *
 *                                           * A
 *                                          / \
 *                                         /   \
 *                                        /     \
 *                                       /       \
 *                                    p1*>------->*q1
 *                                     /           \
 *                                  p2*<-----------<*q2
 *                                   /               \
 *                                  /                 \
 *                                 /                   \
 *                                /                     \
 *                               /                       \
 *                              /                         \
 *                           B *---------------------------*  C
 *
 * For this triangle the class would have:
 * ab = {A, p1, p2, B}
 * bc = {B, C}
 * ca = {C, q2, q1, A}
 * abdir = {-1, 0, 1, -1}
 * bcdir = {-1, -1}
 * cadir = {-1, 0, 1, -1}
 *
 * The dir arrays indicate which direction the line is going (the
 * order of the points):
 *
 * -1 indicates an original point (about which we have no information)
 * 0 indicates an outgoing line
 * 1 indicates an incoming line
 */
class TriIntersections {
    ON_Polyline edges[3];
    ON_SimpleArray<char> dir[3];
    ON_SimpleArray<ON_Line> intersections;
    double tol;
public:
    TriIntersections(ON_3dPoint, ON_3dPoint, ON_3dPoint, double);
    int InsertPoint(ON_3dPoint, uint8_t, EdgeIndex);
    int AddLine(ON_Line);
    int Faces(ON_ClassArray<ON_3dPoint[3]>);
};


TriIntersections::TriIntersections(
    ON_3dPoint A,
    ON_3dPoint B,
    ON_3dPoint C,
    double tolerance
    )
{
    tol = tolerance;
    ON_3dPoint points[3] = {A, B, C};
    for (int i = 0; i < 3; i++) {
	edges[i].Append(points[i]);
	edges[i].Append(points[(i + 1) % 3]);
	dir[i].Append(-1);
	dir[i].Append(-1);
    }
}

int TriIntersections::InsertPoint(
    ON_3dPoint P,
    uint8_t direction,
    EdgeIndex ei
    )
{
    /* first check to see if the point is on the other side of the
     * starting/ending point and return an error if it is
     */
    if ((VDOT((P - edges[ei][0]), (edges[ei][1] - edges[ei][0])) < 0) || (VDOT((P - *edges[ei].Last()), (edges[ei][0] - *edges[ei].Last())) < 0)) {
	return -1;
    }
    double valtobeat = MAGSQ(P - edges[ei][0]);
    for (int i = 1; i < edges[ei].Count(); i++) {
	if (MAGSQ(edges[ei][i] - edges[ei][0]) > valtobeat) {
	    edges[ei].Insert(i, P);
	    dir[ei].Insert(i, direction);
	    return i;
	}
    }

    return 0;
}

int TriIntersections::AddLine(
    ON_Line line
    )
{
    intersections.Append(line);
    return intersections.Count();
}

int TriIntersections::Faces(
    ON_ClassArray<ON_3dPoint[3]> UNUSED(faces)
    )
{
    if (intersections.Count() == 0) {
	return 0;
    }

    /* first we get an array of all the segments we can use to make
     * our faces.
     */
    ON_SimpleArray<ON_Line> segments; /*the segments we have to make faces */
    ON_SimpleArray<bool> flippable; /* whether or not the segment has direction */
    ON_SimpleArray<bool> segexternal; /* whether or not the segment is from the edge */
    for (int i = 0; i < intersections.Count(); i++) {
	segments.Append(intersections[i]);
	segments.Append(intersections[i]);
	flippable.Append(false);
	flippable.Append(false);
	segexternal.Append(false);
	segexternal.Append(false);
    }

    for (int i = 0; i < 3; i++) {
	if (edges[i].Count() == 2) { /* the edge was never intersected */
	    segments.Append(ON_Line(edges[i][0], edges[i][0]));
	    flippable.Append(true);
	    segexternal.Append(true);
	} else {
	    for (int j = 0; j < (edges[i].Count() - 1); j++) {
		if (dir[i][j] == dir[i][j + 1]) {
		    /* this indicates an error in the intersection data */
		    return -1;
		} else if (dir[i][j] == 0 || dir[i][j+1] == 1) {
		    segments.Append(ON_Line(edges[i][j], edges[i][j+1]));
		    flippable.Append(false);
		    segexternal.Append(true);
		} else {
		    segments.Append(ON_Line(edges[i][j+1], edges[i][j]));
		    flippable.Append(false);
		    segexternal.Append(true);
		}
	    }
	}
    }

    /* Now that the segments are all set up it's time to make them
     * into faces.
     */
    ON_ClassArray<ON_Polyline> outlines;
    ON_SimpleArray<bool> line_external; /* stores whether each polyline is internal */
    ON_Polyline outline;
    while (segments.Count() != 0) {
	outline.Append(segments[0].from);
	outline.Append(segments[0].to);
	segments.Remove(0);

	int i = 0;
	bool ext = false; /* keeps track of the ternality of the path we're assembling */
	while (!outline.IsClosed(tol)) {
	    if (i >= segments.Count()) {
		return -1;
	    } else if (VNEAR_EQUAL(segments[i].from, outline[outline.Count() - 1], tol)) {
		outline.Append(segments[i].to);
	    } else if (VNEAR_EQUAL(segments[i].to, outline[0], tol)) {
		outline.Insert(0, segments[i].from);
	    } else if (VNEAR_EQUAL(segments[i].from, outline[0], tol) && flippable[i]) {
		outline.Insert(0, segments[i].to);
	    } else if (VNEAR_EQUAL(segments[i].to, outline[outline.Count() - 1], tol) && flippable[i]) {
		outline.Append(segments[i].from);
	    } else {
		i++;
		continue;
	    }

	    /* only executed when we append edge i */
	    segments.Remove(i);
	    flippable.Remove(i);
	    ext = ext && segexternal[i];
	    segexternal.Remove(i);
	    i = 0;
	}
	outlines.Append(outline);
	line_external.Append(ext);
    }
    /* XXX - now we need to setup the ternality tree for the paths */

    return 0;
}

/**
 * the point index is responsible for keeping track of the points we
 * put into a mesh.  if we give it a point it doesn't have it puts it
 * in the array and returns the index in the mesh where it put it.  if
 * it does have the point then it just returns the index of the point.
 */
class PointIndex{
    ON_Mesh *mesh;
    double tol;
public:
    PointIndex(ON_Mesh*);
    int InsertPoint(ON_3dPoint);
};

PointIndex::PointIndex(
    ON_Mesh *m
    )
{
    mesh = m;
}

int PointIndex::InsertPoint(
    ON_3dPoint P
    )
{
    /* This will eventually be implemented in a much more efficient way */
    for (int i = 0; i < mesh->m_V.Count(); i++) {
	if (VNEAR_EQUAL(mesh->m_V[i], P, tol)) {
	    return i;
	}
    }
    mesh->m_V.Append(ON_3fPoint(P));
    return mesh->m_V.Count();
}

/**
 * Outputs an array of the same size as this.m_V.Count() in which the
 * ith element of the array is an array with all of the faces that use
 * the ith vertex
 */
int GenerateFaceConnectivityList(
    ON_Mesh *mesh,
    ON_ClassArray<ON_SimpleArray<int> > faces
    )
{
    faces.Empty();
    int n_vertices;
    for (int i = 0; i < mesh->m_F.Count(); i++) {
	ON_MeshFace face = mesh->m_F[i];
	if (face.IsTriangle()) {
	    n_vertices = 3;
	} else {
	    n_vertices = 4;
	}
	for (int j = 0; j < n_vertices; j++) {
	    faces[face.vi[j]].Append(i);
	}
    }
    return 0;
}

/**
 * converts all quads in a mesh to triangles
 */
int MeshTriangulate(
    ON_Mesh *mesh
    )
{
    for (int i = 0; i < mesh->m_F.Count(); i++) {
	ON_MeshFace face = mesh->m_F[i];
	if (face.IsQuad()) {
	    ON_MeshFace face1 = face; /* FIXME */
	    mesh->m_F.Append(face1);
	    ON_MeshFace face2 = face; /* FIXME */
	    mesh->m_F.Append(face2);
	    mesh->m_F.Remove(i);
	    i--; /* we just lost an element in the array so the cursor would be off by one */
	}
    }

    return 0;
}

/**
 * Intersect two meshes and returns their intersection in Polylines
 * (ON_3dPoint arrays).  Returns how many polylines where found 0
 * indicates no intersection or -1 on error.
 */
int MeshMeshIntersect(
    ON_Mesh *mesh1,
    ON_Mesh *mesh2,
    double tol
    )
{
    /* First both meshes need to be triangulated */
    MeshTriangulate(mesh1);
    MeshTriangulate(mesh2);
    int i, j;
    for (i = 0; i < mesh1->FaceCount(); i++) {
	ON_MeshFace face1 = mesh1->m_F[i];
	for (j = 0; j < mesh2->FaceCount(); j++) {
	    ON_MeshFace face2 = mesh2->m_F[j];
	    ON_3dPoint result[6];
	    char edge[6];
	    int rv = TriangleTriangleIntersect(ON_3dPoint(mesh1->m_V[face1.vi[0]]), ON_3dPoint(mesh1->m_V[face1.vi[1]]), ON_3dPoint(mesh1->m_V[face1.vi[2]]), ON_3dPoint(mesh2->m_V[face2.vi[0]]), ON_3dPoint(mesh2->m_V[face2.vi[1]]), ON_3dPoint(mesh2->m_V[face2.vi[2]]), result, edge, tol);
	    if (rv == 2) {
	    }
	}
    }

    return 0;
}

int main()
{
    /* create the points */
    ON_3fPoint a1 = ON_3fPoint(1.0, 1.0, -1.0);
    ON_3fPoint b1 = ON_3fPoint(1.0, 1.0, 1.0);
    ON_3fPoint c1 = ON_3fPoint(-1.0, 1.0, 1.0);
    ON_3fPoint d1 = ON_3fPoint(-1.0, 1.0, -1.0);
    ON_3fPoint e1 = ON_3fPoint(1.0, -1.0, -1.0);
    ON_3fPoint f1 = ON_3fPoint(1.0, -1.0, 1.0);
    ON_3fPoint g1 = ON_3fPoint(-1.0, -1.0, 1.0);
    ON_3fPoint h1 = ON_3fPoint(-1.0, -1.0, -1.0);
    ON_3fPoint a2 = ON_3fPoint(0.5, 2.0, -0.5);
    ON_3fPoint b2 = ON_3fPoint(0.5, 2.0, 0.5);
    ON_3fPoint c2 = ON_3fPoint(-0.5, 2.0, 0.5);
    ON_3fPoint d2 = ON_3fPoint(-0.5, 2.0, -0.5);
    ON_3fPoint e2 = ON_3fPoint(0.5, -2.0, -0.5);
    ON_3fPoint f2 = ON_3fPoint(0.5, -2.0, 0.5);
    ON_3fPoint g2 = ON_3fPoint(-0.5, -2.0, 0.5);
    ON_3fPoint h2 = ON_3fPoint(-0.5, -2.0, -0.5);

    /* create the meshes */
    ON_Mesh mesh1;
    mesh1.m_V.Empty();
    mesh1.m_F.Empty();
    ON_Mesh mesh2;
    mesh2.m_V.Empty();
    mesh2.m_F.Empty();

    /* put the points in the meshes */
    mesh1.m_V.Append(a1);
    mesh1.m_V.Append(b1);
    mesh1.m_V.Append(c1);
    mesh1.m_V.Append(d1);
    mesh1.m_V.Append(e1);
    mesh1.m_V.Append(f1);
    mesh1.m_V.Append(g1);
    mesh1.m_V.Append(h1);
    mesh2.m_V.Append(a2);
    mesh2.m_V.Append(b2);
    mesh2.m_V.Append(c2);
    mesh2.m_V.Append(d2);
    mesh2.m_V.Append(e2);
    mesh2.m_V.Append(f2);
    mesh2.m_V.Append(g2);
    mesh2.m_V.Append(h2);

    /* create the faces */
    ON_MeshFace abcd = {{0, 1, 2, 3}};
    ON_MeshFace efba = {{4, 5, 1, 0}};
    ON_MeshFace ehgf = {{4, 7, 6, 5}};
    ON_MeshFace dcgh = {{3, 2, 6, 7}};
    ON_MeshFace adhe = {{0, 3, 7, 4}};
    ON_MeshFace bfgc = {{1, 5, 6, 2}};

    /* put the faces in the meshes */
    mesh1.m_F.Append(abcd);
    mesh1.m_F.Append(efba);
    mesh1.m_F.Append(ehgf);
    mesh1.m_F.Append(dcgh);
    mesh1.m_F.Append(adhe);
    mesh1.m_F.Append(bfgc);
    mesh2.m_F.Append(abcd);
    mesh2.m_F.Append(efba);
    mesh2.m_F.Append(ehgf);
    mesh2.m_F.Append(dcgh);
    mesh2.m_F.Append(adhe);
    mesh2.m_F.Append(bfgc);

    /* and now for the action */
    ON_ClassArray<ON_Polyline> out;

    /* int rv = MeshMeshIntersect(&mesh1, &mesh2, &out, 1.0e-10); */
    /* assert(rv == 2); */
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
