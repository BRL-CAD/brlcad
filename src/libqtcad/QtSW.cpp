/*                          Q T S W . C P P
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
/** @file QtSW.cpp
 *
 * Qt widget for visualizing libosmesa OpenGL software rasterizer output.
 */

#define USE_MGL_NAMESPACE 1

#include <QImage>
#include <QPainter>

extern "C" {
#include "bu/malloc.h"
}
#include "bindings.h"
#include "qtcad/QtSW.h"

QtSW::QtSW(QWidget *parent, struct fb *fbp)
    : QWidget(parent), ifp(fbp)
{
    // View is provided from the GED structure (usually gedp->ged_gvp)
    v = NULL;

    // Don't dm_open until we have the view.
    dmp = NULL;

    // If we weren't supplied with a framebuffer, allocate one.
    // We don't open it until we have the dmp.
    if (!ifp) {
	ifp = fb_raw("swrast");
	fb_set_standalone(ifp, 0);
    }

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

QtSW::~QtSW()
{
    if (dmp)
	dm_close(dmp);
    if (ifp && !fb_get_standalone(ifp)) {
	fb_close_existing(ifp);
    }
    BU_PUT(v, struct bv);
}

void QtSW::need_update()
{
    dm_set_dirty(dmp, 1);
    update();
}

void QtSW::paintEvent(QPaintEvent *e)
{
    // Go ahead and set the flag, but (unlike the rendering thread
    // implementation) we need to do the draw routine every time in paintGL, or
    // we end up with unrendered frames.
    dm_set_dirty(dmp, 0);

    if (!m_init) {
	if (!v)
	    return;
	if (!dmp) {
	    // swrast will need to know the window size
	    v->gv_width = width();
	    v->gv_height = height();

	    // Do the standard libdm attach to get our rendering backend.
	    const char *acmd = "attach";
	    dmp = dm_open((void *)v, NULL, "swrast", 1, &acmd);
	    if (!dmp)
		return;

	    // Let dmp know what the app level widget is (needed so we can
	    // connect framebuffer drawing events to the widget redraw logic.)
	    dm_set_udata(dmp, this);

	    // If we have a framebuffer, now we can open it
	    if (ifp) {
		struct fb_platform_specific *fbps = fb_get_platform_specific(FB_QTGL_MAGIC);
		fbps->data = (void *)dmp;
		fb_setup_existing(ifp, dm_get_width(dmp), dm_get_height(dmp), fbps);
		fb_put_platform_specific(fbps);
	    }
	}

	dm_configure_win(dmp, 0);
	dm_set_pathname(dmp, "SWDM");
	dm_set_zbuffer(dmp, 1);

	// Using the full GED_MIN/GED_MAX was causing drawing artifacts with moss I
	// in shaded mode (I think I was seeing the "Z-fighting" problem:
	// https://www.sjbaker.org/steve/omniv/love_your_z_buffer.html )
	//
	// Setting to (-1,1) clips geometry too quickly as we start to zoom in.
	// -100,100 seems to work, but may need a better long term solution to
	// this... maybe basing it on the currently visible object bounds?
	fastf_t windowbounds[6] = { -1, 1, -1, 1, -100, 100 };
	dm_set_win_bounds(dmp, windowbounds);

	// Associate the view scale with the dmp
	dm_set_vp(dmp, &v->gv_scale);

	// Let the view know it has an associated dm.
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

    matp_t mat = v->gv_model2view;
    dm_loadmatrix(dmp, mat, 0);
    dm_draw_begin(dmp);
    if (base2local && local2base) {
	v->gv_local2base = *local2base;
	v->gv_base2local = *base2local;
    }
    dm_draw_objs(v, v->gv_base2local, v->gv_local2base);
    dm_draw_end(dmp);

    // Set up a QImage with the rendered output..
    unsigned char *dm_image;
    if (dm_get_display_image(dmp, &dm_image, 1, 1)) {
	return;
    }
    QImage image(dm_image, dm_get_width(dmp), dm_get_height(dmp), QImage::Format_RGBA8888);
    QImage img32 = image.convertToFormat(QImage::Format_RGB32);
    QPainter painter(this);
    painter.drawImage(this->rect(), img32);
    bu_free(dm_image, "copy of backend image");
    QWidget::paintEvent(e);
}

void QtSW::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (dmp && v) {
	dm_set_width(dmp, width());
	dm_set_height(dmp, height());
	v->gv_width = width();
	v->gv_height = height();
	dm_configure_win(dmp, 0);
	dm_set_dirty(dmp, 1);
    }
}

void QtSW::keyPressEvent(QKeyEvent *k) {

    if (!dmp || !v) {
	QWidget::keyPressEvent(k);
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

    QWidget::keyPressEvent(k);
}

void QtSW::mousePressEvent(QMouseEvent *e) {

    if (!dmp || !v) {
	QWidget::mousePressEvent(e);
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

    QWidget::mousePressEvent(e);
}

void QtSW::mouseMoveEvent(QMouseEvent *e)
{
    if (!dmp || !v) {
	QWidget::mouseMoveEvent(e);
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

    QWidget::mouseMoveEvent(e);
}

void QtSW::wheelEvent(QWheelEvent *e) {

    if (!dmp || !v) {
	QWidget::wheelEvent(e);
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

    QWidget::wheelEvent(e);
}

void QtSW::mouseReleaseEvent(QMouseEvent *e) {

    // To avoid an abrupt jump in scene motion the next time movement is
    // started with the mouse, after we release we return to the default state.
    x_prev = -INT_MAX;
    y_prev = -INT_MAX;

    QWidget::mouseReleaseEvent(e);
}

void QtSW::save_image() {
    // Set up a QImage with the rendered output..
    unsigned char *dm_image;
    if (dm_get_display_image(dmp, &dm_image, 1, 1)) {
	return;
    }
    QImage image(dm_image, dm_get_width(dmp), dm_get_height(dmp), QImage::Format_RGBA8888);
    QImage img32 = image.convertToFormat(QImage::Format_RGB32);
    img32.save("file.png");
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

