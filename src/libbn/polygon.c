/*                     P O L Y G O N . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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

#include "bu/list.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bn/polygon.h"
#include "bn/plane_struct.h"
#include "bn/plane_calc.h"


int
bn_polygon_area(fastf_t *area, size_t npts, const point_t *pts)
{
    size_t i;
    vect_t v1, v2, tmp, tot = VINIT_ZERO;
    plane_t plane_eqn;
    struct bn_tol tol;

    if (!pts || !area || npts < 3)
	return 1;
    BN_TOL_INIT(&tol);
    tol.dist_sq = BN_TOL_DIST * BN_TOL_DIST;
    if (bn_mk_plane_3pts(plane_eqn, pts[0], pts[1], pts[2], &tol) == -1)
	return 1;

    switch (npts) {
	case 3:
	    /* Triangular Face - for triangular face T:V0, V1, V2,
	     * area = 0.5 * [(V2 - V0) x (V1 - V0)] */
	    VSUB2(v1, pts[1], pts[0]);
	    VSUB2(v2, pts[2], pts[0]);
	    VCROSS(tot, v2, v1);
	    break;
	case 4:
	    /* Quadrilateral Face - for planar quadrilateral
	     * Q:V0, V1, V2, V3 with unit normal N,
	     * area = N/2 ⋅ [(V2 - V0) x (V3 - V1)] */
	    VSUB2(v1, pts[2], pts[0]);
	    VSUB2(v2, pts[3], pts[1]);
	    VCROSS(tot, v2, v1);
	    break;
	default:
	    /* N-Sided Face - compute area using Green's Theorem */
	    for (i = 0; i < npts; i++) {
		VCROSS(tmp, pts[i], pts[i + 1 == npts ? 0 : i + 1]);
		VADD2(tot, tot, tmp);
	    }
	    break;
    }
    *area = fabs(VDOT(plane_eqn, tot)) * 0.5;
    return 0;
}


int
bn_polygon_centroid(point_t *cent, size_t npts, const point_t *pts)
{
    size_t i;
    fastf_t x_0 = 0.0;
    fastf_t x_1 = 0.0;
    fastf_t y_0 = 0.0;
    fastf_t y_1 = 0.0;
    fastf_t z_0 = 0.0;
    fastf_t z_1 = 0.0;
    fastf_t a = 0.0;
    fastf_t signedArea = 0.0;

    if (!pts || !cent || npts < 3)
	return 1;
    /* Calculate Centroid projection for face for x-y-plane */
    for (i = 0; i < npts-1; i++) {
	x_0 = pts[i][0];
	y_0 = pts[i][1];
	x_1 = pts[i+1][0];
	y_1 = pts[i+1][1];
	a = x_0 *y_1 - x_1*y_0;
	signedArea += a;
	*cent[0] += (x_0 + x_1)*a;
	*cent[1] += (y_0 + y_1)*a;
    }
    x_0 = pts[i][0];
    y_0 = pts[i][1];
    x_1 = pts[0][0];
    y_1 = pts[0][1];
    a = x_0 *y_1 - x_1*y_0;
    signedArea += a;
    *cent[0] += (x_0 + x_1)*a;
    *cent[1] += (y_0 + y_1)*a;

    signedArea *= 0.5;
    *cent[0] /= (6.0*signedArea);
    *cent[1] /= (6.0*signedArea);

    /* calculate Centroid projection for face for x-z-plane */

    signedArea = 0.0;
    for (i = 0; i < npts-1; i++) {
	x_0 = pts[i][0];
	z_0 = pts[i][2];
	x_1 = pts[i+1][0];
	z_1 = pts[i+1][2];
	a = x_0 *z_1 - x_1*z_0;
	signedArea += a;
	*cent[2] += (z_0 + z_1)*a;
    }
    x_0 = pts[i][0];
    z_0 = pts[i][2];
    x_1 = pts[0][0];
    z_0 = pts[0][2];
    a = x_0 *z_1 - x_1*z_0;
    signedArea += a;
    *cent[2] += (z_0 + z_1)*a;

    signedArea *= 0.5;
    *cent[2] /= (6.0*signedArea);
    return 0;
}


