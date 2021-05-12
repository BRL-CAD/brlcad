/*                      Q T G L . C P P
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
/** @file QtGL.cpp
 *
 * Use a QOpenGLWidget to display libdm drawing content.
 *
 */

#include <QOpenGLWidget>

extern "C" {
#include "bu/malloc.h"
}
#include "bindings.h"
#include "qtcad/QtGL.h"

// from GED_MIN and GED_MAX
#define QTGL_ZMIN -2048
#define QTGL_ZMAX 2047

QtGL::QtGL(QWidget *parent, struct fb *fbp)
    : QOpenGLWidget(parent), ifp(fbp)
{
    // View is provided from the GED structure (usually gedp->ged_gvp)
    v = NULL;

    // Don't dm_open until we have the view.
    dmp = NULL;

    // If we weren't supplied with a framebuffer, allocate one.
    // We don't open it until we have the dmp.
    if (!ifp) {
	ifp = fb_raw("/dev/qtgl");
	fb_set_standalone(ifp, 0);
    }

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

QtGL::~QtGL()
{
    if (dmp)
	dm_close(dmp);
    if (ifp && !fb_get_standalone(ifp)) {
	fb_close_existing(ifp);
    }
    BU_PUT(v, struct bv);
}


void QtGL::paintGL()
{
    int w = width();
    int h = height();
    // Zero size == nothing to do
    if (!w || !h)
	return;

    if (!m_init) {
	if (!v)
	    return;

	if (!dmp) {

	    // This is needed so we can work with Qt's OpenGL widget
	    // using standard OpenGL functions.
	    initializeOpenGLFunctions();

	    // Do the standard libdm attach to get our rendering backend.
	    const char *acmd = "attach";
	    dmp = dm_open((void *)this, NULL, "qtgl", 1, &acmd);
	    if (!dmp)
		return;

	    // If we have a framebuffer, now we can open it
	    if (ifp) {
		struct fb_platform_specific *fbps = fb_get_platform_specific(FB_QTGL_MAGIC);
		fbps->data = (void *)dmp;
		fb_setup_existing(ifp, dm_get_width(dmp), dm_get_height(dmp), fbps);
		fb_put_platform_specific(fbps);
	    }
	}

	dm_configure_win(dmp, 0);
	dm_set_pathname(dmp, "QTDM");
	dm_set_zbuffer(dmp, 1);

	// QTGL_ZMIN and QTGL_ZMAX are historical - need better
	// documentation on why those specific values are used.
	fastf_t windowbounds[6] = { -1, 1, -1, 1, QTGL_ZMIN, QTGL_ZMAX };
	dm_set_win_bounds(dmp, windowbounds);

	// Associate the view scale with the dmp
	dm_set_vp(dmp, &v->gv_scale);

	// Let the view know it now has an associated display manager
	v->dmp = dmp;

	// Set the view width and height to match the dm
	v->gv_width = dm_get_width(dmp);
	v->gv_height = dm_get_height(dmp);

	// If we have a ptbl defining the current dm set and/or an unset
	// pointer to indicate the current dm, go ahead and set them.
	if (dm_set)
	    bu_ptbl_ins_unique(dm_set, (long int *)dmp);
	if (dm_current && !(*dm_current))
	    (*dm_current) = dmp;

	// Ready to go
	m_init = true;
    }

    if (!m_init || !v || !dmp)
	return;

    // TODO - this clear color probably shouldn't be hardcoded...
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Go ahead and set the flag, but (unlike the rendering thread
    // implementation) we need to do the draw routine every time in paintGL, or
    // we end up with unrendered frames.
    dm_set_dirty(dmp, 0);

    matp_t mat = v->gv_model2view;
    dm_loadmatrix(dmp, mat, 0);
    if (base2local && local2base) {
	v->gv_local2base = *local2base;
	v->gv_base2local = *base2local;
    }
    dm_draw_objs(v, v->gv_base2local, v->gv_local2base);
    dm_draw_end(dmp);
}

void QtGL::resizeGL(int, int)
{
    if (dmp && v) {
	dm_configure_win(dmp, 0);
	v->gv_width = dm_get_width(dmp);
	v->gv_height = dm_get_height(dmp);
	dm_set_dirty(dmp, 1);
    }
}

void QtGL::keyPressEvent(QKeyEvent *k) {

    if (!dmp || !v) {
	QOpenGLWidget::keyPressEvent(k);
	return;
    }

    // Let bv know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();

    if (CADkeyPressEvent(v, x_prev, y_prev, k)) {
	dm_set_dirty(dmp, 1);
	update();
    }

    QOpenGLWidget::keyPressEvent(k);
}

void QtGL::mousePressEvent(QMouseEvent *e) {

    if (!dmp || !v) {
	QOpenGLWidget::mousePressEvent(e);
	return;
    }

    // Let bv know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();

    if (CADmousePressEvent(v, x_prev, y_prev, e)) {
	dm_set_dirty(dmp, 1);
	update();
    }

    bu_log("X,Y: %d, %d\n", e->x(), e->y());

    QOpenGLWidget::mousePressEvent(e);
}

void QtGL::mouseMoveEvent(QMouseEvent *e)
{
    if (!dmp || !v) {
	QOpenGLWidget::mouseMoveEvent(e);
	return;
    }

    // Let bv know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();

    if (CADmouseMoveEvent(v, x_prev, y_prev, e)) {
	dm_set_dirty(dmp, 1);
	update();
    }

    // Current positions are the new previous positions
    x_prev = e->x();
    y_prev = e->y();

    QOpenGLWidget::mouseMoveEvent(e);
}

void QtGL::wheelEvent(QWheelEvent *e) {

    if (!dmp || !v) {
	QOpenGLWidget::wheelEvent(e);
	return;
    }

    // Let bv know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();

    if (CADwheelEvent(v, e)) {
	dm_set_dirty(dmp, 1);
	update();
    }

    QOpenGLWidget::wheelEvent(e);
}

void QtGL::mouseReleaseEvent(QMouseEvent *e) {

    // To avoid an abrupt jump in scene motion the next time movement is
    // started with the mouse, after we release we return to the default state.
    x_prev = -INT_MAX;
    y_prev = -INT_MAX;

    QOpenGLWidget::mouseReleaseEvent(e);
}

void QtGL::save_image() {
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

