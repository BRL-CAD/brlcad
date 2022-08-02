/*                        C A M E R A . H
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
/** @file Camera.h */

#ifndef RT3_CAMERABK_H
#define RT3_CAMERABK_H


#include <QMatrix4x4>
#include <QVector3D>


class Camera {
public:
    QVector3D  axisX = QVector3D(1.0f,0.0f,0.0f);
    QVector3D  axisY = QVector3D(0.0f,1.0f,0.0f);
    QVector3D  axisZ = QVector3D(0.0f,0.0f,1.0f);

    virtual void setWH(float w, float h) = 0;
    virtual void processMoveRequest(const int &deltaX, const int &deltaY) = 0;
    virtual void processRotateRequest(const int & deltaX, const int & deltaY, const bool& thirdAxis) = 0;
    virtual void processZoomRequest(const int & deltaWheelAngle) = 0;

    virtual QMatrix4x4 modelViewMatrix() const = 0;
    virtual QMatrix4x4 modelViewMatrixNoTranslate() const = 0;
    virtual QMatrix4x4 projectionMatrix() const = 0;

    virtual void setEyePosition(float x, float y, float z) = 0;

};


#endif //RT3_CAMERABK_H
