#include "clhost.hpp"
#include "utility.hpp"


#include <stdexcept>


namespace clhost
{


    CLHost::CLHost() : context(nullptr), devices(), queues(), programs()
    {
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        for (const cl::Platform &platform : platforms) {
            std::vector<cl::Device> platform_devices;
            platform.getDevices(CL_DEVICE_TYPE_ALL, &platform_devices);
            devices.insert(devices.end(), platform_devices.begin(), platform_devices.end());
        }
        if (devices.empty()) throw std::runtime_error("no devices found");
        D("found " << devices.size() << " devices on " << platforms.size() << " platforms");

        cl_int error;
        context = new cl::Context(devices, nullptr, nullptr, nullptr, &error);
        if (error != CL_SUCCESS) {
            delete context;
            throw std::runtime_error("failed to create context");
        }

        for (const cl::Device &device : devices) {
            queues.emplace_back(*context, device, 0, &error);
            if (error != CL_SUCCESS)
                throw std::runtime_error("failed to create device command queue");
        }
    }


    cl::Program &CLHost::add_program(const std::vector<std::string> &vsources)
    {
        cl::Program::Sources sources;
        for (const std::string &s : vsources)
            sources.emplace_back(s.c_str(), s.size());

        cl_int error;
        programs.emplace_back(*context, sources, &error);
        if (error != CL_SUCCESS) throw std::runtime_error("failed to initialize cl::Program");
        error = programs.back().build(devices, nullptr, nullptr, nullptr);
        if (error != CL_SUCCESS) throw std::runtime_error("failed to build program");
        return programs.back();
    }


}


