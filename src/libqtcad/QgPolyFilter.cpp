/*                   Q G P O L Y F I L T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2023 United States Government as represented by
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
/** @file QgPolyFilter.cpp
 *
 * Polygon interaction logic for Qt views.
 *
 */

#include "common.h"

extern "C" {
#include "bu/malloc.h"
#include "bg/polygon.h"
#include "bv.h"
#include "raytrace.h" // For finalize polygon sketch export functionality (TODO - need to move...)
}

#include "qtcad/QgPolyFilter.h"
#include "qtcad/QgSignalFlags.h"

QMouseEvent *
QgPolyFilter::view_sync(QEvent *e)
{
    if (!v)
	return NULL;

    // If we don't have one of the relevant mouse operations, there's nothing to do
    QMouseEvent *m_e = NULL;
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove)
	m_e = (QMouseEvent *)e;
    if (!m_e)
	return NULL;

    // We're going to need the mouse position
    int e_x, e_y;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    e_x = m_e->x();
    e_y = m_e->y();
#else
    e_x = m_e->position().x();
    e_y = m_e->position().y();
#endif

    // Update relevant bview variables
    v->gv_prevMouseX = v->gv_mouse_x;
    v->gv_prevMouseY = v->gv_mouse_y;
    v->gv_mouse_x = e_x;
    v->gv_mouse_y = e_y;

    // If we have modifiers, we're most likely doing shift grips
    if (m_e->modifiers() != Qt::NoModifier)
	return NULL;

    return m_e;
}

bool
QgPolyFilter::close_polygon()
{
    // Close the general polygon - if that's what we're creating,
    // at this point it will still be open.
    struct bv_polygon *ip = (struct bv_polygon *)wp->s_i_data;
    if (ip && ip->polygon.contour[0].open) {

	if (ip->polygon.contour[0].num_points < 3) {
	    // If we're trying to finalize and we have less than
	    // three points, just remove - we didn't get enough
	    // to make a closed polygon.
	    bg_polygon_free(&ip->polygon);
	    BU_PUT(ip, struct bv_polygon);
	    bv_obj_put(wp);
	    wp = NULL;
	    return false;
	}

	ip->polygon.contour[0].open = 0;
	bv_update_polygon(wp, wp->s_v, BV_POLYGON_UPDATE_DEFAULT);
    }

    return true;
}

