/*                    P O L Y C L I P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
    genptr_t segment;
};

struct contour_node {
    struct bu_list l;
    struct bu_list head;
};


static void
load_polygon(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, ged_polygon *gpoly, fastf_t sf, matp_t mat)
{
    register size_t j, k, n;
    ClipperLib::Polygon curr_poly;

    for (j = 0; j < gpoly->gp_num_contours; ++j) {
	n = gpoly->gp_contour[j].gpc_num_points;
	curr_poly.resize(n);
	for (k = 0; k < n; k++) {
	    point_t vpoint;

 	    /* Convert to view coordinates */
 	    MAT4X3PNT(vpoint, mat, gpoly->gp_contour[j].gpc_point[k]);
 
 	    curr_poly[k].X = (ClipperLib::long64)(vpoint[X] * sf);
 	    curr_poly[k].Y = (ClipperLib::long64)(vpoint[Y] * sf);
	}

	try {
	    clipper.AddPolygon(curr_poly, ptype);
	} catch (...) {
	    bu_log("Exception thrown by clipper\n");
	}
    }
}

static void
load_polygons(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, ged_polygons *subj, fastf_t sf, matp_t mat)
{
    register size_t i;

    for (i = 0; i < subj->gp_num_polygons; ++i)
	load_polygon(clipper, ptype, &subj->gp_polygon[i], sf, mat);
}


/*
 * Process/extract the clipper_polys into a ged_polygon.
 */
static ged_polygon *
extract(ClipperLib::ExPolygons &clipper_polys, fastf_t sf, matp_t mat)
{
    register size_t i, j, k, n;
    size_t num_contours = 0;
    ged_polygon *result_poly;

    /* Count up the number of contours. */
    for (i = 0; i < clipper_polys.size(); ++i)
	/* Add the outer and the holes */
	num_contours += clipper_polys[i].holes.size() + 1;

    result_poly = (ged_polygon *)bu_calloc(1, sizeof(ged_polygon), "result_poly");
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
	    VSET(vpoint, (fastf_t)(clipper_polys[i].outer[j].X) * sf, (fastf_t)(clipper_polys[i].outer[j].Y) * sf, 1.0);

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
		VSET(vpoint, (fastf_t)(clipper_polys[i].holes[j][k].X) * sf, (fastf_t)(clipper_polys[i].holes[j][k].Y) * sf, 1.0);

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
    ClipperLib::Clipper clipper;
    ClipperLib::Polygon curr_poly;
    ClipperLib::ExPolygons result_clipper_polys;
    ClipperLib::ClipType ctOp;

    /* need to scale the points up/down and then convert to/from long64 */
    /* need a matrix to rotate into a plane */
    /* need the inverse of the matrix above to put things back after clipping */

    /* Load subject polygon into clipper */
    load_polygon(clipper, ClipperLib::ptSubject, subj, sf, model2view);

    /* Load clip polygon into clipper */
    load_polygon(clipper, ClipperLib::ptClip, clip, sf, model2view);

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
    return extract(result_clipper_polys, inv_sf, view2model);
}


ged_polygon *
ged_clip_polygons(GedClipType op, ged_polygons *subj, ged_polygons *clip, fastf_t sf, matp_t model2view, matp_t view2model)
{
    fastf_t inv_sf;
    ClipperLib::Clipper clipper;
    ClipperLib::Polygon curr_poly;
    ClipperLib::ExPolygons result_clipper_polys;
    ClipperLib::ClipType ctOp;

    /* need to scale the points up/down and then convert to/from long64 */
    /* need a matrix to rotate into a plane */
    /* need the inverse of the matrix above to put things back after clipping */

    /* Load subject polygons into clipper */
    load_polygons(clipper, ClipperLib::ptSubject, subj, sf, model2view);

    /* Load clip polygons into clipper */
    load_polygons(clipper, ClipperLib::ptClip, clip, sf, model2view);

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
    return extract(result_clipper_polys, inv_sf, view2model);
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
    internal.idb_meth = &rt_functab[ID_SKETCH];
    internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_sketch_internal), "rt_sketch_internal");
    sketch_ip = (struct rt_sketch_internal *)internal.idb_ptr;
    sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;
    sketch_ip->vert_count = num_verts;
    sketch_ip->verts = (point2d_t *)bu_calloc(sketch_ip->vert_count, sizeof(point2d_t), "sketch_ip->verts");
    sketch_ip->curve.count = num_verts;
    sketch_ip->curve.reverse = (int *)bu_calloc(sketch_ip->curve.count, sizeof(int), "sketch_ip->curve.reverse");
    sketch_ip->curve.segment = (genptr_t *)bu_calloc(sketch_ip->curve.count, sizeof(genptr_t), "sketch_ip->curve.segment");

    bn_mat_inv(invRot, gdpsp->gdps_rotation);
    VSET(view, 1.0, 0.0, 0.0);
    MAT4X3PNT(sketch_ip->u_vec, invRot, view);

    VSET(view, 0.0, 1.0, 0.0);
    MAT4X3PNT(sketch_ip->v_vec, invRot, view);

