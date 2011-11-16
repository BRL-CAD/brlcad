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


typedef struct
{
    ClipperLib::long64 x;
    ClipperLib::long64 y;
} clipper_vertex;


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

	clipper.AddPolygon(curr_poly, ptype);
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


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
