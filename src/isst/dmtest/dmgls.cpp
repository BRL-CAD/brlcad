/*                      D M G L S . C P P
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
/** @file dmgls.cpp
 *
 * Brief description
 *
 */

#include <QOpenGLWidget>
#include <QKeyEvent>
#include <QGuiApplication> // for qGuiApp

extern "C" {
#include "bu/parallel.h"
#include "dm/bview_util.h"
#include "ged.h"
}
#include "dmgls.h"

dmGL::dmGL(QWidget *parent)
    : QOpenGLWidget(parent)
{
    BU_GET(v, struct bview);
    bview_init(v);

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

dmGL::~dmGL()
{
    BU_PUT(v, struct bview);
}


void dmGL::paintGL()
{
    int w = width();
    int h = height();
    // Zero size == nothing to do
    if (!w || !h)
	return;

    if (!gedp) {
	return;
    }

    if (!m_init) {
	initializeOpenGLFunctions();
	m_init = true;
	if (!dmp) {
	    const char *acmd = "attach";
	    dmp = dm_open((void *)this, NULL, "qtgl", 1, &acmd);
	    if (gedp) {
		gedp->ged_dmp = (void *)dmp;
		dm_set_vp(dmp, &gedp->ged_gvp->gv_scale);
	    }
	    dm_configure_win(dmp, 0);
	}
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Go ahead and set the flag, but (unlike the rendering thread
    // implementation) we need to do the draw routine every time in paintGL, or
    // we end up with unrendered frames.
    dm_set_dirty(dmp, 0);

    if (gedp) {
	matp_t mat = gedp->ged_gvp->gv_model2view;
	dm_loadmatrix(dmp, mat, 0);
	unsigned char geometry_default_color[] = { 255, 0, 0 };
	dm_draw_display_list(dmp, gedp->ged_gdp->gd_headDisplay,
		1.0, gedp->ged_gvp->gv_isize, 255, 0, 0, 1,
		0, 0, geometry_default_color, 1, 0);
    }
}

void dmGL::resizeGL(int, int)
{
    if (gedp && gedp->ged_dmp) {
	dm_configure_win((struct dm *)gedp->ged_dmp, 0);
	dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
    }
}

void dmGL::keyPressEvent(QKeyEvent *k) {
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
    QOpenGLWidget::keyPressEvent(k);
}

void dmGL::mousePressEvent(QMouseEvent *e) {

    // If we're intending the mouse motion to do the work,
    // then the press has to be a no-op.  If we're going
    // to do configurable key bindings, this will take some
    // thought - if we want unmodded left button to be a
    // rotation, and Ctrl+Left to do something else, these
    // checks are all going to have to be exact-flag-combo-only
    // actions.
    if (e->modifiers()) {
	QOpenGLWidget::mousePressEvent(e);
	return;
    }

    int dx = 0;
    int dy = 0;
    unsigned long long view_flags = BVIEW_IDLE;

    // Let bview know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();

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
    VSCALE(center, center, gedp->ged_wdbp->dbip->dbi_base2local);
    if (bview_adjust(v, dx, dy, center, BVIEW_VIEW, view_flags)) {
	dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	update();
    }


    QOpenGLWidget::mousePressEvent(e);
}

void dmGL::mouseMoveEvent(QMouseEvent *e)
{
    unsigned long long view_flags = BVIEW_IDLE;

    if (x_prev == -INT_MAX) {
	x_prev = e->x();
	y_prev = e->y();
	QOpenGLWidget::mouseMoveEvent(e);
	return;
    }

    if (!gedp || !gedp->ged_dmp) {
	QOpenGLWidget::mouseMoveEvent(e);
	return;
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

    // Let bview know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();

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
    VSCALE(center, center, gedp->ged_wdbp->dbip->dbi_base2local);
    if (bview_adjust(v, dx, dy, center, BVIEW_VIEW, view_flags)) {
	 dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	 update();
    }

    // Current positions are the new previous positions
    x_prev = e->x();
    y_prev = e->y();

    QOpenGLWidget::mouseMoveEvent(e);
}

void dmGL::wheelEvent(QWheelEvent *e) {
    QPoint delta = e->angleDelta();
    int incr = delta.y() / 8;

    // Let bview know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();

    point_t origin = VINIT_ZERO;
    if (bview_adjust(v, 0, incr, origin, BVIEW_VIEW, BVIEW_SCALE)) {
	 dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	 update();
    }

    QOpenGLWidget::wheelEvent(e);
}

void dmGL::mouseReleaseEvent(QMouseEvent *e) {

    // To avoid an abrupt jump in scene motion the next time movement is
    // started with the mouse, after we release we return to the default state.
    x_prev = -INT_MAX;
    y_prev = -INT_MAX;

    QOpenGLWidget::mouseReleaseEvent(e);
}

void dmGL::save_image() {
    QImage image = this->grabFramebuffer();
    image.save("file.png");
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

