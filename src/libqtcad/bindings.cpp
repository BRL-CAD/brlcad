/*                      B I N D I N G S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2022 United States Government as represented by
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

#include "common.h"

extern "C" {
#include "bn/str.h"
#include "bv/defines.h"
#include "bv/util.h"
}

#include "bindings.h"

// TODO - look into QShortcut, see if it might be a better way
// to manage this
int CADkeyPressEvent(struct bview *v, int UNUSED(x_prev), int UNUSED(y_prev), QKeyEvent *k)
{
    if (!v)
	return 0;
#if 0
    QString kstr = QKeySequence(k->key()).toString();
    bu_log("%s\n", kstr.toStdString().c_str());
#endif
    switch (k->key()) {
	case 'A':
	    v->gv_s->gv_adc.draw = !v->gv_s->gv_adc.draw;
	    return 1;
	case 'M':
	    v->gv_s->gv_model_axes.draw = !v->gv_s->gv_model_axes.draw;
	    return 1;
	case 'V':
	    v->gv_s->gv_view_axes.draw = !v->gv_s->gv_view_axes.draw;
	    return 1;
	case '2':
	    bn_decode_vect(v->gv_aet, "35 -25 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case '3':
	    bn_decode_vect(v->gv_aet, "35 25 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case '4':
	    bn_decode_vect(v->gv_aet, "45 45 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case '5':
	    bn_decode_vect(v->gv_aet, "145 25 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case '6':
	    bn_decode_vect(v->gv_aet, "215 25 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case '7':
	    bn_decode_vect(v->gv_aet, "325 25 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case 'F':
	    bn_decode_vect(v->gv_aet, "0 0 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case 'T':
	    bn_decode_vect(v->gv_aet, "270 90 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case 'B':
	    bn_decode_vect(v->gv_aet, "270 -90 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case 'L':
	    bn_decode_vect(v->gv_aet, "90 0 0");
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	case 'R':
	    if (k->modifiers().testFlag(Qt::ShiftModifier) == true) {
		bn_decode_vect(v->gv_aet, "180 0 0");
	    } else {
		bn_decode_vect(v->gv_aet, "270 0 0");
	    }
	    bv_mat_aet(v);
	    bv_update(v);
	    return 1;
	default:
	    break;
    }
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

    if (e->buttons().testFlag(Qt::LeftButton)) {
	bu_log("Press Left\n");
    }
    if (e->buttons().testFlag(Qt::RightButton)) {
	bu_log("Press Right\n");
    }
    if (e->buttons().testFlag(Qt::MiddleButton)) {
	bu_log("Press Middle\n");
    }

    return 0;
}

int CADmouseReleaseEvent(struct bview *v, double x_press, double y_press, int UNUSED(x_prev), int UNUSED(y_prev), QMouseEvent *e, int mode)
{

    if (!v)
	return 0;

    // If we're intending the mouse motion to do the work,
    // then the release has to be a no-op.  If we're going
    // to do configurable key bindings, this will take some
    // thought - if we want unmodded left button to be a
    // rotation, and Ctrl+Left to do something else, these
    // checks are all going to have to be exact-flag-combo-only
    // actions.
    if (e->modifiers()) {
	return 0;
    }
    if (e->buttons().testFlag(Qt::LeftButton) || e->buttons().testFlag(Qt::RightButton)) {
	return 0;
    }

    double cx, cy;
#ifdef USE_QT6
    cx = e->position().x();
    cy = e->position().y();
#else
    cx = (double)e->x();
    cy = (double)e->y();
#endif
    if ((fabs(cx - x_press) > 10) || (fabs(cy - y_press) > 10))
	return 0;

    int dx = 1;
    int dy = 1;
    unsigned long long view_flags = BV_IDLE;

    if (e->button() == Qt::LeftButton) {
	bu_log("Release Left\n");
	if (mode != BV_CENTER) {
	    view_flags = BV_SCALE;
	    dx = 10;
	    dy = 5;
	} else {
	    view_flags = BV_CENTER;
	    dx = (int)cx;
	    dy = (int)cy;
	}
    }
    if (e->button() == Qt::RightButton) {
	bu_log("Release Right\n");
	if (mode == BV_CENTER)
	    return 0;
	view_flags = BV_SCALE;
	dx = 1;
	dy = 2;
    }

    if (e->button() == Qt::MiddleButton) {
	bu_log("Release Center\n");
	view_flags = BV_CENTER;
	dx = (int)cx;
	dy = (int)cy;
    }

    point_t keypt = VINIT_ZERO;
    return bv_adjust(v, dx, dy, keypt, 0, view_flags);
}

int CADmouseMoveEvent(struct bview *v, int x_prev, int y_prev, QMouseEvent *e, int mode)
{

    if (!v)
	return 0;

    unsigned long long view_flags = BV_IDLE;

    if (x_prev == -INT_MAX) {
	//x_prev = e->x();
	//y_prev = e->y();
	return 0;
    }

    view_flags = mode;
    if (mode == BV_CENTER)
	view_flags = BV_SCALE;

    if (e->buttons().testFlag(Qt::LeftButton)) {
	bu_log("Left\n");

	if (e->modifiers().testFlag(Qt::ControlModifier)) {
	    bu_log("Ctrl+Left\n");
	    view_flags = BV_ROT;
	}

	if (e->modifiers().testFlag(Qt::ShiftModifier)) {
	    bu_log("Shift+Left\n");
	    view_flags = BV_TRANS;
	}

	if (e->modifiers().testFlag(Qt::ShiftModifier) && e->modifiers().testFlag(Qt::ControlModifier)) {
	    bu_log("Ctrl+Shift+Left\n");
	    view_flags = BV_SCALE;
	}
    }

    if (e->buttons().testFlag(Qt::MiddleButton)) {
	bu_log("Middle\n");
    }

    if (e->buttons().testFlag(Qt::RightButton)) {
	bu_log("Right\n");
    }

    if (!e->buttons().testFlag(Qt::LeftButton))
	return 0;

#ifdef USE_QT6
    int dx = e->position().x() - x_prev;
    int dy = e->position().y() - y_prev;
#else
    int dx = e->x() - x_prev;
    int dy = e->y() - y_prev;
#endif

    if (view_flags == BV_SCALE) {
	// Build in some sensitivity to how much the mouse moved when doing
	// a motion based scale
	int mdelta = (abs(dx) > abs(dy)) ? dx : -dy;
	int f = (int)(2*100*(double)abs(mdelta)/(double)v->gv_height);

	if (mdelta > 0) {
	    dy = 101 + f;
	    dx = 100;
	} else {
	    dy = 99 - f;
	    dx = 100;
	}
    }


    // TODO - the key point and the mode/flags are all hardcoded
    // right now, but eventually for shift grips they will need to
    // respond to the various mod keys.  The intent is to set flags
    // based on which mod keys are set to allow bv_adjust to
    // do the correct math.
    point_t center;
    MAT_DELTAS_GET_NEG(center, v->gv_center);
    return bv_adjust(v, dx, dy, center, 0, view_flags);

}

int CADwheelEvent(struct bview *v, QWheelEvent *e)
{

    if (!v)
	return 0;

    QPoint delta = e->angleDelta();
    int mdelta = -1* delta.y() / 8;

    int dx = 100 + mdelta;
    int dy = 100;

    point_t origin = VINIT_ZERO;
    return bv_adjust(v, dx, dy, origin, 0, BV_SCALE);

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

