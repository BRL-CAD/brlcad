/*                      B I N D I N G S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file bindings.cpp
 *
 * Brief description
 *
 */

extern "C" {
#include "bview/defines.h"
#include "bview/util.h"
}

#include "bindings.h"

int CADkeyPressEvent(struct bview *v, int UNUSED(x_prev), int UNUSED(y_prev), QKeyEvent *UNUSED(k))
{

    if (!v)
	return 0;
    //QString kstr = QKeySequence(k->key()).toString();
    //bu_log("%s\n", kstr.toStdString().c_str());
#if 0
    switch (k->key()) {
	case '=':
	    m_renderer->res_incr();
	    emit renderRequested();
	    update();
	    return;
	    break;
	case '-':
	    m_renderer->res_decr();
	    emit renderRequested();
	    update();
	    return;
	    break;
    }
#endif
    return 0;

}

int CADmousePressEvent(struct bview *v, int UNUSED(x_prev), int UNUSED(y_prev), QMouseEvent *e)
{

    if (!v)
	return 0;

    // If we're intending the mouse motion to do the work,
    // then the press has to be a no-op.  If we're going
    // to do configurable key bindings, this will take some
    // thought - if we want unmodded left button to be a
    // rotation, and Ctrl+Left to do something else, these
    // checks are all going to have to be exact-flag-combo-only
    // actions.
    if (e->modifiers()) {
	return 0;
    }

    int dx = 1000;
    int dy = 0;
    unsigned long long view_flags = BVIEW_IDLE;

    if (e->buttons().testFlag(Qt::LeftButton)) {
	bu_log("Press Left\n");
	view_flags = BVIEW_SCALE;
	dy = -100;
    }
    if (e->buttons().testFlag(Qt::RightButton)) {
	bu_log("Press Right\n");
	view_flags = BVIEW_SCALE;
	dy = 100;
    }
    if (e->buttons().testFlag(Qt::MiddleButton)) {
	bu_log("Press Middle\n");
    }

    point_t center;
    MAT_DELTAS_GET_NEG(center, v->gv_center);
    return bview_adjust(v, dx, dy, center, BVIEW_VIEW, view_flags);

}

int CADmouseMoveEvent(struct bview *v, int x_prev, int y_prev, QMouseEvent *e)
{

    if (!v)
	return 0;

    unsigned long long view_flags = BVIEW_IDLE;

    if (x_prev == -INT_MAX) {
	x_prev = e->x();
	y_prev = e->y();
	return 0;
    }

    if (e->buttons().testFlag(Qt::LeftButton)) {
	bu_log("Left\n");

	if (e->modifiers().testFlag(Qt::ControlModifier)) {
	    bu_log("Ctrl+Left\n");
	    view_flags = BVIEW_ROT;
	}

	if (e->modifiers().testFlag(Qt::ShiftModifier)) {
	    bu_log("Shift+Left\n");
	    view_flags = BVIEW_TRANS;
	}

	if (e->modifiers().testFlag(Qt::ShiftModifier) && e->modifiers().testFlag(Qt::ControlModifier)) {
	    bu_log("Ctrl+Shift+Left\n");
	    view_flags = BVIEW_SCALE;
	}
    }

    if (e->buttons().testFlag(Qt::MiddleButton)) {
	bu_log("Middle\n");
    }

    if (e->buttons().testFlag(Qt::RightButton)) {
	bu_log("Right\n");
    }

    // Start following MGED's mouse motions to see how it handles view
    // updates.  The trail starts at doevent.c's motion_event_handler,
    // which in turn generates a command fed to f_knob.
    int dx = e->x() - x_prev;
    int dy = e->y() - y_prev;


    // TODO - the key point and the mode/flags are all hardcoded
    // right now, but eventually for shift grips they will need to
    // respond to the various mod keys.  The intent is to set flags
    // based on which mod keys are set to allow bview_adjust to
    // do the correct math.
    point_t center;
    MAT_DELTAS_GET_NEG(center, v->gv_center);
    return bview_adjust(v, dx, dy, center, BVIEW_VIEW, view_flags);

}

int CADwheelEvent(struct bview *v, QWheelEvent *e)
{

    if (!v)
	return 0;

    QPoint delta = e->angleDelta();
    int incr = delta.y() / 8;
    int dx = 1000;
    int dy = (incr > 0) ? 10 : -10;

    point_t origin = VINIT_ZERO;
    return bview_adjust(v, dx, dy, origin, BVIEW_VIEW, BVIEW_SCALE);

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

