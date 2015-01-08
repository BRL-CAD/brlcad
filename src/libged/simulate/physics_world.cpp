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

#include "common.h"

#include "physics_world.hpp"
#include "collision.hpp"

#include <stdexcept>
#include <limits>


namespace
{


HIDDEN btRigidBody::btRigidBodyConstructionInfo
build_construction_info(btMotionState &motion_state,
			btCollisionShape &collision_shape, btScalar mass)
{
    btVector3 inertia;
    collision_shape.calculateLocalInertia(mass, inertia);
    return btRigidBody::btRigidBodyConstructionInfo(mass, &motion_state,
	    &collision_shape, inertia);
}


}


namespace simulate
{


class MatrixMotionState : public btMotionState
{
public:
    MatrixMotionState(mat_t matrix);

    virtual void getWorldTransform(btTransform &dest) const;
    virtual void setWorldTransform(const btTransform &transform);


private:
    MatrixMotionState(const MatrixMotionState &source);
    MatrixMotionState &operator=(const MatrixMotionState &source);

    const matp_t m_matrix;
};


MatrixMotionState::MatrixMotionState(mat_t matrix) :
    m_matrix(matrix)
{}


void
MatrixMotionState::getWorldTransform(btTransform &dest) const
{
    /*
    // rotation
    // matrix decomposition

    // translation
    btVector3 translation(0.0, 0.0, 0.0);
    MAT_DELTAS_GET(translation, m_matrix);
    dest.setOrigin(translation);
    */

    btScalar bt_matrix[16];
    MAT_TRANSPOSE(bt_matrix, m_matrix);
    dest.setFromOpenGLMatrix(bt_matrix);
}


void
MatrixMotionState::setWorldTransform(const btTransform &transform)
{
    /*
    // rotation
    // matrix decomposition

    // translation
    MAT_DELTAS_VEC(m_matrix, transform.getOrigin());
    */

    btScalar bt_matrix[16];
    transform.getOpenGLMatrix(bt_matrix);
    MAT_TRANSPOSE(m_matrix, bt_matrix);
}


class WorldObject
{
public:
    WorldObject(matp_t matrix, btScalar mass,
		const btVector3 &bounding_box_dimensions, const btVector3 &linear_velocity,
		const btVector3 &angular_velocity);

    void add_to_world(btDiscreteDynamicsWorld &world);


private:
    bool m_in_world;
    MatrixMotionState m_motion_state;
    RtCollisionShape m_collision_shape;
    btRigidBody m_rigid_body;
};


WorldObject::WorldObject(matp_t matrix, btScalar mass,
			 const btVector3 &bounding_box_dimensions, const btVector3 &linear_velocity,
			 const btVector3 &angular_velocity) :
    m_in_world(false),
    m_motion_state(matrix),
    m_collision_shape(bounding_box_dimensions / 2.0),
    m_rigid_body(build_construction_info(m_motion_state, m_collision_shape, mass))
{
    m_rigid_body.setLinearVelocity(linear_velocity);
    m_rigid_body.setAngularVelocity(angular_velocity);
}


void
WorldObject::add_to_world(btDiscreteDynamicsWorld &world)
{
    if (m_in_world)
	throw std::runtime_error("already in world");
    else {
	m_in_world = true;
	world.addRigidBody(&m_rigid_body);
    }
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
	RtCollisionShape::RT_SHAPE_TYPE, RtCollisionShape::RT_SHAPE_TYPE,
	new RtCollisionAlgorithm::CreateFunc);
    m_world.setGravity(btVector3(0.0, 0.0, -9.8));
}


PhysicsWorld::~PhysicsWorld()
{
    for (std::vector<WorldObject *>::iterator it = m_objects.begin();
	 it != m_objects.end(); ++it)
	delete *it;
}


void
PhysicsWorld::step(btScalar seconds)
{
    m_world.stepSimulation(seconds, std::numeric_limits<int>::max());
}


void
PhysicsWorld::add_object(matp_t matrix, btScalar mass,
			 const btVector3 &bounding_box_dimensions, const btVector3 &linear_velocity,
			 const btVector3 &angular_velocity)
{
    m_objects.push_back(new WorldObject(matrix, mass, bounding_box_dimensions,
					linear_velocity, angular_velocity));
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
