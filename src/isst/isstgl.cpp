/*                      I S S T G L . C P P
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
/** @file isstgl.cpp
 *
 * Brief description
 *
 */

#include <QOpenGLWidget>
#include <QKeyEvent>
#include <QGuiApplication> // for qGuiApp

#include "bu/parallel.h"
#include "isstgl.h"

#include <chrono>
#include <thread>

TIERenderer::TIERenderer(isstGL *w)
    : m_w(w)
{
   // Initialize TIE camera
    camera.type = RENDER_CAMERA_PERSPECTIVE;
    camera.fov = 25;
    render_camera_init(&camera, bu_avail_cpus());
    render_phong_init(&camera.render, NULL);

    // Initialize texture buffer
    TIENET_BUFFER_INIT(buffer_image);
    texdata = realloc(texdata, camera.w * camera.h * 3);
    texdata_size = camera.w * camera.h;

    // Initialize TIE tile
    //
    // Note:  If orig_x and orig_Y are not initialized, output pixel placement
    // in the buffer may be randomly offset - you may see no image, or an image
    // in the wrong place (or it may happen to work if the values happen to be
    // zero anyway...)
    tile.orig_x = 0;
    tile.orig_y = 0;
    tile.format = RENDER_CAMERA_BIT_DEPTH_24;
}

TIERenderer::~TIERenderer()
{
    TIENET_BUFFER_FREE(buffer_image);
    free(texdata);
}

void TIERenderer::resize()
{
    // If something changed, we need to re-render - otherwise, no-op
    if (!changed)
	return;

    int w = m_w->width();
    int h = m_w->height();

    // Translated from Tcl/Tk ISST logic for resolution adjustment
    if (resolution_factor == 0) {
	camera.w = w;
	camera.h = h;
    } else {
	camera.w = resolution_factor;
	camera.h = camera.w * h / w;
    }

    // Set tile size
    tile.size_x = camera.w;
    tile.size_y = camera.h;


    // Set up the raytracing image buffer
    TIENET_BUFFER_SIZE(buffer_image, (uint32_t)(3 * camera.w * camera.h));

    if (texdata_size < camera.w * camera.h) {
	texdata_size = camera.w * camera.h;
	texdata = realloc(texdata, camera.w * camera.h * 3);
    }

    // Set up the TeXImage2D buffer that will hold the results of the raytrace
    // for OpenGL
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, camera.w, camera.h, 0, GL_RGB, GL_UNSIGNED_BYTE, texdata);
}

void TIERenderer::res_incr()
{
    // Increase setting controlling raytracing grid density.
    // Maximum is one raytraced pixel per window pixel
    resolution++;
    CLAMP(resolution, 1, 20);
    resolution_factor = (resolution == 20) ? 0 : lrint(floor(m_w->width() * .05 * resolution));
    scaled = true;
}

void TIERenderer::res_decr()
{
    // Decrease setting controlling raytracing grid density.
    // Minimum is clamped - too course and the image is meaningless
    resolution--;
    CLAMP(resolution, 1, 20);
    resolution_factor = (resolution == 20) ? 0 : lrint(floor(m_w->height() * .05 * resolution));
    scaled = true;
}

