/*                   Q G P O L Y F I L T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2026 United States Government as represented by
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
#include "bg/polygon.h"
#include "bsg.h"
#include "raytrace.h" // For finalize polygon sketch export functionality (TODO - need to move...)
}

#include "qtcad/QgPolyFilter.h"
#include "qtcad/QgSignalFlags.h"

bool
QgPolyFilter::close_polygon()
{
	// Close the general polygon - if that's what we're creating,
	// at this point it will still be open.
	struct bsg_polygon_record rec;
	if (bsg_polygon_record_get(polygon, &rec) && rec.first_contour_open) {
		if (!bsg_polygon_close(polygon)) {
			polygon = {0, 0};
			return false;
		}
	}

	return true;
}

bool
QgPolyCreateFilter::eventFilter(QObject *, QEvent *e)
{
	QMouseEvent *m_e = view_sync(e);
	if (!m_e)
		return false;

	struct bsg_view *v = view();
	if (!v)
		return false;

	// Handle Left Click
	if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {

		if (bsg_polygon_ref_is_null(polygon)) {

			bsg_screen_pt(&v->gv_point, v->gv_mouse_x, v->gv_mouse_y, v);

			polygon = bsg_create_view_polygon_ref(v, ptype, &v->gv_point);
			bsg_polygon_set_view(polygon, v);
			bsg_polygon_set_visual(polygon, &edge_color, &fill_color, fill_slope_x, fill_slope_y, fill_density, vZ, fill_poly ? 1 : 0);

			// It doesn't get a "proper" name until its finalized
			bsg_polygon_set_name(polygon, "_tmp_view_polygon");

			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}

		// If we don't have a polygon at this point, we're done - subsequent logic assumes it
		if (bsg_polygon_ref_is_null(polygon))
			return true;

		// If we are in the process of creating a general polygon, after the initial creation
		// left clicks will append new points
		struct bsg_polygon_record rec;
		if (bsg_polygon_record_get(polygon, &rec) && rec.type == BSG_POLYGON_GENERAL) {
			bsg_polygon_update(polygon, v, BSG_POLYGON_UPDATE_PT_APPEND);
			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}

		// When we're dealing with polygons stray left clicks shouldn't zoom - just
		// consume them if we're not using them above.
		return true;
	}

	if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::RightButton)) {
		// No-op if we're not in the process of creating a polygon
		if (bsg_polygon_ref_is_null(polygon))
			return true;

		// Non-general polygon creation doesn't use right click.
		struct bsg_polygon_record rec;
		if (!bsg_polygon_record_get(polygon, &rec) || rec.type != BSG_POLYGON_GENERAL)
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
		if (bsg_polygon_ref_is_null(polygon))
			return true;

		// General polygon creation doesn't use mouse movement.
		struct bsg_polygon_record rec;
		if (bsg_polygon_record_get(polygon, &rec) && rec.type == BSG_POLYGON_GENERAL)
			return true;

		// For every other polygon type, call the libbv update routine
		// with the view's x,y coordinates
		if (m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
			bsg_polygon_update(polygon, v, BSG_POLYGON_UPDATE_DEFAULT);
			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}
	}

	// For the constrained polygon shapes, we're done creating once we release
	// the mouse button (i.e. a "click, hold and move" creation paradigm)
	if (m_e->type() == QEvent::MouseButtonRelease) {

		// No-op if no current polygon is defined
		if (bsg_polygon_ref_is_null(polygon))
			return true;

		// General polygons are finalized by a right-click close, since
		// appending multiple points requires multiple mouse click-and-release
		// operations
		struct bsg_polygon_record rec;
		if (bsg_polygon_record_get(polygon, &rec) && rec.type == BSG_POLYGON_GENERAL)
			return true;

		// For all non-general polygons, mouse release is the signal
		// to finish up.
		finalize(true);
		polygon = {0, 0};

		return true;
	}

	return false;
}

void
QgPolyCreateFilter::finalize(bool)
{
	int icnt = 0;

	if (bsg_polygon_ref_is_null(polygon))
		return;

	if (!close_polygon())
		return;

	if (op == bg_None || bool_objs.empty()) {
		// No interactions, so we're keeping it - assign a proper name
		bsg_polygon_set_name(polygon, vname.c_str());
	}
	else {

		for (auto target : bool_objs) {
			icnt += bsg_polygon_csg_ref(target, polygon, op);
		}

		// When doing boolean operations, the convention is if there were one
		// or more interactions with other polygons, the original polygon is
		// not retained
		if (icnt || op == bg_Difference || op == bg_Intersection) {
			bsg_polygon_remove(polygon);
			polygon = {0, 0};
		}
		else {
			// No interactions, so we're keeping it - assign a proper name
			bsg_polygon_set_name(polygon, vname.c_str());
		}
	}

	emit view_updated(QG_VIEW_REFRESH);
	emit finalized((icnt > 0) ? true : false);
}

bool
QgPolyUpdateFilter::eventFilter(QObject *, QEvent *e)
{
	QMouseEvent *m_e = view_sync(e);
	if (!m_e)
		return false;

	// The update filter needs an active polygon to operate on
	if (bsg_polygon_ref_is_null(polygon))
		return false;

	// We don't want other stray mouse clicks to do something surprising
	if (m_e->type() == QEvent::MouseButtonPress || m_e->type() == QEvent::MouseButtonRelease) {
		return true;
	}

	if (m_e->type() == QEvent::MouseMove) {

		// General polygon creation doesn't use mouse movement.
		struct bsg_polygon_record rec;
		if (bsg_polygon_record_get(polygon, &rec) && rec.type == BSG_POLYGON_GENERAL)
			return true;

		// For every other polygon type, call the libbv update routine
		// with the view's x,y coordinates
		if (m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
			bsg_polygon_update(polygon, view(), BSG_POLYGON_UPDATE_DEFAULT);
			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}

	}

	return false;
}

bool
QgPolySelectFilter::eventFilter(QObject *, QEvent *e)
{
	QMouseEvent *m_e = view_sync(e);
	if (!m_e)
		return false;

	struct bsg_view *v = view();
	if (!v)
		return false;

	// Handle Left Click
	if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {
		/* Use typed polygon selection records rather than the legacy
		 * feature-table and ptbl compatibility path. */
		polygon = bsg_view_select_polygon_ref(v, &v->gv_point);
		if (bsg_polygon_ref_is_null(polygon))
			return true;
		struct bsg_polygon_record rec;
		if (bsg_polygon_record_get(polygon, &rec)) {
			ptype = rec.type;
			close_general_poly = rec.first_contour_open;
		}

		return true;
	}

	// We also don't want other stray mouse clicks to do something surprising
	if (m_e->type() == QEvent::MouseButtonPress || m_e->type() == QEvent::MouseButtonRelease)
		return true;

	return false;
}

