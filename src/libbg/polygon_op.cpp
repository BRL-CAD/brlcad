/*                 P O L Y G O N _ O P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file polygon_op.cpp
 *
 * Routines for operating on polygons.
 *
 */

#include "common.h"

#include "clipper.hpp"

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn/mat.h"
#include "bg/plane.h"
#include "bg/defines.h"
#include "bg/polygon.h"

#define FREE_BV_SCENE_OBJ(p, fp) { \
    BU_LIST_APPEND(fp, &((p)->l)); }

fastf_t
bg_find_polygon_area(struct bg_polygon *gpoly, fastf_t sf, matp_t model2view, fastf_t size)
{
    size_t j, k, n;
    ClipperLib::Path poly;
    fastf_t area = 0.0;

    if (NEAR_ZERO(sf, SMALL_FASTF))
        return 0.0;

    for (j = 0; j < gpoly->num_contours; ++j) {
        n = gpoly->contour[j].num_points;
        poly.resize(n);
        for (k = 0; k < n; k++) {
            point_t vpoint;

            /* Convert to view coordinates */
            MAT4X3PNT(vpoint, model2view, gpoly->contour[j].point[k]);

            poly[k].X = (ClipperLib::long64)(vpoint[X] * sf);
            poly[k].Y = (ClipperLib::long64)(vpoint[Y] * sf);
        }

        area += (fastf_t)ClipperLib::Area(poly);
    }

    sf = 1.0/(sf*sf) * size * size;

    return (area * sf);
}


typedef struct {
    size_t      pc_num_points;
    point2d_t   *pc_point;
} poly_contour_2d;

typedef struct {
    size_t              p_num_contours;
    int                 *p_hole;
    poly_contour_2d     *p_contour;
} polygon_2d;

