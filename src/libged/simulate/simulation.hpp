/*                  S I M U L A T I O N . H P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
 * Brief description
 *
 */


#ifndef SIMULATION_H
#define SIMULATION_H


#include "physics_world.hpp"
#include "world_object.hpp"


namespace simulate
{


class Simulation : public PhysicsWorld
{
public:
    Simulation(db_i &db_instance, directory &vdirectory);
    virtual ~Simulation();


private:
    void get_tree_objects(tree &vtree);

    db_i &m_db_instance;
    directory &m_directory;
    TreeUpdater m_tree_updater;
    std::vector<WorldObject *> m_objects;
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
