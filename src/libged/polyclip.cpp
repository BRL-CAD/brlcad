/*                    P O L Y C L I P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/polyclip.cpp
 *
 * An interface to src/other/clipper.
 *
 */
/** @} */

#include "common.h"

#include <clipper.hpp>

#include "ged.h"


typedef struct {
    ClipperLib::long64 x;
    ClipperLib::long64 y;
} clipper_vertex;

struct segment_node {
    struct bu_list l;
    int reverse;
    int used;
    void *segment;
};

struct contour_node {
    struct bu_list l;
    struct bu_list head;
};


static fastf_t
load_polygon(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, ged_polygon *gpoly, fastf_t sf, matp_t mat)
{
    register size_t j, k, n;
    ClipperLib::Polygon curr_poly;
    fastf_t vZ = 1.0;

    for (j = 0; j < gpoly->gp_num_contours; ++j) {
	n = gpoly->gp_contour[j].gpc_num_points;
	curr_poly.resize(n);
	for (k = 0; k < n; k++) {
	    point_t vpoint;

	    /* Convert to view coordinates */
	    MAT4X3PNT(vpoint, mat, gpoly->gp_contour[j].gpc_point[k]);
	    vZ = vpoint[Z];

	    curr_poly[k].X = (ClipperLib::long64)(vpoint[X] * sf);
	    curr_poly[k].Y = (ClipperLib::long64)(vpoint[Y] * sf);
	}

	try {
	    clipper.AddPolygon(curr_poly, ptype);
	} catch (...) {
	    bu_log("Exception thrown by clipper\n");
	}
    }

    return vZ;
}

static fastf_t
load_polygons(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, ged_polygons *subj, fastf_t sf, matp_t mat)
{
    register size_t i;
    fastf_t vZ = 1.0;

    for (i = 0; i < subj->gp_num_polygons; ++i)
	vZ = load_polygon(clipper, ptype, &subj->gp_polygon[i], sf, mat);

    return vZ;
}


/*
 * Process/extract the clipper_polys into a ged_polygon.
 */
static ged_polygon *
extract(ClipperLib::ExPolygons &clipper_polys, fastf_t sf, matp_t mat, fastf_t vZ)
{
    register size_t i, j, k, n;
    size_t num_contours = 0;
    ged_polygon *result_poly;

    /* Count up the number of contours. */
    for (i = 0; i < clipper_polys.size(); ++i)
	/* Add the outer and the holes */
	num_contours += clipper_polys[i].holes.size() + 1;

    BU_ALLOC(result_poly, ged_polygon);
    result_poly->gp_num_contours = num_contours;

    if (num_contours < 1)
	return result_poly;

    result_poly->gp_hole = (int *)bu_calloc(num_contours, sizeof(int), "gp_hole");
    result_poly->gp_contour = (ged_poly_contour *)bu_calloc(num_contours, sizeof(ged_poly_contour), "gp_contour");

    n = 0;
    for (i = 0; i < clipper_polys.size(); ++i) {
	point_t vpoint;

	result_poly->gp_hole[n] = 0;
	result_poly->gp_contour[n].gpc_num_points = clipper_polys[i].outer.size();
	result_poly->gp_contour[n].gpc_point =
	    (point_t *)bu_calloc(result_poly->gp_contour[n].gpc_num_points,
				 sizeof(point_t), "gpc_point");

	for (j = 0; j < result_poly->gp_contour[n].gpc_num_points; ++j) {
	    VSET(vpoint, (fastf_t)(clipper_polys[i].outer[j].X) * sf, (fastf_t)(clipper_polys[i].outer[j].Y) * sf, vZ);

	    /* Convert to model coordinates */
	    MAT4X3PNT(result_poly->gp_contour[n].gpc_point[j], mat, vpoint);
	}

	++n;
	for (j = 0; j < clipper_polys[i].holes.size(); ++j) {
	    result_poly->gp_hole[n] = 1;
	    result_poly->gp_contour[n].gpc_num_points = clipper_polys[i].holes[j].size();
	    result_poly->gp_contour[n].gpc_point =
		(point_t *)bu_calloc(result_poly->gp_contour[n].gpc_num_points,
				     sizeof(point_t), "gpc_point");

	    for (k = 0; k < result_poly->gp_contour[n].gpc_num_points; ++k) {
		VSET(vpoint, (fastf_t)(clipper_polys[i].holes[j][k].X) * sf, (fastf_t)(clipper_polys[i].holes[j][k].Y) * sf, vZ);

		/* Convert to model coordinates */
		MAT4X3PNT(result_poly->gp_contour[n].gpc_point[k], mat, vpoint);
	    }

	    ++n;
	}
    }

    return result_poly;
}