bool
QPolyCreateFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;


    // Handle Left Click
    if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {

	if (!wp) {
	    wp = bv_create_polygon(v, BV_VIEW_OBJS, ptype, v->gv_mouse_x, v->gv_mouse_y);
	    wp->s_v = v;

	    struct bv_polygon *ip = (struct bv_polygon *)wp->s_i_data;
	    if (ptype == BV_POLYGON_GENERAL) {
		// For general polygons, we need to identify the active contour
		// for update operations to work.
		//
		// TODO: At some point we'll need to add support for adding and
		// removing contours...
		ip->curr_contour_i = 0;
	    }

	    // Get edge color
	    bu_color_to_rgb_chars(&edge_color, wp->s_color);

	    // fill color
	    BU_COLOR_CPY(&ip->fill_color, &fill_color);

	    // fill settings
	    vect2d_t vdir = V2INIT_ZERO;
	    vdir[0] = fill_slope_x;
	    vdir[1] = fill_slope_y;
	    V2MOVE(ip->fill_dir, vdir);
	    ip->fill_delta = fill_density;

	    // Z offset
	    ip->vZ = vZ;
	    ip->prev_point[2] = ip->vZ;

	    // Set fill
	    if (fill_poly && !ip->fill_flag) {
		ip->fill_flag = 1;
		bv_update_polygon(wp, wp->s_v, BV_POLYGON_UPDATE_PROPS_ONLY);
	    }
	    if (!fill_poly && ip->fill_flag) {
		ip->fill_flag = 0;
		bv_update_polygon(wp, wp->s_v, BV_POLYGON_UPDATE_DEFAULT);
	    }

	    // Name appropriately
	    bu_vls_init(&wp->s_name);

	    // It doesn't get a "proper" name until its finalized
	    bu_vls_printf(&wp->s_name, "_tmp_view_polygon");

	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}

	// If we don't have a polygon at this point, we're done - subsequent logic assumes it
	if (!wp)
	    return true;

	// If we are in the process of creating a general polygon, after the initial creation
	// left clicks will append new points
	struct bv_polygon *ip = (struct bv_polygon *)wp->s_i_data;
	if (ip->type == BV_POLYGON_GENERAL) {
	    wp->s_v->gv_mouse_x = v->gv_mouse_x;
	    wp->s_v->gv_mouse_y = v->gv_mouse_y;
	    bv_update_polygon(wp, wp->s_v, BV_POLYGON_UPDATE_PT_APPEND);
	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}

	// When we're dealing with polygons stray left clicks shouldn't zoom - just
	// consume them if we're not using them above.
	return true;
    }

    if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::RightButton)) {
	// No-op if we're not in the process of creating a polygon
	if (!wp)
	    return true;

	// Non-general polygon creation doesn't use right click.
	struct bv_polygon *ip = (struct bv_polygon *)wp->s_i_data;
	if (ip->type != BV_POLYGON_GENERAL)
	    return true;

	// When creating a general polygon, right click indicates we're done.
	finalize(true);

	return true;
    }

    if (m_e->type() == QEvent::MouseButtonPress) {
	// We don't want other stray mouse clicks to do something surprising
	return true;
    }

    // During initial add/creation of non-general polygons, mouse movement
    // adjusts the shape
    if (m_e->type() == QEvent::MouseMove) {
	// No-op if no current polygon is defined
	if (!wp)
	    return true;

	// General polygon creation doesn't use mouse movement.
	struct bv_polygon *ip = (struct bv_polygon *)wp->s_i_data;
	if (ip->type == BV_POLYGON_GENERAL)
	    return true;

	// For every other polygon type, call the libbv update routine
	// with the view's x,y coordinates
	if (m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
	    bv_update_polygon(wp, wp->s_v, BV_POLYGON_UPDATE_DEFAULT);
	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}
    }

    // For the constrained polygon shapes, we're done creating once we release
    // the mouse button (i.e. a "click, hold and move" creation paradigm)
    if (m_e->type() == QEvent::MouseButtonRelease) {

	// No-op if no current polygon is defined
	if (!wp)
	    return true;

	// General polygons are finalized by a right-click close, since
	// appending multiple points requires multiple mouse click-and-release
	// operations
	struct bv_polygon *ip = (struct bv_polygon *)wp->s_i_data;
	if (ip && ip->type == BV_POLYGON_GENERAL)
	    return true;

	// For all non-general polygons, mouse release is the signal
	// to finish up.
	finalize(true);
	wp = NULL;

	return true;
    }

    return false;
}

void
QPolyCreateFilter::finalize(bool)
{
    int icnt = 0;

    if (!wp)
	return;

    if (!close_polygon())
	return;

    if (op == bg_None || !BU_PTBL_LEN(&bool_objs)) {
	// No interactions, so we're keeping it - assign a proper name
	bu_vls_sprintf(&wp->s_name, "%s", vname.c_str());
    } else {

	for (size_t i = 0; i < BU_PTBL_LEN(&bool_objs); i++) {
	    struct bv_scene_obj *target = (struct bv_scene_obj *)BU_PTBL_GET(&bool_objs, i);
	    icnt += bv_polygon_csg(target, wp, op);
	}

	// When doing boolean operations, the convention is if there were one
	// or more interactions with other polygons, the original polygon is
	// not retained
	if (icnt || op == bg_Difference || op == bg_Intersection) {
	    bv_obj_put(wp);
	    wp = NULL;
	} else {
	    // No interactions, so we're keeping it - assign a proper name
	    bu_vls_sprintf(&wp->s_name, "%s", vname.c_str());
	}
    }

    // No longer need mouse movements to adjust parameters - turn off callback
    if (wp)
	wp->s_update_callback = NULL;

    emit view_updated(QG_VIEW_REFRESH);
    emit finalized((icnt > 0) ? true : false);
}

