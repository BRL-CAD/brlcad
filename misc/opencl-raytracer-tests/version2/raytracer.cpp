/*                   R A Y T R A C E R . C P P
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
/** @file raytracer.cpp
 *
 * Brief description
 *
 */

#include "raytracer.hpp"


#include <memory>
#include <chrono>
#include <thread>
#include <fstream>


namespace rt
{

    Raytracer::Raytracer() :
	device(get_device()),
	context(),
	queue(),
	program()
    {
	cl_int error;
	context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &error);
	if (error != CL_SUCCESS) throw std::runtime_error("failed to initialize context");
	queue = clCreateCommandQueue(context, device, 0, &error);
	if (error != CL_SUCCESS) {
	    clReleaseContext(context);
	    throw std::runtime_error("failed to create command queue");
	}
	program = get_program(context, device);
    }


    Raytracer::~Raytracer()
    {
	clReleaseCommandQueue(queue);
	clReleaseProgram(program);
	clReleaseContext(context);
    }



    // for now, just get the first device from the first platform
    cl_device_id Raytracer::get_device()
    {
	cl_platform_id platform;
	if (clGetPlatformIDs(1, &platform, nullptr) != CL_SUCCESS)
	    throw std::runtime_error("failed to find a platform");

	cl_device_id device;
	cl_int error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
	if (error == CL_DEVICE_NOT_FOUND)
	    error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, nullptr);
	if (error != CL_SUCCESS)
	    throw std::runtime_error("failed to find a device");

	return device;
    }


    cl_program Raytracer::get_program(cl_context context, cl_device_id device)
    {
	const char * const path = "rt.cl";

	std::ifstream t(path);
	std::string code((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

	cl_int error;
	const char *ccode = code.c_str();
	const std::size_t code_size = code.size();
	cl_program program = clCreateProgramWithSource(context, 1, &ccode, &code_size, &error);
	if (error != CL_SUCCESS) throw std::runtime_error("failed to initialize program");

	if (clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr) != CL_SUCCESS) {
	    std::size_t log_size;
	    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
	    std::unique_ptr<char[]> log(new char[log_size]);
	    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size+1, log.get(), nullptr);
	    throw std::runtime_error(std::string("failed to build program\nbuild log:\n") + log.get());
	}

	return program;
    }


    void Raytracer::ray_trace()
    {
	const char * const kernel_name = "ray_trace";
	const cl_int nspheres = 10;

	cl_int error;
	// OpenCL images are not supported on this laptop's CPU implementation
	cl_mem output = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY, 400*300*sizeof(cl_uchar)*4, nullptr, &error);
	if (error != CL_SUCCESS) throw std::runtime_error("failed to create output buffer");

	Sphere spheres[nspheres]; spheres[0].is_light=1234567890;
	cl_mem input = clCreateBuffer(context, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, nspheres*sizeof(Sphere), spheres, &error);
	if (error != CL_SUCCESS) {
	    clReleaseMemObject(output);
	    throw std::runtime_error("failed to create input buffer");
	}

	cl_kernel kernel = clCreateKernel(program, kernel_name, &error);
	if (error != CL_SUCCESS) {
	    clReleaseMemObject(input);
	    clReleaseMemObject(output);
	    throw std::runtime_error("failed to create kernel");
	}

	error = clSetKernelArg(kernel, 0, sizeof(cl_mem), &output);
	error |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &input);
	if (error != CL_SUCCESS) {
	    clReleaseKernel(kernel);
	    clReleaseMemObject(input);
	    clReleaseMemObject(output);
	    throw std::runtime_error("failed to set kernel arguments");
	}

	std::size_t global_size = nspheres;
	error = clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_size, nullptr, 0, nullptr, nullptr);
	clReleaseKernel(kernel);
	clReleaseMemObject(input);
	clReleaseMemObject(output);
	if (error != CL_SUCCESS) throw std::runtime_error("failed to enqueue kernel");

	std::this_thread::sleep_for(std::chrono::seconds(2));
    }


}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
