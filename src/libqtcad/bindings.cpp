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
int cadkey_press_event(struct bview *v, int UNUSED(x_prev), int UNUSED(y_prev), QKeyEvent *k)
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

int cad_mouse_press_event(struct bview *v, int UNUSED(x_prev), int UNUSED(y_prev), QMouseEvent *e)
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
    unsigned long long view_flags = BV_IDLE;

    if (e->buttons().testFlag(Qt::LeftButton)) {
	bu_log("Press Left\n");
	view_flags = BV_SCALE;
	dy = -100;
    }
    if (e->buttons().testFlag(Qt::RightButton)) {
	bu_log("Press Right\n");
	view_flags = BV_SCALE;
	dy = 100;
    }
    if (e->buttons().testFlag(Qt::MiddleButton)) {
	bu_log("Press Middle\n");
    }

    point_t center;
    MAT_DELTAS_GET_NEG(center, v->gv_center);
    return bv_adjust(v, dx, dy, center, 0, view_flags);

}

int cad_mouse_move_event(struct bview *v, int x_prev, int y_prev, QMouseEvent *e)
{

    if (!v)
	return 0;

    unsigned long long view_flags = BV_IDLE;

    if (x_prev == -INT_MAX) {
	//x_prev = e->x();
	//y_prev = e->y();
	return 0;
    }

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

#ifdef USE_QT6
    int dx = e->position().x() - x_prev;
    int dy = e->position().y() - y_prev;
#else
    int dx = e->x() - x_prev;
    int dy = e->y() - y_prev;
#endif

    if (view_flags == BV_SCALE) {
	int mdelta = (abs(dx) > abs(dy)) ? dx : -dy;
	double f = (double)mdelta/(double)v->gv_height;
	dy = (int)(2 * f * 1000);
	dx = 1000;
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

int cad_wheel_event(struct bview *v, QWheelEvent *e)
{

    if (!v)
	return 0;

    QPoint delta = e->angleDelta();
    int incr = delta.y() / 8;
    int dx = 1000;
    int dy = (incr > 0) ? 20 : -20;

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

