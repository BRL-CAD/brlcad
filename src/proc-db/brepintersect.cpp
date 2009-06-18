/*                 B R E P I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file brepintersect.cpp
 *
 * Breplicator is a tool for testing the new boundary representation
 * (BREP) primitive type in librt.  It creates primitives via
 * mk_brep() for testing purposes.
 *
 */

/*Until further notice this code is in a state of heavy flux as part of
 * GSoC 2009 as such it would be very foolish to write anything that
 * depends on it right now
 * This code is written and maintained by Joe Doliner jdoliner@gmail.com*/

#include "brepintersect.h"


/* tests whether a point is inside of the triangle using vector math 
 * the point has to be in the same plane as the triangle, otherwise
 * it returns false. */
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

    if (!NEAR_ZERO(VDOT(normal, P-a), tol))
	return false;

    /* we have a point that we know is in the plane,
     * but we need to check that it's in the triangle
     * the cleanest way to check this is to check that
     * the crosses of edges and vectors from vertices to P are all parallel or 0
     * The reader could try to prove this if s/he were ambitious
     */
    double v1[3];
    VCROSS(v1, b-a, P-a);
    if (VNEAR_ZERO(v1, tol)) {
	VSETALL(v1, 0.0);
    } else
	VUNITIZE(v1);
    double v2[3];
    VCROSS(v2, c-b, P-b);
    if (VNEAR_ZERO(v2, tol)) {
	VSETALL(v2, 0.0);
    } else
	VUNITIZE(v2);
    double v3[3];
    VCROSS(v3, a-c, P-c);
    if (VNEAR_ZERO(v3, tol)) {
	VSETALL(v3, 0.0);
    } else
	VUNITIZE(v3);

    /* basically we need to check that v1 == v2 == v3, and 0 vectors get in for free
     * if 2 of them are 0 vectors, leaving the final vector with nothing to be equal too
     * then P is in the triangle (this actually means P is a vertex of our triangle)
     * I can't think of any slick way to do this, so it gets kinda ugly
     */

    if (VNEAR_ZERO(v1,tol)) {
	if (VNEAR_ZERO(v2,tol)) {
	    return true;
	} else if (VNEAR_ZERO(v3, tol)) {
	    return true;
	} else if (VAPPROXEQUAL(v2,v3,tol)) {
	    return true;
	} else
	    return false;
    } else if (VNEAR_ZERO(v2,tol)) {
	if (VNEAR_ZERO(v3,tol)) {
	    return true;
	} else if (VAPPROXEQUAL(v1,v3,tol)) {
	    return true;
	} else
	    return false;
    } else if (VAPPROXEQUAL(v1,v2,tol)) {
	if (VNEAR_ZERO(v3,tol)) {
	    return true;
	} else if (VAPPROXEQUAL(v2,v3,tol)) {
	    return true;
	} else
	    return false;
    } else
	return false;
}
/* finds the intersection point between segments x1,x2 and x3,x4
 * and stores the result in x we assume that the segments are coplanar
 * return values:
 *	0: no intersection
 *	1: intersection in a point
 *	2: intersection in a line
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
 * P(s) = x1 + s (x2 - x1) s in [0,1]
 * Q(t) = x3 + t (x4 - x3) t in [0,1]
 *
 * so we need to find s and t s.t. P(s) = Q(t)
 * So some vector calculus tells us that:
 *
 *          (CXB) dot (AXB)
 *    s =   ---------------
 *              |AXB|^2
 *
 *         (-CXA) dot (BXA)
 *    t =  ----------------
 *              |BXA|^2
 *
 *
 * Where we define:
 *
 *	A = (x2-x1)
 *	B = (x4-x3)
 *	C = (x3-x1)
 *
 * This equation blows up if |AXB|^2 is 0 (in which case |BXA|^2 is also 0), which indicates
 * that the lines are parallel
 * which is kind of a pain
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
    VCROSS(BXA, B,A);
    double CXB[3];
    VCROSS(CXB, C,B);
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
	     * so if (x3-x1) dot (x4-x1) is negative, then x1 lies on the segment (x3,x4)
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
	    if (VDOT((x3 - x1),(x4 - x1)) < 0) {
		x[points] = x1;
		points++;
	    }
	    if (VDOT((x3 - x2),(x4 - x2)) < 0) {
		x[points] = x2;
		points++;
	    }
	    if (VDOT((x1 - x3),(x2 - x3)) < 0) {
		x[points] = x3;
		points++;
	    }
	    if (VDOT((x1 - x4),(x2 - x4)) < 0) {
		x[points] = x4;
		points++;
	    }

	    assert(points <= 2);
	    return points;
	}
    } else{
	double s = VDOT(CXB, AXB)/MAGSQ(AXB);
	double t = VDOT(negCXA, BXA)/MAGSQ(BXA);
	/* now we need to perform some tests to make sure we're not
	 * outside these bounds by tiny little amounts */
	if (-tol<=s && s<=1.0+tol && -tol<=t && t<=1.0+tol) {
	    ON_3dPoint Ps = x1 + s * (x2 - x1); /* The answer according to equation P*/
	    ON_3dPoint Qt = x3 + t * (x4 - x3); /* The answer according to equation Q*/
	    assert(VAPPROXEQUAL(Ps, Qt, tol)); /* check to see if they agree, just a sanity check*/
	    x[0] = Ps;
	    return 1;
	} else /* this happens when the lines through x1,x2 and x3,x4 intersect but not the segments*/
	    return 0;
    }
}

