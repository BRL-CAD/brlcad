/*                          Q G S W . C P P
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
/** @file QgSW.cpp
 *
 * Qt widget for visualizing libosmesa OpenGL software rasterizer output.
 */

#define USE_MGL_NAMESPACE 1

#include "common.h"

#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QtGlobal>
#include <QWheelEvent>

#include "QgCanvasState.h"   /* pimpl definition + shared helpers */
#include "qtcad/QgSW.h"

extern "C" {
#include "bsg/util.h"
}

// Using the full BSG_VIEW_MIN/BSG_VIEW_MAX was causing drawing artifacts with moss I
// in shaded mode (I think I was seeing the "Z-fighting" problem:
// https://www.sjbaker.org/steve/omniv/love_your_z_buffer.html )
//
// Setting to (-1,1) clips geometry too quickly as we start to zoom in.
// -100,100 seems to work, but may need a better long term solution to
// this... maybe basing it on the currently visible object bounds?
#define QTSW_ZMIN -100
#define QTSW_ZMAX 100
/* Background grey level used when capturing the viewport as an image.
 * Dark but not black, so the yellow wireframe is clearly visible. */
#define QTSW_SCREENSHOT_BG_GREY 40

QgSW::QgSW(QWidget *parent, struct fb *fbp)
    : QWidget(parent)
{
    d = new QgCanvasState();
    d->ifp = fbp;
    d->lmouse_mode = BSG_SCALE;

    // Provide a view specific to this widget - set gedp->ged_gvp to v
    // if this is the current view
    BU_GET(d->local_v, struct bsg_view);
    bsg_init(d->local_v, nullptr);
    bu_vls_sprintf(&d->local_v->gv_name, "swrast");
    d->v = d->local_v;

    // Don't dm_open until we have the view.
    d->dmp = nullptr;

    // If we weren't supplied with a framebuffer, allocate one.
    // We don't open it until we have the dmp.
    if (!d->ifp) {
d->ifp = fb_raw("swrast");
fb_set_standalone(d->ifp, 0);
    }

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

QgSW::~QgSW()
{
    if (d->dmp)
dm_close(d->dmp);
    if (d->ifp && !fb_get_standalone(d->ifp)) {
fb_close_existing(d->ifp);
    }
    BU_PUT(d->local_v, struct bv);
    delete d;
    d = nullptr;
}

struct bsg_view *
QgSW::view() const
{
    return d->v;
}

struct dm *
QgSW::displayManager() const
{
    return d->dmp;
}

struct fb *
QgSW::frameBuffer() const
{
    return d->ifp;
}

int
QgSW::currentView() const
{
    return d->current;
}

void
QgSW::set_current(int active)
{
    d->current = active;
}

void
QgSW::setDisplayManagerSet(struct bu_ptbl *set)
{
    d->dm_set = set;
}

void
QgSW::set_view(struct bsg_view *nv)
{
    qgcanvas_set_view(*d, nv);
}

void QgSW::need_update()
{
    QTCAD_SLOT("QgSW::need_update", 1);
    if (!d->dmp)
return;
    qgcanvas_request_update(*d, BSG_VIEW_REFRESH_FRAMEBUFFER | BSG_VIEW_REFRESH_FORCE);
    if (d->fb_update_queued)
return;
    d->fb_update_queued = true;
    QMetaObject::invokeMethod(this, "queued_update", Qt::QueuedConnection);
}

void QgSW::queued_update()
{
    d->fb_update_queued = false;
    update();
}

void QgSW::paintEvent(QPaintEvent *e)
{
    // Go ahead and set the flag, but (unlike the rendering thread
    // implementation) we need to do the draw routine every time in paintGL, or
    // we end up with unrendered frames.
    dm_set_native_repaint_pending(d->dmp, 0);

    // Without a view, SWrast can't work
    if (!d->v)
return;

    if (!d->m_init) {

if (!d->dmp) {
    // swrast will need to know the window size
    QSize rsize = qgcanvas_render_size(this);
    d->v->gv_width  = rsize.width();
    d->v->gv_height = rsize.height();

    // Do the standard libdm attach to get our rendering backend.
    const char *acmd = "attach";
    d->dmp = dm_open((void *)d->v, nullptr, "swrast", 1, &acmd);
    if (!d->dmp)
return;

    // Let dmp know what the app level widget is (needed so we can
    // connect framebuffer drawing events to the widget redraw logic.)
    dm_set_udata(d->dmp, this);

    // If we have a framebuffer, now we can open it
    if (d->ifp) {
struct fb_platform_specific *fbps = fb_get_platform_specific(FB_QTGL_MAGIC);
fbps->data = (void *)d->dmp;
fb_setup_existing(d->ifp, dm_get_width(d->dmp), dm_get_height(d->dmp), fbps);
fb_put_platform_specific(fbps);
    }
}

dm_configure_win(d->dmp, 0);
dm_set_pathname(d->dmp, "SWDM");
dm_set_zbuffer(d->dmp, 1);

fastf_t windowbounds[6] = { -1, 1, -1, 1, QTSW_ZMIN, QTSW_ZMAX };
dm_set_win_bounds(d->dmp, windowbounds);

// Associate the view scale with the dmp
dm_set_vp(d->dmp, &d->v->gv_scale);

// Let the view know it has an associated dm.
d->v->dmp = d->dmp;

// Set the view width and height to match the dm
d->v->gv_width  = dm_get_width(d->dmp);
d->v->gv_height = dm_get_height(d->dmp);

// If we have a ptbl defining the current dm set and/or an unset
// pointer to indicate the current dm, go ahead and set them.
if (d->dm_set)
    bu_ptbl_ins_unique(d->dm_set, (long int *)d->dmp);

// Ready to go
d->m_init = true;

emit init_done();
    }

    if (!d->m_init || !d->dmp)
return;

    QSize rsize = qgcanvas_render_size(this);
    if (dm_get_width(d->dmp) != rsize.width() || dm_get_height(d->dmp) != rsize.height()) {
dm_set_width(d->dmp, rsize.width());
dm_set_height(d->dmp, rsize.height());
dm_configure_win(d->dmp, 0);
if (d->ifp)
    fb_configure_window(d->ifp, rsize.width(), rsize.height());
    }
    d->v->gv_width  = dm_get_width(d->dmp);
    d->v->gv_height = dm_get_height(d->dmp);

    unsigned char *dm_bg1;
    unsigned char *dm_bg2;
    dm_get_bg(&dm_bg1, &dm_bg2, d->dmp);
    dm_set_bg(d->dmp, dm_bg1[0], dm_bg1[1], dm_bg1[2], dm_bg2[0], dm_bg2[1], dm_bg2[2]);

    (void)bsg_view_refresh_consume(d->v);
    dm_draw_begin(d->dmp);
    dm_draw_objs(d->v);
    dm_draw_end(d->dmp);
    bsg_view_refresh_complete(d->v);

    // Set up a QImage with the rendered output..
    unsigned char *dm_image;
    if (dm_get_display_image(d->dmp, &dm_image, 0, 1)) {
return;
    }
    QImage image(dm_image, dm_get_width(d->dmp), dm_get_height(d->dmp), QImage::Format_RGBX8888);
    image.setDevicePixelRatio(devicePixelRatioF());
    QPainter painter(this);
    painter.translate(0, height());
    painter.scale(1.0, -1.0);
    painter.drawImage(QPoint(0, 0), image);
    QWidget::paintEvent(e);
}

void QgSW::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (d->dmp && d->v) {
	QSize rsize = qgcanvas_render_size(this);
	dm_set_width(d->dmp, rsize.width());
	dm_set_height(d->dmp, rsize.height());
	d->v->gv_width  = rsize.width();
	d->v->gv_height = rsize.height();
	dm_configure_win(d->dmp, 0);
	if (d->ifp) {
	    fb_configure_window(d->ifp, d->v->gv_width, d->v->gv_height);
	}
	qgcanvas_request_update(*d, BSG_VIEW_REFRESH_VIEW | BSG_VIEW_REFRESH_FRAMEBUFFER);
	emit changed();
    }
}

