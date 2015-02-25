/*                     P O L Y G O N . C P P
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

extern "C" {
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bn/polygon.h"
#include "bn/plane_struct.h"
#include "bn/plane_calc.h"
#include "./bn_private.h"

#include "plot3.h"
}

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

/* Translation to libbn data types of the polypartition triangulation code
 * from https://code.google.com/p/polypartition/
 *
 * The below copyright applies to just the function bn_polygon_triangulate,
 * the supporting fuctions ported from polypartition, and associated data
 * types, not the whole of polygon.c
 *
 * Copyright (C) 2011 by Ivan Fratric
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

typedef struct PartitionVertex pv_t;

struct PartitionVertex {
    int isActive;
    int isConvex;
    int isEar;

    int p;  /* Index into pts array */
    double angle;
    pv_t *previous;
    pv_t *next;
};

HIDDEN void
partition_normalize(point2d_t *p) {
    point2d_t r;
    double n = sqrt((*p)[0]*(*p)[0] + (*p)[1]*(*p)[1]);
    if(!NEAR_ZERO(n,SMALL_FASTF)) {
	/*r = p/n;*/
	V2MOVE(r,(*p));
	V2SCALE(r, r, 1/n);
    } else {
	V2SETALL(r, 0);
    }
    V2MOVE((*p), r);
}


HIDDEN int
partition_isconvex(const point2d_t p1, const point2d_t p2, const point2d_t p3) {
    double angle;
    double dot, m1, m2; 
    vect_t v1, v2;
    V2SUB2(v1, p1, p2);
    V2SUB2(v2, p3, p2);
    dot = V2DOT(v1, v2);
    m1 = MAGNITUDE2(v1);
    m2 = MAGNITUDE2(v2);
    angle = acos(dot/(m1 * m2));
    if (angle < M_PI) return 1;
    else return 0;
}

HIDDEN int
partition_isinside(const point2d_t *p1, const point2d_t *p2, const point2d_t *p3, const point2d_t *p) {
    point2d_t points[3];
    V2MOVE(points[0], *p1);
    V2MOVE(points[1], *p2);
    V2MOVE(points[2], *p3);
    return bn_pt_in_polygon(3, (const point2d_t *)points, p);
}

HIDDEN void
triangulate_updatevertex(pv_t *v, pv_t *vertices, const point2d_t *pts, size_t numvertices) {
    size_t i;
    pv_t *v1,*v3;
    point2d_t vec1,vec3;

    v1 = v->previous;
    v3 = v->next;

    v->isConvex = partition_isconvex(pts[v1->p],pts[v->p],pts[v3->p]);

    /* vec1 = Normalize(v1->p - v->p);*/
    V2SUB2(vec1, pts[v1->p], pts[v->p]);
    partition_normalize(&vec1);

    /* vec3 = Normalize(v3->p - v->p); */
    V2SUB2(vec3, pts[v3->p], pts[v->p]);
    partition_normalize(&vec3);

    v->angle = vec1[0]*vec3[0] + vec1[1]*vec3[1];

    if(v->isConvex) {
	v->isEar = 1;
	for(i = 0; i < numvertices; i++) {
	    if (vertices[i].p == v->p) continue;
	    if (vertices[i].p == v1->p) continue;
	    if (vertices[i].p == v3->p) continue;
#if 0
	    if(NEAR_ZERO(pts[vertices[i].p][0] - pts[v->p][0], SMALL_FASTF) && NEAR_ZERO(pts[vertices[i].p][1] - pts[v->p][1], SMALL_FASTF)) continue;
	    if(NEAR_ZERO(pts[vertices[i].p][0] - pts[v1->p][0], SMALL_FASTF) && NEAR_ZERO(pts[vertices[i].p][1] - pts[v1->p][1], SMALL_FASTF)) continue;
	    if(NEAR_ZERO(pts[vertices[i].p][0] - pts[v3->p][0], SMALL_FASTF) && NEAR_ZERO(pts[vertices[i].p][1] - pts[v3->p][1], SMALL_FASTF)) continue;
#endif
	    if(partition_isinside((const point2d_t *)&(pts[v1->p]),(const point2d_t *)&(pts[v->p]),(const point2d_t *)&(pts[v3->p]),(const point2d_t *)&(pts[vertices[i].p]))) {
		bu_log("isinside failed: %d\n", i);
		v->isEar = 0;
		break;
	    }
	}
    } else {
	v->isEar = 0;
    }
}

