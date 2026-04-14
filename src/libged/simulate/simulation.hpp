/*                  S I M U L A T I O N . H P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2025 United States Government as represented by
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

#include <string>

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


    explicit Simulation(db_i &db, const db_full_path &path,
		       bool use_saved_state = false);
    ~Simulation();

    void step(fastf_t seconds, DebugMode debug_mode);

    // Save current body velocities to the database as simulate::state_* attributes
    // so the simulation can be resumed later with --resume.
    void saveState();

    // Write a .pl wireframe plot file showing the current AABB of each body.
    // Dynamic bodies are drawn in green; static bodies are drawn in red.
    void writePlotFile(const std::string &filename) const;


private:
    class Region;

    void updateRoiProxies(fastf_t seconds);

    // ROI update constants
    static const btScalar ROI_MAX_PREDICTION_TIME; // Maximum time horizon for prediction
    static const btScalar ROI_PADDING; // Padding around dynamic bodies in meters
    static const btScalar ROI_MIN_SIZE; // Minimum ROI size in meters
    static const btScalar ROI_CHANGE_THRESHOLD; // Minimum change to trigger broadphase update

    db_i &m_db;
    RtDebugDraw m_debug_draw;
    btDbvtBroadphase m_broadphase;
    btDefaultCollisionConfiguration m_collision_config;
    btCollisionDispatcher m_collision_dispatcher;
    btSequentialImpulseConstraintSolver m_constraint_solver;
    btDiscreteDynamicsWorld m_world;
    std::vector<Region *> m_regions;
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
