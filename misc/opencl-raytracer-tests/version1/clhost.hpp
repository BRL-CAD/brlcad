/*                      C L H O S T . H P P
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
/** @file clhost.hpp
 *
 * Brief description
 *
 */

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


