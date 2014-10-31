/*               P H Y S I C S _ W O R L D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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


#include "physics_world.hpp"
#include "collision.hpp"

#include <stdexcept>
#include <limits>


namespace simulate
{


class PhysicsWorld::WorldObject
{
public:
    WorldObject(const btVector3 &bounding_box, btScalar mass, matp_t matrix);

    void add_to_world(btDiscreteDynamicsWorld &world);
    void read_matrix();
    void write_matrix();


private:
    WorldObject(const WorldObject &source);
    WorldObject &operator=(const WorldObject &source);
    static btVector3 calculate_inertia(const btCollisionShape &collision_shape,
				       btScalar mass);

    bool m_in_world;
    matp_t m_matrix;
    btDefaultMotionState m_motion_state;
    collision::RtCollisionShape m_collision_shape;
    btRigidBody m_rigid_body;
};


btVector3
PhysicsWorld::WorldObject::calculate_inertia(const btCollisionShape
	&collision_shape, btScalar mass)
{
    btVector3 result;
    collision_shape.calculateLocalInertia(mass, result);
    return result;
}


PhysicsWorld::WorldObject::WorldObject(const btVector3 &bounding_box,
				       btScalar mass, matp_t matrix) :
    m_in_world(false),
    m_matrix(matrix),
    m_motion_state(),
    m_collision_shape(bounding_box),
    m_rigid_body(mass, &m_motion_state, &m_collision_shape,
		 calculate_inertia(m_collision_shape, mass))
{}


void
PhysicsWorld::WorldObject::add_to_world(btDiscreteDynamicsWorld &world)
{
    if (m_in_world)
	throw std::runtime_error("already in world");
    else {
	m_in_world = true;
	world.addRigidBody(&m_rigid_body);
    }
}


void
PhysicsWorld::WorldObject::read_matrix()
{
    btTransform xform;
    {
	btScalar bt_matrix[16];
	MAT_TRANSPOSE(bt_matrix, m_matrix);
	xform.setFromOpenGLMatrix(bt_matrix);
    }

    m_motion_state.setWorldTransform(xform);
}


void
PhysicsWorld::WorldObject::write_matrix()
{
    btTransform xform;
    m_motion_state.getWorldTransform(xform);
    btScalar bt_matrix[16];
    xform.getOpenGLMatrix(bt_matrix);
    MAT_TRANSPOSE(m_matrix, bt_matrix);
}


PhysicsWorld::PhysicsWorld() :
    m_broadphase(),
    m_collision_config(),
    m_collision_dispatcher(&m_collision_config),
    m_constraint_solver(),
    m_world(&m_collision_dispatcher, &m_broadphase, &m_constraint_solver,
	    &m_collision_config),
    m_objects()
{
    m_collision_dispatcher.registerCollisionCreateFunc(
	collision::RT_SHAPE_TYPE,
	collision::RT_SHAPE_TYPE,
	new collision::RtCollisionAlgorithm::CreateFunc);
    m_world.setGravity(btVector3(0, 0, -9.8));
}


PhysicsWorld::~PhysicsWorld()
{
    for (std::vector<WorldObject *>::iterator it = m_objects.begin();
	 it != m_objects.end(); ++it)
	delete *it;
}


void
PhysicsWorld::step(fastf_t seconds)
{
    for (std::vector<WorldObject *>::iterator it = m_objects.begin();
	 it != m_objects.end(); ++it)
	(*it)->read_matrix();

    m_world.stepSimulation(seconds, std::numeric_limits<int>::max());

    for (std::vector<WorldObject *>::const_iterator it = m_objects.begin();
	 it != m_objects.end(); ++it)
	(*it)->write_matrix();
}


void
PhysicsWorld::add_object(const btVector3 &bounding_box, btScalar mass,
			 matp_t matrix)
{
    m_objects.push_back(new WorldObject(bounding_box, mass, matrix));
    m_objects.back()->add_to_world(m_world);
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