ged_polygon *
ged_clip_polygon(GedClipType op, ged_polygon *subj, ged_polygon *clip, fastf_t sf, matp_t model2view, matp_t view2model)
{
    fastf_t inv_sf;
    fastf_t vZ;
    ClipperLib::Clipper clipper;
    ClipperLib::ExPolygons result_clipper_polys;
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
    case gctIntersection:
	ctOp = ClipperLib::ctIntersection;
	break;
    case gctUnion:
	ctOp = ClipperLib::ctUnion;
	break;
    case gctDifference:
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


ged_polygon *
ged_clip_polygons(GedClipType op, ged_polygons *subj, ged_polygons *clip, fastf_t sf, matp_t model2view, matp_t view2model)
{
    fastf_t inv_sf;
    fastf_t vZ;
    ClipperLib::Clipper clipper;
    ClipperLib::ExPolygons result_clipper_polys;
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
    case gctIntersection:
	ctOp = ClipperLib::ctIntersection;
	break;
    case gctUnion:
	ctOp = ClipperLib::ctUnion;
	break;
    case gctDifference:
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
ged_export_polygon(struct ged *gedp, ged_data_polygon_state *gdpsp, size_t polygon_i, const char *sname)
{
    register size_t j, k, n;
    register size_t num_verts = 0;
    struct rt_db_internal internal;
    struct rt_sketch_internal *sketch_ip;
    struct line_seg *lsg;
    struct directory *dp;
    vect_t view;
    point_t vorigin;
    mat_t invRot;

    GED_CHECK_EXISTS(gedp, sname, LOOKUP_QUIET, GED_ERROR);
    RT_DB_INTERNAL_INIT(&internal);

    if (polygon_i >= gdpsp->gdps_polygons.gp_num_polygons ||
	gdpsp->gdps_polygons.gp_polygon[polygon_i].gp_num_contours < 1)
	return GED_ERROR;

    for (j = 0; j < gdpsp->gdps_polygons.gp_polygon[polygon_i].gp_num_contours; ++j)
	num_verts += gdpsp->gdps_polygons.gp_polygon[polygon_i].gp_contour[j].gpc_num_points;

    if (num_verts < 3)
	return GED_ERROR;

    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_SKETCH;
    internal.idb_meth = &OBJ[ID_SKETCH];

    BU_ALLOC(internal.idb_ptr, struct rt_sketch_internal);
    sketch_ip = (struct rt_sketch_internal *)internal.idb_ptr;
    sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;
    sketch_ip->vert_count = num_verts;
    sketch_ip->verts = (point2d_t *)bu_calloc(sketch_ip->vert_count, sizeof(point2d_t), "sketch_ip->verts");
    sketch_ip->curve.count = num_verts;
    sketch_ip->curve.reverse = (int *)bu_calloc(sketch_ip->curve.count, sizeof(int), "sketch_ip->curve.reverse");
    sketch_ip->curve.segment = (void **)bu_calloc(sketch_ip->curve.count, sizeof(void *), "sketch_ip->curve.segment");

    bn_mat_inv(invRot, gdpsp->gdps_rotation);
    VSET(view, 1.0, 0.0, 0.0);
    MAT4X3PNT(sketch_ip->u_vec, invRot, view);

    VSET(view, 0.0, 1.0, 0.0);
    MAT4X3PNT(sketch_ip->v_vec, invRot, view);

    /* should already be unit vectors */
    VUNITIZE(sketch_ip->u_vec);
    VUNITIZE(sketch_ip->v_vec);

    /* Project the origin onto the front of the viewing cube */
    MAT4X3PNT(vorigin, gdpsp->gdps_model2view, gdpsp->gdps_origin);
    vorigin[Z] = 1.0;

    /* Convert back to model coordinates for storage */
    MAT4X3PNT(sketch_ip->V, gdpsp->gdps_view2model, vorigin);

    n = 0;
    for (j = 0; j < gdpsp->gdps_polygons.gp_polygon[polygon_i].gp_num_contours; ++j) {
	size_t cstart = n;

	for (k = 0; k < gdpsp->gdps_polygons.gp_polygon[polygon_i].gp_contour[j].gpc_num_points; ++k) {
	    point_t vpt;
	    vect_t vdiff;

	    MAT4X3PNT(vpt, gdpsp->gdps_model2view, gdpsp->gdps_polygons.gp_polygon[polygon_i].gp_contour[j].gpc_point[k]);
	    VSUB2(vdiff, vpt, vorigin);
	    VSCALE(vdiff, vdiff, gdpsp->gdps_scale);
	    V2MOVE(sketch_ip->verts[n], vdiff);

	    if (k) {
		BU_ALLOC(lsg, struct line_seg);
		sketch_ip->curve.segment[n-1] = (void *)lsg;
		lsg->magic = CURVE_LSEG_MAGIC;
		lsg->start = n-1;
		lsg->end = n;
	    }

	    ++n;
	}

	if (k) {
	    BU_ALLOC(lsg, struct line_seg);
	    sketch_ip->curve.segment[n-1] = (void *)lsg;
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = n-1;
	    lsg->end = cstart;
	}
    }


    GED_DB_DIRADD(gedp, dp, sname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    return GED_OK;
}


ged_polygon *
ged_import_polygon(struct ged *gedp, const char *sname)
{
    register size_t j, n;
    register size_t ncontours = 0;
    struct rt_db_internal intern;
    struct rt_sketch_internal *sketch_ip;
    struct bu_list HeadSegmentNodes;
    struct bu_list HeadContourNodes;
    struct segment_node *all_segment_nodes;
    struct segment_node *curr_snode;
    struct contour_node *curr_cnode;
    ged_polygon *gpp;

    if (wdb_import_from_path(gedp->ged_result_str, &intern, sname, gedp->ged_wdbp) == GED_ERROR)
	return (ged_polygon *)0;

    sketch_ip = (rt_sketch_internal *)intern.idb_ptr;
    if (sketch_ip->vert_count < 3 || sketch_ip->curve.count < 1) {
	rt_db_free_internal(&intern);
	return (ged_polygon *)0;
    }

    all_segment_nodes = (struct segment_node *)bu_calloc(sketch_ip->curve.count, sizeof(struct segment_node), "all_segment_nodes");

    BU_LIST_INIT(&HeadSegmentNodes);
    BU_LIST_INIT(&HeadContourNodes);
    for (n = 0; n < sketch_ip->curve.count; ++n) {
	all_segment_nodes[n].segment = sketch_ip->curve.segment[n];
	all_segment_nodes[n].reverse = sketch_ip->curve.reverse[n];
	BU_LIST_INSERT(&HeadSegmentNodes, &all_segment_nodes[n].l);
    }

    curr_cnode = (struct contour_node *)0;
    while (BU_LIST_NON_EMPTY(&HeadSegmentNodes)) {
	struct segment_node *unused_snode = BU_LIST_FIRST(segment_node, &HeadSegmentNodes);
	uint32_t *magic = (uint32_t *)unused_snode->segment;
	struct line_seg *unused_lsg;

	BU_LIST_DEQUEUE(&unused_snode->l);

	/* For the moment, skipping everything except line segments */
	if (*magic != CURVE_LSEG_MAGIC)
	    continue;

	unused_lsg = (struct line_seg *)unused_snode->segment;
	if (unused_snode->reverse) {
	    int tmp = unused_lsg->start;
	    unused_lsg->start = unused_lsg->end;
	    unused_lsg->end = tmp;
	}

	/* Find a contour to add the unused segment to. */
	for (BU_LIST_FOR(curr_cnode, contour_node, &HeadContourNodes)) {
	    for (BU_LIST_FOR(curr_snode, segment_node, &curr_cnode->head)) {
		struct line_seg *curr_lsg = (struct line_seg *)curr_snode->segment;

		if (unused_lsg->start == curr_lsg->end) {
		    unused_snode->used = 1;
		    BU_LIST_APPEND(&curr_snode->l, &unused_snode->l);
		    goto end;
		}

		if (unused_lsg->end == curr_lsg->start) {
		    unused_snode->used = 1;
		    BU_LIST_INSERT(&curr_snode->l, &unused_snode->l);
		    goto end;
		}
	    }
	}

    end:

	if (!unused_snode->used) {
	    ++ncontours;
	    BU_ALLOC(curr_cnode, struct contour_node);
	    BU_LIST_INSERT(&HeadContourNodes, &curr_cnode->l);
	    BU_LIST_INIT(&curr_cnode->head);
	    BU_LIST_INSERT(&curr_cnode->head, &unused_snode->l);
	}
    }

    BU_ALLOC(gpp, ged_polygon);
    gpp->gp_num_contours = ncontours;
    gpp->gp_hole = (int *)bu_calloc(ncontours, sizeof(int), "gp_hole");
    gpp->gp_contour = (ged_poly_contour *)bu_calloc(ncontours, sizeof(ged_poly_contour), "gp_contour");

    j = 0;
    while (BU_LIST_NON_EMPTY(&HeadContourNodes)) {
	register size_t k = 0;
	size_t npoints = 0;
	struct line_seg *curr_lsg = NULL;

	curr_cnode = BU_LIST_FIRST(contour_node, &HeadContourNodes);
	BU_LIST_DEQUEUE(&curr_cnode->l);

	/* Count the number of segments in this contour */
	for (BU_LIST_FOR(curr_snode, segment_node, &curr_cnode->head))
	    ++npoints;

	gpp->gp_contour[j].gpc_num_points = npoints;
	gpp->gp_contour[j].gpc_point = (point_t *)bu_calloc(npoints, sizeof(point_t), "gpc_point");

	while (BU_LIST_NON_EMPTY(&curr_cnode->head)) {
	    curr_snode = BU_LIST_FIRST(segment_node, &curr_cnode->head);
	    BU_LIST_DEQUEUE(&curr_snode->l);

	    curr_lsg = (struct line_seg *)curr_snode->segment;

	    /* Convert from UV space to model space */
	    VJOIN2(gpp->gp_contour[j].gpc_point[k], sketch_ip->V,
		   sketch_ip->verts[curr_lsg->start][0], sketch_ip->u_vec,
		   sketch_ip->verts[curr_lsg->start][1], sketch_ip->v_vec);
	    ++k;
	}

	/* free contour node */
	bu_free((void *)curr_cnode, "curr_cnode");

	++j;
    }

    /* Clean up */
    bu_free((void *)all_segment_nodes, "all_segment_nodes");
    rt_db_free_internal(&intern);

    return gpp;
}


fastf_t
ged_find_polygon_area(ged_polygon *gpoly, fastf_t sf, matp_t model2view, fastf_t size)
{
    register size_t j, k, n;
    ClipperLib::Polygon poly;
    fastf_t area = 0.0;

    if (NEAR_ZERO(sf, SMALL_FASTF))
	return 0.0;

    for (j = 0; j < gpoly->gp_num_contours; ++j) {
	n = gpoly->gp_contour[j].gpc_num_points;
	poly.resize(n);
	for (k = 0; k < n; k++) {
	    point_t vpoint;

	    /* Convert to view coordinates */
	    MAT4X3PNT(vpoint, model2view, gpoly->gp_contour[j].gpc_point[k]);

	    poly[k].X = (ClipperLib::long64)(vpoint[X] * sf);
	    poly[k].Y = (ClipperLib::long64)(vpoint[Y] * sf);
	}

	area += (fastf_t)ClipperLib::Area(poly);
    }

    sf = 1.0/(sf*sf) * size * size;

    return (area * sf);
}


typedef struct {
    size_t	pc_num_points;
    point2d_t	*pc_point;
} poly_contour_2d;

typedef struct {
    size_t 		p_num_contours;
    int			*p_hole;
    poly_contour_2d	*p_contour;
} polygon_2d;


int
ged_polygons_overlap(struct ged *gedp, ged_polygon *polyA, ged_polygon *polyB)
{
    register size_t i, j;
    register size_t beginA, endA, beginB, endB;
    fastf_t tol_dist;
    fastf_t tol_dist_sq;
    fastf_t scale;
    polygon_2d polyA_2d;
    polygon_2d polyB_2d;
    point2d_t pt_2d;
    point_t A;
    size_t winding = 0;
    int ret = 0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);

    if (polyA->gp_num_contours < 1 || polyA->gp_contour[0].gpc_num_points < 1 ||
	polyB->gp_num_contours < 1 || polyB->gp_contour[0].gpc_num_points < 1)
	return 0;

    if (gedp->ged_wdbp->wdb_tol.dist > 0.0)
	tol_dist = gedp->ged_wdbp->wdb_tol.dist;
    else
	tol_dist = BN_TOL_DIST;

    if (gedp->ged_wdbp->wdb_tol.dist_sq > 0.0)
	tol_dist_sq = gedp->ged_wdbp->wdb_tol.dist_sq;
    else
	tol_dist_sq = tol_dist * tol_dist;

    if (gedp->ged_gvp->gv_scale > (fastf_t)UINT16_MAX)
	scale = gedp->ged_gvp->gv_scale;
    else
	scale = (fastf_t)UINT16_MAX;

    /* Project polyA and polyB onto the view plane */
    polyA_2d.p_num_contours = polyA->gp_num_contours;
    polyA_2d.p_hole = (int *)bu_calloc(polyA->gp_num_contours, sizeof(int), "p_hole");
    polyA_2d.p_contour = (poly_contour_2d *)bu_calloc(polyA->gp_num_contours, sizeof(poly_contour_2d), "p_contour");

    for (i = 0; i < polyA->gp_num_contours; ++i) {
	polyA_2d.p_hole[i] = polyA->gp_hole[i];
	polyA_2d.p_contour[i].pc_num_points = polyA->gp_contour[i].gpc_num_points;
	polyA_2d.p_contour[i].pc_point = (point2d_t *)bu_calloc(polyA->gp_contour[i].gpc_num_points, sizeof(point2d_t), "pc_point");

	for (j = 0; j < polyA->gp_contour[i].gpc_num_points; ++j) {
	    point_t vpoint;

	    MAT4X3PNT(vpoint, gedp->ged_gvp->gv_model2view, polyA->gp_contour[i].gpc_point[j]);
	    VSCALE(vpoint, vpoint, scale);
	    V2MOVE(polyA_2d.p_contour[i].pc_point[j], vpoint);
	}
    }

    polyB_2d.p_num_contours = polyB->gp_num_contours;
    polyB_2d.p_hole = (int *)bu_calloc(polyB->gp_num_contours, sizeof(int), "p_hole");
    polyB_2d.p_contour = (poly_contour_2d *)bu_calloc(polyB->gp_num_contours, sizeof(poly_contour_2d), "p_contour");

    for (i = 0; i < polyB->gp_num_contours; ++i) {
	polyB_2d.p_hole[i] = polyB->gp_hole[i];
	polyB_2d.p_contour[i].pc_num_points = polyB->gp_contour[i].gpc_num_points;
	polyB_2d.p_contour[i].pc_point = (point2d_t *)bu_calloc(polyB->gp_contour[i].gpc_num_points, sizeof(point2d_t), "pc_point");

	for (j = 0; j < polyB->gp_contour[i].gpc_num_points; ++j) {
	    point_t vpoint;

	    MAT4X3PNT(vpoint, gedp->ged_gvp->gv_model2view, polyB->gp_contour[i].gpc_point[j]);
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

		    if (bn_isect_lseg2_lseg2(distvec,
					     polyA_2d.p_contour[i].pc_point[beginA], dirA,
					     polyB_2d.p_contour[j].pc_point[beginB], dirB,
					     &gedp->ged_wdbp->wdb_tol) == 1) {
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

    for (i = 0; i < polyA->gp_num_contours; ++i)
	bu_free((void *)polyA_2d.p_contour[i].pc_point, "pc_point");
    for (i = 0; i < polyB->gp_num_contours; ++i)
	bu_free((void *)polyB_2d.p_contour[i].pc_point, "pc_point");

    bu_free((void *)polyA_2d.p_hole, "p_hole");
    bu_free((void *)polyA_2d.p_contour, "p_contour");
    bu_free((void *)polyB_2d.p_hole, "p_hole");
    bu_free((void *)polyB_2d.p_contour, "p_contour");

    return ret;
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
