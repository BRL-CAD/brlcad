/*                      Q G G L . C P P
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
/** @file QgGL.cpp
 *
 * Use a QOpenGLWidget to display libdm drawing content.
 *
 */

#include "common.h"

#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QtGlobal>

#include "QgCanvasState.h"   /* pimpl definition + shared helpers */
#include "qtcad/QgGL.h"

extern "C" {
#include "bsg/util.h"
}

// FROM MGED
#define XMIN            (-2048)
#define XMAX            (2047)
#define YMIN            (-2048)
#define YMAX            (2047)

// from BSG_VIEW_MIN and BSG_VIEW_MAX
#define QTGL_ZMIN -2048
#define QTGL_ZMAX 2047

QgGL::QgGL(QWidget *parent, struct fb *fbp)
    : QOpenGLWidget(parent)
{
    d = new QgCanvasState();
    d->ifp = fbp;
    d->lmouse_mode = BSG_SCALE;

    // Provide a view specific to this widget - set gedp->ged_gvp to v
    // if this is the current view
    BU_GET(d->local_v, struct bsg_view);
    bsg_init(d->local_v, nullptr);
    bu_vls_sprintf(&d->local_v->gv_name, "qtgl");
    d->v = d->local_v;

    // We can't initialize dmp successfully until more of the OpenGL
    // initialization is complete
    d->dmp = nullptr;

    // If we weren't supplied with a framebuffer, allocate one.
    // We don't open it until we have the dmp.
    if (!d->ifp) {
d->ifp = fb_raw("qtgl");
fb_set_standalone(d->ifp, 0);
    }

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

QgGL::~QgGL()
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
QgGL::view() const
{
    return d->v;
}

struct dm *
QgGL::displayManager() const
{
    return d->dmp;
}

struct fb *
QgGL::frameBuffer() const
{
    return d->ifp;
}

int
QgGL::currentView() const
{
    return d->current;
}

void
QgGL::set_current(int active)
{
    d->current = active;
}

void
QgGL::setDisplayManagerSet(struct bu_ptbl *set)
{
    d->dm_set = set;
}

void
QgGL::set_view(struct bsg_view *nv)
{
    qgcanvas_set_view(*d, nv);
}

void QgGL::paintGL()
{
    int w = width();
    int h = height();
    // Zero size == nothing to do
    if (!w || !h)
return;

    if (!d->m_init) {

if (!d->dmp) {

    // This is needed so we can work with Qt's OpenGL widget
    // using standard OpenGL functions.
    initializeOpenGLFunctions();

    // Do the standard libdm attach to get our rendering backend.
    const char *acmd = "attach";
    d->dmp = dm_open((void *)this, nullptr, "qtgl", 1, &acmd);
    if (!d->dmp)
return;

    // If we have a framebuffer, now we can open it
    if (d->ifp) {
struct fb_platform_specific *fbps = fb_get_platform_specific(FB_QTGL_MAGIC);
fbps->data = (void *)d->dmp;
fb_setup_existing(d->ifp, dm_get_width(d->dmp), dm_get_height(d->dmp), fbps);
fb_put_platform_specific(fbps);
    }

    dm_set_pathname(d->dmp, "QTDM");
}

// QTGL_ZMIN and QTGL_ZMAX are historical - need better
// documentation on why those specific values are used.
//fastf_t windowbounds[6] = { XMIN, XMAX, YMIN, YMAX, QTGL_ZMIN, QTGL_ZMAX };
fastf_t windowbounds[6] = { -1, 1, -1, 1, QTGL_ZMIN, QTGL_ZMAX };
dm_set_win_bounds(d->dmp, windowbounds);

if (d->v) {
    // Associate the view scale with the dmp
    dm_set_vp(d->dmp, &d->v->gv_scale);

    // Let the view know it now has an associated display manager
    d->v->dmp = d->dmp;

    // Set the view width and height to match the dm
    d->v->gv_width  = dm_get_width(d->dmp);
    d->v->gv_height = dm_get_height(d->dmp);
}

// If we have a ptbl defining the current dm set and/or an unset
// pointer to indicate the current dm, go ahead and set them.
if (d->dm_set)
    bu_ptbl_ins_unique(d->dm_set, (long int *)d->dmp);

// Ready to go
d->m_init = true;
emit init_done();
    }

    if (!d->m_init || !d->dmp || !d->v)
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

    // Re-draw the background to clear any previous drawing
    unsigned char *dm_bg1;
    unsigned char *dm_bg2;
    dm_get_bg(&dm_bg1, &dm_bg2, d->dmp);
    dm_set_bg(d->dmp, dm_bg1[0], dm_bg1[1], dm_bg1[2], dm_bg2[0], dm_bg2[1], dm_bg2[2]);

    // Go ahead and set the flag, but (unlike the rendering thread
    // implementation) we need to do the draw routine every time in paintGL, or
    // we end up with unrendered frames.
    (void)bsg_view_refresh_consume(d->v);
    dm_set_native_repaint_pending(d->dmp, 0);
    dm_draw_objs(d->v);
    dm_draw_end(d->dmp);
    bsg_view_refresh_complete(d->v);
}

void QgGL::resizeGL(int, int)
{
    if (!d->dmp || !d->v)
return;
    dm_configure_win(d->dmp, 0);
    d->v->gv_width  = dm_get_width(d->dmp);
    d->v->gv_height = dm_get_height(d->dmp);
    if (d->ifp) {
fb_configure_window(d->ifp, d->v->gv_width, d->v->gv_height);
    }
    if (d->dmp)
qgcanvas_request_update(*d, BSG_VIEW_REFRESH_VIEW);
    emit changed();
}

void QgGL::resizeEvent(QResizeEvent *e)
{
    QOpenGLWidget::resizeEvent(e);
    if (!d->dmp || !d->v)
return;
    QSize rsize = qgcanvas_render_size(this);
    dm_set_width(d->dmp, rsize.width());
    dm_set_height(d->dmp, rsize.height());
    d->v->gv_width  = rsize.width();
    d->v->gv_height = rsize.height();
    dm_configure_win(d->dmp, 0);
    if (d->ifp) {
fb_configure_window(d->ifp, d->v->gv_width, d->v->gv_height);
    }
    if (d->dmp)
qgcanvas_request_update(*d, BSG_VIEW_REFRESH_VIEW);
    emit changed();
}

void QgGL::need_update()
{
    bsg_log(4, "QgGL::need_update");
    QTCAD_SLOT("QgGL::need_update", 1);
    if (!d->dmp)
return;
    qgcanvas_request_update(*d, BSG_VIEW_REFRESH_VIEW);
    if (d->fb_update_queued)
return;
    d->fb_update_queued = true;
    QMetaObject::invokeMethod(this, "queued_update", Qt::QueuedConnection);
}

void QgGL::queued_update()
{
    d->fb_update_queued = false;
    update();
}

void QgGL::keyPressEvent(QKeyEvent *k)
{

    if (!d->dmp || !d->v || !d->current || !d->use_default_keybindings) {
QOpenGLWidget::keyPressEvent(k);
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

    QOpenGLWidget::keyPressEvent(k);
}

void QgGL::mousePressEvent(QMouseEvent *e)
{

    if (d->ifp && fb_get_standalone(d->ifp) && e->button() == Qt::RightButton) {
if (window())
    window()->close();
return;
    }

    if (!d->dmp || !d->v || !d->current || !d->use_default_mousebindings) {
QOpenGLWidget::mousePressEvent(e);
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

    QOpenGLWidget::mousePressEvent(e);
}

void QgGL::mouseReleaseEvent(QMouseEvent *e)
{

    if (!d->v) {
QOpenGLWidget::mouseReleaseEvent(e);
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

    QOpenGLWidget::mouseReleaseEvent(e);
}

void QgGL::mouseMoveEvent(QMouseEvent *e)
{
    if (!d->dmp || !d->v || !d->current || !d->use_default_mousebindings) {
QOpenGLWidget::mouseMoveEvent(e);
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

    QOpenGLWidget::mouseMoveEvent(e);
}

void QgGL::wheelEvent(QWheelEvent *e)
{

    if (!d->dmp || !d->v || !d->current || !d->use_default_mousebindings) {
QOpenGLWidget::wheelEvent(e);
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

    QOpenGLWidget::wheelEvent(e);
}

void QgGL::stash_hashes()
{
    qgcanvas_stash_hashes(*d);
}

bool QgGL::diff_hashes()
{
    bool ret = qgcanvas_diff_hashes_check(*d);
    if (ret) {
need_update();
emit changed();
    }
    return ret;
}

void QgGL::save_image()
{
    QImage image = this->grabFramebuffer();
    image.save("file.png");
}

void QgGL::render_to_file(const QString &filename)
{
    QImage img;
    get_viewport_image(img);
    if (!img.isNull())
img.convertToFormat(QImage::Format_RGB32).save(filename);
}

void QgGL::get_viewport_image(QImage &img)
{
    if (!d->m_init || !d->dmp || !d->v) {
img = QImage();
return;
    }
    img = grabFramebuffer();
}

void QgGL::aet(double a, double e, double t)
{
    qgcanvas_aet(*d, a, e, t);
}

void
QgGL::enableDefaultKeyBindings()
{
    d->use_default_keybindings = true;
}

void
QgGL::disableDefaultKeyBindings()
{
    d->use_default_keybindings = false;
}

void
QgGL::enableDefaultMouseBindings()
{
    d->use_default_mousebindings = true;
}

void
QgGL::disableDefaultMouseBindings()
{
    d->use_default_mousebindings = false;
}

int
QgGL::lmouseMoveDefault() const
{
    return d->lmouse_mode;
}

void
QgGL::set_lmouse_move_default(int mm)
{
    QTCAD_SLOT("QgGL::set_lmouse_move_default", 1);
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
