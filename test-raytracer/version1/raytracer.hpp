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


