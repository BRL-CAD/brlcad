/* This is a C translation of Mark Dickinson's point-in-polyhedron test from
 * https://github.com/mdickinson/polyhedron/ */

/* TODO - should also look into this code to see if it can do the same job
 * faster...:
 *
 * https://github.com/GavinBarill/fast-winding-number-soups
 */

/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2019, Mark Dickinson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Robust point-in-polyhedron test.
 *
 * Given an closed, oriented surface in R^3 described by a triangular mesh, the
 * code below gives a robust algorithm for determining whether a given point is
 * inside, on the boundary of, or outside, the surface.  The algorithm should give
 * correct results even in degenerate cases, and applies to disconnected
 * polyhedra, non simply-connected surfaces, and so on.  There are no requirements
 * for the surface to be convex, simple, connected or simply-connected.
 *
 * More precisely, we give a method for computing the *winding number* of a closed
 * oriented surface S around a point TP that doesn't lie on S.  Roughly speaking,
 * the winding number of the closed oriented surface S around a point TP not on S
 * is the number of times that the surface encloses that point; for a simple
 * outward-oriented surface (like that of a convex polyhedron, for example), the
 * winding number will be 1 for points inside the surface and 0 for points
 * outside.
 *
 * For a precise definition of winding number, we can turn to algebraic topology:
 * our oriented surface is presented as a collection of combinatorial data
 * defining abstract vertices, edges and triangles, together with a mapping of
 * those vertices to R^3.  The combinatorial data describe a simplicial complex C,
 * and assuming that TP doesn't lie on the surface, the mapping of the vertices to
 * R^3 gives a continuous map from the geometric realization of C to R^3 - {TP}.
 * This in turn induces a map on second homology groups:
 *
 *    H^2(C, Z) -> H^2(R^3 - {TP}, Z)
 *
 * and by taking the usual right-handed orientation in R^3 we identify H^2(R^3 -
 * {TP}, Z) with Z.  The image of [S] under this map gives the winding number.  In
 * particular, the well-definedness of the winding number does not depend on
 * topological properties of the embedding: it doesn't matter if the surface is
 * self-intersecting, or has degenerate triangles.  The only condition is that TP
 * does not lie on the surface S.
 *
 * Algorithm
 * ---------
 * The algorithm is based around the usual method of ray-casting: we take a
 * vertical line L through TP and count the intersections of this line with the
 * triangles of the surface, keeping track of orientations as we go.  Let's ignore
 * corner cases for a moment and assume that:
 *
 * (1) TP does not lie on the surface, and
 * (2) for each triangle T (thought of as a closed subset of R^3) touched by
 *     our vertical line L, L meets the interior of T in exactly one point Q
 *
 * Then there are four possibilities for each such triangle T:
 *
 * 1. T lies *above* TP and is oriented *upwards* (*away* from TP).
 * 2. T lies *above* TP and is oriented *downwards* (*towards* TP).
 * 3. T lies *below* TP and is oriented *downwards* (*away* from TP).
 * 4. T lies *below* TP and is oriented *upwards* (*towards* TP).
 *
 * Let's write N1, N2, N3 and N4 for the counts of triangles satisfying conditions
 * 1, 2, 3 and 4 respectively.  Since we have a closed surface, these numbers
 * are not independent; they satisfy the relation:
 *
 *     N1 + N4 == N2 + N3
 *
 * That is, the number of upward-facing triangles must match the number of
 * downward-facing triangles.  The winding number w is then given by:
 *
 *     w = N1 - N2 == N3 - N4
 *
 * In the code below, we simply compute 2*w = (N1 + N3) - (N2 + N4), so each
 * triangle oriented away from TP contributes 1 to 2w, while each triangle oriented
 * towards TP contributes -1.
 *
 *
 * Making the algorithm robust
 * ---------------------------
 * Now we describe how to augment the basic algorithm described above to include:
 *
 * - correct treatment of corner cases (vertical triangles, cases where L meets an
 *   edge or vertex directly, etc.)
 *
 * - detection of the case where the point lies directly on the surface.
 *
 * It turns out that to make the algorithm robust, all we need to do is be careful
 * and consistent about classifying vertices, edges and triangles.  We do this as
 * follows:
 *
 * - Each vertex of the surface that's not equal to TP is considered *positive* if
 *   its coordinates are lexicographically greater than TP, and *negative*
 *   otherwise.
 *
 * - For an edge PQ of the surface that's not collinear with TP, we first describe
 *   the classification in the case that P is negative and Q is positive, and
 *   then extend to arbitrary PQ.
 *
 *   For P negative and Q positive, there are two cases:
 *
 *   1. P and Q have distinct x coordinates.  In that case we classify the edge
 *      PQ by its intersection with the plane passing through TP and parallel
 *      to the yz-plane: the edge is *positive* if the intersection point is
 *      positive, and *negative* otherwise.
 *
 *   2. P and Q have the same x coordinate, in which case they must have
 *      distinct y coordinates.  (If the x and the y coordinates both match
 *      then PQ passes through TP.)  We classify by the intersection of PQ
 *      with the line parallel to the y-axis through TP.
 *
 *   For P positive and Q negative, we classify as above but reverse the sign.
 *   For like-signed P and Q, the classification isn't used.
 *
 *   Computationally, in case 1 above, the y-coordinate of the intersection
 *   point is:
 *
 *       Py + (Qy - Py) * (TPx - Px) / (Qx - Px)
 *
 *   and this is greater than TPy iff
 *
 *       (Py - TPy) * (Qx - TPx) - (Px - TPx) * (Qy - TPy)
 *
 *   is positive, so the sign of the edge is the sign of the above expression.
 *   Similarly, if this quantity is zero then we need to look at the z-coordinate
 *   of the intersection, and the sign of the edge is given by
 *
 *       (Pz - TPz) * (Qx - TPx) - (Px - TPx) * (Qz - TPz)
 *
 *   In case 2, both of the above quantities are zero, and the sign of the edge is
 *   the sign of
 *
 *       (Pz - TPz) * (Qy - TPy) - (Py - TPy) * (Qz - TPz)
 *
 *   Another way to look at this: if P, Q and TP are not collinear then the
 *   matrix
 *
 *    ( Px Qx TPx )
 *    ( Py Qy TPx )
 *    ( Pz Qz TPx )
 *    (  1  1  1 )
 *
 *   has rank 3.  It follows that at least one of the three 3x3 minors
 *
 *    | Px Qx TPx |  | Px Qx TPx |  | Py Qy TPy |
 *    | Py Qy TPy |  | Pz Qz TPz |  | Pz Qz TPz |
 *    |  1  1  1 |  |  1  1  1 |  |  1  1  1 |
 *
 *   is nonzero.  We define the sign of PQ to be the *negative* of the sign of the
 *   first nonzero minor in that list.
 *
 * - Each triangle PQR of the surface that's not coplanar with TP is considered
 *   *positive* if its normal points away from TP, and *negative* if its normal
 *   points towards TP.
 *
 *   Computationally, the sign of the triangle PQR is the sign of the 4x4
 *   determinant
 *
 *     | Px Qx Rx TPx |
 *     | Py Qy Ry TPy |
 *     | Pz Qz Rz TPz |
 *     |  1  1  1  1 |
 *
 *   or equivalently of the 3x3 determinant
 *
 *     | Px-TPx Qx-TPx Rx-TPx |
 *     | Py-TPy Qy-TPy Ry-TPy |
 *     | Pz-TPz Qz-TPz Rz-TPz |
 *
 *
 * Now to compute the contribution of any given triangle to the total winding
 * number:
 *
 * 1. Classify the vertices of the triangle.  At the same time, we can check that
 *    none of the vertices is equal to TP.  If all vertices have the same sign,
 *    then the winding number contribution is zero.
 *
 * 2. Assuming that the vertices do not all have the same sign, two of the three
 *    edges connect two differently-signed vertices.  Classify both those edges
 *    (and simultaneously check that they don't pass through TP).  If the edges
 *    have opposite classification, then the winding number contribution is zero.
 *
 * 3. Now two of the edges have the same sign: classify the triangle itself.  If
 *    the triangle is positive it contributes 1/2 to the winding number total; if
 *    negative it contributes -1/2.  In practice we count contributions of 1 and
 *    -1, and halve the total at the end.
 *
 * Note that an edge between two like-signed vertices can never pass through TP, so
 * there's no need to check the third edge in step 2.  Similarly, a triangle whose
 * edge-cycle is trivial can't contain TP in its interior.
 *
 * To understand what's going on above, it's helpful to step into the world of
 * homology again. The homology of R^3 - {TP} can be identified with that of the
 * two-sphere S^2 by deformation retract, and we can decompose the two-sphere as a
 * CW complex consisting of six cells, as follows:
 *
 * 0-cells B and F, where B = (-1, 0, 0) and F = (1, 0, 0)
 * 1-cells L and R, where
 *      L = {(cos t, sin t, 0) | -pi <= t <= 0 }
 *      R = {(cos t, sin t, 0) | 0 <= t <= pi }
 * 2-cells U and D, where U is the top half of the sphere (z >= 0)
 *   and D is the bottom half (z <= 0), both oriented outwards.
 *
 * And the homology of the CW complex is now representable in terms of cellular
 * homology:
 *
 *                d               d
 *   Z[U] + Z[D] --> Z[L] + Z[R] --> Z[B] + Z[F]
 *
 * with boundary maps given by:
 *
 *   d[U] = [L] + [R]; d[D] = -[L] - [R]
 *   d[R] = [B] - [F]; d[L] = [F] - [B]
 *
 * Now the original map C -> R^3 - {TP} from the geometric realization of the
 * simplicial complex is homotopic to a map C -> S^2 that sends:
 *
 * * each positive vertex to F and each negative vertex to B
 * * each edge with boundary [F] - [B] to L if the edge is negative, and -R if the
 *   edge is positive
 * * each edge with boundary [B] - [F] to R if the edge is positive, and -L if the
 *   edge is negative
 * * all other edges to 0
 * * each triangle whose boundary is [L] + [R] to either U or -D,
 *   depending on whether the triangle is positive or negative
 * * each triangle whose boundary is -[L] - [R] to either D or -U,
 *   depending on whether the triangle is positive or negative
 * * all other triangles to 0
 *
 * Mapping all of the triangles in the surface this way, and summing the results
 * in second homology, we end up with (winding number)*([U] + [D]).
 */

#include "common.h"

#include "vmath.h"
#include "bu/log.h"
#include "bg/trimesh.h"

/* Return 1 if x is positive, -1 if it's negative, and 0 if it's zero. */
static int ptm_sign(double x)
{
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
}

/* Sign of the vertex P with respect to O, as defined above. */
static int ptm_vertex_sign(point_t V, point_t TP, int *exact_flag)
{
    int rx,ry, rz;
    rx = ptm_sign(V[X] - TP[X]);
    if (rx) return rx;

    ry = ptm_sign(V[Y] - TP[Y]);
    if (ry) return ry;

    rz = ptm_sign(V[Z] - TP[Z]);
    if (rz) return rz;

    (*exact_flag) = 1;
    /*bu_log("Error: vertex coincides with test point\n");*/
    return 0;
}

/* Sign of the edge PQ with respect to O, as defined above. */
static int ptm_edge_sign(point_t P, point_t Q, point_t TP, int *exact_flag)
{
    int r1, r2, r3;
    r1 = ptm_sign((P[Y] - TP[Y]) * (Q[X] - TP[X]) - (P[X] - TP[X]) * (Q[Y] - TP[Y]));
    if (r1) return r1;

    r2 = ptm_sign((P[Z] - TP[Z]) * (Q[X] - TP[X]) - (P[X] - TP[X]) * (Q[Z] - TP[Z]));
    if (r2) return r2;

    r3 = ptm_sign((P[Z] - TP[Z]) * (Q[Y] - TP[Y]) - (P[Y] - TP[Y]) * (Q[Z] - TP[Z]));
    if (r3) return r3;

    (*exact_flag) = 1;
    /*bu_log("Error: vertices collinear with origin\n");*/
    return 0;
}

/* Sign of the triangle PQR with respect to O, as defined above. */
static int ptm_triangle_sign(point_t P, point_t Q, point_t R, point_t TP, int *exact_flag)
{
    double m1_0 = P[X] - TP[X];
    double m1_1 = P[Y] - TP[Y];
    double m2_0 = Q[X] - TP[X];
    double m2_1 = Q[Y] - TP[Y];
    double m3_0 = R[X] - TP[X];
    double m3_1 = R[Y] - TP[Y];

    int r = ptm_sign(
	    (m1_0 * m2_1 - m1_1 * m2_0) * (R[Z] - TP[Z]) +
	    (m2_0 * m3_1 - m2_1 * m3_0) * (P[Z] - TP[Z]) +
	    (m3_0 * m1_1 - m3_1 * m1_0) * (Q[Z] - TP[Z]));

    if (!r) {
	(*exact_flag) = 1;
	/*bu_log("vertices coplanar with origin\n");*/
    }
    return r;
}

/* Return the contribution of this triangle to the winding number. */
int
bg_ptm_triangle_chain(point_t v1, point_t v2, point_t v3, point_t tp, int *exact_flag)
{
    int ef = 0;
    int v1sign = ptm_vertex_sign(v1, tp, &ef);
    int v2sign = ptm_vertex_sign(v2, tp, &ef);
    int v3sign = ptm_vertex_sign(v3, tp, &ef);
    int face_boundary = 0;
    int tsign;

    if (v1sign != v2sign) {
	face_boundary += ptm_edge_sign(v1, v2, tp, &ef);
    }
    if (v2sign != v3sign) {
	face_boundary += ptm_edge_sign(v2, v3, tp, &ef);
    }
    if (v3sign != v1sign) {
	face_boundary += ptm_edge_sign(v3, v1, tp, &ef);
    }
    if (!face_boundary) {
	if (ef) {
	    (*exact_flag) = 1;
	}
	return 0;
    }

    tsign = ptm_triangle_sign(v1, v2, v3, tp, &ef);
    if (ef) {
	(*exact_flag) = 1;
    }
    return tsign;
}


int
bg_trimesh_pt_in(point_t tp, int num_faces, int *faces, int num_vertices, point_t *pts)
{
    int exact = 0;
    int wn = 0;
    int not_solid = bg_trimesh_solid2(num_vertices, num_faces, (fastf_t *)pts, faces, NULL);
    if (not_solid) {
	return -1;
    }

    for (int i = 0; i < num_faces; i++) {
	point_t v1, v2, v3;
	VMOVE(v1, pts[faces[3*i+0]]);
	VMOVE(v2, pts[faces[3*i+1]]);
	VMOVE(v3, pts[faces[3*i+2]]);
	wn += bg_ptm_triangle_chain(v1, v2, v3, tp, &exact);
	if (exact) return 2;
    }

    return wn;
}



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
