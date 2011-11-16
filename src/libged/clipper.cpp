/*                       C L I P P E R . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2011 United States Government as represented by
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
/** @file libged/clipper.c
 *
 * An interface to src/other/clipper.
 *
 */
/** @} */

#include "clipper.hpp"
#include "ged.h"


typedef struct
{
    ClipperLib::long64 x;
    ClipperLib::long64 y;
} clipper_vertex;


void
ged_clipper_load_polygon(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, ged_polygon *gpoly) 
{
    register size_t j, k, n;
    ClipperLib::Polygon curr_poly;
    fastf_t sf = 1000;

    for (j = 0; j < gpoly->gp_num_contours; ++j) {
	n = gpoly->gp_contour[j].gpc_num_points;
	curr_poly.resize(n);
	for (k = 0; k < n; k++) {
	    curr_poly[k].X = (ClipperLib::long64)(gpoly->gp_contour[j].gpc_point[k][0] * sf);
	    curr_poly[k].Y = (ClipperLib::long64)(gpoly->gp_contour[j].gpc_point[k][1] * sf);
	}

	clipper.AddPolygon(curr_poly, ptype);
    }
}

void
ged_clipper_load_polygons(ClipperLib::Clipper &clipper, ClipperLib::PolyType ptype, ged_polygons *subj) 
{
    register size_t i;

    for (i = 0; i < subj->gp_num_polygons; ++i)
	ged_clipper_load_polygon(clipper, ptype, &subj->gp_polygon[i]);
}


/*
 * Process/extract the clipper_polys into a ged_polygon.
 */
ged_polygon *
ged_clipper_extract(ClipperLib::ExPolygons &clipper_polys)
{
    register size_t i, j, k, n;
    size_t num_contours = 0;
    ged_polygon *result_poly;
    fastf_t sf = 0.001;

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
	result_poly->gp_hole[n] = 0;
	result_poly->gp_contour[n].gpc_num_points = clipper_polys[i].outer.size();
	result_poly->gp_contour[n].gpc_point =
	    (point_t *)bu_calloc(result_poly->gp_contour[n].gpc_num_points,
				 sizeof(point_t), "gpc_point");

	for (j = 0; j < result_poly->gp_contour[n].gpc_num_points; ++j) {
	    result_poly->gp_contour[n].gpc_point[j][0] = (fastf_t)(clipper_polys[i].outer[j].X) * sf;
	    result_poly->gp_contour[n].gpc_point[j][1] = (fastf_t)(clipper_polys[i].outer[j].Y) * sf;
	}

	++n;
	for (j = 0; j < clipper_polys[i].holes.size(); ++j) {
	    result_poly->gp_hole[n] = 1;
	    result_poly->gp_contour[n].gpc_num_points = clipper_polys[i].holes[j].size();
	    result_poly->gp_contour[n].gpc_point =
		(point_t *)bu_calloc(result_poly->gp_contour[n].gpc_num_points,
				     sizeof(point_t), "gpc_point");

	    for (k = 0; k < result_poly->gp_contour[n].gpc_num_points; ++k) {
		result_poly->gp_contour[n].gpc_point[k][0] = (fastf_t)(clipper_polys[i].holes[j][k].X) * sf;
		result_poly->gp_contour[n].gpc_point[k][1] = (fastf_t)(clipper_polys[i].holes[j][k].Y) * sf;
	    }

	    ++n;
	}
    }

    return result_poly;
}


ged_polygon *
ged_clip_polygon(GedClipType op, ged_polygon *subj, ged_polygon *clip)
{
    ClipperLib::Clipper clipper;
    ClipperLib::Polygon curr_poly;
    ClipperLib::ExPolygons result_clipper_polys;
    ClipperLib::ClipType ctOp;

    /* need to scale the points up/down and then convert to/from long64 */
    /* need a matrix to rotate into a plane */
    /* need the inverse of the matrix above to put things back after clipping */

    /* Load subject polygon into clipper */
    ged_clipper_load_polygon(clipper, ClipperLib::ptSubject, subj);

    /* Load clip polygon into clipper */
    ged_clipper_load_polygon(clipper, ClipperLib::ptClip, clip);

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

    return ged_clipper_extract(result_clipper_polys);
}


ged_polygon *
ged_clip_polygons(GedClipType op, ged_polygons *subj, ged_polygons *clip)
{
    ClipperLib::Clipper clipper;
    ClipperLib::Polygon curr_poly;
    ClipperLib::ExPolygons result_clipper_polys;
    ClipperLib::ClipType ctOp;

    /* need to scale the points up/down and then convert to/from long64 */
    /* need a matrix to rotate into a plane */
    /* need the inverse of the matrix above to put things back after clipping */

    /* Load subject polygons into clipper */
    ged_clipper_load_polygons(clipper, ClipperLib::ptSubject, subj);

    /* Load clip polygons into clipper */
    ged_clipper_load_polygons(clipper, ClipperLib::ptClip, clip);

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

    return ged_clipper_extract(result_clipper_polys);
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
