/*                          Q G S W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2024 United States Government as represented by
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
/** @file QgSW.cpp
 *
 * Qt widget for visualizing libosmesa OpenGL software rasterizer output.
 */

#define USE_MGL_NAMESPACE 1

#include "common.h"

#include <QImage>
#include <QPainter>
#include <QtGlobal>

extern "C" {
#include "bu/malloc.h"
}
#include "bindings.h"
#include "qtcad/QgSW.h"

// Using the full GED_MIN/GED_MAX was causing drawing artifacts with moss I
// in shaded mode (I think I was seeing the "Z-fighting" problem:
// https://www.sjbaker.org/steve/omniv/love_your_z_buffer.html )
//
// Setting to (-1,1) clips geometry too quickly as we start to zoom in.
// -100,100 seems to work, but may need a better long term solution to
// this... maybe basing it on the currently visible object bounds?
#define QTSW_ZMIN -100
#define QTSW_ZMAX 100

QgSW::QgSW(QWidget *parent, struct fb *fbp)
    : QWidget(parent), ifp(fbp)
{
    // Provide a view specific to this widget - set gedp->ged_gvp to v
    // if this is the current view
    BU_GET(local_v, struct bview);
    bv_init(local_v, NULL);
    bu_vls_sprintf(&local_v->gv_name, "swrast");
    v = local_v;

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

QgSW::~QgSW()
{
    if (dmp)
	dm_close(dmp);
    if (ifp && !fb_get_standalone(ifp)) {
	fb_close_existing(ifp);
    }
    BU_PUT(local_v, struct bv);
}

void QgSW::need_update()
{
    QTCAD_SLOT("QgSW::need_update", 1);
    dm_set_dirty(dmp, 1);
    update();
}

void QgSW::paintEvent(QPaintEvent *e)
{
    // Go ahead and set the flag, but (unlike the rendering thread
    // implementation) we need to do the draw routine every time in paintGL, or
    // we end up with unrendered frames.
    dm_set_dirty(dmp, 0);

    // Without a view, SWrast can't work
    if (!v)
	return;

    if (!m_init) {

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

	fastf_t windowbounds[6] = { -1, 1, -1, 1, QTSW_ZMIN, QTSW_ZMAX };
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

	// Ready to go
	m_init = true;

	emit init_done();
    }

    if (!m_init || !dmp)
	return;

    unsigned char *dm_bg1;
    unsigned char *dm_bg2;
    dm_get_bg(&dm_bg1, &dm_bg2, dmp);
    dm_set_bg(dmp, dm_bg1[0], dm_bg1[1], dm_bg1[2], dm_bg2[0], dm_bg2[1], dm_bg2[2]);

    matp_t mat = v->gv_model2view;
    dm_loadmatrix(dmp, mat, 0);
    dm_draw_begin(dmp);
    dm_draw_objs(v, draw_custom, draw_udata);
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

void QgSW::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (dmp && v) {
	dm_set_width(dmp, width());
	dm_set_height(dmp, height());
	v->gv_width = width();
	v->gv_height = height();
	dm_configure_win(dmp, 0);
	if (ifp) {
	    fb_configure_window(ifp, v->gv_width, v->gv_height);
	}
	dm_set_dirty(dmp, 1);
	emit changed();
    }
}

void QgSW::keyPressEvent(QKeyEvent *k) {

    if (!dmp || !v || !current || !use_default_keybindings) {
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
	emit changed();
    }

    QWidget::keyPressEvent(k);
}

void QgSW::mousePressEvent(QMouseEvent *e) {

    if (!dmp || !v || !current || !use_default_mousebindings) {
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
	emit changed();
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    x_press_pos = (double)e->x();
    y_press_pos = (double)e->y();
#else
    x_press_pos = e->position().x();
    y_press_pos = e->position().y();
#endif
    //bu_log("X,Y: %g, %g\n", x_press_pos, y_press_pos);

    QWidget::mousePressEvent(e);
}

void QgSW::mouseReleaseEvent(QMouseEvent *e) {
    if (!v) {
	QWidget::mouseReleaseEvent(e);
	return;
    }

    // To avoid an abrupt jump in scene motion the next time movement is
    // started with the mouse, after we release we return to the default state.
    x_prev = -INT_MAX;
    y_prev = -INT_MAX;

    if (CADmouseReleaseEvent(v, x_press_pos, y_press_pos, x_prev, y_prev, e, lmouse_mode)) {
	dm_set_dirty(dmp, 1);
	update();
	emit changed();
    }

    QWidget::mouseReleaseEvent(e);
}


void QgSW::mouseMoveEvent(QMouseEvent *e)
{
    if (!dmp || !v || !current || !use_default_mousebindings) {
	QWidget::mouseMoveEvent(e);
	return;
    }

    // Let bv know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();

    if (CADmouseMoveEvent(v, x_prev, y_prev, e, lmouse_mode)) {
	dm_set_dirty(dmp, 1);
	update();
	emit changed();
    }

    // Current positions are the new previous positions
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    x_prev = e->x();
    y_prev = e->y();
#else
    x_prev = e->position().x();
    y_prev = e->position().y();
#endif

    QWidget::mouseMoveEvent(e);
}

void QgSW::wheelEvent(QWheelEvent *e) {

    if (!dmp || !v || !current || !use_default_mousebindings) {
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
	emit changed();
    }

    QWidget::wheelEvent(e);
}

void QgSW::stash_hashes()
{
    if (!dmp) {
	prev_dhash = 0;
    } else {
	prev_dhash = dm_hash(dmp);
    }
    prev_vhash = bv_hash(v);
}

bool QgSW::diff_hashes()
{
    bool ret = false;
    unsigned long long c_dhash = 0;
    unsigned long long c_vhash = 0;

    if (dmp) {
	c_dhash = dm_hash(dmp);
    }
    if (v) {
	c_vhash = bv_hash(v);
    }

    if (dmp && dm_get_dirty(dmp))
	ret = true;

    if (prev_dhash != c_dhash) {
	if (dmp)
	    dm_set_dirty(dmp, 1);
	ret = true;
    }
    if (prev_vhash != c_vhash) {
	if (dmp)
	    dm_set_dirty(dmp, 1);
	ret = true;
    }

    if (ret) {
	need_update();
	emit changed();
    }

    return ret;
}

void QgSW::save_image() {
    // Set up a QImage with the rendered output..
    unsigned char *dm_image;
    if (dm_get_display_image(dmp, &dm_image, 1, 1)) {
	return;
    }
    QImage image(dm_image, dm_get_width(dmp), dm_get_height(dmp), QImage::Format_RGBA8888);
    QImage img32 = image.convertToFormat(QImage::Format_RGB32);
    img32.save("file.png");
}

void QgSW::aet(double a, double e, double t)
{
    if (!v)
	return;

    fastf_t aet[3];
    double aetd[3];
    aetd[0] = a;
    aetd[1] = e;
    aetd[2] = t;

    /* convert from double to fastf_t */
    VMOVE(aet, aetd);

    VMOVE(v->gv_aet, aet);

    /* TODO - based on the suspect bv_mat_aet... */
    mat_t tmat;
    fastf_t twist;
    fastf_t c_twist;
    fastf_t s_twist;
    bn_mat_angles(v->gv_rotation, 270.0 + v->gv_aet[1], 0.0, 270.0 - v->gv_aet[0]);
    twist = -v->gv_aet[2] * DEG2RAD;
    c_twist = cos(twist);
    s_twist = sin(twist);
    bn_mat_zrot(tmat, s_twist, c_twist);
    bn_mat_mul2(tmat, v->gv_rotation);

    bv_update(v);
}

void
QgSW::enableDefaultKeyBindings()
{
    use_default_keybindings = true;
}

void
QgSW::disableDefaultKeyBindings()
{
    use_default_keybindings = false;
}

void
QgSW::enableDefaultMouseBindings()
{
    use_default_mousebindings = true;
}

void
QgSW::disableDefaultMouseBindings()
{
    use_default_mousebindings = false;
}

void
QgSW::set_lmouse_move_default(int mm)
{
    QTCAD_SLOT("QgSW::set_lmouse_move_default", 1);
    lmouse_mode = mm;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