int
bg_polygons_overlap(struct bg_polygon *polyA, struct bg_polygon *polyB, matp_t model2view, const struct bn_tol *tol, fastf_t iscale)
{
    size_t i, j;
    size_t beginA, endA, beginB, endB;
    fastf_t tol_dist;
    fastf_t tol_dist_sq;
    fastf_t scale;
    polygon_2d polyA_2d;
    polygon_2d polyB_2d;
    point2d_t pt_2d;
    point_t A;
    size_t winding = 0;
    int ret = 0;

    if (polyA->num_contours < 1 || polyA->contour[0].num_points < 1 ||
	polyB->num_contours < 1 || polyB->contour[0].num_points < 1)
	return 0;

    tol_dist = (tol->dist > 0.0) ? tol->dist : BN_TOL_DIST;
    tol_dist_sq = (tol->dist_sq > 0.0) ? tol->dist_sq : tol_dist * tol_dist;
    scale = (iscale > (fastf_t)UINT16_MAX) ? iscale : (fastf_t)UINT16_MAX;

    /* Project polyA and polyB onto the view plane */
    polyA_2d.p_num_contours = polyA->num_contours;
    polyA_2d.p_hole = (int *)bu_calloc(polyA->num_contours, sizeof(int), "p_hole");
    polyA_2d.p_contour = (poly_contour_2d *)bu_calloc(polyA->num_contours, sizeof(poly_contour_2d), "p_contour");

    for (i = 0; i < polyA->num_contours; ++i) {
	polyA_2d.p_hole[i] = polyA->hole[i];
	polyA_2d.p_contour[i].pc_num_points = polyA->contour[i].num_points;
	polyA_2d.p_contour[i].pc_point = (point2d_t *)bu_calloc(polyA->contour[i].num_points, sizeof(point2d_t), "pc_point");

	for (j = 0; j < polyA->contour[i].num_points; ++j) {
	    point_t vpoint;

	    MAT4X3PNT(vpoint, model2view, polyA->contour[i].point[j]);
	    VSCALE(vpoint, vpoint, scale);
	    V2MOVE(polyA_2d.p_contour[i].pc_point[j], vpoint);
	}
    }

    polyB_2d.p_num_contours = polyB->num_contours;
    polyB_2d.p_hole = (int *)bu_calloc(polyB->num_contours, sizeof(int), "p_hole");
    polyB_2d.p_contour = (poly_contour_2d *)bu_calloc(polyB->num_contours, sizeof(poly_contour_2d), "p_contour");

    for (i = 0; i < polyB->num_contours; ++i) {
	polyB_2d.p_hole[i] = polyB->hole[i];
	polyB_2d.p_contour[i].pc_num_points = polyB->contour[i].num_points;
	polyB_2d.p_contour[i].pc_point = (point2d_t *)bu_calloc(polyB->contour[i].num_points, sizeof(point2d_t), "pc_point");

	for (j = 0; j < polyB->contour[i].num_points; ++j) {
	    point_t vpoint;

	    MAT4X3PNT(vpoint, model2view, polyB->contour[i].point[j]);
	    VSCALE(vpoint, vpoint, scale);
	    V2MOVE(polyB_2d.p_contour[i].pc_point[j], vpoint);
	}
    }

    /*
     * Check every line segment of polyA against every line segment of polyB.
     * If there are any intersecting line segments, there exists an overlap.
     */
    for (i = 0; i < polyA_2d.p_num_contours; ++i) {
	for (beginA = 0; beginA < polyA_2d.p_contour[i].pc_num_points; ++beginA) {
	    vect2d_t dirA;

	    if (beginA == polyA_2d.p_contour[i].pc_num_points-1)
		endA = 0;
	    else
		endA = beginA + 1;

	    V2SUB2(dirA, polyA_2d.p_contour[i].pc_point[endA], polyA_2d.p_contour[i].pc_point[beginA]);

	    for (j = 0; j < polyB_2d.p_num_contours; ++j) {
		for (beginB = 0; beginB < polyB_2d.p_contour[j].pc_num_points; ++beginB) {
		    vect2d_t distvec;
		    vect2d_t dirB;

		    if (beginB == polyB_2d.p_contour[j].pc_num_points-1)
			endB = 0;
		    else
			endB = beginB + 1;

		    V2SUB2(dirB, polyB_2d.p_contour[j].pc_point[endB], polyB_2d.p_contour[j].pc_point[beginB]);

		    if (bg_isect_lseg2_lseg2(distvec,
					     polyA_2d.p_contour[i].pc_point[beginA], dirA,
					     polyB_2d.p_contour[j].pc_point[beginB], dirB,
					     tol) == 1) {
			/* Check to see if intersection is near an end point */
			if (!NEAR_EQUAL(distvec[0], 0.0, tol_dist_sq) &&
			    !NEAR_EQUAL(distvec[0], 1.0, tol_dist_sq) &&
			    !NEAR_EQUAL(distvec[1], 0.0, tol_dist_sq) &&
			    !NEAR_EQUAL(distvec[1], 1.0, tol_dist_sq)) {
			    ret = 1;
			    goto end;
			}
		    }
		}
	    }
	}
    }

    for (i = 0; i < polyB_2d.p_num_contours; ++i) {
	size_t npts_on_contour = 0;
	size_t npts_outside = 0;

	/* Skip holes */
	if (polyB_2d.p_hole[i])
	    continue;

	/* Check if all points in the current polygon B contour are inside A */
	for (beginB = 0; beginB < polyB_2d.p_contour[i].pc_num_points; ++beginB) {
	    winding = 0;
	    V2MOVE(pt_2d, polyB_2d.p_contour[i].pc_point[beginB]);
	    V2MOVE(A, pt_2d);
	    A[2] = 0.0;

	    for (j = 0; j < polyA_2d.p_num_contours; ++j) {
		point_t B, C;
		vect_t BmA, CmA;
		vect_t vcross;

		for (beginA = 0; beginA < polyA_2d.p_contour[j].pc_num_points; ++beginA) {
		    fastf_t dot;

		    if (beginA == polyA_2d.p_contour[j].pc_num_points-1)
			endA = 0;
		    else
			endA = beginA + 1;

		    if (V2NEAR_EQUAL(polyA_2d.p_contour[j].pc_point[beginA], pt_2d, tol_dist_sq) ||
			V2NEAR_EQUAL(polyA_2d.p_contour[j].pc_point[endA], pt_2d, tol_dist_sq)) {
			/* pt_2d is the same as one of the end points, so count it */
			++npts_on_contour;

			if (polyA_2d.p_hole[j])
			    winding = 0;
			else
			    winding = 1;

			goto endA;
		    }

		    if ((polyA_2d.p_contour[j].pc_point[beginA][1] <= pt_2d[1] &&
			 polyA_2d.p_contour[j].pc_point[endA][1] > pt_2d[1])) {
			V2MOVE(B, polyA_2d.p_contour[j].pc_point[endA]);
			B[2] = 0.0;
			V2MOVE(C, polyA_2d.p_contour[j].pc_point[beginA]);
			C[2] = 0.0;
		    } else if ((polyA_2d.p_contour[j].pc_point[endA][1] <= pt_2d[1] &&
				polyA_2d.p_contour[j].pc_point[beginA][1] > pt_2d[1])) {
			V2MOVE(B, polyA_2d.p_contour[j].pc_point[beginA]);
			B[2] = 0.0;
			V2MOVE(C, polyA_2d.p_contour[j].pc_point[endA]);
			C[2] = 0.0;
		    } else {
			/* check if the point is on a horizontal edge */
			if (NEAR_EQUAL(polyA_2d.p_contour[j].pc_point[beginA][1],
				       polyA_2d.p_contour[j].pc_point[endA][1], tol_dist_sq) &&
			    NEAR_EQUAL(polyA_2d.p_contour[j].pc_point[beginA][1], pt_2d[1], tol_dist_sq) &&
			    ((polyA_2d.p_contour[j].pc_point[beginA][0] <= pt_2d[0] &&
			      polyA_2d.p_contour[j].pc_point[endA][0] >= pt_2d[0]) ||
			     (polyA_2d.p_contour[j].pc_point[endA][0] <= pt_2d[0] &&
			      polyA_2d.p_contour[j].pc_point[beginA][0] >= pt_2d[0]))) {
			    ++npts_on_contour;

			    if (polyA_2d.p_hole[j])
				winding = 0;
			    else
				winding = 1;

			    goto endA;
			}

			continue;
		    }

		    VSUB2(BmA, B, A);
		    VSUB2(CmA, C, A);
		    VUNITIZE(BmA);
		    VUNITIZE(CmA);
		    dot = VDOT(BmA, CmA);

		    if (NEAR_EQUAL(dot, -1.0, tol_dist_sq) || NEAR_EQUAL(dot, 1.0, tol_dist_sq)) {
			++npts_on_contour;

			if (polyA_2d.p_hole[j])
			    winding = 0;
			else
			    winding = 0;

			goto endA;
		    }

		    VCROSS(vcross, BmA, CmA);

		    if (vcross[2] > 0)
			++winding;
		}
	    }

	endA:
	    /* found a point on a polygon B contour that's outside of polygon A */
	    if (!(winding%2)) {
		++npts_outside;
		if (npts_outside != npts_on_contour) {
		    break;
		}
	    }
	}

	/* found a B polygon contour that's completely inside polygon A */
	if (winding%2 || (npts_outside != 0 &&
			  npts_outside != polyB_2d.p_contour[i].pc_num_points &&
			  npts_outside == npts_on_contour)) {
	    ret = 1;
	    goto end;
	}
    }

    for (i = 0; i < polyA_2d.p_num_contours; ++i) {
	size_t npts_on_contour = 0;
	size_t npts_outside = 0;

	/* Skip holes */
	if (polyA_2d.p_hole[i])
	    continue;

	/* Check if all points in the current polygon A contour are inside B */
	for (beginA = 0; beginA < polyA_2d.p_contour[i].pc_num_points; ++beginA) {
	    winding = 0;
	    V2MOVE(pt_2d, polyA_2d.p_contour[i].pc_point[beginA]);
	    V2MOVE(A, pt_2d);
	    A[2] = 0.0;

	    for (j = 0; j < polyB_2d.p_num_contours; ++j) {
		point_t B, C;
		vect_t BmA, CmA;
		vect_t vcross;

		for (beginB = 0; beginB < polyB_2d.p_contour[j].pc_num_points; ++beginB) {
		    fastf_t dot;

		    if (beginB == polyB_2d.p_contour[j].pc_num_points-1)
			endB = 0;
		    else
			endB = beginB + 1;

		    if (V2NEAR_EQUAL(polyB_2d.p_contour[j].pc_point[beginB], pt_2d, tol_dist_sq) ||
			V2NEAR_EQUAL(polyB_2d.p_contour[j].pc_point[endB], pt_2d, tol_dist_sq)) {
			/* pt_2d is the same as one of the end points, so count it */

			if (polyB_2d.p_hole[j])
			    winding = 0;
			else
			    winding = 1;

			++npts_on_contour;
			goto endB;
		    }

		    if ((polyB_2d.p_contour[j].pc_point[beginB][1] <= pt_2d[1] &&
			 polyB_2d.p_contour[j].pc_point[endB][1] > pt_2d[1])) {
			V2MOVE(B, polyB_2d.p_contour[j].pc_point[endB]);
			B[2] = 0.0;
			V2MOVE(C, polyB_2d.p_contour[j].pc_point[beginB]);
			C[2] = 0.0;
		    } else if ((polyB_2d.p_contour[j].pc_point[endB][1] <= pt_2d[1] &&
				polyB_2d.p_contour[j].pc_point[beginB][1] > pt_2d[1])) {
			V2MOVE(B, polyB_2d.p_contour[j].pc_point[beginB]);
			B[2] = 0.0;
			V2MOVE(C, polyB_2d.p_contour[j].pc_point[endB]);
			C[2] = 0.0;
		    } else {
			/* check if the point is on a horizontal edge */
			if (NEAR_EQUAL(polyB_2d.p_contour[j].pc_point[beginB][1],
				       polyB_2d.p_contour[j].pc_point[endB][1], tol_dist_sq) &&
			    NEAR_EQUAL(polyB_2d.p_contour[j].pc_point[beginB][1], pt_2d[1], tol_dist_sq) &&
			    ((polyB_2d.p_contour[j].pc_point[beginB][0] <= pt_2d[0] &&
			      polyB_2d.p_contour[j].pc_point[endB][0] >= pt_2d[0]) ||
			     (polyB_2d.p_contour[j].pc_point[endB][0] <= pt_2d[0] &&
			      polyB_2d.p_contour[j].pc_point[beginB][0] >= pt_2d[0]))) {
			    if (polyB_2d.p_hole[j])
				winding = 0;
			    else
				winding = 1;

			    ++npts_on_contour;

			    goto endB;
			}

			continue;
		    }

		    VSUB2(BmA, B, A);
		    VSUB2(CmA, C, A);
		    VUNITIZE(BmA);
		    VUNITIZE(CmA);
		    dot = VDOT(BmA, CmA);

		    if (NEAR_EQUAL(dot, -1.0, tol_dist_sq) || NEAR_EQUAL(dot, 1.0, tol_dist_sq)) {
			if (polyB_2d.p_hole[j])
			    winding = 0;
			else
			    winding = 1;

			++npts_on_contour;
			goto endB;
		    }

		    VCROSS(vcross, BmA, CmA);

		    if (vcross[2] > 0)
			++winding;
		}
	    }

	endB:
	    /* found a point on a polygon A contour that's outside of polygon B */
	    if (!(winding%2)) {
		++npts_outside;
		if (npts_outside != npts_on_contour) {
		    break;
		}
	    }
	}

	/* found an A polygon contour that's completely inside polygon B */
	if (winding%2 || (npts_outside != 0 &&
			  npts_outside != polyA_2d.p_contour[i].pc_num_points &&
			  npts_outside == npts_on_contour)) {
	    ret = 1;
	    break;
	} else
	    ret = 0;
    }

end:

    for (i = 0; i < polyA->num_contours; ++i)
	bu_free((void *)polyA_2d.p_contour[i].pc_point, "pc_point");
    for (i = 0; i < polyB->num_contours; ++i)
	bu_free((void *)polyB_2d.p_contour[i].pc_point, "pc_point");

    bu_free((void *)polyA_2d.p_hole, "p_hole");
    bu_free((void *)polyA_2d.p_contour, "p_contour");
    bu_free((void *)polyB_2d.p_hole, "p_hole");
    bu_free((void *)polyB_2d.p_contour, "p_contour");

    return ret;
}


