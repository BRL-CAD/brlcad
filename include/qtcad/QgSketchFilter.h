/*                   Q G S K E T C H F I L T E R . H
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file QgSketchFilter.h
 *
 * Qt mouse-event filters for interactive 2-D sketch editing.
 *
 * These filters are designed to be installed on a QgView (or its
 * underlying canvas widget) via QgView::add_event_filter().  Each
 * filter encapsulates one editing mode and drives the primitive edit
 * API (rt_edit / ECMD_SKETCH_* commands) defined in
 * include/rt/rt_ecmds.h and include/rt/edit.h.
 *
 * Typical usage sequence
 * ----------------------
 *  1. Create a QgView and obtain its struct bview * via view().
 *  2. Call rt_edit_create() to open a sketch primitive for editing.
 *  3. Instantiate whichever filter(s) you need, setting the .v and
 *     .es public members.
 *  4. Install the active filter:  view->add_event_filter(filter);
 *  5. Connect filter->view_updated(int) to your view's need_update(ull)
 *     slot, and filter->sketch_changed() to whatever slot performs a
 *     wireframe refresh.
 *  6. To switch mode, clear the old filter and install the new one.
 *
 * The demo program src/libqtcad/tests/qsketch.cpp shows a complete
 * integration.
 */

#ifndef QGSKETCHFILTER_H
#define QGSKETCHFILTER_H

#include "common.h"

extern "C" {
#include "vmath.h"
#include "bv.h"
#include "rt/edit.h"
#include "rt/primitives/sketch.h"
#include "rt/rt_ecmds.h"
}

#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include "qtcad/defines.h"

/**
 * QgSketchFilter — base class for sketch editing mouse-event filters.
 *
 * Provides common state and helper methods shared by all sketch editing
 * filter modes.  Follows the same design pattern as QgPolyFilter and
 * QgMeasureFilter.
 */
class QTCAD_EXPORT QgSketchFilter : public QObject
{
    Q_OBJECT

public:
    /**
     * Synchronise Qt mouse event with the bview coordinate state.
     *
     * Updates v->gv_mouse_x/y, v->gv_prevMouseX/Y, and v->gv_point.
     * Returns the cast QMouseEvent on success, NULL if the event is
     * not a mouse event or if a non-None keyboard modifier is active
     * (modifier keys are typically used for view navigation, not editing).
     */
    QMouseEvent *view_sync(QEvent *e);

    /**
     * Convert screen pixel coordinates to normalised view coordinates.
     *
     * Output is in the range roughly [-1, +1] x [-1, +1] (the exact
     * range depends on the view aspect ratio), matching the coordinate
     * space expected by EDOBJ[].ft_edit_xy().
     */
    void screen_to_view(int sx, int sy, vect_t mvec) const;

    /**
     * Convert screen pixel coordinates to sketch-plane UV coordinates.
     *
     * Unprojects the screen position to a 3-D model-space point (on the
     * view-centre plane, z=0 in view space) and then projects it onto
     * the sketch's coordinate system using the sketch's V origin, u_vec
     * and v_vec unit vectors.
     *
     * @param[in]  sx    screen X in pixels (left = 0)
     * @param[in]  sy    screen Y in pixels (top  = 0)
     * @param[out] u_out U coordinate in model base units
     * @param[out] v_out V coordinate in model base units
     * @return true on success; false if v or es is NULL
     */
    bool screen_to_uv(int sx, int sy, fastf_t *u_out, fastf_t *v_out) const;

    /**
     * Convert screen pixel coordinates to sketch UV, optionally snapping
     * to the nearest existing sketch vertex if its projected view-space
     * distance is within snap_px pixels.
     *
     * When snapping, *snapped_idx is set to the vertex index that was
     * snapped to (or -1 if no snap occurred).  Pass NULL for snapped_idx
     * if you don't need that information.
     *
     * @param snap_px     Snap radius in screen pixels (0 disables snapping)
     * @param snapped_idx Output: index of snapped vertex, or -1
     */
    bool snap_vertex_uv(int sx, int sy,
			fastf_t *u_out, fastf_t *v_out,
			fastf_t snap_px = 10.0,
			int *snapped_idx = NULL) const;

