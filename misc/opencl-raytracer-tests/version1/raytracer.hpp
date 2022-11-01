/*                   R A Y T R A C E R . H P P
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
/** @file raytracer.hpp
 *
 * Brief description
 *
 */

#ifndef __RAYTRACER_H
#define __RAYTRACER_H


#include "clhost.hpp"


#include <memory>


namespace rt
{


    enum class PrimitiveType { none, light, sphere };


    class Primitive
    {
        public:
            PrimitiveType type = PrimitiveType::none;
            virtual ~Primitive() {}
    };


    struct Light : public Primitive
    {
        double position[3];
        double brightness;

        Light(const double vposition[3], double vbrightness) :
            position{vposition[0], vposition[1], vposition[2]},
            brightness(vbrightness)
        {}
    };


    struct Sphere : public Primitive
    {
        double position[3];
        double radius;
        Sphere(const double vposition[3], double vradius) :
            position{vposition[0], vposition[1], vposition[2]},
            radius(vradius)
        {}
    };


    struct Scene
    {
        std::vector<std::shared_ptr<Primitive>> primitives;
    };


    class Raytracer : public clhost::CLHost
    {
    };


}


#endif