typedef struct {
    ClipperLib::long64 x;
    ClipperLib::long64 y;
} clipper_vertex;


static fastf_t
load_polygon(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, struct bg_polygon *gpoly, fastf_t sf, matp_t mat)
{
    size_t j, k, n;
    ClipperLib::Path curr_poly;
    fastf_t vZ = 1.0;
    mat_t idmat = MAT_INIT_IDN;

    for (j = 0; j < gpoly->num_contours; ++j) {
	n = gpoly->contour[j].num_points;
	curr_poly.resize(n);
	for (k = 0; k < n; k++) {
	    point_t vpoint;

	    /* Convert to view coordinates */
	    if (mat) {
		MAT4X3PNT(vpoint, mat, gpoly->contour[j].point[k]);
	    } else {
		MAT4X3PNT(vpoint, idmat, gpoly->contour[j].point[k]);
	    }
	    vZ = vpoint[Z];

	    curr_poly[k].X = (ClipperLib::long64)(vpoint[X] * sf);
	    curr_poly[k].Y = (ClipperLib::long64)(vpoint[Y] * sf);
	}

	try {
	    clipper.AddPath(curr_poly, ptype, !gpoly->contour[j].open);
	} catch (...) {
	    bu_log("Exception thrown by clipper\n");
	}
    }

    return vZ;
}

