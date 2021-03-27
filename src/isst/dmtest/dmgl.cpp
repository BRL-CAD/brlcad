/*                      D M G L . C P P
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
/** @file dmgl.cpp
 *
 * Brief description
 *
 */

#include "GL/glu.h"

#include <QOpenGLWidget>
#include <QKeyEvent>
#include <QGuiApplication> // for qGuiApp

extern "C" {
#include "bu/parallel.h"
#include "dm/bview_util.h"
#include "ged.h"
}
#include "dmgl.h"

DMRenderer::DMRenderer(dmGL *w)
    : m_w(w)
{
}

DMRenderer::~DMRenderer()
{
}

void DMRenderer::resize()
{
}

void DMRenderer::render()
{
    if (m_exiting)
	return;

    int w = m_w->width();
    int h = m_w->height();
    // Zero size == nothing to do
    if (!w || !h)
	return;

    if (!changed) {
	// Avoid a hot spin
	usleep(10000);
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
	if (!dmp) {
	    const char *acmd = "attach";
	    dmp = dm_open((void *)m_w, "qtgl", 1, &acmd);
	    m_w->gedp->ged_dmp = (void *)dmp;
	}
    }

    glViewport(0, 0, m_w->width(), m_w->height());
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


#if 1
    // TODO - need to set changed flag f gd_headDisplay list is altered - right
    // now, view is only updated if bview changes, which obviously isn't
    // correct...
    if (bu_list_len(m_w->gedp->ged_gdp->gd_headDisplay)) {

	matp_t mat = m_w->gedp->ged_gvp->gv_model2view;
	dm_loadmatrix(dmp, mat, 0);

	unsigned char geometry_default_color[] = { 255, 0, 0 };
	dm_draw_display_list(dmp, m_w->gedp->ged_gdp->gd_headDisplay,
		1.0, m_w->gedp->ged_gvp->gv_isize, 255, 0, 0, 1,
		0, 0, geometry_default_color, 1, 0);

	changed = false;
    }
#endif

#if 0
    /* The above drawing isn't yet working - draw the example triangle
     * to ensure that the context is working as expected... */
    // Clear color buffer
    glViewport(0, 0, m_w->width(), m_w->height());
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Select and setup the projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(65.0f, (GLfloat)m_w->width()/(GLfloat)m_w->height(), 1.0f, 100.0f);

    // Select and setup the modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-90, 1,0,0);
    glTranslatef(0,0,-1.0f);

    // Draw a colorful triangle
    glTranslatef(0.0f, 14.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-5.0f, 0.0f, -4.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(5.0f, 0.0f, -4.0f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 6.0f);
    glEnd();
#endif

    // Make no context current on this thread and move the QOpenGLWidget's
    // context back to the gui thread.
    m_w->doneCurrent();
    ctx->moveToThread(qGuiApp->thread());

    // Schedule composition. Note that this will use QueuedConnection, meaning
    // that update() will be invoked on the gui thread.
    QMetaObject::invokeMethod(m_w, "update");

    // Avoid a hot spin
    usleep(10000);
}

dmGL::dmGL(QWidget *parent)
    : QOpenGLWidget(parent)
{
    BU_GET(v, struct bview);
    ged_view_init(v);

    connect(this, &QOpenGLWidget::aboutToCompose, this, &dmGL::onAboutToCompose);
    connect(this, &QOpenGLWidget::frameSwapped, this, &dmGL::onFrameSwapped);
    connect(this, &QOpenGLWidget::aboutToResize, this, &dmGL::onAboutToResize);
    connect(this, &QOpenGLWidget::resized, this, &dmGL::onResized);

    m_thread = new QThread;

    // Create the renderer
    m_renderer = new DMRenderer(this);
    m_renderer->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_renderer, &QObject::deleteLater);

    // Let dmGL know to trigger the renderer
    connect(this, &dmGL::renderRequested, m_renderer, &DMRenderer::render);
    connect(m_renderer, &DMRenderer::contextWanted, this, &dmGL::grabContext);

    m_thread->start();

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

dmGL::~dmGL()
{
    m_renderer->prepareExit();
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
    BU_PUT(v, struct bview);
}

void dmGL::onAboutToCompose()
{
    // We are on the gui thread here. Composition is about to
    // begin. Wait until the render thread finishes.
    m_renderer->lockRenderer();
}

void dmGL::onFrameSwapped()
{
    m_renderer->unlockRenderer();
    // Assuming a blocking swap, our animation is driven purely by the
    // vsync in this example.
    emit renderRequested();
}

void dmGL::onAboutToResize()
{
    m_renderer->lockRenderer();
}

void dmGL::onResized()
{
    m_renderer->changed = true;
    m_renderer->unlockRenderer();
}

void dmGL::grabContext()
{
    if (m_renderer->m_exiting)
	return;
    m_renderer->lockRenderer();
    QMutexLocker lock(m_renderer->grabMutex());
    context()->moveToThread(m_thread);
    m_renderer->grabCond()->wakeAll();
    m_renderer->unlockRenderer();
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


void dmGL::mouseMoveEvent(QMouseEvent *e) {

    bu_log("(%d,%d)\n", e->x(), e->y());
    if (x_prev > -INT_MAX && y_prev > -INT_MAX) {
	bu_log("Delta: (%d,%d)\n", e->x() - x_prev, e->y() - y_prev);
    }

    x_prev = e->x();
    y_prev = e->y();

    QOpenGLWidget::mouseMoveEvent(e);
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

