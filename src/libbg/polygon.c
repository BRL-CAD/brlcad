/*                     P O L Y G O N . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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

#include "common.h"

#include <bio.h>

#include "bu/malloc.h"
#include "bu/sort.h"
#include "bg/plane.h"
#include "bv/plot3.h"
#include "bn/tol.h"
#include "bg/polygon.h"

void
bg_polygon_free(struct bg_polygon *gpp)
{
    if (gpp->num_contours == 0)
	return;

    for (size_t j = 0; j < gpp->num_contours; ++j)
	if (gpp->contour[j].num_points > 0)
	    bu_free((void *)gpp->contour[j].point, "contour points");

    bu_free((void *)gpp->contour, "contour");
    bu_free((void *)gpp->hole, "hole");
    gpp->num_contours = 0;
}

void
bg_polygons_free(struct bg_polygons *gpp)
{
    if (gpp->num_polygons == 0)
	return;

    for (size_t i = 0; i < gpp->num_polygons; ++i) {
	bg_polygon_free(&gpp->polygon[i]);
    }

    bu_free((void *)gpp->polygon, "data polygons");
    gpp->polygon = (struct bg_polygon *)0;
    gpp->num_polygons = 0;
}


int
bg_3d_polygon_area(fastf_t *area, size_t npts, const point_t *pts)
{
    size_t i;
    vect_t v1, v2, tmp, tot = VINIT_ZERO;
    plane_t plane_eqn;
    struct bn_tol tol;

    if (!pts || !area || npts < 3)
	return 1;
    BN_TOL_INIT(&tol);
    tol.dist_sq = BN_TOL_DIST * BN_TOL_DIST;
    if (bg_make_plane_3pnts(plane_eqn, pts[0], pts[1], pts[2], &tol) == -1)
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
bg_3d_polygon_centroid(point_t *cent, size_t npts, const point_t *pts)
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
    z_1 = pts[0][2];
    a = x_0 *z_1 - x_1*z_0;
    signedArea += a;
    *cent[2] += (z_0 + z_1)*a;

    signedArea *= 0.5;
    *cent[2] /= (6.0*signedArea);
    return 0;
}


int
bg_3d_polygon_make_pnts_planes(size_t *npts, point_t **pts, size_t neqs, const plane_t *eqs)
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
		if (bg_make_pnt_3planes(pt, eqs[i], eqs[j], eqs[k]) < 0)
		    continue;
		/* discard pt if it is outside the polyhedron */
		for (l = 0; l < neqs; l++) {
		    if (l == i || l == j || l == k)
			continue;
		    if (DIST_PNT_PLANE(pt, eqs[l]) > BN_TOL_DIST) {
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
sort_ccw_3d(const void *x, const void *y, void *cmp)
{
    vect_t tmp;
    VCROSS(tmp, ((fastf_t *)x), ((fastf_t *)y));
    return VDOT(*((point_t *)cmp), tmp);
}


int
bg_3d_polygon_sort_ccw(size_t npts, point_t *pts, plane_t cmp)
{
    if (!pts || npts < 3)
	return 1;
    bu_sort(pts, npts, sizeof(point_t), sort_ccw_3d, &cmp);
    return 0;
}

int
bg_polygon_direction(size_t npts, const point2d_t *pts, const int *pt_indices)
{
    size_t i;
    double sum = 0;
    const int *pt_order = NULL;
    int *tmp_pt_order = NULL;
    /* If no array of indices into pts is supplied, construct a
     * temporary version based on the point order in the array */
    if (pt_indices) pt_order = pt_indices;
    if (!pt_order) {
	tmp_pt_order = (int *)bu_calloc(npts, sizeof(size_t), "temp ordering array");
	for (i = 0; i < npts; i++)
	    tmp_pt_order[i] = (int)i;
	pt_order = (const int *)tmp_pt_order;
    }

    /* Conduct the actual CCW test */
    for (i = 0; i < npts; i++) {
	if (i + 1 == npts) {
	    sum += (pts[pt_order[0]][0] - pts[pt_order[i]][0]) * (pts[pt_order[0]][1] + pts[pt_order[i]][1]);
	} else {
	    sum += (pts[pt_order[i+1]][0] - pts[pt_order[i]][0]) * (pts[pt_order[i+1]][1] + pts[pt_order[i]][1]);
	}
    }

    /* clean up and evaluate results */
    if (tmp_pt_order)
	bu_free(tmp_pt_order, "free tmp_pt_order");
    if (NEAR_ZERO(sum, SMALL_FASTF))
	return 0;
    return (sum > 0) ? BG_CW : BG_CCW;
}


void
bg_polygon_cpy(struct bg_polygon *dest, struct bg_polygon *src)
{
    if (!dest || !src)
	return;

    dest->num_contours = src->num_contours;
    dest->hole = (int *)bu_calloc(src->num_contours, sizeof(int), "hole");
    dest->contour = (struct bg_poly_contour *)bu_calloc(src->num_contours, sizeof(struct bg_poly_contour), "contour");
    for (size_t i = 0; i < src->num_contours; i++) {
	dest->hole[i] = src->hole[i];
    }
    for (size_t i = 0; i < src->num_contours; i++) {
	dest->contour[i].num_points = src->contour[i].num_points;
	dest->contour[i].open = src->contour[i].open;
	dest->contour[i].point = (point_t *)bu_calloc(src->contour[i].num_points, sizeof(point_t), "point");
	for (size_t j = 0; j < src->contour[i].num_points; j++) {
	    VMOVE(dest->contour[i].point[j], src->contour[i].point[j]);
	}
    }
}

void
bg_polygon_plot_2d(const char *filename, const point2d_t *pnts, int npnts, int r, int g, int b)
{
    point_t bnp;
    FILE* plot_file = fopen(filename, "wb");
    pl_color(plot_file, r, g, b);

    VSET(bnp, pnts[0][X], pnts[0][Y], 0);
    pdv_3move(plot_file, bnp);

    for (int i = 1; i < npnts; i++) {
	VSET(bnp, pnts[i][X], pnts[i][Y], 0);
	pdv_3cont(plot_file, bnp);
    }

    VSET(bnp, pnts[0][X], pnts[0][Y], 0);
    pdv_3cont(plot_file, bnp);

    fclose(plot_file);
}

void
bg_polygon_plot(const char *filename, const point_t *pnts, int npnts, int r, int g, int b)
{
    point_t bnp;
    FILE* plot_file = fopen(filename, "wb");
    pl_color(plot_file, r, g, b);

    VSET(bnp, pnts[0][X], pnts[0][Y], 0);
    pdv_3move(plot_file, bnp);

    for (int i = 1; i < npnts; i++) {
	VSET(bnp, pnts[i][X], pnts[i][Y], pnts[i][Z]);
	pdv_3cont(plot_file, bnp);
    }

    VSET(bnp, pnts[0][X], pnts[0][Y], pnts[0][Z]);
    pdv_3cont(plot_file, bnp);

    fclose(plot_file);
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
