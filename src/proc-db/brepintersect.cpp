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
	if (-tol <= s && s <= 1.0 + tol && -tol <= t && t <= 1.0 + tol) {
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
		for (i = 0; i < 3; i++) {
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


/* intersects triangle abc with triangle def
 * returns the number of intersections found [0-6]
 * more accurately it returns the number of points needed
 * to describe the intersection so 6 points indicates intersection
 * in a hexagon.
 * Although it's never explicitly tested the points returned will be within
 * the tolerance of planar
 * TODO: make sure that the edges are outputted in an order representative of the polygonal intersection
 */
int TriangleTriangleIntersect(
	const ON_3dPoint a,
	const ON_3dPoint b,
	const ON_3dPoint c,
	const ON_3dPoint d,
	const ON_3dPoint e,
	const ON_3dPoint f,
	ON_3dPoint out[6],
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
	    ON_3dPoint P = result[0];
	    bool dup = false;
	    for (k = 0; k < number_found; k++) {
		if (VAPPROXEQUAL(out[k], P, tol)) {
		    dup = true;
		    break;
		}
	    }
	    if (!dup)
		out[number_found] = P;
	}
	number_found += rv;
    }

    /* intersect the edges of triangle def with triangle abc*/
    for (i = 0; i < 3; i++) {
	rv = SegmentTriangleIntersect(a, b, c, def[i], def[(i + 1) % 3], result, tol);
	for (j = 0; j < rv; j++) {
	    ON_3dPoint P = result[0];
	    bool dup = false;
	    for (k = 0; k < number_found; k++) {
		if (VAPPROXEQUAL(out[k], P, tol)) {
		    dup = true;
		    break;
		}
	    }
	    if (!dup)
		out[number_found] = P;
	}
	number_found += rv;
    }

    return number_found;
}

int MeshMeshIntersect(
	const ON_Mesh *mesh1,
	const ON_Mesh *mesh2,
	ON_SimpleArray<ON_Polyline>& out,
	double tol
	)
{
    /* First we go through and intersect the meshes triangle by triangle.
     * Then we take all the lines we get back and store them in an array.
     * If we get a point back from an intersection we can ignore that because:
     *   -either we're going to get that point back anyways when we get the lines on either side of it back
     *   -or it's an intersection of just a point and we can ignore it. */
    ON_SimpleArray<ON_Line> segments;
    int i,j,k,l;
    for (i = 0; i < mesh1->FaceCount(); i++) {
	ON_MeshFace face = mesh1->m_F[i];
	int n_triangles1;
	ON_3dPoint triangles1[2][3];
	if (face.IsTriangle()) {
	    n_triangles1 = 1;
	    for (j = 0; j < 3; j++) { /* load the points from the mesh */
		triangles1[0][j] = (const ON_3dPoint&) (face.vi[j]);
	    }
	} else { /* we have a quad which we need to treat as two triangles */
	    n_triangles1 = 2;
	    for (j = 0; j < 3; j++) {
		triangles1[0][j] = ON_3dPoint(mesh1->m_V[face.vi[j]]);
		triangles1[1][j] = ON_3dPoint(mesh1->m_V[face.vi[(j + 2) % 4]]);
	    }
	}
	for (j = 0; j < n_triangles1; j++) {
	    ON_3dPoint T[3] = {triangles1[j][0], triangles1[j][1], triangles1[j][2]};
	    for (k = 0; k < mesh2->FaceCount(); k++) {
		ON_MeshFace face = mesh2->m_F[k];
		ON_3dPoint result[6];
		int n_triangles2;
		ON_3dPoint triangles2[2][3]; /* we may need room for two triangles */
		if (face.IsTriangle()) {
		    n_triangles2 = 1;
		    for (l = 0; l < 3; l++) { /* load the points from the mesh */
			triangles2[0][l] = (const ON_3dPoint&) (face.vi[l]);
		    }
		} else { /* we have a quad which we need to treat as two triangles */
		    n_triangles2 = 2;
		    for (l = 0; l < 3; l++) {
			triangles2[0][l] = ON_3dPoint(mesh2->m_V[face.vi[l]]);
			triangles2[1][l] = ON_3dPoint(mesh2->m_V[face.vi[(l + 2) % 4]]);
		    }
		}
		for (l = 0; l < n_triangles2; l++) {
		    int rv = TriangleTriangleIntersect(T[0], T[1], T[2], triangles2[l][0], triangles2[l][1], triangles2[l][2], result, tol);
		    if (rv == 2) {
			ON_Line segment;
			segment[0] = result[0];
			segment[1] = result[1];
			segments.Append(segment);
		    }
		}
	    }
	}
    }
    /* Now we have all the lines in an array, but we need to arrange them in some Polylines 
     * Remember two meshes could intersect in arbitrarily many entirely distinct polylines */
    out.Empty();
    ON_Polyline answer;
    while (segments.Count() > 0) {
	answer.Empty();
	answer.Append(segments.First()->from); /* initialize the Polyline with the first segment */
	answer.Append(segments.First()->to);
	segments.Remove(0);
	/* now we look for segments attached to our base Polyline */
	while (!answer.IsClosed(tol)) { 
	    for (j = 0; j < segments.Count(); j++) {
		ON_Line segment = segments[j];
		if (VAPPROXEQUAL(segment.from, *answer.First(), tol)) {
		    answer.Insert(0, segment.to);
		    segments.Remove(j);
		    break;
		} else if (VAPPROXEQUAL(segment.from, *answer.Last(), tol)) {
		    answer.Append(segment.to);
		    segments.Remove(j);
		    break;
		} else if (VAPPROXEQUAL(segment.to, *answer.First(), tol)) {
		    answer.Insert(0, segment.from);
		    segments.Remove(j);
		    break;
		} else if (VAPPROXEQUAL(segment.to, *answer.Last(), tol)) {
		    answer.Append(segment.from);
		    segments.Remove(j);
		    break;
		}
	    }
	    /* This breaks if we complete a loop which only happens if we don't find anything, which means we won't
	     * ever find anything. Or this breaks if we try to do a loop with segments.Count() == 0. In which case
	     * there's nothing to find. */
	    if (j == segments.Count())
		break;
	}
	out.Append(answer);
    }
    return i + 1;
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
    /* create the faces  */
    ON_MeshFace abcd = {0, 1, 2, 3};
    ON_MeshFace efba = {4, 5, 1, 0};
    ON_MeshFace ehgf = {4, 7, 6, 5};
    ON_MeshFace dcgh = {3, 2, 6, 7};
    ON_MeshFace adhe = {0, 3, 7, 4};
    ON_MeshFace bfgc = {1, 5, 6, 2};
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
    ON_SimpleArray<ON_Polyline> out;
    int rv = MeshMeshIntersect(&mesh1, &mesh2, out, 1.0e-10);
    assert(rv == 2);
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