void QgSW::keyPressEvent(QKeyEvent *k)
{

    if (!d->dmp || !d->v || !d->current || !d->use_default_keybindings) {
QWidget::keyPressEvent(k);
return;
    }

    // Let bv know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    QSize rsize = qgcanvas_render_size(this);
    d->v->gv_width  = rsize.width();
    d->v->gv_height = rsize.height();

    if (d->input.keyPressEvent(d->v, d->x_prev, d->y_prev, k)) {
qgcanvas_request_update(*d, BSG_VIEW_REFRESH_VIEW);
update();
emit changed();
    }

    QWidget::keyPressEvent(k);
}

void QgSW::mousePressEvent(QMouseEvent *e)
{

    if (d->ifp && fb_get_standalone(d->ifp) && e->button() == Qt::RightButton) {
if (window())
    window()->close();
return;
    }

    if (!d->dmp || !d->v || !d->current || !d->use_default_mousebindings) {
QWidget::mousePressEvent(e);
return;
    }

    // Let bv know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    QSize rsize = qgcanvas_render_size(this);
    d->v->gv_width  = rsize.width();
    d->v->gv_height = rsize.height();

    if (d->input.mousePressEvent(d->v, d->x_prev, d->y_prev, e)) {
qgcanvas_request_update(*d, BSG_VIEW_REFRESH_VIEW);
update();
emit changed();
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    d->x_press_pos = (double)e->x();
    d->y_press_pos = (double)e->y();
#else
    d->x_press_pos = e->position().x();
    d->y_press_pos = e->position().y();
#endif
    //bu_log("X,Y: %g, %g\n", d->x_press_pos, d->y_press_pos);

    QWidget::mousePressEvent(e);
}

void QgSW::mouseReleaseEvent(QMouseEvent *e)
{
    if (!d->v) {
QWidget::mouseReleaseEvent(e);
return;
    }

    // To avoid an abrupt jump in scene motion the next time movement is
    // started with the mouse, after we release we return to the default state.
    d->x_prev = -INT_MAX;
    d->y_prev = -INT_MAX;

    if (d->input.mouseReleaseEvent(d->v, d->x_press_pos, d->y_press_pos,
   d->x_prev, d->y_prev, e, d->lmouse_mode)) {
qgcanvas_request_update(*d, BSG_VIEW_REFRESH_VIEW);
update();
emit changed();
    }

    QWidget::mouseReleaseEvent(e);
}


void QgSW::mouseMoveEvent(QMouseEvent *e)
{
    if (!d->dmp || !d->v || !d->current || !d->use_default_mousebindings) {
QWidget::mouseMoveEvent(e);
return;
    }

    // Let bv know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    QSize rsize = qgcanvas_render_size(this);
    d->v->gv_width  = rsize.width();
    d->v->gv_height = rsize.height();

    int mret = d->input.mouseMoveEvent(d->v, d->x_prev, d->y_prev, e, d->lmouse_mode);
    if (mret > 0) {
qgcanvas_request_update(*d, BSG_VIEW_REFRESH_VIEW);
update();
emit changed();
    }

    // Current positions are the new previous positions
    if (mret != -1) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
d->x_prev = e->x();
d->y_prev = e->y();
#else
d->x_prev = e->position().x();
d->y_prev = e->position().y();
#endif
    }

    QWidget::mouseMoveEvent(e);
}

