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
int bn_pt_in_polygon(size_t nvert, const point2d_t *pnts, const point2d_t *test)
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
