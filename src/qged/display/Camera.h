/*                        C A M E R A . H
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
/** @file Camera.h */

#ifndef RT3_CAMERABK_H
#define RT3_CAMERABK_H


#include <glm/detail/type_mat4x4.hpp>


class Camera {
public:
    glm::vec3  axisX = glm::vec3(1.0f,0.0f,0.0f);
    glm::vec3  axisY = glm::vec3(0.0f,1.0f,0.0f);
    glm::vec3  axisZ = glm::vec3(0.0f,0.0f,1.0f);

    virtual void setWH(float w, float h) = 0;
    virtual void processMoveRequest(const int &deltaX, const int &deltaY) = 0;
    virtual void processRotateRequest(const int & deltaX, const int & deltaY, const bool& thirdAxis) = 0;
    virtual void processZoomRequest(const int & deltaWheelAngle) = 0;

    virtual glm::mat4 modelViewMatrix() const = 0;
    virtual glm::mat4 projectionMatrix() const = 0;

};


#endif //RT3_CAMERABK_H