void QgSW::wheelEvent(QWheelEvent *e)
{

    if (!d->dmp || !d->v || !d->current || !d->use_default_mousebindings) {
QWidget::wheelEvent(e);
return;
    }

    // Let bv know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    QSize rsize = qgcanvas_render_size(this);
    d->v->gv_width  = rsize.width();
    d->v->gv_height = rsize.height();

    if (d->input.wheelEvent(d->v, e)) {
qgcanvas_request_update(*d, BSG_VIEW_REFRESH_VIEW);
update();
emit changed();
    }

    QWidget::wheelEvent(e);
}

void QgSW::stash_hashes()
{
    qgcanvas_stash_hashes(*d);
}

bool QgSW::diff_hashes()
{
    bool ret = qgcanvas_diff_hashes_check(*d);
    if (ret) {
need_update();
emit changed();
    }
    return ret;
}

void QgSW::save_image()
{
    // Set up a QImage with the rendered output..
    unsigned char *dm_image;
    if (dm_get_display_image(d->dmp, &dm_image, 0, 1)) {
return;
    }
    QImage image(dm_image, dm_get_width(d->dmp), dm_get_height(d->dmp), QImage::Format_RGBX8888);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    image.flipped(Qt::Vertical).save("file.png");
#else
    image.mirrored(false, true).save("file.png");
#endif
}

