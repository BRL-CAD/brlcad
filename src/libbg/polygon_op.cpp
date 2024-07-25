/*                 P O L Y G O N _ O P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2024 United States Government as represented by
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
extern "C" {
#include "bg/polygon.h"
}

fastf_t
bg_find_polygon_area(struct bg_polygon *gpoly, fastf_t sf, plane_t *vp, fastf_t size)
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
	    fastf_t fx, fy;
	    bg_plane_closest_pt(&fx, &fy, vp, &gpoly->contour[j].point[k]);

            poly[k].X = (ClipperLib::long64)(fx * sf);
            poly[k].Y = (ClipperLib::long64)(fy * sf);
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

extern "C" int
bg_polygons_overlap(struct bg_polygon *polyA, struct bg_polygon *polyB, plane_t *vp, const struct bn_tol *tol, fastf_t iscale)
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
	    fastf_t fx, fy;
	    bg_plane_closest_pt(&fx, &fy, vp, &polyA->contour[i].point[j]);
	    VSET(vpoint, fx * scale, fy * scale, 0);
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
	    fastf_t fx, fy;
	    bg_plane_closest_pt(&fx, &fy, vp, &polyB->contour[i].point[j]);
	    VSET(vpoint, fx * scale, fy * scale, 0);
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
load_polygon(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, struct bg_polygon *gpoly, fastf_t sf, plane_t *vp)
{
    size_t j, k, n;
    ClipperLib::Path curr_poly;
    fastf_t vZ = 1.0;

    for (j = 0; j < gpoly->num_contours; ++j) {
	n = gpoly->contour[j].num_points;
	curr_poly.resize(n);
	for (k = 0; k < n; k++) {
	    fastf_t fx = gpoly->contour[j].point[k][0];
	    fastf_t fy = gpoly->contour[j].point[k][1];
	    if (vp)
		bg_plane_closest_pt(&fx, &fy, vp, &gpoly->contour[j].point[k]);

	    curr_poly[k].X = (ClipperLib::long64)(fx * sf);
	    curr_poly[k].Y = (ClipperLib::long64)(fy * sf);
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
load_polygons(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, struct bg_polygons *subj, fastf_t sf, plane_t *vp)
{
    size_t i;
    fastf_t vZ = 1.0;

    for (i = 0; i < subj->num_polygons; ++i)
	vZ = load_polygon(clipper, ptype, &subj->polygon[i], sf, vp);

    return vZ;
}

/*
 * Process/extract the clipper_polys into a struct bg_polygon.
 */
static struct bg_polygon *
extract(ClipperLib::PolyTree &clipper_polytree, fastf_t sf, plane_t *vp)
{
    size_t j, n;
    size_t num_contours = clipper_polytree.Total();
    struct bg_polygon *outp;

    BU_ALLOC(outp, struct bg_polygon);
    outp->num_contours = num_contours;

    if (num_contours < 1)
	return outp;

    outp->hole = (int *)bu_calloc(num_contours, sizeof(int), "hole");
    outp->contour = (struct bg_poly_contour *)bu_calloc(num_contours, sizeof(struct bg_poly_contour), "contour");

    ClipperLib::PolyNode *polynode = clipper_polytree.GetFirst();
    n = 0;
    while (polynode) {
	ClipperLib::Path &path = polynode->Contour;

	outp->hole[n] = polynode->IsHole();
	outp->contour[n].num_points = path.size();
	outp->contour[n].open = polynode->IsOpen();
	outp->contour[n].point = (point_t *)bu_calloc(outp->contour[n].num_points, sizeof(point_t), "point");

	for (j = 0; j < outp->contour[n].num_points; ++j) {
	    if (vp) {
		bg_plane_pt_at(&outp->contour[n].point[j], vp, (fastf_t)(path[j].X) * sf, (fastf_t)(path[j].Y) * sf);
	    } else {
		VSET(outp->contour[n].point[j], (fastf_t)(path[j].X) * sf, (fastf_t)(path[j].Y) * sf, 0);
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
bg_clip_polygon(bg_clip_t op, struct bg_polygon *subj, struct bg_polygon *clip, fastf_t sf, plane_t *vp)
{
    fastf_t inv_sf;
    ClipperLib::Clipper clipper;
    ClipperLib::PolyTree result_clipper_polys;
    ClipperLib::ClipType ctOp;

    /* need to scale the points up/down and then convert to/from long64 */
    /* need a matrix to rotate into a plane */
    /* need the inverse of the matrix above to put things back after clipping */

    /* Load subject polygon into clipper */
    load_polygon(clipper, ClipperLib::ptSubject, subj, sf, vp);

    /* Load clip polygon into clipper */
    load_polygon(clipper, ClipperLib::ptClip, clip, sf, vp);

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
    return extract(result_clipper_polys, inv_sf, vp);
}


struct bg_polygon *
bg_clip_polygons(bg_clip_t op, struct bg_polygons *subj, struct bg_polygons *clip, fastf_t sf, plane_t *vp)
{
    fastf_t inv_sf;
    ClipperLib::Clipper clipper;
    ClipperLib::PolyTree result_clipper_polys;
    ClipperLib::ClipType ctOp;

    /* need to scale the points up/down and then convert to/from long64 */
    /* need a matrix to rotate into a plane */
    /* need the inverse of the matrix above to put things back after clipping */

    /* Load subject polygons into clipper */
    load_polygons(clipper, ClipperLib::ptSubject, subj, sf, vp);

    /* Load clip polygons into clipper */
    load_polygons(clipper, ClipperLib::ptClip, clip, sf, vp);

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
    return extract(result_clipper_polys, inv_sf, vp);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