int
bn_polygon_mk_pts_planes(size_t *npts, point_t **pts, size_t neqs, const plane_t *eqs)
{
    size_t i, j, k, l;
    if (!npts || !pts || neqs < 4 || !eqs)
	return 1;
    /* find all vertices */
    for (i = 0; i < neqs - 2; i++) {
	for (j = i + 1; j < neqs - 1; j++) {
	    for (k = j + 1; k < neqs; k++) {
		point_t pt;
		int keep_point = 1;
		if (bn_mkpoint_3planes(pt, eqs[i], eqs[j], eqs[k]) < 0)
		    continue;
		/* discard pt if it is outside the polyhedron */
		for (l = 0; l < neqs; l++) {
		    if (l == i || l == j || l == k)
			continue;
		    if (DIST_PT_PLANE(pt, eqs[l]) > BN_TOL_DIST) {
			keep_point = 0;
			break;
		    }
		}
		/* found a good point, add it to each of the intersecting faces */
		if (keep_point) {
		    VMOVE(pts[i][npts[i]], (pt)); npts[i]++;
		    VMOVE(pts[j][npts[j]], (pt)); npts[j]++;
		    VMOVE(pts[k][npts[k]], (pt)); npts[k]++;
		}
	    }
	}
   }
    return 0;
}


HIDDEN int
sort_ccw(const void *x, const void *y, void *cmp)
{
    vect_t tmp;
    VCROSS(tmp, ((fastf_t *)x), ((fastf_t *)y));
    return VDOT(*((point_t *)cmp), tmp);
}


int
bn_polygon_sort_ccw(size_t npts, point_t *pts, plane_t cmp)
{
    if (!pts || npts < 3)
	return 1;
    bu_sort(pts, npts, sizeof(point_t), sort_ccw, &cmp);
    return 0;
}


/*
 * Translation to libbn data types of Franklin's point-in-polygon test.
 * See http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html
 * for a discussion of the subtleties involved with the inequality tests.
 * The below copyright applies to just the function bn_pt_in_polygon,
 * not the whole of polygon.c
 *
 * Copyright (c) 1970-2003, Wm. Randolph Franklin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimers.  Redistributions
 * in binary form must reproduce the above copyright notice in the
 * documentation and/or other materials provided with the distribution.
 * The name of W. Randolph Franklin may not be used to endorse or promote
 * products derived from this Software without specific prior written
 * permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
int
bn_pt_in_polygon(size_t nvert, const point2d_t *pnts, const point2d_t *test)
{
    size_t i = 0;
    size_t j = 0;
    int c = 0;
    for (i = 0, j = nvert-1; i < nvert; j = i++) {
	if ( ((pnts[i][1] > (*test)[1]) != (pnts[j][1] > (*test)[1])) &&
		((*test)[0] < (pnts[j][0]-pnts[i][0]) * ((*test)[1]-pnts[i][1]) / (pnts[j][1]-pnts[i][1]) + pnts[i][0]) )
	    c = !c;
    }
    return c;
}




struct pt_vertex {
    struct bu_list l;
    int index;
    int isConvex;
    int isEar;
    fastf_t angle;
};

struct pt_vertex_ref {
    struct bu_list l;
    struct pt_vertex *v;
};

#define PT_ADD_VREF(_list, vert) {\
	struct pt_vertex_ref *n_ref; \
	BU_GET(n_ref, struct pt_vertex_ref); \
	n_ref->v = vert; \
	BU_LIST_PUSH(&(_list->l), &(n_ref->l)); \
}


HIDDEN int
is_inside(const point2d_t p1, const point2d_t p2, const point2d_t p3, const point2d_t *test) {
    point2d_t tri[3];
    V2MOVE(tri[0], p1);
    V2MOVE(tri[1], p2);
    V2MOVE(tri[2], p3);
    return bn_pt_in_polygon(3, (const point2d_t *)tri, test);
}


HIDDEN int
is_ear(int p_ind, int p_prev_ind, int p_next_ind, struct pt_vertex_ref *reflex_list, const point2d_t *pts)
{
    int ear = 1;
    struct pt_vertex_ref *vref = NULL;
    for (BU_LIST_FOR_BACKWARDS(vref, pt_vertex_ref, &(reflex_list->l))) {
	int vrind = vref->v->index;
	if (vrind == p_ind || vrind == p_prev_ind || vrind == p_next_ind) continue;
	if (is_inside(pts[p_ind], pts[p_prev_ind], pts[p_next_ind], (const point2d_t *)&pts[vrind])) {
	    ear = 0;
	    break;
	}
    }
    return ear;
}

/* Use the convexity test from polypartition */
HIDDEN int
is_convex(const point2d_t test, const point2d_t p1, const point2d_t p2) {

    double testval;
    testval = (p2[1]-p1[1])*(test[0]-p1[0])-(p2[0]-p1[0])*(test[1]-p1[1]);
    return (testval > 0) ? 1 : 0;
}

HIDDEN void
pt_vertex_init(struct pt_vertex *v)
{
    v->index = -1;
    v->isConvex = -1;
    v->isEar = -1;
    v->angle = -1;
}

