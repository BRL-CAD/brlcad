/*                     P O L Y G O N . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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

#include "bn/mat.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bg/plane.h"
#ifndef PLOT_PREFIX_STR
#  define PLOT_PREFIX_STR bg_plot3_
#endif
#include "bv/plot3.h"
#include "bn/tol.h"
#include "bg/polygon.h"

void
bg_polygon_free(struct bg_polygon *gpp)
{
    if (!gpp || gpp->num_contours == 0)
	return;

    if (gpp->contour) {
	for (size_t j = 0; j < gpp->num_contours; ++j) {
	    if (gpp->contour[j].num_points > 0 && gpp->contour[j].point) {
		bu_free((void *)gpp->contour[j].point, "contour points");
		gpp->contour[j].point = NULL;
		gpp->contour[j].num_points = 0;
	    }
	}
	bu_free((void *)gpp->contour, "contour");
	gpp->contour = NULL;
    }

    if (gpp->hole) {
	bu_free((void *)gpp->hole, "hole");
	gpp->hole = NULL;
    }
    gpp->num_contours = 0;
}

void
bg_polygons_free(struct bg_polygons *gpp)
{
    if (!gpp || gpp->num_polygons == 0)
	return;

    if (gpp->polygon) {
	for (size_t i = 0; i < gpp->num_polygons; ++i) {
	    bg_polygon_free(&gpp->polygon[i]);
	}
	bu_free((void *)gpp->polygon, "data polygons");
	gpp->polygon = (struct bg_polygon *)0;
    }
    gpp->num_polygons = 0;
}

void
bg_polygon_view_bbox(point2d_t *bmin, point2d_t *bmax, struct bg_polygon *p, matp_t model2view)
{
    if (!bmin || !bmax || !p || !model2view)
	return;

    // Initialize
    V2SET(*bmin, INFINITY, INFINITY);
    V2SET(*bmax, -INFINITY, -INFINITY);

    if (!p->num_contours || !p->contour)
	return;

    // NOTE:  Holes don't define positive area, so their points are not
    // considered for the bbox dimensions even if they are outside the positive
    // contours.  ONLY considering positive contour points.
    for (size_t i = 0; i < p->num_contours; i++) {
	struct bg_poly_contour *c = &p->contour[i];
	if (!c->num_points || !c->point)
	    continue;
	for (size_t j = 0; j < c->num_points; j++) {
	    point_t vpoint;
	    MAT4X3PNT(vpoint, model2view, c->point[j]);
	    point2d_t v2d;
	    v2d[0] = vpoint[0];
	    v2d[1] = vpoint[1];
	    V2MINMAX(*bmin, *bmax, v2d);
	}
    }
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
    vect_t normal = VINIT_ZERO;
    point_t c_acc = VINIT_ZERO;
    fastf_t total_weight = 0.0;
    fastf_t mag_normal;

    if (!pts || !cent || npts < 3)
	return 1;

    VSETALL(*cent, 0.0);

    /* Compute normal vector using Newell's method */
    for (i = 0; i < npts; i++) {
	size_t next = (i + 1 == npts) ? 0 : i + 1;
	normal[0] += (pts[i][1] - pts[next][1]) * (pts[i][2] + pts[next][2]);
	normal[1] += (pts[i][2] - pts[next][2]) * (pts[i][0] + pts[next][0]);
	normal[2] += (pts[i][0] - pts[next][0]) * (pts[i][1] + pts[next][1]);
    }

    mag_normal = MAGNITUDE(normal);
    if (mag_normal < VDIVIDE_TOL) {
	/* Degenerate polygon; fall back to vertex average */
	for (i = 0; i < npts; i++) {
	    VADD2(*cent, *cent, pts[i]);
	}
	VSCALE(*cent, *cent, 1.0 / (fastf_t)npts);
	return 0;
    }

    VSCALE(normal, normal, 1.0 / mag_normal);

    /* Triangulate fan from pts[0] and compute weighted area centroid */
    for (i = 1; i < npts - 1; i++) {
	vect_t edge1, edge2, cross;
	fastf_t area;
	point_t tri_cent;

	VSUB2(edge1, pts[i], pts[0]);
	VSUB2(edge2, pts[i + 1], pts[0]);
	VCROSS(cross, edge1, edge2);
	area = 0.5 * VDOT(cross, normal);

	/* Triangle centroid */
	VADD2(tri_cent, pts[0], pts[i]);
	VADD2(tri_cent, tri_cent, pts[i + 1]);
	VSCALE(tri_cent, tri_cent, 1.0 / 3.0);

	/* Accumulate */
	c_acc[0] += area * tri_cent[0];
	c_acc[1] += area * tri_cent[1];
	c_acc[2] += area * tri_cent[2];
	total_weight += area;
    }

    if (fabs(total_weight) < VDIVIDE_TOL) {
	/* Degenerate fan or self-cancelling areas; fall back to vertex average */
	for (i = 0; i < npts; i++) {
	    VADD2(*cent, *cent, pts[i]);
	}
	VSCALE(*cent, *cent, 1.0 / (fastf_t)npts);
	return 0;
    }

    VSCALE(*cent, c_acc, 1.0 / total_weight);
    return 0;
}


