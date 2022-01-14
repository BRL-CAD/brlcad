/*                     U T I L I T Y . H P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
 * Helpers used by simulate.
 *
 */


#ifndef SIMULATE_UTILITY_H
#define SIMULATE_UTILITY_H


#include "common.h"

#include <sstream>
#include <stdexcept>
#include <vector>

#include "bu/malloc.h"
#include "rt/db_instance.h"

#include "btBulletDynamicsCommon.h"


namespace simulate
{


// Bullet units are meters; application units are millimeters
static const btScalar world_to_application = 1.0e3;


namespace detail
{


template <typename T> void
autoptr_wrap_bu_free(T * const ptr)
{
    bu_free(ptr, "AutoPtr");
}


}


class InvalidSimulationError : public std::runtime_error
{
public:
    explicit InvalidSimulationError(const std::string &value) :
	std::runtime_error(value)
    {}
};


template <typename Target, typename Source>
Target lexical_cast(Source arg, const std::string &message)
{
    std::stringstream interpreter;
    Target result;

    if (!(interpreter << arg) ||
	!(interpreter >> result) ||
	!(interpreter >> std::ws).eof())
	throw InvalidSimulationError(message);

    return result;
}


template <typename T, void free_fn(T *) = detail::autoptr_wrap_bu_free>
struct AutoPtr {
    explicit AutoPtr(T * const value) :
	ptr(value)
    {}


    ~AutoPtr()
    {
	if (ptr)
	    free_fn(ptr);
    }


    T * const ptr;


private:
    AutoPtr(const AutoPtr &source);
    AutoPtr &operator=(const AutoPtr &source);
};


// When an object of this class is constructed, it ensures that the object at
// the specified path is the topmost region within the path (note: any child
// regions placed below this object in the hierarchy will remain as regions;
// this does not impact ray tracing results). Reverses any modifications upon
// destruction.
class TemporaryRegionHandle
{
public:
    explicit TemporaryRegionHandle(db_i &db, const db_full_path &path);
    ~TemporaryRegionHandle();


private:
    TemporaryRegionHandle(const TemporaryRegionHandle &source);
    TemporaryRegionHandle &operator=(const TemporaryRegionHandle &source);

    db_i &m_db;
    directory &m_dir;
    bool m_dir_modified;
    std::vector<directory *> m_parent_regions;
};


}


#endif


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