int
bn_polygon_triangulate(int **faces, int *num_faces, const point2d_t *pts, size_t npts)
{
    pv_t *vertices;
    pv_t *ear;
    int fp;
    int *local_faces;
    /*TPPLPoly triangle;*/
    size_t i,j;
    int earfound;
    int offset = 0;
    int face_cnt = 0;

    int active_vert_cnt = 0;

    if(npts < 3) return 1;
    if (!faces || !num_faces || !pts) return 1;

    if(npts == 3) {
	(*num_faces) = 1;
	(*faces) = (int *)bu_calloc(3, sizeof(int), "face");
	(*faces)[0] = 0;
	(*faces)[1] = 1;
	(*faces)[2] = 2;

	return 0;
    }

    local_faces = (int *)bu_calloc(3*3*npts, sizeof(int), "triangles");
    vertices = (pv_t *)bu_calloc(npts, sizeof(pv_t), "vertices");
    for(i=0;i<npts;i++) {
	vertices[i].isActive = 1;
	active_vert_cnt++;
	vertices[i].p = i;
	if(i==(npts-1)) vertices[i].next=&(vertices[0]);
	else vertices[i].next=&(vertices[i+1]);
	if(i==0) vertices[i].previous = &(vertices[npts-1]);
	else vertices[i].previous = &(vertices[i-1]);
    }
    for(i = 0; i < npts; i++) {
	triangulate_updatevertex(&vertices[i],vertices,pts,npts);
    }

    for(i = 0; i < npts - 3; i++) {
	earfound = 0;
	/* find the most extruded ear */
	for(j=0;j<npts;j++) {
	    if(!vertices[j].isActive) continue;
	    if(!vertices[j].isEar) continue;
	    if(!earfound) {
		earfound = 1;
		ear = &(vertices[j]);
	    } else {
		if(vertices[j].angle > ear->angle) {
		    ear = &(vertices[j]);
		}
	    }
	}
	if(!earfound) {
	    FILE *plot_file = fopen("bn_poly_tri.pl", "w");
	    int have_start = 0;
	    int start_vert = 0;
	    for(j=0;j<npts;j++) {
		if(!vertices[j].isActive) continue;
		if (!have_start) {
		    pd_3move(plot_file, pts[vertices[j].p][0], pts[vertices[j].p][1], 0);
		    have_start = 1;
		    start_vert = j;
		} else {
		    pd_3cont(plot_file, pts[vertices[j].p][0], pts[vertices[j].p][1], 0);
		}
	    }
	    pd_3cont(plot_file, pts[vertices[start_vert].p][0], pts[vertices[start_vert].p][1], 0);
	    fclose(plot_file);

	    bu_free(vertices, "free vertices");
	    bu_log("no ears found\n");
	    return 1;
	}

	/*triangles->push_back(triangle);*/
	local_faces[offset+0] = ear->previous->p;
	local_faces[offset+1] = ear->p;
	local_faces[offset+2] = ear->next->p;
	offset = offset + 3;
	face_cnt++;

	ear->isActive = 0;
	ear->previous->next = ear->next;
	ear->next->previous = ear->previous;

	if(i==npts-4) break;

	triangulate_updatevertex(ear->previous,vertices,pts,npts);
	triangulate_updatevertex(ear->next,vertices,pts,npts);
    }
    for(i = 0; i < npts; i++) {
	if(vertices[i].isActive) {
	    /*triangle.Triangle(vertices[i].previous->p,vertices[i].p,vertices[i].next->p);*/
	    /* triangles->push_back(triangle);*/
	    local_faces[offset+0] = vertices[i].previous->p;
	    local_faces[offset+1] = vertices[i].p;
	    local_faces[offset+2] = vertices[i].next->p;
	    offset = offset + 3;
	    vertices[i].isActive = 0;
	    face_cnt++;
	    break;
	}
    }

    (*num_faces) = face_cnt;
    (*faces) = (int *)bu_calloc(face_cnt*3, sizeof(int), "final faces array");
    for (fp = 0; fp < face_cnt; fp++) {
	(*faces)[fp*3] = local_faces[fp*3];
	(*faces)[fp*3+1] = local_faces[fp*3+1];
	(*faces)[fp*3+2] = local_faces[fp*3+2];
    }
    bu_free(local_faces, "free local faces array");
    bu_free(vertices, "free vertices");
    return 0;
}

int
bn_3d_polygon_triangulate(int **faces, int *num_faces, const point_t *pts, size_t n)
{
    int ret = 0;
    point_t origin_pnt;
    vect_t u_axis, v_axis;
    point2d_t *points_tmp = (point2d_t *)bu_calloc(n + 1, sizeof(point2d_t), "points_2d");

    ret += coplanar_2d_coord_sys(&origin_pnt, &u_axis, &v_axis, pts, n);
    ret += coplanar_3d_to_2d(&points_tmp, (const point_t *)&origin_pnt, (const vect_t *)&u_axis, (const vect_t *)&v_axis, pts, n);

    if (ret) return 1;

    ret = bn_polygon_triangulate(faces, num_faces, (const point2d_t *)points_tmp, n);

    bu_free(points_tmp, "free temp points\n");
    return ret;
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
