#ifndef __CLHOST_H__
#define __CLHOST_H__


#include <OpenCL/cl.hpp>


namespace clhost
{


    class CLHost
    {
        protected:
            cl::Context *context;
            std::vector<cl::Device> devices;
            std::vector<cl::CommandQueue> queues;
            std::vector<cl::Program> programs;
            cl::Program &add_program(const std::vector<std::string> &vsources);


        public:
            CLHost();
            CLHost(const CLHost &) = delete;
            CLHost &operator=(const CLHost &) = delete;


            virtual ~CLHost()
            {
                delete context;
            }
    };


}


#endif


