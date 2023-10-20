/*                 P O L Y G O N _ O P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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

#include <unordered_map>
#include <set>

#include "RTree.h"
#include "clipper.hpp"

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn/mat.h"
#include "bg/plane.h"
#include "bg/defines.h"
#include "bg/polygon.h"
#include "bv/util.h"

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
    size_t beginA, endA, beginB, endB;
    fastf_t scale;
    polygon_2d polyA_2d;
    polygon_2d polyB_2d;
    int ret = 0;

    if (polyA->num_contours < 1 || polyA->contour[0].num_points < 1 ||
	polyB->num_contours < 1 || polyB->contour[0].num_points < 1)
	return 0;

    scale = (iscale > (fastf_t)UINT16_MAX) ? iscale : (fastf_t)UINT16_MAX;

    /* Project polyA and polyB onto the view plane */
    polyA_2d.p_num_contours = polyA->num_contours;
    polyA_2d.p_hole = (int *)bu_calloc(polyA->num_contours, sizeof(int), "p_hole");
    polyA_2d.p_contour = (poly_contour_2d *)bu_calloc(polyA->num_contours, sizeof(poly_contour_2d), "p_contour");

    for (size_t i = 0; i < polyA->num_contours; ++i) {
	polyA_2d.p_hole[i] = polyA->hole[i];
	polyA_2d.p_contour[i].pc_num_points = polyA->contour[i].num_points;
	polyA_2d.p_contour[i].pc_point = (point2d_t *)bu_calloc(polyA->contour[i].num_points, sizeof(point2d_t), "pc_point");

	for (size_t j = 0; j < polyA->contour[i].num_points; ++j) {
	    point_t vpoint;

	    MAT4X3PNT(vpoint, model2view, polyA->contour[i].point[j]);
	    VSCALE(vpoint, vpoint, scale);
	    V2MOVE(polyA_2d.p_contour[i].pc_point[j], vpoint);
	}
    }

    polyB_2d.p_num_contours = polyB->num_contours;
    polyB_2d.p_hole = (int *)bu_calloc(polyB->num_contours, sizeof(int), "p_hole");
    polyB_2d.p_contour = (poly_contour_2d *)bu_calloc(polyB->num_contours, sizeof(poly_contour_2d), "p_contour");

    for (size_t i = 0; i < polyB->num_contours; ++i) {
	polyB_2d.p_hole[i] = polyB->hole[i];
	polyB_2d.p_contour[i].pc_num_points = polyB->contour[i].num_points;
	polyB_2d.p_contour[i].pc_point = (point2d_t *)bu_calloc(polyB->contour[i].num_points, sizeof(point2d_t), "pc_point");

	for (size_t j = 0; j < polyB->contour[i].num_points; ++j) {
	    point_t vpoint;

	    MAT4X3PNT(vpoint, model2view, polyB->contour[i].point[j]);
	    VSCALE(vpoint, vpoint, scale);
	    V2MOVE(polyB_2d.p_contour[i].pc_point[j], vpoint);
	}
    }

    // Next, assemble RTrees of the polygon lines so we can quickly identify
    // which pairs we need to check for intersection.  In order to provide a
    // single index for the RTree to identify each segment, we map a count of
    // segments to a contour/point number pair
    RTree<size_t, double, 2> rtree_2d_A;
    std::unordered_map<size_t, std::pair<size_t, size_t>> s_indA;
    int s_cnt_A = 0;
    for (size_t i = 0; i < polyA_2d.p_num_contours; ++i) {
	for (beginA = 0; beginA < polyA_2d.p_contour[i].pc_num_points; ++beginA) {
	    point2d_t p1, p2;
	    endA = (beginA == polyA_2d.p_contour[i].pc_num_points-1) ? 0 : beginA + 1;
	    V2MOVE(p1, polyA_2d.p_contour[i].pc_point[beginA]);
	    V2MOVE(p2, polyA_2d.p_contour[i].pc_point[endA]);
	    s_indA[s_cnt_A] = std::make_pair(i, beginA);
	    double tMin[2], tMax[2];
	    tMin[0] = (p1[X] < p2[X]) ? p1[X] : p2[X];
	    tMin[1] = (p1[Y] < p2[Y]) ? p1[Y] : p2[Y];
	    tMax[0] = (p1[X] > p2[X]) ? p1[X] : p2[X];
	    tMax[1] = (p1[Y] > p2[Y]) ? p1[Y] : p2[Y];
	    rtree_2d_A.Insert(tMin, tMax, s_cnt_A);
	    s_cnt_A++;
	}
    }
    RTree<size_t, double, 2> rtree_2d_B;
    std::unordered_map<size_t, std::pair<size_t, size_t>> s_indB;
    int s_cnt_B = 0;
    for (size_t i = 0; i < polyB_2d.p_num_contours; ++i) {
	for (beginB = 0; beginB < polyB_2d.p_contour[i].pc_num_points; ++beginB) {
	    point2d_t p1, p2;
	    endB = (beginB == polyB_2d.p_contour[i].pc_num_points-1) ? 0 : beginB + 1;
	    V2MOVE(p1, polyB_2d.p_contour[i].pc_point[beginB]);
	    V2MOVE(p2, polyB_2d.p_contour[i].pc_point[endB]);
	    s_indB[s_cnt_B] = std::make_pair(i, beginB);
	    double tMin[2], tMax[2];
	    tMin[0] = (p1[X] < p2[X]) ? p1[X] : p2[X];
	    tMin[1] = (p1[Y] < p2[Y]) ? p1[Y] : p2[Y];
	    tMax[0] = (p1[X] > p2[X]) ? p1[X] : p2[X];
	    tMax[1] = (p1[Y] > p2[Y]) ? p1[Y] : p2[Y];
	    rtree_2d_B.Insert(tMin, tMax, s_cnt_B);
	    s_cnt_B++;
	}
    }

    std::set<std::pair<size_t, size_t>> test_segs;
    size_t ovlp_cnt = rtree_2d_A.Overlaps(rtree_2d_B, &test_segs);
    //bu_log("ovlp_cnt: %zd\n", ovlp_cnt);
    //rtree_2d_A.plot2d("TreeA.plot3");
    //rtree_2d_B.plot2d("TreeB.plot3");

    /*
     * Check every line segment pairing from polyA & polyB where the bounding
     * boxes overlapped.  If there are any intersecting line segments, there
     * exists an overlap.
     */
    if (ovlp_cnt) {
	std::set<std::pair<size_t, size_t>>::iterator ts_it;
	for (ts_it = test_segs.begin(); ts_it != test_segs.end(); ts_it++) {
	    vect2d_t dirA, dirB;
	    size_t iA = s_indA[ts_it->first].first;
	    beginA = s_indA[ts_it->first].second;
	    endA = (beginA == polyA_2d.p_contour[iA].pc_num_points-1) ? 0 : beginA + 1;
	    V2SUB2(dirA, polyA_2d.p_contour[iA].pc_point[endA], polyA_2d.p_contour[iA].pc_point[beginA]);

	    size_t iB = s_indB[ts_it->second].first;
	    beginB = s_indB[ts_it->second].second;
	    endB = (beginB == polyB_2d.p_contour[iB].pc_num_points-1) ? 0 : beginB + 1;
	    V2SUB2(dirB, polyB_2d.p_contour[iB].pc_point[endB], polyB_2d.p_contour[iB].pc_point[beginB]);

	    vect2d_t distvec;
	    if (bg_isect_lseg2_lseg2(distvec,
			polyA_2d.p_contour[iA].pc_point[beginA], dirA,
			polyB_2d.p_contour[iB].pc_point[beginB], dirB,
			tol) == 1) {
		ret = 1;
		goto end;
	    }
	}
    }
  
    /* If there aren't any overlapping segments, then it boils down to whether
     * one of the polygons is fully inside the other but NOT fully inside a
     * hole.  Since we have ruled out segment intersections, we can simply
     * check one point on each contour against the other polygon's contours.
     * If it's inside a hole, it's not an overlap.  If it's not inside a hole
     * but IS inside a contour, it's an overlap.  This doesn't cover multiply
     * nested contours, but that's not currently a use case of interest - in the
     * event that use case does come up, we'll have to sort out in the original
     * polygon which positive contours are fully inside holes and establish a
     * hierarchy. */ 
    for (size_t i = 0; i < polyA_2d.p_num_contours; ++i) {
	bool in_hole = false;
	// No points, no work to do
	if (!polyA_2d.p_contour[i].pc_num_points)
	    continue;
	const point2d_t *tp = &polyA_2d.p_contour[i].pc_point[0];
	for (size_t j = 0; j < polyB_2d.p_num_contours; ++j) {
	    if (!polyB_2d.p_contour[j].pc_num_points || !polyB_2d.p_hole[j])
		continue;
	    int is_in = bg_pnt_in_polygon(polyB_2d.p_contour[j].pc_num_points, polyB_2d.p_contour[j].pc_point, tp);
	    if (is_in) {
		in_hole = true;
		// If we assume non-nested contour, we now know the answer for this contour
		break;
	    }
	}
	if (in_hole)
	    continue;

	// Not inside a hole, see if we're inside a positive contour
	for (size_t j = 0; j < polyB_2d.p_num_contours; ++j) {
	    if (!polyB_2d.p_contour[j].pc_num_points || polyB_2d.p_hole[j])
		continue;
	    int is_in = bg_pnt_in_polygon(polyB_2d.p_contour[j].pc_num_points, polyB_2d.p_contour[j].pc_point, tp);
	    if (is_in) {
		// If we assume non-nested contour, we now know the answer
		ret = 1;
		goto end;
	    }
	}
    }

    // Check B points against a contours
    for (size_t i = 0; i < polyB_2d.p_num_contours; ++i) {
	bool in_hole = false;
	// No points, no work to do
	if (!polyB_2d.p_contour[i].pc_num_points)
	    continue;
	const point2d_t *tp = &polyB_2d.p_contour[i].pc_point[0];
	for (size_t j = 0; j < polyA_2d.p_num_contours; ++j) {
	    if (!polyA_2d.p_contour[j].pc_num_points || !polyA_2d.p_hole[j])
		continue;
	    int is_in = bg_pnt_in_polygon(polyA_2d.p_contour[j].pc_num_points, polyA_2d.p_contour[j].pc_point, tp);
	    if (is_in) {
		in_hole = true;
		// If we assume non-nested contour, we now know the answer for this contour
		break;
	    }
	}
	if (in_hole)
	    continue;

	// Not inside a hole, see if we're inside a positive contour
	for (size_t j = 0; j < polyA_2d.p_num_contours; ++j) {
	    if (!polyA_2d.p_contour[j].pc_num_points || polyA_2d.p_hole[j])
		continue;
	    int is_in = bg_pnt_in_polygon(polyA_2d.p_contour[j].pc_num_points, polyA_2d.p_contour[j].pc_point, tp);
	    if (is_in) {
		// If we assume non-nested contour, we now know the answer
		ret = 1;
		goto end;
	    }
	}
    }

