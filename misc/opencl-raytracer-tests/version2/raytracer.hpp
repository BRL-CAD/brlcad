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