/* Render the current view to a file without relying on Qt paint events.
 * This is safe to call in headless / off-screen mode. */
void QgSW::render_to_file(const QString &filename)
{
    QImage img;
    get_viewport_image(img);
    if (!img.isNull())
img.convertToFormat(QImage::Format_RGB32).save(filename);
}

/* Render the current view and return the raw DM image.
 * img will be a null QImage if rendering fails. */
void QgSW::get_viewport_image(QImage &img)
{
    img = QImage();  /* null sentinel */
    if (!d->v) return;

    /* Ensure DM is initialised (reuse render_to_file init logic) */
    if (!d->m_init) {
if (!d->dmp) {
    int rw = (width()  > 50) ? width()  : 800;
    int rh = (height() > 50) ? height() : 600;
    d->v->gv_width  = rw;
    d->v->gv_height = rh;
    const char *acmd = "attach";
    d->dmp = dm_open((void *)d->v, nullptr, "swrast", 1, &acmd);
    if (!d->dmp) return;
    dm_set_udata(d->dmp, this);
}
dm_configure_win(d->dmp, 0);
dm_set_pathname(d->dmp, "SWDM");
dm_set_zbuffer(d->dmp, 1);
fastf_t windowbounds[6] = { -1, 1, -1, 1, QTSW_ZMIN, QTSW_ZMAX };
dm_set_win_bounds(d->dmp, windowbounds);
dm_set_vp(d->dmp, &d->v->gv_scale);
d->v->dmp       = d->dmp;
d->v->gv_width  = dm_get_width(d->dmp);
d->v->gv_height = dm_get_height(d->dmp);
if (d->dm_set)
    bu_ptbl_ins_unique(d->dm_set, (long int *)d->dmp);
d->m_init = true;
    }
    if (!d->dmp) return;

    /* Render */
    unsigned char *dm_bg1;
    unsigned char *dm_bg2;
    dm_get_bg(&dm_bg1, &dm_bg2, d->dmp);
    /* Use a dark-grey background for better visibility in screenshots;
     * fall through to the stored background if it is already non-black. */
    unsigned char bg1r = dm_bg1[0], bg1g = dm_bg1[1], bg1b = dm_bg1[2];
    unsigned char bg2r = dm_bg2[0], bg2g = dm_bg2[1], bg2b = dm_bg2[2];
    if (bg1r == 0 && bg1g == 0 && bg1b == 0 &&
    bg2r == 0 && bg2g == 0 && bg2b == 0) {
/* Default black: override with a neutral dark background */
bg1r = bg1g = bg1b = QTSW_SCREENSHOT_BG_GREY;
bg2r = bg2g = bg2b = QTSW_SCREENSHOT_BG_GREY;
    }
    dm_set_bg(d->dmp, bg1r, bg1g, bg1b, bg2r, bg2g, bg2b);
    dm_loadmatrix(d->dmp, d->v->gv_model2view, 0);
    (void)bsg_view_refresh_consume(d->v);
    dm_draw_begin(d->dmp);
    dm_draw_objs(d->v);
    dm_draw_end(d->dmp);
    bsg_view_refresh_complete(d->v);

    unsigned char *vp_image = nullptr;
    if (dm_get_display_image(d->dmp, &vp_image, 1, 1) || !vp_image) return;
    /* Copy pixel data into a QImage (QImage doesn't own vp_image) */
    img = QImage(vp_image, dm_get_width(d->dmp), dm_get_height(d->dmp),
 QImage::Format_RGBA8888).copy();
    bu_free(vp_image, "copy of backend image");
}

void QgSW::aet(double a, double e, double t)
{
    qgcanvas_aet(*d, a, e, t);
}

void
QgSW::enableDefaultKeyBindings()
{
    d->use_default_keybindings = true;
}

void
QgSW::disableDefaultKeyBindings()
{
    d->use_default_keybindings = false;
}

void
QgSW::enableDefaultMouseBindings()
{
    d->use_default_mousebindings = true;
}

void
QgSW::disableDefaultMouseBindings()
{
    d->use_default_mousebindings = false;
}

int
QgSW::lmouseMoveDefault() const
{
    return d->lmouse_mode;
}

void
QgSW::set_lmouse_move_default(int mm)
{
    QTCAD_SLOT("QgSW::set_lmouse_move_default", 1);
    d->lmouse_mode = mm;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
