/*                 P O L Y G O N _ O P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bsg/polygon.h"
#include "bsg/util.h"
#include "draw_source_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "polygon_private.h"

int
bsg_polygon_csg(struct bsg_node *target, struct bsg_node *stencil, bg_clip_t op)
{
    // Need data
    if (!target || !stencil)
	return 0;

    // Need polygons
    if (!bsg_node_has_geometry_role(target, BSG_GEOMETRY_POLYGON_REGION) ||
	    !bsg_node_has_geometry_role(stencil, BSG_GEOMETRY_POLYGON_REGION))
	return 0;

    // None op == no change
    if (op == bg_None)
	return 0;

    struct bsg_polygon *polyA = bsg_node_polygon(target);
    struct bsg_polygon *polyB = bsg_node_polygon(stencil);
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
	struct bsg_view *view = bsg_node_get_view(stencil);
	if (!view)
	    return 0;
	int ovlp = bg_polygons_overlap(&polyA->polygon, &polyB->polygon, &polyA->vp, &poly_tol, view->gv_scale);
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
	// origin_point is important to updating.
	polyA->type = polyB->type;
	polyA->vZ = polyB->vZ;
	polyA->curr_contour_i = polyB->curr_contour_i;
	polyA->curr_point_i = polyB->curr_point_i;
	VMOVE(polyA->origin_point, polyB->origin_point);
	HMOVE(polyA->vp, polyB->vp);
	bsg_update_polygon(target, bsg_node_get_view(target), BSG_POLYGON_UPDATE_DEFAULT);
	return 1;
    }

    // Perform the specified operation and get the new polygon
    struct bg_polygon *cp = bg_clip_polygon(op, &polyA->polygon, &polyB->polygon, CLIPPER_MAX, &polyA->vp);

    // Replace the original target polygon with the result
    bg_polygon_free(&polyA->polygon);
    polyA->polygon.num_contours = cp->num_contours;
    polyA->polygon.hole = cp->hole;
    polyA->polygon.contour = cp->contour;

    // We stole the data from cp and put it in polyA - no longer need the
    // original cp container
    BU_PUT(cp, struct bg_polygon);

    // clipper results are always general polygons
    polyA->type = BSG_POLYGON_GENERAL;

    // Make sure everything's current
    bsg_update_polygon(target, bsg_node_get_view(target), BSG_POLYGON_UPDATE_DEFAULT);

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
