/*                       D I S P L A Y . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file Display.h */


#ifndef RT3_DISPLAY_H
#define RT3_DISPLAY_H

#include <QtWidgets/QOpenGLWidget>
#include "Camera.h"
#include "AxesRenderer.h"
#include <QMouseEvent>
#include <rt/db_instance.h>

class DisplayManager;
class GeometryRenderer;


class Display : public QOpenGLWidget{

Q_OBJECT
public:
    Display();

    Camera *camera;
    void onDatabaseUpdated();
    void refresh();

    int getW() const;
    int getH() const;

protected:
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *k) override ;

private:
    int w = 400;
    int h = 400;
    int prevMouseX = -1;
    int prevMouseY = -1;
    bool skipNextMouseMoveEvent = false;
    float keyPressSimulatedMouseMoveDistance = 8;

    Qt::MouseButton rotateCameraMouseButton = Qt::LeftButton;
    Qt::MouseButton moveCameraMouseButton = Qt::RightButton;
    Qt::KeyboardModifier rotateAroundThirdAxisModifier = Qt::ShiftModifier;

    DisplayManager *displayManager;
    rt_wdb *database = nullptr;
    GeometryRenderer * geometryRenderer;
    AxesRenderer * axesRenderer;
    std::vector<Renderable*> renderers;

public slots:
    void onDatabaseOpen();
};


#endif //RT3_DISPLAY_H