static fastf_t
load_polygons(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, struct bg_polygons *subj, fastf_t sf, matp_t mat)
{
    size_t i;
    fastf_t vZ = 1.0;

    for (i = 0; i < subj->num_polygons; ++i)
	vZ = load_polygon(clipper, ptype, &subj->polygon[i], sf, mat);

    return vZ;
}

/*
 * Process/extract the clipper_polys into a struct bg_polygon.
 */
static struct bg_polygon *
extract(ClipperLib::PolyTree &clipper_polytree, fastf_t sf, matp_t mat, fastf_t vZ)
{
    size_t j, n;
    size_t num_contours = clipper_polytree.Total();
    struct bg_polygon *outp;
    mat_t idmat = MAT_INIT_IDN;

    BU_ALLOC(outp, struct bg_polygon);
    outp->num_contours = num_contours;

    if (num_contours < 1)
	return outp;

    outp->hole = (int *)bu_calloc(num_contours, sizeof(int), "hole");
    outp->contour = (struct bg_poly_contour *)bu_calloc(num_contours, sizeof(struct bg_poly_contour), "contour");

    ClipperLib::PolyNode *polynode = clipper_polytree.GetFirst();
    n = 0;
    while (polynode) {
	point_t vpoint;
	ClipperLib::Path &path = polynode->Contour;

	outp->hole[n] = polynode->IsHole();
	outp->contour[n].num_points = path.size();
	outp->contour[n].open = polynode->IsOpen();
	outp->contour[n].point = (point_t *)bu_calloc(outp->contour[n].num_points, sizeof(point_t), "point");

	for (j = 0; j < outp->contour[n].num_points; ++j) {
	    VSET(vpoint, (fastf_t)(path[j].X) * sf, (fastf_t)(path[j].Y) * sf, vZ);

	    /* Convert to model coordinates */
	    if (mat) {
		MAT4X3PNT(outp->contour[n].point[j], mat, vpoint);
	    } else {
		MAT4X3PNT(outp->contour[n].point[j], idmat, vpoint);
	    }
	}

	++n;
	polynode = polynode->GetNext();
    }

    // In case we had to skip any contours, finalize the num_contours value.
    outp->num_contours = n;

    return outp;
}


