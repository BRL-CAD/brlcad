/*          O R T H O G R A P H I C C A M E R A . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file OrthographicCamera.cpp */


#include "common.h"

#include "OrthographicCamera.h"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

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

OrthographicCamera::OrthographicCamera() = default;

glm::mat4 OrthographicCamera::modelViewMatrix() const {
    auto rotationMatrixAroundX = glm::rotate(glm::radians(angleAroundAxes.x), axisX);
    auto rotationMatrixAroundY = glm::rotate(glm::radians(angleAroundAxes.y), axisY);
    auto rotationMatrixAroundZ = glm::rotate(glm::radians(angleAroundAxes.z), axisZ);
    auto rotationMatrix = rotationMatrixAroundX * rotationMatrixAroundY * rotationMatrixAroundZ;

    return glm::translate(rotationMatrix, eyePosition);
}

glm::mat4 OrthographicCamera::projectionMatrix() const {
    return glm::ortho(-zoom * w / h, zoom * w / h, -zoom, zoom, nearPlane, farPlane);
}

void OrthographicCamera::setWH(float w, float h) {
    this->w = w;
    this->h = h;
}

void OrthographicCamera::processRotateRequest(const int &deltaX, const int &deltaY, const bool &thirdAxis) {
    if (deltaX < -mouseMaxDrag || deltaX > +mouseMaxDrag || deltaY < -mouseMaxDrag || deltaY > +mouseMaxDrag) {
        return;
    }

    float deltaAngleX = float(deltaX) / h;
    float deltaAngleY = float(deltaY) / h;
    if (thirdAxis) {
        angleAroundAxes.y += deltaAngleX * eyeRotationPerMouseDelta;
    } else {
        angleAroundAxes.z += deltaAngleX * eyeRotationPerMouseDelta;
    }
    angleAroundAxes.x += deltaAngleY * eyeRotationPerMouseDelta;

}

void OrthographicCamera::processMoveRequest(const int &deltaX, const int &deltaY) {
    if (deltaX < -mouseMaxDrag || deltaX > +mouseMaxDrag || deltaY < -mouseMaxDrag || deltaY > +mouseMaxDrag) {
        return;
    }

    auto rotationMatrixAroundZ = glm::rotate(glm::radians(-angleAroundAxes.z), axisZ);
    auto rotationMatrixAroundY = glm::rotate(glm::radians(-angleAroundAxes.y), axisY);
    auto rotationMatrixAroundX = glm::rotate(glm::radians(-angleAroundAxes.x), axisX);
    auto rotationMatrix = rotationMatrixAroundZ * rotationMatrixAroundY * rotationMatrixAroundX;

    glm::vec3 cameraRightDirection(rotationMatrix * glm::vec4(axisX, 1.0));
    eyePosition += float(deltaX) * eyeMovementPerMouseDelta * cameraRightDirection * zoom;

    glm::vec3 cameraUpDirection(rotationMatrix * glm::vec4(axisY, 1.0));
    eyePosition -= float(deltaY) * eyeMovementPerMouseDelta * cameraUpDirection * zoom;
}

void OrthographicCamera::processZoomRequest(const int &deltaWheelAngle) {
    float zoomFactor = 1 - zoomFactorMultiplier * float(deltaWheelAngle);
    zoom = pow(zoom, zoomFactor);
    if (zoom < zoomLowerBound) zoom = zoomLowerBound;
}

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

