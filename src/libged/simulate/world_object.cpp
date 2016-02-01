/*                W O R L D _ O B J E C T . C P P
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
/** @file world_object.cpp
 *
 * Brief description
 *
 */


#ifdef HAVE_BULLET


#include "common.h"

#include "world_object.hpp"

#include <sstream>
#include <stdexcept>


namespace
{


static const std::string attribute_prefix = "simulate::";


template <typename T, void (*Destructor)(T *)>
struct AutoDestroyer {
    AutoDestroyer(T *vptr) : ptr(vptr) {}


    ~AutoDestroyer()
    {
	Destructor(ptr);
    }


    T * const ptr;


private:
    AutoDestroyer(const AutoDestroyer &source);
    AutoDestroyer &operator=(const AutoDestroyer &source);
};


template<typename Target, typename Source>
Target lexical_cast(Source arg,
		    const std::string &description = "bad lexical_cast")
{
    std::stringstream interpreter;
    Target result;

    if (!(interpreter << arg) ||
	!(interpreter >> result) ||
	!(interpreter >> std::ws).eof())
	throw std::invalid_argument(description);

    return result;
}


HIDDEN std::pair<btVector3, btVector3>
get_bounding_box(db_i &db_instance, directory &dir)
{
    btVector3 bounding_box_min, bounding_box_max;
    {
	point_t bb_min, bb_max;

	if (rt_bound_internal(&db_instance, &dir, bb_min, bb_max))
	    throw std::runtime_error("failed to get bounding box");

	VMOVE(bounding_box_min, bb_min);
	VMOVE(bounding_box_max, bb_max);
    }

    btVector3 bounding_box_dims = bounding_box_max - bounding_box_min;
    btVector3 bounding_box_pos = bounding_box_dims / 2.0 + bounding_box_min;
    return std::make_pair(bounding_box_pos, bounding_box_dims);
}


HIDDEN btScalar
get_volume(db_i &db_instance, directory &dir)
{
    rt_db_internal internal;

    if (rt_db_get_internal(&internal, &dir, &db_instance, bn_mat_identity,
			   &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    AutoDestroyer<rt_db_internal, rt_db_free_internal> internal_autodestroy(
	&internal);

    if (internal.idb_meth->ft_volume) {
	fastf_t volume;
	internal.idb_meth->ft_volume(&volume, &internal);
	return volume;
    }

    // approximate volume using the bounding box
    btVector3 bounding_box_dims = get_bounding_box(db_instance, dir).second;
    return bounding_box_dims.getX() * bounding_box_dims.getY() *
	   bounding_box_dims.getZ();
}


HIDDEN btVector3
deserialize_vector(const std::string &source)
{
    std::istringstream stream(source);
    btVector3 result;

    if ((stream >> std::ws).get() != '<')
	throw std::invalid_argument("invalid vector");

    for (int i = 0; i < 3; ++i) {
	std::string value;
	std::getline(stream, value, i != 2 ? ',' : '>');
	result[i] = lexical_cast<btScalar>(value, "invalid vector");
    }

    if (stream.unget().get() != '>' || !(stream >> std::ws).eof())
	throw std::invalid_argument("invalid vector");

    return result;
}


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


MatrixMotionState::MatrixMotionState(mat_t matrix, const btVector3 &origin,
				     TreeUpdater &tree_updater) :
    m_matrix(matrix),
    m_origin(origin),
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
    VADD2(&bt_matrix[12], &bt_matrix[12], m_origin);
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
    VSUB2(&bt_matrix[12], &bt_matrix[12], m_origin);
    MAT_TRANSPOSE(m_matrix, bt_matrix);

    m_tree_updater.mark_modified();
}


WorldObject *
WorldObject::create(db_i &db_instance, directory &vdirectory, mat_t matrix,
		    TreeUpdater &tree_updater)
{
    btVector3 linear_velocity(0.0, 0.0, 0.0), angular_velocity(0.0, 0.0, 0.0);
    btScalar mass;

    std::pair<btVector3, btVector3> bounding_box = get_bounding_box(db_instance,
	    vdirectory);
    // apply matrix scaling
    bounding_box.second[X] *= 1.0 / matrix[MSX];
    bounding_box.second[Y] *= 1.0 / matrix[MSY];
    bounding_box.second[Z] *= 1.0 / matrix[MSZ];
    bounding_box.second *= 1.0 / matrix[MSA];

    {
	const btScalar density = 1.0;
	mass = density * get_volume(db_instance, vdirectory);

	bu_attribute_value_set obj_avs;
	BU_AVS_INIT(&obj_avs);

	if (db5_get_attributes(&db_instance, &obj_avs, &vdirectory) < 0)
	    throw std::runtime_error("db5_get_attributes() failed");

	AutoDestroyer<bu_attribute_value_set, bu_avs_free> obj_avs_autodestroy(
	    &obj_avs);

	for (std::size_t i = 0; i < obj_avs.count; ++i)
	    if (attribute_prefix.compare(0, std::string::npos, obj_avs.avp[i].name,
					 attribute_prefix.size()) == 0) {
		const std::string name = &obj_avs.avp[i].name[attribute_prefix.size()];
		const std::string value = obj_avs.avp[i].value;

		if (name == "mass") {
		    mass = lexical_cast<btScalar>(value, "invalid attribute 'mass'");

		    if (mass < 0.0) throw std::invalid_argument("invalid attribute 'mass'");
		} else if (name == "linear_velocity") {
		    linear_velocity = deserialize_vector(value);
		} else if (name == "angular_velocity") {
		    angular_velocity = deserialize_vector(value);
		} else
		    throw std::invalid_argument("invalid attribute '" + name + "'");
	    }
    }

    WorldObject *object = new WorldObject(vdirectory, matrix, tree_updater,
					  bounding_box.first, bounding_box.second, mass);
    object->m_rigid_body.setLinearVelocity(linear_velocity);
    object->m_rigid_body.setAngularVelocity(angular_velocity);
    return object;
}


WorldObject::WorldObject(directory &vdirectory, mat_t matrix,
			 TreeUpdater &tree_updater, btVector3 bounding_box_pos,
			 btVector3 bounding_box_dims, btScalar mass) :
    m_world(NULL),
    m_motion_state(matrix, bounding_box_pos, tree_updater),
    m_collision_shape(tree_updater, vdirectory.d_namep, bounding_box_dims / 2.0),
    m_rigid_body(build_construction_info(m_motion_state, m_collision_shape, mass))
{}


WorldObject::~WorldObject()
{
    if (m_world) m_world->removeRigidBody(&m_rigid_body);
}


void
WorldObject::add_to_world(btDiscreteDynamicsWorld &world)
{
    if (m_world)
	throw std::runtime_error("already in world");
    else {
	m_world = &world;
	m_world->addRigidBody(&m_rigid_body);
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
