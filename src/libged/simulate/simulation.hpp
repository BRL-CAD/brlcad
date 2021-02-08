/*                  S I M U L A T I O N . H P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file simulation.hpp
 *
 * Programmatic interface for simulate.
 *
 */


#ifndef SIMULATE_SIMULATION_H
#define SIMULATE_SIMULATION_H


#include "common.h"

#include "rt_debug_draw.hpp"
#include "rt_instance.hpp"

#include "rt/rt_instance.h"


namespace simulate
{


class Simulation
{
public:
    enum DebugMode {
	debug_none = 0,
	debug_aabb = 1 << 0,
	debug_contact = 1 << 1,
	debug_ray = 1 << 2
    };


    explicit Simulation(db_i &db, const db_full_path &path);
    ~Simulation();

    void step(fastf_t seconds, DebugMode debug_mode);


private:
    class Region;

    RtDebugDraw m_debug_draw;
    btDbvtBroadphase m_broadphase;
    btDefaultCollisionConfiguration m_collision_config;
    btCollisionDispatcher m_collision_dispatcher;
    btSequentialImpulseConstraintSolver m_constraint_solver;
    btDiscreteDynamicsWorld m_world;
    std::vector<const Region *> m_regions;
    const RtInstance m_rt_instance;
};


inline Simulation::DebugMode
operator|(Simulation::DebugMode a, Simulation::DebugMode b)
{
    return static_cast<Simulation::DebugMode>(
	       static_cast<int>(a) | static_cast<int>(b));
}


inline bool
operator&(Simulation::DebugMode a, Simulation::DebugMode b)
{
    return static_cast<int>(a) & static_cast<int>(b);
}


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