void TIERenderer::render()
{
    if (m_exiting)
	return;

    int w = m_w->width();
    int h = m_w->height();
    // Zero size == nothing to do
    if (!w || !h)
	return;

    if (scaled) {
	changed = true;
	scaled = false;
    }

    if (!changed) {
	// Avoid a hot spin
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	return;
    }

    // Since we're in a separate rendering thread, there is
    // some preliminary work we need to do before proceeding
    // with OpenGL calls
    QOpenGLContext *ctx = m_w->context();
    if (!ctx) // QOpenGLWidget not yet initialized
	return;
    // Grab the context.
    m_grabMutex.lock();
    emit contextWanted();
    m_grabCond.wait(&m_grabMutex);
    QMutexLocker lock(&m_renderMutex);
    m_grabMutex.unlock();
    if (m_exiting)
	return;
    Q_ASSERT(ctx->thread() == QThread::currentThread());


    // Have context, initialize if necessary
    m_w->makeCurrent();
    if (!m_init) {
	initializeOpenGLFunctions();
	m_init = true;
    }

    // Ready for actual OpenGL calls.
    resize();


    changed = false;

    // IMPORTANT - this reset is necessary or the resultant image will
    // not display correctly in the buffer.
    buffer_image.ind = 0;

    // Core TIE render
    render_camera_prep(&camera);
    render_camera_render(&camera, tie, &tile, &buffer_image);

    glDisable(GL_LIGHTING);

    glViewport(0,0, m_w->width(), m_w->height());
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho(0, m_w->width(), m_w->height(), 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);

    glClear(GL_COLOR_BUFFER_BIT);

    glClear(GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glColor3f(1,1,1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, camera.w, camera.h, GL_RGB, GL_UNSIGNED_BYTE, buffer_image.data + sizeof(camera_tile_t));
    glBegin(GL_TRIANGLE_STRIP);

    glTexCoord2d(0, 0); glVertex3f(0, 0, 0);
    glTexCoord2d(0, 1); glVertex3f(0, m_w->height(), 0);
    glTexCoord2d(1, 0); glVertex3f(m_w->width(), 0, 0);
    glTexCoord2d(1, 1); glVertex3f(m_w->width(), m_w->height(), 0);

    glEnd();


    // Make no context current on this thread and move the QOpenGLWidget's
    // context back to the gui thread.
    m_w->doneCurrent();
    ctx->moveToThread(qGuiApp->thread());

    // Schedule composition. Note that this will use QueuedConnection, meaning
    // that update() will be invoked on the gui thread.
    QMetaObject::invokeMethod(m_w, "update");

    // Avoid a hot spin
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

isstGL::isstGL(QWidget *parent)
    : QOpenGLWidget(parent)
{
    connect(this, &QOpenGLWidget::aboutToCompose, this, &isstGL::onAboutToCompose);
    connect(this, &QOpenGLWidget::frameSwapped, this, &isstGL::onFrameSwapped);
    connect(this, &QOpenGLWidget::aboutToResize, this, &isstGL::onAboutToResize);
    connect(this, &QOpenGLWidget::resized, this, &isstGL::onResized);

    m_thread = new QThread;

    // Create the renderer
    m_renderer = new TIERenderer(this);
    m_renderer->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_renderer, &QObject::deleteLater);

    // Let isstGL know to trigger the renderer
    connect(this, &isstGL::renderRequested, m_renderer, &TIERenderer::render);
    connect(m_renderer, &TIERenderer::contextWanted, this, &isstGL::grabContext);

    m_thread->start();

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

isstGL::~isstGL()
{
    m_renderer->prepareExit();
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
}

void isstGL::onAboutToCompose()
{
    // We are on the gui thread here. Composition is about to
    // begin. Wait until the render thread finishes.
    m_renderer->lockRenderer();
}

void isstGL::onFrameSwapped()
{
    m_renderer->unlockRenderer();
    // Assuming a blocking swap, our animation is driven purely by the
    // vsync in this example.
    emit renderRequested();
}

void isstGL::onAboutToResize()
{
    m_renderer->lockRenderer();
}

void isstGL::onResized()
{
    m_renderer->changed = true;
    m_renderer->unlockRenderer();
}

void isstGL::grabContext()
{
    if (m_renderer->m_exiting)
	return;
    m_renderer->lockRenderer();
    QMutexLocker lock(m_renderer->grabMutex());
    context()->moveToThread(m_thread);
    m_renderer->grabCond()->wakeAll();
    m_renderer->unlockRenderer();
}

void
isstGL::set_tie(struct tie_s *in_tie)
{
    m_renderer->tie = in_tie;

    // Initialize the camera position
    VSETALL(m_renderer->camera.pos, m_renderer->tie->radius);
    VMOVE(m_renderer->camera.focus, m_renderer->tie->mid);

    // Record the initial settings for use in subsequent calculations
    VSETALL(m_renderer->camera_pos_init, m_renderer->tie->radius);
    VMOVE(m_renderer->camera_focus_init, m_renderer->tie->mid);

    // Having just loaded a new TIE scene,
    // we need a new image
    emit renderRequested();
}

void isstGL::keyPressEvent(QKeyEvent *k) {
    //QString kstr = QKeySequence(k->key()).toString();
    //bu_log("%s\n", kstr.toStdString().c_str());
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
    QOpenGLWidget::keyPressEvent(k);
}


void isstGL::mouseMoveEvent(QMouseEvent *e) {

    bu_log("(%d,%d)\n", e->x(), e->y());
    if (x_prev > -INT_MAX && y_prev > -INT_MAX) {
	bu_log("Delta: (%d,%d)\n", e->x() - x_prev, e->y() - y_prev);
    }

    x_prev = e->x();
    y_prev = e->y();

    QOpenGLWidget::mouseMoveEvent(e);
}

void isstGL::save_image() {
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