bool
QgPolyPointFilter::eventFilter(QObject *, QEvent *e)
{
	QMouseEvent *m_e = view_sync(e);
	if (!m_e)
		return false;

	// The point filter needs an active general polygon to operate on
	if (bsg_polygon_ref_is_null(polygon) || ptype != BSG_POLYGON_GENERAL)
		return false;

	struct bsg_polygon_record rec;
	if (!bsg_polygon_record_get(polygon, &rec))
		return false;

	// If we have a Left release, clear point selection
	if (m_e->type() == QEvent::MouseButtonRelease) {
		bsg_polygon_update(polygon, view(), BSG_POLYGON_UPDATE_PT_SELECT_CLEAR);
		emit view_updated(QG_VIEW_REFRESH);
		return true;
	}

	// Left press selects a point
	if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {
		bsg_polygon_update(polygon, view(), BSG_POLYGON_UPDATE_PT_SELECT);
		emit view_updated(QG_VIEW_REFRESH);
		return true;
	}

	// We also don't want other stray mouse clicks to do something surprising
	if (m_e->type() == QEvent::MouseButtonPress || m_e->type() == QEvent::MouseButtonRelease)
		return true;

	// Handle Mouse Move - move selected point with a left button click-and-hold
	if (m_e->type() == QEvent::MouseMove) {
		if (rec.curr_point_i < 0) {
			// No selected point
			return true;
		}
		if (m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
			bsg_polygon_update(polygon, view(), BSG_POLYGON_UPDATE_PT_MOVE);
			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}

		return true;
	}

	return false;
}

bool
QgPolyMoveFilter::eventFilter(QObject *, QEvent *e)
{
	QMouseEvent *m_e = view_sync(e);
	if (!m_e)
		return false;

	struct bsg_view *v = view();
	if (!v)
		return false;

	// The move filter needs an active polygon to operate on
	if (bsg_polygon_ref_is_null(polygon) && move_objs.empty())
		return false;

	// We don't want other stray mouse clicks to do something surprising
	if (m_e->type() == QEvent::MouseButtonPress || m_e->type() == QEvent::MouseButtonRelease) {
		VMOVE(v->gv_prev_point, v->gv_point);
		return true;
	}

	// If we're clicking-and-holding, it's time to move
	if (m_e->type() == QEvent::MouseMove) {
		if (m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
			if (!move_objs.empty()) {
				for (auto mpoly : move_objs)
					bsg_polygon_move(mpoly, &v->gv_point, &v->gv_prev_point);
			}
			else {
				bsg_polygon_move(polygon, &v->gv_point, &v->gv_prev_point);
			}
			emit view_updated(QG_VIEW_REFRESH);
		}
		VMOVE(v->gv_prev_point, v->gv_point);
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
