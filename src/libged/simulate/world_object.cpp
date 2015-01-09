/*                W O R L D _ O B J E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file world_object.cpp
 *
 * Brief description
 *
 */


#ifdef HAVE_BULLET


#include "common.h"

#include "world_object.hpp"

#include <stdexcept>


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


MatrixMotionState::MatrixMotionState(mat_t matrix, TreeUpdater &tree_updater) :
    m_matrix(matrix),
    m_tree_updater(tree_updater)
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

    m_tree_updater.mark_modified();
}


rt_i &
MatrixMotionState::get_rt_instance() const
{
    return m_tree_updater.get_rt_instance();
}


WorldObject::WorldObject(mat_t matrix, TreeUpdater &tree_updater, btScalar mass,
			 const btVector3 &bounding_box_dimensions, const btVector3 &linear_velocity,
			 const btVector3 &angular_velocity) :
    m_in_world(false),
    m_motion_state(matrix, tree_updater),
    m_collision_shape(bounding_box_dimensions / 2.0),
    m_rigid_body(build_construction_info(m_motion_state, m_collision_shape, mass))
{
    m_rigid_body.setLinearVelocity(linear_velocity);
    m_rigid_body.setAngularVelocity(angular_velocity);
}


void
WorldObject::add_to_world(PhysicsWorld &world)
{
    if (m_in_world)
	throw std::runtime_error("already in world");
    else {
	m_in_world = true;
	world.add_rigid_body(m_rigid_body);
    }
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