    /* Make eventFilter virtual so callers can store a base-class pointer
     * and swap derived implementations without an explicit cast. */
    virtual bool eventFilter(QObject *, QEvent *) { return false; }

signals:
    /**
     * Emitted when the view image needs refreshing (e.g. after a
     * vertex move that should be reflected immediately in the viewport).
     * The integer argument carries QG_VIEW_* flag bits from
     * QgSignalFlags.h; typically QG_VIEW_REFRESH.
     */
    void view_updated(int);

    /**
     * Emitted when the sketch data has been structurally changed (a
     * vertex moved, segment added/deleted, etc.) and the caller should
     * rebuild whatever higher-level representations depend on it (e.g.
     * regenerate wireframe, update vertex/segment tables).
     */
    void sketch_changed();

public:
    /** The bview the filter is watching.  Must be set before install. */
    struct bview    *v  = NULL;

    /** The rt_edit context managing the sketch being edited.
     *  Must be set before install.  The filter does NOT take ownership. */
    struct rt_edit  *es = NULL;
};


/**
 * QgSketchPickVertexFilter — left-click to select the nearest vertex.
 *
 * On left button press the filter uses view-space proximity to find
 * and select the closest vertex (ECMD_SKETCH_PICK_VERTEX with the
 * v_pos / v_pos_valid path in edsketch.c).
 *
 * After a successful pick sketch_changed() is emitted so callers can
 * update selection highlighting in their tables/views.
 */
class QTCAD_EXPORT QgSketchPickVertexFilter : public QgSketchFilter
{
    Q_OBJECT

public:
    bool eventFilter(QObject *, QEvent *e);
};


/**
 * QgSketchMoveVertexFilter — left-drag to move the currently selected vertex.
 *
 * Requires that a vertex has already been selected (es->ipe_ptr's
 * curr_vert >= 0, typically set by QgSketchPickVertexFilter).
 *
 * On left button press the mouse position is recorded.  While the
 * left button is held and the mouse moves the selected vertex is
 * repositioned to the cursor's sketch-plane UV coordinates using
 * ECMD_SKETCH_MOVE_VERTEX.  Both view_updated and sketch_changed are
 * emitted during the drag.
 */
class QTCAD_EXPORT QgSketchMoveVertexFilter : public QgSketchFilter
{
    Q_OBJECT

public:
    bool eventFilter(QObject *, QEvent *e);

private:
    bool m_dragging = false;
};


/**
 * QgSketchAddVertexFilter — left-click to add a new vertex.
 *
 * Each left button press adds a vertex at the cursor's sketch-plane UV
 * position using ECMD_SKETCH_ADD_VERTEX and selects it (curr_vert is
 * updated).  The newly placed vertex index is reported via
 * sketch_changed() so the application can update its vertex table.
 *
 * This mode is the building block for interactive segment creation:
 * the application places vertices and then calls an append command.
 */
class QTCAD_EXPORT QgSketchAddVertexFilter : public QgSketchFilter
{
    Q_OBJECT

public:
    bool eventFilter(QObject *, QEvent *e);
};


/**
 * QgSketchPickSegmentFilter — left-click to select a segment by proximity.
 *
 * Iterates over the sketch's curve segments, samples each one at a
 * fixed number of points, and selects the segment whose projected
 * view-space sample is closest to the mouse cursor.  Uses
 * ECMD_SKETCH_PICK_SEGMENT (index path) once the nearest segment is
 * identified.
 *
 * After a successful pick sketch_changed() is emitted.
 */
class QTCAD_EXPORT QgSketchPickSegmentFilter : public QgSketchFilter
{
    Q_OBJECT

public:
    bool eventFilter(QObject *, QEvent *e);
};


