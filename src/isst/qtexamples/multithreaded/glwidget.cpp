/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "GL/glu.h"
#include "glwidget.h"
#include <QGuiApplication> // for qGuiApp

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(300, 250);

    connect(this, &QOpenGLWidget::aboutToCompose, this, &GLWidget::onAboutToCompose);
    connect(this, &QOpenGLWidget::frameSwapped, this, &GLWidget::onFrameSwapped);
    connect(this, &QOpenGLWidget::aboutToResize, this, &GLWidget::onAboutToResize);
    connect(this, &QOpenGLWidget::resized, this, &GLWidget::onResized);

    m_thread = new QThread;
    m_renderer = new Renderer(this);
    m_renderer->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_renderer, &QObject::deleteLater);

    connect(this, &GLWidget::renderRequested, m_renderer, &Renderer::render);
    connect(m_renderer, &Renderer::contextWanted, this, &GLWidget::grabContext);

    m_thread->start();
}

GLWidget::~GLWidget()
{
    m_renderer->prepareExit();
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
}

void GLWidget::onAboutToCompose()
{
    // We are on the gui thread here. Composition is about to
    // begin. Wait until the render thread finishes.
    m_renderer->lockRenderer();
}

void GLWidget::onFrameSwapped()
{
    m_renderer->unlockRenderer();
    // Assuming a blocking swap, our animation is driven purely by the
    // vsync in this example.
    emit renderRequested();
}

void GLWidget::onAboutToResize()
{
    m_renderer->lockRenderer();
}

void GLWidget::onResized()
{
    m_renderer->unlockRenderer();
}

void GLWidget::grabContext()
{
    m_renderer->lockRenderer();
    QMutexLocker lock(m_renderer->grabMutex());
    context()->moveToThread(m_thread);
    m_renderer->grabCond()->wakeAll();
    m_renderer->unlockRenderer();
}

Renderer::Renderer(GLWidget *w) : m_glwidget(w) {}

void Renderer::render()
{
    if (m_exiting)
        return;

    QOpenGLContext *ctx = m_glwidget->context();
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

    // Make the context (and an offscreen surface) current for this thread.
    m_glwidget->makeCurrent();

    if (!m_inited) {
       m_inited = true;
       initializeOpenGLFunctions();
    }

    //qDebug("%p elapsed %lld", QThread::currentThread(), m_elapsed.restart());

    // Clear color buffer
    glViewport(0, 0, m_glwidget->width(), m_glwidget->height());
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Select and setup the projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(65.0f, (GLfloat)m_glwidget->width()/(GLfloat)m_glwidget->height(), 1.0f, 100.0f);

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

    // Make no context current on this thread and move the QOpenGLWidget's
    // context back to the gui thread.
    m_glwidget->doneCurrent();
    ctx->moveToThread(qGuiApp->thread());

    // Schedule composition. Note that this will use QueuedConnection, meaning
    // that update() will be invoked on the gui thread.
    QMetaObject::invokeMethod(m_glwidget, "update");
}