/* intersects a triangle ABC with a line PQ
 * return values:
 * -1: error
 *	0: no intersection
 *	1: intersects in a point
 *	2: intersects in a line
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
     * P0 can be any of {a,b,c} so that's easy
     * Finding N
     */

    double normal[3];
    VCROSS(normal, b - a, c - a);
    VUNITIZE(normal);

    ON_3dPoint P0 = a; /* could be b or c*/

    /* Now we've got our plane in a manageable form (two of them actually)
     * So here's the rest of the plan:
     * Every point P on the line can be written as: P = p + u (q-p)
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
	} else 
	    return 0;
    } else {/* If we're here it means that the line and plane are parallel*/
	if (NEAR_ZERO(VDOT(normal, p-P0), tol)) {/* yahtzee!!*/
	    /* The line segment is in the same plane as the triangle*/
	    /* So first we check if the points are inside or outside the triangle*/
	    bool p_in = PointInTriangle(a, b, c, p, tol);
	    bool q_in = PointInTriangle(a, b , c , q ,tol);
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
	    } else { /* neither q nor p is in the triangle*/
		int i;
		int points_found = 0;
		int rv;
		for (i=0; i<3; i++) {
		    rv = SegmentSegmentIntersect(triangle[i], triangle[(i+1)%3], p, q, x, tol);
		    if (rv == 1) {
			if (points_found == 0 || !VAPPROXEQUAL(out[0], x[0], tol)) { /* in rare cases we can get the same point twice*/
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


/* intersects triangle ABC with triangle DEF
 * return values:
 *				   -1: degenerate (triangles are coincident or an edge of 1 triangle incident on the other triangle)
 *					0: no intersection
 *					1: the triangles intersect in a point
 *					2: the triangles intersect in a line
 */
int TriangleTriangleIntersect(
	const ON_3dPoint T1[3],
	const ON_3dPoint T2[3],
	ON_Line& out,
	double tol
	)
{
    double abc[2][3];
    double t[2];
    int rv;
    ON_3dPoint p1, p2;
    int number_found = 0; /* number_found <= 2*/
    /* intersect the edges of triangle 1 with triangle 2*/
    for (int i = 0; i<3; i++) {
	rv = ON_LineTriangleIntersect(T2[0], T2[1], T2[2], T1[i], T1[(i+1)%3], abc, t, tol);
	if (rv == 2)
	    /* for the time being we'll consider this as a degenerate case*/
	    return -1;
	else if (rv == 1) {
	    /* now we're in business the line intersects the triangle at one point*/
	    if (number_found == 0) {
		p1 = abc[0];
		number_found++;
	    } else if (number_found == 1)
		if(p1 != ON_3dPoint(abc[0])) { /* this should use tolerance does it?*/
		    p2 = abc[0];
		    number_found++;
		    break;
		}
	}
    }
    for (int i = 0; i<3; i++) {
	/* now we intersect the edges of triangle 2 with triangle 1*/
	rv = ON_LineTriangleIntersect(T1[0], T1[1], T1[2], T2[i], T2[(i+1)%3], abc, t, tol);
	if (rv == 2)
	    /* again degenerate*/
	    return -1;
	else if (rv == 1) {
	    if (number_found == 0) {
		p1 = abc[0];
		number_found++;
	    } else if (number_found == 1)
		if (p1 != ON_3dPoint(abc[0])) {
		    p2 = abc[0];
		    number_found++;
		    break;
		}
	}
    }
    if (number_found == 0)
	return 0; /* we didn't find anything*/
    else if (number_found == 1) {
	out.from = p1;
	return 1;
    } else if (number_found == 2) {
	out.from = p1;
	out.to = p2;
	return 2;
    }
    return 0;
}

int TriangleBrepIntersect(
	const ON_3dPoint T1[3],
	const ON_Brep brep,
	ON_Curve& out,
	double tol
	)
{
    return 0;
}

int main()
{
    /* Tests for PointInTriangle */
    ON_3dPoint a = ON_3dPoint(0.0, 0.0, 0.0);
    ON_3dPoint b = ON_3dPoint(100.0, 0.0, 0.0);
    ON_3dPoint c = ON_3dPoint(0.0, 100.0, 0.0);
    /* ON_3dPoint P = ON_3dPoint(0.0, 0.0, 0.0); */
    ON_3dPoint out[2];

    /* double cos = -0.737368878;
    double sin = -0.675490294; */
    double cos = 0.6;
    double sin = 0.8;
    mat_t rotX = {1.0, 0.0, 0.0, 0.0, 0.0, cos, sin, 0.0, 0.0, -sin, cos, 0.0, 0.0, 0.0, 0.0, 1.0};
    mat_t rotY = {cos, 0.0, -sin, 0.0, 0.0, 1.0, 0.0, 0.0, sin, 0.0, cos, 0.0, 0.0, 0.0, 0.0, 1.0};
    mat_t rotZ = {cos, sin, 0.0, 0.0, -sin, cos, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0};

    int i, j, k, l, m, failed = 0, total = 0;
    /* 
    for (i=0; i<101; i++) {
	for (j=0; j<101; j++) {
	    ON_3dPoint Q = ON_3dPoint(P.x + i, P.y + j, P.z);
	    ON_3dPoint A = a;
	    ON_3dPoint B = b;
	    ON_3dPoint C = c;
	    ON_3dPoint tmpa = A;
	    ON_3dPoint tmpb = B;
	    ON_3dPoint tmpc = C;
	    ON_3dPoint tmpq = Q;
	    for (k=0; k<2; k++) {
		MAT4X3PNT(A, rotX, tmpa);
		MAT4X3PNT(B, rotX, tmpb);
		MAT4X3PNT(C, rotX, tmpc);
		MAT4X3PNT(Q, rotX, tmpq);
		for (l=0; l<2; l++) {
		    tmpa = A;
		    tmpb = B;
		    tmpc = C;
		    tmpq = Q;
		    MAT4X3PNT(A, rotY, tmpa);
		    MAT4X3PNT(B, rotY, tmpb);
		    MAT4X3PNT(C, rotY, tmpc);
		    MAT4X3PNT(Q, rotY, tmpq);
		    for (m=0; m<2; m++) {
			tmpa = A;
			tmpb = B;
			tmpc = C;
			tmpq = Q;
			MAT4X3PNT(A, rotZ, tmpa);
			MAT4X3PNT(B, rotZ, tmpb);
			MAT4X3PNT(C, rotZ, tmpc);
			MAT4X3PNT(Q, rotZ, tmpq);
			bool rv = PointInTriangle(A, B, C, Q, 1.0e-10);
			total++;
			if ((i + j < 101 && !rv) || (i + j >= 101 && rv)) {
			    failed++;
			}
		    }
		}
	    }
	}
    }
    bu_log("Failed: %i of %i tests in PointInTriangle\n", failed, total); */

    /* SegmentSegmentIntersect Tests */
    /* {
	ON_3dPoint a = ON_3dPoint(-50.0, 0.0, 0.0);
	ON_3dPoint b = ON_3dPoint(50.0, 0.0, 0.0);
	ON_3dPoint c = ON_3dPoint(0.0, 0.0, 0.0);
	ON_3dPoint d = ON_3dPoint(0.0, 100.0, 100.0);
	ON_3dPoint out[2];
	int rv = SegmentSegmentIntersect(a, b, c, d, out, 1.0e-10);
	if (! (rv == 1 && VAPPROXEQUAL(c, out[0], 1.0e-10))) 
	    bu_log("Failed \n");
    } */

    /* SegmentTriangleIntersect Tests */
    failed = total = 0;
    a = ON_3dPoint(0.0, 0.0, 0.0);
    b = ON_3dPoint(100.0, 0.0, 0.0);
    c = ON_3dPoint(0.0, 100.0, 0.0);
    ON_3dPoint p = ON_3dPoint(0.0, 0.0, 100.0);
    ON_3dPoint q = ON_3dPoint(0.0, 0.0, -100.0);
    ON_3dPoint p2 = ON_3dPoint(50.0, 0.0, 0.0);
    ON_3dPoint q2 = ON_3dPoint(0.0, 50.0, 0.0);
    ON_3dPoint p3 = ON_3dPoint(55.0, -5.0, 0.0);
    ON_3dPoint q3 = ON_3dPoint(-5.0, 55.0, 0.0);
    ON_3dPoint p4 = ON_3dPoint(-100.0, -100.0, 0.0);
    ON_3dPoint q4 = ON_3dPoint(100.0, 100.0, 0.0);
    ON_3dPoint p5 = ON_3dPoint(-5.0, 0.0, 0.0);
    ON_3dPoint q5 = ON_3dPoint(105.0, 0.0, 0.0);
    /* ON_3dPoint Q; */
    /* for (i=0; i<101; i++) {
	for (j=0; j<101; j++) { */
	    ON_3dPoint P = ON_3dPoint(p.x + i, p.y + j, p.z);
	    ON_3dPoint Q = ON_3dPoint(q.x + i, q.y + j, q.z);
	    ON_3dPoint ANSWER = ON_3dPoint(50.0, 50.0, 0.0);
	    ON_3dPoint A = a;
	    ON_3dPoint B = b;
	    ON_3dPoint C = c;
	    ON_3dPoint tmpa = A;
	    ON_3dPoint tmpb = B;
	    ON_3dPoint tmpc = C;
	    ON_3dPoint tmpans = ANSWER;
	    ON_3dPoint tmpp = P;
	    ON_3dPoint tmpq = Q;
	    ON_3dPoint tmpp2 = p2;
	    ON_3dPoint tmpq2 = q2;
	    ON_3dPoint tmpp3 = p3;
	    ON_3dPoint tmpq3 = q3;
	    ON_3dPoint tmpp4 = p4;
	    ON_3dPoint tmpq4 = q4;
	    ON_3dPoint tmpp5 = p5;
	    ON_3dPoint tmpq5 = q5;
	    for (k=0; k<10; k++) {
		MAT4X3PNT(A, rotX, tmpa);
		MAT4X3PNT(B, rotX, tmpb);
		MAT4X3PNT(C, rotX, tmpc);
		MAT4X3PNT(P, rotX, tmpp);
		MAT4X3PNT(Q, rotX, tmpq);
		MAT4X3PNT(p2, rotX, tmpp2);
		MAT4X3PNT(q2, rotX, tmpq2);
		MAT4X3PNT(p3, rotX, tmpp3);
		MAT4X3PNT(q3, rotX, tmpq3);
		MAT4X3PNT(p4, rotX, tmpp4);
		MAT4X3PNT(q4, rotX, tmpq4);
		MAT4X3PNT(p5, rotX, tmpp5);
		MAT4X3PNT(q5, rotX, tmpq5);
		MAT4X3PNT(ANSWER, rotX, tmpans);
		for (l=0; l<10; l++) {
		    tmpa = A;
		    tmpb = B;
		    tmpc = C;
		    tmpans = ANSWER;
		    tmpp = P;
		    tmpq = Q;
		    tmpp2 = p2;
		    tmpq2 = q2;
		    tmpp3 = p3;
		    tmpq3 = q3;
		    tmpp4 = p4;
		    tmpq4 = q4;
		    tmpp5 = p5;
		    tmpq5 = q5;
		    MAT4X3PNT(A, rotY, tmpa);
		    MAT4X3PNT(B, rotY, tmpb);
		    MAT4X3PNT(C, rotY, tmpc);
		    MAT4X3PNT(P, rotY, tmpp);
		    MAT4X3PNT(Q, rotY, tmpq);
		    MAT4X3PNT(p2, rotY, tmpp2);
		    MAT4X3PNT(q2, rotY, tmpq2);
		    MAT4X3PNT(p3, rotY, tmpp3);
		    MAT4X3PNT(q3, rotY, tmpq3);
		    MAT4X3PNT(p4, rotY, tmpp4);
		    MAT4X3PNT(q4, rotY, tmpq4);
		    MAT4X3PNT(p5, rotY, tmpp5);
		    MAT4X3PNT(q5, rotY, tmpq5);
		    MAT4X3PNT(ANSWER, rotY, tmpans);
		    for (m=0; m<10; m++) {
			tmpa = A;
			tmpb = B;
			tmpc = C;
			tmpans = ANSWER;
			tmpp = P;
			tmpq = Q;
			tmpp2 = p2;
			tmpq2 = q2;
			tmpp3 = p3;
			tmpq3 = q3;
			tmpp4 = p4;
			tmpq4 = q4;
			tmpp5 = p5;
			tmpq5 = q5;
			MAT4X3PNT(A, rotZ, tmpa);
			MAT4X3PNT(B, rotZ, tmpb);
			MAT4X3PNT(C, rotZ, tmpc);
			MAT4X3PNT(P, rotZ, tmpp);
			MAT4X3PNT(Q, rotZ, tmpq);
			MAT4X3PNT(p2, rotZ, tmpp2);
			MAT4X3PNT(q2, rotZ, tmpq2);
			MAT4X3PNT(p3, rotZ, tmpp3);
			MAT4X3PNT(q3, rotZ, tmpq3);
			MAT4X3PNT(p4, rotZ, tmpp4);
			MAT4X3PNT(q4, rotZ, tmpq4);
			MAT4X3PNT(p5, rotZ, tmpp5);
			MAT4X3PNT(q5, rotZ, tmpq5);
			MAT4X3PNT(ANSWER, rotZ, tmpans);
			int rv;
			/* rv = SegmentTriangleIntersect(A, B, C, P, Q, out, 1.0e-10);
			total++;
			if (i + j < 101) {
			    if (rv != 1 || !VAPPROXEQUAL(ANSWER, out[0], 1.0e-10)) {
				bu_log("Failed with i = %i and j = %i \n", i, j);
				failed++;
			    }
			} else {
			    if (rv != 0) {
				bu_log("Failed with i = %i and j = %i \n", i, j);
				failed++;
			    }
			} */
			/* rv = SegmentTriangleIntersect(A, B, C, p3, q3, out, 1.0e-10);
			total++;
			if (!(rv == 2 && ((VAPPROXEQUAL(p2, out[0], 1.0e-10) && VAPPROXEQUAL(q2, out[1], 1.0e-10)) || (VAPPROXEQUAL(q2, out[0], 1.0e-10) && VAPPROXEQUAL(p2, out[1], 1.0e-10))))) {
			    bu_log("Failed with k = %i, l = %i, m = %i \n", k, l, m);
			    failed++;
			} */
			/* rv = SegmentTriangleIntersect(A, B, C, p4, q4, out, 1.0e-10);
			total++;
			if (!(rv == 2 && ((VAPPROXEQUAL(A, out[0], 1.0e-10) && VAPPROXEQUAL(ANSWER, out[1], 1.0e-10)) || (VAPPROXEQUAL(ANSWER, out[0], 1.0e-10) && VAPPROXEQUAL(A, out[1], 1.0e-10))))) {
			    bu_log("Failed with k = %i, l = %i, m = %i \n", k, l, m);
			    failed++;
			} */
			rv = SegmentTriangleIntersect(A, B, C, p5, q5, out, 1.0e-10);
			total++;
			if (!(rv == 2 && ((VAPPROXEQUAL(A, out[0], 1.0e-10) && VAPPROXEQUAL(B, out[1], 1.0e-10)) || (VAPPROXEQUAL(B, out[0], 1.0e-10) && VAPPROXEQUAL(A, out[1], 1.0e-10))))) {
			    bu_log("Failed with k = %i, l = %i, m = %i \n", k, l, m);
			    failed++;
			} 
		    }
		}
	    }
	/* }
    } */
    bu_log("Failed: %i of %i tests in SegmentTriangleIntersect\n", failed, total);

    return 0;
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