static int
append_plane_point(size_t *count, point_t *points, size_t capacity,
	const point_t point)
{
    size_t i;

    for (i = 0; i < *count; i++) {
	if (VNEAR_EQUAL(points[i], point, BN_TOL_DIST))
	    return 0;
    }
    if (*count >= capacity)
	return 1;

    VMOVE(points[*count], point);
    (*count)++;
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
		    size_t capacity = neqs - 1;
		    if (append_plane_point(&npts[i], pts[i], capacity, pt) ||
			    append_plane_point(&npts[j], pts[j], capacity, pt) ||
			    append_plane_point(&npts[k], pts[k], capacity, pt))
			return 1;
		}
	    }
	}
    }
    return 0;
}


struct sort_ccw_data {
    vect_t x_axis;
    vect_t y_axis;
};


static int
sort_ccw_3d(const void *left, const void *right, void *context)
{
    const struct sort_ccw_data *data = (const struct sort_ccw_data *)context;
    const fastf_t *left_point = (const fastf_t *)left;
    const fastf_t *right_point = (const fastf_t *)right;
    double left_angle = atan2(VDOT(left_point, data->y_axis),
	    VDOT(left_point, data->x_axis));
    double right_angle = atan2(VDOT(right_point, data->y_axis),
	    VDOT(right_point, data->x_axis));

    if (left_angle < right_angle)
	return -1;
    if (left_angle > right_angle)
	return 1;

    double left_radius = MAGSQ(left_point);
    double right_radius = MAGSQ(right_point);
    if (left_radius < right_radius)
	return -1;
    if (left_radius > right_radius)
	return 1;
    return 0;
}


int
bg_3d_polygon_sort_ccw(size_t npts, point_t *pts, plane_t cmp)
{
    size_t i;
    point_t centroid;
    vect_t normal;
    struct sort_ccw_data data;

    if (!pts || npts < 3)
	return 1;
    if (MAGNITUDE(cmp) < VDIVIDE_TOL)
	return 1;

    VMOVE(normal, cmp);
    VUNITIZE(normal);
    bn_vec_ortho(data.x_axis, normal);
    VCROSS(data.y_axis, normal, data.x_axis);

    /* Angular ordering is about the polygon center, not the model origin.
     * Translate temporarily so the comparator only needs the plane axes. */
    VSETALL(centroid, 0.0);
    for (i = 0; i < npts; i++)
	VADD2(centroid, centroid, pts[i]);
    VSCALE(centroid, centroid, 1.0 / (double)npts);

    for (i = 0; i < npts; i++)
	VSUB2(pts[i], pts[i], centroid);

    bu_sort(pts, npts, sizeof(point_t), sort_ccw_3d, &data);

    for (i = 0; i < npts; i++)
	VADD2(pts[i], pts[i], centroid);

    return 0;
}

int
bg_polygon_direction(size_t npts, const point2d_t *pts, const int *pt_indices)
{
    size_t i;
    double sum = 0;
    const int *pt_order = NULL;
    int *tmp_pt_order = NULL;

    if (!pts || npts < 3)
	return 0;

    /* If no array of indices into pts is supplied, construct a
     * temporary version based on the point order in the array */
    if (pt_indices) pt_order = pt_indices;
    if (!pt_order) {
	tmp_pt_order = (int *)bu_calloc(npts, sizeof(int), "temp ordering array");
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
    FILE* plot_file;

    if (!filename || !pnts || npnts <= 0)
	return;

    plot_file = fopen(filename, "wb");
    if (!plot_file)
	return;

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
    FILE* plot_file;

    if (!filename || !pnts || npnts <= 0)
	return;

    plot_file = fopen(filename, "wb");
    if (!plot_file)
	return;

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
