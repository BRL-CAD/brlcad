/*                     D I S P L A Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file Display.cpp */


#include "Display.h"
#include <QtWidgets/QApplication>
#include <OrthographicCamera.h>
#include "DisplayManager.h"
#include "GeometryRenderer.h"
#include "cadapp.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wshadow"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#endif


using namespace std;


BRLCADDisplay::BRLCADDisplay() {
    camera = new OrthographicCamera();
    displayManager = new DisplayManager(this);
    geometryRenderer = new GeometryRenderer(displayManager);
    axesRenderer = new AxesRenderer();

    resizeTimer = new QTimer(this);
    resizeTimer->setSingleShot(true);
    resizeTimer->setInterval(1); 
    connect(resizeTimer, SIGNAL(timeout()), SLOT(enableResize()));

    renderers.push_back(geometryRenderer);
    renderers.push_back(axesRenderer);
}

void
BRLCADDisplay::resizeEvent(QResizeEvent *e)
{
    resizeTimer->start();
    //setUpdatesEnabled(false);
    QOpenGLWidget::resizeEvent(e);
}

void BRLCADDisplay::resizeGL(int w, int h) {
    camera->setWH(w,h);
    this->w = w;
    this->h = h;
}

void BRLCADDisplay::paintGL() {
    displayManager->drawBegin();

    displayManager->loadMatrix(camera->modelViewMatrix().data());
    displayManager->loadPMatrix(camera->projectionMatrix().data());
 
    for (auto i:renderers) i->render();
}

void BRLCADDisplay::refresh() {
    makeCurrent();
    update();
}


void BRLCADDisplay::mouseMoveEvent(QMouseEvent *event) {
    int x = event->x();
    int y = event->y();
    int globalX = event->globalX();
    int globalY = event->globalY();

    bool resetX = false, resetY = false;

    if(prevMouseX != -1 && prevMouseY != -1 && (event->buttons() & (rotateCameraMouseButton|moveCameraMouseButton))) {
        if (skipNextMouseMoveEvent) {
            skipNextMouseMoveEvent = false;
            return;
        }
        if(event->buttons() & (rotateCameraMouseButton)) {
            bool rotateThirdAxis = QApplication::keyboardModifiers().testFlag(rotateAroundThirdAxisModifier);
            camera->processRotateRequest(x- prevMouseX, y - prevMouseY,rotateThirdAxis);
        }
        if(event->buttons() & (moveCameraMouseButton)){
            camera->processMoveRequest(x- prevMouseX, y - prevMouseY);
        }

        refresh();

        auto topLeft = mapToGlobal(QPoint(0,0));
        auto bottomRight = mapToGlobal(QPoint(size().width(),size().height()));

        int newX = -1;
        int newY = -1;

        if (globalX <= topLeft.x()) {
            newX = bottomRight.x()-1;
            resetX = true;
        }
        if (globalX >= bottomRight.x()) {
            newX = topLeft.x()+1;
            resetX = true;
        }
        if (globalY <= topLeft.y()) {
            newY = bottomRight.y()-1;
            resetY = true;
        }
        if (globalY >= bottomRight.y()) {
            newY = topLeft.y()+1;
            resetY = true;
        }

        if (resetX || resetY) {
            prevMouseX = resetX ? mapFromGlobal(QPoint(newX,newY)).x() : x;
            prevMouseY = resetY ? mapFromGlobal(QPoint(newX,newY)).y() : y;
            QCursor::setPos(resetX ? newX : globalX, resetY ? newY : globalY);
            skipNextMouseMoveEvent = true;
        }
    }

    if(!resetX && !resetY) {
        prevMouseX = x;
        prevMouseY = y;
    }
}

void BRLCADDisplay::mousePressEvent(QMouseEvent *event) {
    prevMouseX = event->x();
    prevMouseY = event->y();
}

void BRLCADDisplay::mouseReleaseEvent(QMouseEvent *UNUSED(event)) {
    prevMouseX = -1;
    prevMouseY = -1;
}

void BRLCADDisplay::wheelEvent(QWheelEvent *event) {

    if (event->phase() == Qt::NoScrollPhase || event->phase() == Qt::ScrollUpdate || event->phase() == Qt::ScrollMomentum) {
        camera->processZoomRequest(event->angleDelta().y() / 8);
        refresh();
    }
}

void BRLCADDisplay::keyPressEvent( QKeyEvent *k ) {
    switch (k->key()) {
        case Qt::Key_Up:
            camera->processMoveRequest(0, keyPressSimulatedMouseMoveDistance);
            refresh();
            break;
        case Qt::Key_Down:
            camera->processMoveRequest(0, -keyPressSimulatedMouseMoveDistance);
            refresh();
            break;
        case Qt::Key_Left:
            camera->processMoveRequest(keyPressSimulatedMouseMoveDistance, 0);
            refresh();
            break;
        case Qt::Key_Right:
            camera->processMoveRequest(-keyPressSimulatedMouseMoveDistance, 0);
            refresh();
            break;
    }
}

void BRLCADDisplay::onDatabaseUpdated() {
    geometryRenderer->onDatabaseUpdated();
}

int BRLCADDisplay::getW() const {
    return w;
}

int BRLCADDisplay::getH() const {
    return h;
}

void BRLCADDisplay::onDatabaseOpen() {
    makeCurrent();
    geometryRenderer->setDatabase(((CADApp *)qApp)->wdbp());
    onDatabaseUpdated();
    update();
}

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