struct bg_polygon *
bg_clip_polygon(bg_clip_t op, struct bg_polygon *subj, struct bg_polygon *clip, fastf_t sf, matp_t model2view, matp_t view2model)
{
    fastf_t inv_sf;
    fastf_t vZ;
    ClipperLib::Clipper clipper;
    ClipperLib::PolyTree result_clipper_polys;
    ClipperLib::ClipType ctOp;

    /* need to scale the points up/down and then convert to/from long64 */
    /* need a matrix to rotate into a plane */
    /* need the inverse of the matrix above to put things back after clipping */

    /* Load subject polygon into clipper */
    load_polygon(clipper, ClipperLib::ptSubject, subj, sf, model2view);

    /* Load clip polygon into clipper */
    vZ = load_polygon(clipper, ClipperLib::ptClip, clip, sf, model2view);

    /* Convert op from BRL-CAD to Clipper */
    switch (op) {
    case bg_Intersection:
	ctOp = ClipperLib::ctIntersection;
	break;
    case bg_Union:
	ctOp = ClipperLib::ctUnion;
	break;
    case bg_Difference:
	ctOp = ClipperLib::ctDifference;
	break;
    default:
	ctOp = ClipperLib::ctXor;
	break;
    }

    /* Clip'em */
    clipper.Execute(ctOp, result_clipper_polys, ClipperLib::pftEvenOdd, ClipperLib::pftEvenOdd);

    inv_sf = 1.0/sf;
    return extract(result_clipper_polys, inv_sf, view2model, vZ);
}


