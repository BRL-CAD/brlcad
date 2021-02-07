/*                     U T I L I T Y . H P P
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
/** @file utility.hpp
 *
 * Brief description
 *
 */

#ifndef __UTILITY_H__
#define __UTILITY_H__


#include <stdexcept>
#include <sstream>


#ifdef DEBUG_BUILD
#define DEBUG(x) do {x;} while (0)
#else
#define DEBUG(x) do {} while (0)
#endif


namespace utility
{


    // modified from Boost
    class bad_lexical_cast : public std::runtime_error
    {
        public:
            bad_lexical_cast(const std::string &vwhat) : std::runtime_error(vwhat) {}
    };


    template<typename Target, typename Source>
        Target lexical_cast(const Source arg)
        {
            std::stringstream interpreter;
            Target result;
            if(!(interpreter << arg) || !(interpreter >> result) || !(interpreter >> std::ws).eof())
                throw bad_lexical_cast("invalid conversion value");
            return result;
        }



}


#endif