bool
QPolyUpdateFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    // The update filter needs an active polygon to operate on
    if (!wp)
	return false;

    // We don't want other stray mouse clicks to do something surprising
    if (m_e->type() == QEvent::MouseButtonPress || m_e->type() == QEvent::MouseButtonRelease) {
	return true;
    }

    if (m_e->type() == QEvent::MouseMove) {

	// General polygon creation doesn't use mouse movement.
	struct bv_polygon *ip = (struct bv_polygon *)wp->s_i_data;
	if (ip->type == BV_POLYGON_GENERAL)
	    return true;

	// For every other polygon type, call the libbv update routine
	// with the view's x,y coordinates
	if (m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
	    bv_update_polygon(wp, wp->s_v, BV_POLYGON_UPDATE_DEFAULT);
	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}

    }

    return false;
}

bool
QPolySelectFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;


    // Handle Left Click
    if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {
	struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
	if (view_objs) {
	    wp = bv_select_polygon(view_objs, v);
	    if (!wp)
		return true;
	    struct bv_polygon *vp = (struct bv_polygon *)wp->s_i_data;
	    ptype = vp->type;
	    close_general_poly = (vp->polygon.contour) ? vp->polygon.contour[0].open : 1;
	    // TODO - either set or sync other C++ class setting copies (color, fill, etc.)
	}

	return true;
    }

    // We also don't want other stray mouse clicks to do something surprising
    if (m_e->type() == QEvent::MouseButtonPress || m_e->type() == QEvent::MouseButtonRelease)
	return true;

    return false;
}

bool
QPolyPointFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    // The point filter needs an active general polygon to operate on
    if (!wp || ptype != BV_POLYGON_GENERAL)
	return false;

    struct bv_polygon *vp = (struct bv_polygon *)wp->s_i_data;

    // Handle Left Click - either selects or clears a point selection (the
    // latter occurs if the click is too far from any active points)
    if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {
	if (vp->curr_point_i < 0) {
	    bv_update_polygon(wp, wp->s_v, BV_POLYGON_UPDATE_PT_SELECT);
	    emit view_updated(QG_VIEW_REFRESH);
	}
	return true;
    }

    // Handle Right Click - clear point selection
    if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::RightButton)) {
	vp->curr_point_i = -1;
	bv_update_polygon(wp, wp->s_v, BV_POLYGON_UPDATE_PROPS_ONLY);
	emit view_updated(QG_VIEW_REFRESH);
	return true;
    }

    // We also don't want other stray mouse clicks to do something surprising
    if (m_e->type() == QEvent::MouseButtonPress || m_e->type() == QEvent::MouseButtonRelease)
	return true;

    // Handle Mouse Move - move selected point with a left button click-and-hold
    if (m_e->type() == QEvent::MouseMove) {
	if (vp->curr_point_i < 0) {
	    // No selected point
	    return true;
	}
	if (m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
	    bv_update_polygon(wp, wp->s_v, BV_POLYGON_UPDATE_PT_MOVE);
	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}

	return true;
    }

    return false;
}

bool
QPolyMoveFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    // The move filter needs an active polygon to operate on
    if (!wp && !BU_PTBL_LEN(&move_objs))
	return false;

    // We don't want other stray mouse clicks to do something surprising
    if (m_e->type() == QEvent::MouseButtonPress || m_e->type() == QEvent::MouseButtonRelease) {
	return true;
    }

    // If we're clicking-and-holding, it's time to move
    if (m_e->type() == QEvent::MouseMove) {
	if (m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
	    if (BU_PTBL_LEN(&move_objs)) {
		for (size_t i = 0; i < BU_PTBL_LEN(&move_objs); i++) {
		    struct bv_scene_obj *mpoly = (struct bv_scene_obj *)BU_PTBL_GET(&move_objs, i);
		    bv_move_polygon(mpoly);
		}
	    } else {
		bv_move_polygon(wp);
	    }
	    emit view_updated(QG_VIEW_REFRESH);
	}
	return true;
    }

    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