end:
    for (size_t i = 0; i < polyA->num_contours; ++i)
	bu_free((void *)polyA_2d.p_contour[i].pc_point, "pc_point");
    for (size_t i = 0; i < polyB->num_contours; ++i)
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
bv_polygon_csg(struct bv_scene_obj *target, struct bv_scene_obj *stencil, bg_clip_t op)
{
    // Need data
    if (!target || !stencil)
	return 0;

    // Need polygons
    if (!(target->s_type_flags & BV_POLYGONS) || !(stencil->s_type_flags & BV_POLYGONS))
	return 0;

    // None op == no change
    if (op == bg_None)
	return 0;

    struct bv_polygon *polyA = (struct bv_polygon *)target->s_i_data;
    struct bv_polygon *polyB = (struct bv_polygon *)stencil->s_i_data;
    if (!polyA || !polyB)
	return 0;

    // If the stencil is empty, it's all moot
    if (!polyB->polygon.num_contours)
	return 0;

    // Make sure the polygons overlap before we operate, since clipper results are
    // always general polygons.  We don't want to perform a no-op clip and lose our
    // type info.  There is however one exception to this - if our target is empty
    // and our op is a union, we still want to proceed even without an overlap.
    if (polyA->polygon.num_contours || op != bg_Union) {
	const struct bn_tol poly_tol = BN_TOL_INIT_TOL;
	int ovlp = bg_polygons_overlap(&polyA->polygon, &polyB->polygon, polyA->v.gv_model2view, &poly_tol, polyA->v.gv_scale);
	if (!ovlp)
	    return 0;
    } else {
	// In the case of a union into an empty polygon, what we do is copy the
	// stencil intact into target and preserve its type - no need to use
	// bg_clip_polygon and lose the type info
	bg_polygon_free(&polyA->polygon);
	bg_polygon_cpy(&polyA->polygon, &polyB->polygon);

	// We want to leave the color and fill settings in dest, but we should
	// sync some of the information so the target polygon shape can be
	// updated correctly.  In particular, for a non-generic polygon,
	// prev_point is important to updating.
	polyA->type = polyB->type;
	polyA->vZ = polyB->vZ;
	polyA->curr_contour_i = polyB->curr_contour_i;
	polyA->curr_point_i = polyB->curr_point_i;
	VMOVE(polyA->prev_point, polyB->prev_point);
	bv_sync(&polyA->v, &polyB->v);
	bv_update_polygon(target, target->s_v, BV_POLYGON_UPDATE_DEFAULT);
	return 1;
    }

    // Perform the specified operation and get the new polygon
    struct bg_polygon *cp = bg_clip_polygon(op, &polyA->polygon, &polyB->polygon, CLIPPER_MAX, polyA->v.gv_model2view, polyA->v.gv_view2model);

    // Replace the original target polygon with the result
    bg_polygon_free(&polyA->polygon);
    polyA->polygon.num_contours = cp->num_contours;
    polyA->polygon.hole = cp->hole;
    polyA->polygon.contour = cp->contour;

    // We stole the data from cp and put it in polyA - no longer need the
    // original cp container
    BU_PUT(cp, struct bg_polygon);

    // clipper results are always general polygons
    polyA->type = BV_POLYGON_GENERAL;

    // Make sure everything's current
    bv_update_polygon(target, target->s_v, BV_POLYGON_UPDATE_DEFAULT);

    return 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

