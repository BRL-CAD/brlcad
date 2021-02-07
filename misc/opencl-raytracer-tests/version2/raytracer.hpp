/*                   R A Y T R A C E R . H P P
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
/** @file raytracer.hpp
 *
 * Brief description
 *
 */

#ifndef __RAYTRACER_H__
#define __RAYTRACER_H__


#include <OpenCL/cl.h>


struct Sphere
{
    alignas(sizeof(cl_float3)) cl_float3 position = {{0, 0, 0}};
    alignas(sizeof(cl_float)) cl_float radius = 0;
    alignas(sizeof(cl_int)) cl_int is_light = 0;
};


namespace rt
{


    class Raytracer
    {
        cl_device_id device;
        cl_context context;
        cl_command_queue queue;
        cl_program program;


        static cl_device_id get_device();
        static cl_program get_program(cl_context context, cl_device_id device);


	Raytracer(const Raytracer &src) = delete;
	Raytracer &operator=(const Raytracer &src) = delete;

        public:
        Raytracer();
	~Raytracer();
        void ray_trace();
    };


}


#endif


