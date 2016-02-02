/*               P H Y S I C S _ W O R L D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2016 United States Government as represented by
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
/** @file physics_world.cpp
 *
 * Brief description
 *
 */


#ifdef HAVE_BULLET

#include "common.h"

#include "physics_world.hpp"


namespace simulate
{


PhysicsWorld::PhysicsWorld() :
    m_broadphase(),
    m_collision_config(),
    m_collision_dispatcher(&m_collision_config),
    m_constraint_solver(),
    m_world(&m_collision_dispatcher, &m_broadphase, &m_constraint_solver,
	    &m_collision_config)
{}


PhysicsWorld::~PhysicsWorld()
{}


void
PhysicsWorld::step(btScalar seconds)
{
    for (int i = 0; i < 600.0 * seconds; ++i)
	m_world.stepSimulation(1.0 / 600.0, 6000);
}


void
PhysicsWorld::add_rigid_body(btRigidBody &rigid_body)
{
    m_world.addRigidBody(&rigid_body);
}


void
PhysicsWorld::remove_rigid_body(btRigidBody &rigid_body)
{
    m_world.removeRigidBody(&rigid_body);
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