int bn_polygon_triangulate(int **faces, int *num_faces, const point2d_t *pts, size_t npts)
{
    size_t i = 0;
    fastf_t max_angle = 0.0;
    size_t seed_vert = -1;
    struct pt_vertex *v = NULL;
    struct pt_vertex *vertex_list = NULL;
    struct pt_vertex_ref *vref = NULL;
    struct pt_vertex_ref *ear_list = NULL;
    struct pt_vertex_ref *reflex_list = NULL;
    struct pt_vertex_ref *convex_list = NULL;

    if(npts < 3) return 1;
    if (!faces || !num_faces || !pts) return 1;

    BU_GET(vertex_list, struct pt_vertex);
    BU_LIST_INIT(&(vertex_list->l));
    vertex_list->index = -1;

    BU_GET(ear_list, struct pt_vertex_ref);
    BU_LIST_INIT(&(ear_list->l));
    ear_list->v = NULL;

    BU_GET(reflex_list, struct pt_vertex_ref);
    BU_LIST_INIT(&(reflex_list->l));
    reflex_list->v = NULL;

    BU_GET(convex_list, struct pt_vertex_ref);
    BU_LIST_INIT(&(convex_list->l));
    convex_list->v = NULL;

    /* Iniitalize vertex list. */
    for (i = 0; i < npts; i++) {
	struct pt_vertex *new_vertex;
	BU_GET(new_vertex, struct pt_vertex);
	pt_vertex_init(new_vertex);
	new_vertex->index = i;
	BU_LIST_PUSH(&(vertex_list->l), &(new_vertex->l));
    }

    /* Point ordering ends up opposite to that of the points in the array, so
     * everything is backwards */

    /* Find the initial convex and reflex point sets */
    for (BU_LIST_FOR_BACKWARDS(v, pt_vertex, &(vertex_list->l)))
    {
	struct pt_vertex *prev = BU_LIST_PNEXT_CIRC(pt_vertex, &v->l);
	struct pt_vertex *next = BU_LIST_PPREV_CIRC(pt_vertex, &v->l);
#if 0
	bu_log("v[%d]: %f %f 0\n", v->index, pts[v->index][0], pts[v->index][1]);
	bu_log("vprev[%d]: %f %f 0\n", prev->index, pts[prev->index][0], pts[prev->index][1]);
	bu_log("vnext[%d]: %f %f 0\n", next->index, pts[next->index][0], pts[next->index][1]);
#endif
	v->isConvex = is_convex(pts[v->index], pts[prev->index], pts[next->index]);
	if (v->isConvex) {
	    PT_ADD_VREF(convex_list, v);
	} else {
	    v->isEar = 0;
	    v->angle = 0;
	    PT_ADD_VREF(reflex_list, v);
	}
    }
    /* Now that we know which are the convex and reflex verts, find the initial ears */
    for (BU_LIST_FOR_BACKWARDS(vref, pt_vertex_ref, &(convex_list->l)))
    {
	struct pt_vertex *p = BU_LIST_PNEXT_CIRC(pt_vertex, &vref->v->l);
	struct pt_vertex *n = BU_LIST_PPREV_CIRC(pt_vertex, &vref->v->l);
	vref->v->isEar = is_ear(vref->v->index, p->index, n->index, reflex_list, pts);
	if (vref->v->isEar) {
	    point2d_t v1, v2;
	    PT_ADD_VREF(ear_list, vref->v);
	    V2SUB2(v1, pts[p->index], pts[vref->v->index]);
	    V2SUB2(v2, pts[n->index], pts[vref->v->index]);
	    V2UNITIZE(v1);
	    V2UNITIZE(v2);
	    vref->v->angle = fabs(v1[0]*v2[0] + v1[1]*v2[1]);
	    if (vref->v->angle > max_angle) {
		seed_vert = vref->v->index;
		max_angle = vref->v->angle;
	    }
	} else {
	    vref->v->angle = 0;
	}
    }


    for (BU_LIST_FOR_BACKWARDS(vref, pt_vertex_ref, &(convex_list->l))){
	bu_log("convex vert: %d\n", vref->v->index);
    }
    for (BU_LIST_FOR_BACKWARDS(vref, pt_vertex_ref, &(reflex_list->l))){
	bu_log("reflex vert: %d\n", vref->v->index);
    }
    for (BU_LIST_FOR_BACKWARDS(vref, pt_vertex_ref, &(ear_list->l))){
	bu_log("ear vert: %d\n", vref->v->index);
    }
    bu_log("seed vert: %d\n", seed_vert);

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
