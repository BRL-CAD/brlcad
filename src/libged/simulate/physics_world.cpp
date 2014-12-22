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
    WorldObject(const btVector3 &bounding_box_dimensions, btScalar mass,
		matp_t matrix);

    void add_to_world(btDiscreteDynamicsWorld &world);
    void read_matrix();
    void write_matrix();


private:
    WorldObject(const WorldObject &source);
    WorldObject &operator=(const WorldObject &source);

    static btTransform matrix_to_transform(const mat_t matrix);
    static btRigidBody::btRigidBodyConstructionInfo build_construction_info(
	btCollisionShape &collision_shape, matp_t const matrix, btScalar mass);


    bool m_in_world;
    const matp_t m_matrix;
    RtCollisionShape m_collision_shape;
    btRigidBody m_rigid_body;
};


btTransform
PhysicsWorld::WorldObject::matrix_to_transform(const mat_t matrix)
{
    // FIXME: use only translation and rotation information

    btTransform xform;
    {
	btScalar bt_matrix[16];
	MAT_TRANSPOSE(bt_matrix, matrix);
	// scale mm to m
	bt_matrix[12] /= 1000;
	bt_matrix[13] /= 1000;
	bt_matrix[14] /= 1000;
	xform.setFromOpenGLMatrix(bt_matrix);
    }

    return xform;
}


btRigidBody::btRigidBodyConstructionInfo
PhysicsWorld::WorldObject::build_construction_info(btCollisionShape
	&collision_shape, matp_t const matrix, btScalar mass)
{
    btVector3 inertia;
    collision_shape.calculateLocalInertia(mass, inertia);
    btRigidBody::btRigidBodyConstructionInfo construction_info(mass, NULL,
	    &collision_shape, inertia);
    construction_info.m_startWorldTransform = matrix_to_transform(matrix);

    return construction_info;
}


PhysicsWorld::WorldObject::WorldObject(const btVector3 &bounding_box_dimensions,
				       btScalar mass, matp_t matrix) :
    m_in_world(false),
    m_matrix(matrix),
    m_collision_shape(bounding_box_dimensions / 2),
    m_rigid_body(build_construction_info(m_collision_shape, m_matrix, mass))
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
    m_rigid_body.setCenterOfMassTransform(matrix_to_transform(m_matrix));
}


void
PhysicsWorld::WorldObject::write_matrix()
{
    // FIXME: write only translation and rotation information

    btScalar bt_matrix[16];
    m_rigid_body.getCenterOfMassTransform().getOpenGLMatrix(bt_matrix);
    //scale m to mm
    bt_matrix[12] *= 1000;
    bt_matrix[13] *= 1000;
    bt_matrix[14] *= 1000;
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
	RtCollisionShape::RT_SHAPE_TYPE, RtCollisionShape::RT_SHAPE_TYPE,
	new RtCollisionAlgorithm::CreateFunc);
    m_world.setGravity(btVector3(0, 0, static_cast<btScalar>(-9.8)));
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
    for (std::vector<WorldObject *>::iterator it = m_objects.begin();
	 it != m_objects.end(); ++it)
	(*it)->read_matrix();

    m_world.stepSimulation(seconds, std::numeric_limits<int>::max());

    for (std::vector<WorldObject *>::const_iterator it = m_objects.begin();
	 it != m_objects.end(); ++it)
	(*it)->write_matrix();
}


void
PhysicsWorld::add_object(const vect_t &cad_bounding_box_dimensions,
			 fastf_t mass, matp_t matrix)
{
    btVector3 bounding_box_dimensions;
    // scale mm to m
    VSCALE(bounding_box_dimensions, cad_bounding_box_dimensions, 1e-3);
    m_objects.push_back(new WorldObject(bounding_box_dimensions, mass, matrix));
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