#if 0
    /* should already be unit vectors */
    VUNITIZE(sketch_ip->u_vec);
    VUNITIZE(sketch_ip->V_vec);
#endif

    /* Project the origin onto the front of the viewing cube */
    MAT4X3PNT(vorigin, gdpsp->gdps_model2view, gdpsp->gdps_origin);
    vorigin[Z] = 1.0;

    /* Convert back to model coordinates for storage */
    MAT4X3PNT(sketch_ip->V, gdpsp->gdps_view2model, vorigin);

    n = 0;
    for (j = 0; j < gdpsp->gdps_polygons.gp_polygon[polygon_i].gp_num_contours; ++j)
	for (k = 0; k < gdpsp->gdps_polygons.gp_polygon[polygon_i].gp_contour[j].gpc_num_points; ++k) {
	    point_t vpt;
	    vect_t vdiff;

	    MAT4X3PNT(vpt, gdpsp->gdps_model2view, gdpsp->gdps_polygons.gp_polygon[polygon_i].gp_contour[j].gpc_point[k]);
	    VSUB2(vdiff, vpt, vorigin);
	    VSCALE(vdiff, vdiff, gdpsp->gdps_scale);
	    V2MOVE(sketch_ip->verts[n], vdiff);

	    if (n) {
		lsg = (struct line_seg *)bu_calloc(1, sizeof(struct line_seg), "segment");
		sketch_ip->curve.segment[n-1] = (genptr_t)lsg;
		lsg->magic = CURVE_LSEG_MAGIC;
		lsg->start = n-1;
		lsg->end = n;
	    }

	    ++n;
	}


    if (n) {
	lsg = (struct line_seg *)bu_calloc(1, sizeof(struct line_seg), "segment");
	sketch_ip->curve.segment[n-1] = (genptr_t)lsg;
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = n-1;
	lsg->end = 0;
    }

    GED_DB_DIRADD(gedp, dp, sname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&internal.idb_type, GED_ERROR);
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
	    curr_cnode = (struct contour_node *)bu_calloc(1, sizeof(struct contour_node), "curr_cnode");
	    BU_LIST_INSERT(&HeadContourNodes, &curr_cnode->l);
	    BU_LIST_INIT(&curr_cnode->head);
	    BU_LIST_INSERT(&curr_cnode->head, &unused_snode->l);
	}
    }

    gpp = (ged_polygon *)bu_calloc(1, sizeof(ged_polygon), "poly");
    gpp->gp_num_contours = ncontours;
    gpp->gp_hole = (int *)bu_calloc(ncontours, sizeof(int), "gp_hole");
    gpp->gp_contour = (ged_poly_contour *)bu_calloc(ncontours, sizeof(ged_poly_contour), "gp_contour");

    j = 0;
    while (BU_LIST_NON_EMPTY(&HeadContourNodes)) {
	register size_t k = 0;
	size_t npoints = 1;
	struct line_seg *curr_lsg;

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

	VJOIN2(gpp->gp_contour[j].gpc_point[k], sketch_ip->V,
	       sketch_ip->verts[curr_lsg->end][0], sketch_ip->u_vec,
	       sketch_ip->verts[curr_lsg->end][1], sketch_ip->v_vec);

	/* free contour node */
	bu_free((genptr_t)curr_cnode, "curr_cnode");
    }

    /* Clean up */
    bu_free((genptr_t)all_segment_nodes, "all_segment_nodes");
    rt_db_free_internal(&intern);

    return gpp;
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
