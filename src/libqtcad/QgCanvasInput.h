/*                   Q G C A N V A S I N P U T . H
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
/** @file QgCanvasInput.h
 *
 * Per-canvas CAD-specific mouse/keyboard binding handler.
 *
 * This class replaces the previous free-function API in bindings.h and
 * the global static drag-tracking maps in bindings.cpp.  Moving the state
 * inside a class instance eliminates cross-canvas interference when
 * multiple canvases are simultaneously active (Phase 3 of the libqtcad
 * refactor plan).
 */

#ifndef QGCANVASINPUT_H
#define QGCANVASINPUT_H

#include "common.h"

#include <unordered_map>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include "bsg/defines.h"

typedef void (*qgcanvas_bounds_update_t)(struct bsg_view *);

/**
 * Per-canvas input-binding handler.
 *
 * Each QgGL / QgSW canvas embeds one instance of this class (via
 * QgCanvasState::input).  The drag-tracking maps that were previously
 * global statics in bindings.cpp are now per-instance, so concurrent
 * canvases cannot interfere with each other.
 */
class QgCanvasInput {
public:
	QgCanvasInput()  = default;
	~QgCanvasInput() = default;

	int keyPressEvent(struct bsg_view *v, int x_prev, int y_prev,
	                  QKeyEvent *k);

	int mousePressEvent(struct bsg_view *v, int x_prev, int y_prev,
	                    QMouseEvent *e);

	int mouseReleaseEvent(struct bsg_view *v, double x_press, double y_press,
	                      int x_prev, int y_prev, QMouseEvent *e, int mode);

	int mouseMoveEvent(struct bsg_view *v, int x_prev, int y_prev,
	                   QMouseEvent *e, int mode);

	int wheelEvent(struct bsg_view *v, QWheelEvent *e);

private:
	void suspendDragBoundsUpdate(struct bsg_view *v);
	void restoreDragBoundsUpdate(struct bsg_view *v, int refresh_bounds);

	std::unordered_map<struct bsg_view *, qgcanvas_bounds_update_t> m_drag_bounds_updates;
	std::unordered_map<struct bsg_view *, long long>                m_drag_update_ts;

	static const long long s_drag_update_interval_ms = 16;
};

#endif /* QGCANVASINPUT_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