struct bg_polygon *
bg_clip_polygons(bg_clip_t op, struct bg_polygons *subj, struct bg_polygons *clip, fastf_t sf, matp_t model2view, matp_t view2model)
{
    fastf_t inv_sf;
    fastf_t vZ;
    ClipperLib::Clipper clipper;
    ClipperLib::PolyTree result_clipper_polys;
    ClipperLib::ClipType ctOp;

    /* need to scale the points up/down and then convert to/from long64 */
    /* need a matrix to rotate into a plane */
    /* need the inverse of the matrix above to put things back after clipping */

    /* Load subject polygons into clipper */
    load_polygons(clipper, ClipperLib::ptSubject, subj, sf, model2view);

    /* Load clip polygons into clipper */
    vZ = load_polygons(clipper, ClipperLib::ptClip, clip, sf, model2view);

    /* Convert op from BRL-CAD to Clipper */
    switch (op) {
    case bg_Intersection:
	ctOp = ClipperLib::ctIntersection;
	break;
    case bg_Union:
	ctOp = ClipperLib::ctUnion;
	break;
    case bg_Difference:
	ctOp = ClipperLib::ctDifference;
	break;
    default:
	ctOp = ClipperLib::ctXor;
	break;
    }

    /* Clip'em */
    clipper.Execute(ctOp, result_clipper_polys, ClipperLib::pftEvenOdd, ClipperLib::pftEvenOdd);

    inv_sf = 1.0/sf;
    return extract(result_clipper_polys, inv_sf, view2model, vZ);
}


int
bv_polygon_csg(struct bu_ptbl *objs, struct bv_scene_obj *p, bg_clip_t op, int merge)
{
    if (!objs || !p)
	return -1;

    struct bu_ptbl null_polys = BU_PTBL_INIT_ZERO;
    int pcnt = 0;
    struct bv_scene_obj *bp = p;
    struct bv_scene_obj *free_scene_obj = p->free_scene_obj;

    for (size_t i = 0; i < BU_PTBL_LEN(objs); i++) {
	struct bv_scene_obj *vp = (struct bv_scene_obj *)BU_PTBL_GET(objs, i);
	if (!(vp->s_type_flags & BV_POLYGONS))
	    continue;
	if (p == vp)
	    continue;
	struct bv_polygon *polyA = (struct bv_polygon *)vp->s_i_data;
	struct bv_polygon *polyB = (struct bv_polygon *)bp->s_i_data;

	// Make sure the polygons overlap before we operate, since clipper results are
	// always general polygons.  We don't want to perform a no-op clip and lose our
	// type info.
	const struct bn_tol poly_tol = BG_TOL_INIT;
	int ovlp = bg_polygons_overlap(&polyA->polygon, &polyB->polygon, polyA->v.gv_model2view, &poly_tol, polyA->v.gv_scale);
	if (!ovlp)
	    continue;

	// Perform the specified operation
	struct bg_polygon *cp = bg_clip_polygon(op, &polyA->polygon, &polyB->polygon, CLIPPER_MAX, polyA->v.gv_model2view, polyA->v.gv_view2model);
	bg_polygon_free(&polyA->polygon);
	polyA->polygon.num_contours = cp->num_contours;
	polyA->polygon.hole = cp->hole;
	polyA->polygon.contour = cp->contour;

	// clipper results are always general polygons
	polyA->type = BV_POLYGON_GENERAL;

	if (op != bg_Difference && merge && polyA->polygon.num_contours) {

	    // The seed polygon is handled elsewhere, but if we're
	    // consolidating other view polygons into polyA we need to log the
	    // consolidated polys for removal here.
	    if (bp != p) {
		bu_ptbl_ins_unique(&null_polys, (long *)bp);
	    }

	    // If we're merging results, subsequent operations will be done
	    // using the results of this operation, not the original polygon
	    bp = vp;
	}

	if (!polyA->polygon.num_contours) {
	    // operation eliminated polyA - stash for removal from view
	    bu_ptbl_ins_unique(&null_polys, (long *)vp);
	} else {
	    bv_update_polygon(vp, BV_POLYGON_UPDATE_DEFAULT);
	}

	BU_PUT(cp, struct bg_polygon);
	pcnt++;
    }

    // If we're eliminating any polygons from the view as a result of the operations, do it now
    for (size_t i = 0; i < BU_PTBL_LEN(&null_polys); i++) {
	struct bv_scene_obj *np = (struct bv_scene_obj *)BU_PTBL_GET(&null_polys, i);
	struct bv_polygon *ip = (struct bv_polygon *)np->s_i_data;
	bg_polygon_free(&ip->polygon);
	BU_PUT(ip, struct bv_polygon);
	bu_ptbl_rm(objs, (long *)np);
	FREE_BV_SCENE_OBJ(np, &free_scene_obj->l);
    }

    return pcnt;
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