/**
 * QgSketchMoveSegmentFilter — left-drag to translate the selected segment.
 *
 * Requires that a segment has already been selected (curr_seg >= 0).
 * On each mouse-move event while the left button is held the delta
 * between consecutive cursor positions is translated to UV coordinates
 * and applied via ECMD_SKETCH_MOVE_SEGMENT.
 */
class QTCAD_EXPORT QgSketchMoveSegmentFilter : public QgSketchFilter
{
    Q_OBJECT

public:
    bool eventFilter(QObject *, QEvent *e);

private:
    bool    m_dragging = false;
    fastf_t m_prev_u   = 0.0;
    fastf_t m_prev_v   = 0.0;
};


/**
 * QgSketchArcRadiusFilter — left-drag to resize the selected arc's radius.
 *
 * Requires that a CARC segment has already been selected (curr_seg >= 0
 * and the segment at that index must have CURVE_CARC_MAGIC).
 *
 * On left button press the arc centre is computed from the current sketch
 * geometry (perpendicular bisector for partial arcs; end vertex for full
 * circles).  While the left button is held the Euclidean distance from the
 * cursor to the stored arc centre is used as the new radius, applied via
 * ECMD_SKETCH_SET_ARC_RADIUS on every mouse-move event.  For full-circle
 * arcs (radius < 0) the negative-radius convention is preserved.
 *
 * Both view_updated and sketch_changed are emitted during the drag.
 */
class QTCAD_EXPORT QgSketchArcRadiusFilter : public QgSketchFilter
{
    Q_OBJECT

public:
    bool eventFilter(QObject *, QEvent *e);

private:
    bool    m_dragging  = false;
    fastf_t m_center_u  = 0.0;
    fastf_t m_center_v  = 0.0;
    bool    m_full_circle = false; /* preserve sign for full-circle arcs */
};


/**
 * QgSketchCursorTracker — passive mouse-move tracker for UV coordinate display.
 *
 * Install this filter alongside (not instead of) the active editing filter.
 * It watches MouseMove events, computes the cursor's sketch-plane UV
 * position via screen_to_uv(), and emits uv_moved().  The event is
 * never consumed (eventFilter always returns false) so it does not
 * interfere with other filters.
 *
 * Typical usage: connect uv_moved to a QLabel in the status bar to provide
 * a live cursor coordinate readout.
 */
class QTCAD_EXPORT QgSketchCursorTracker : public QgSketchFilter
{
    Q_OBJECT

public:
    bool eventFilter(QObject *, QEvent *e);

signals:
    /**
     * Emitted on every mouse-move event with the current cursor position
     * in sketch-plane coordinates (model base units, e.g. mm).
     */
    void uv_moved(double u, double v);
};


/**
 * QgSketchSetTangencyFilter — left-click to select an adjacent segment
 * and make the currently selected arc tangent to it at their shared vertex.
 *
 * Workflow:
 *   1. Install this filter when the user activates tangency mode.
 *   2. The filter picks the nearest segment to the click position (same
 *      proximity logic as QgSketchPickSegmentFilter).
 *   3. It then fires ECMD_SKETCH_SET_TANGENCY with:
 *        e_para[0] = index of the clicked adjacent segment
 *        e_para[1] = tangency_angle (in radians, default 0 = smooth join)
 *   4. Emits sketch_changed() and view_updated(QG_VIEW_REFRESH).
 *   5. Deactivates itself after one successful pick (single-shot).
 *
 * The user can change tangency_angle via the public field before
 * installing the filter.
 */
class QTCAD_EXPORT QgSketchSetTangencyFilter : public QgSketchFilter
{
    Q_OBJECT

public:
    /** Tangency angle in radians (0 = smooth / G1 join). */
    fastf_t tangency_angle = 0.0;

    bool eventFilter(QObject *, QEvent *e);
};


#endif /* QGSKETCHFILTER_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
