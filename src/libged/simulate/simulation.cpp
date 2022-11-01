/*                  S I M U L A T I O N . C P P
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
/** @file simulation.cpp
 *
 * Programmatic interface for simulate.
 *
 */


#include "common.h"


#ifdef HAVE_BULLET


#include "simulation.hpp"
#include "rt_collision_algorithm.hpp"
#include "rt_collision_shape.hpp"
#include "rt_motion_state.hpp"
#include "utility.hpp"

#include "bu/str.h"
#include "rt/db_attr.h"
#include "rt/db_io.h"
#include "rt/search.h"

#include <limits>
#include <stack>


namespace
{


const char * const attribute_prefix = "simulate::";


HIDDEN std::string
error_at(const std::string &message, const db_full_path &path)
{
    RT_CK_FULL_PATH(&path);

    return message + " at '" + DB_FULL_PATH_CUR_DIR(&path)->d_namep + "'";
}


HIDDEN btRigidBody::btRigidBodyConstructionInfo
get_rigid_body_construction_info(btMotionState &motion_state,
				 btCollisionShape &collision_shape, const db_i &db, const btScalar mass)
{
    RT_CK_DBI(&db);

    if (mass < 0.0)
	bu_bomb("invalid argument");

    btVector3 inertia(0.0, 0.0, 0.0);
    collision_shape.calculateLocalInertia(mass, inertia);
    return btRigidBody::btRigidBodyConstructionInfo(mass, &motion_state,
	    &collision_shape, inertia);
}


HIDDEN std::pair<btVector3, btVector3>
get_aabb(db_i &db, const db_full_path &path)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    const simulate::AutoPtr<rt_i, rt_free_rti> rti(rt_new_rti(&db));

    if (!rti.ptr)
	bu_bomb("rt_new_rti() failed");

    const simulate::AutoPtr<char> path_str(db_path_to_string(&path));

    if (rt_gettree(rti.ptr, path_str.ptr))
	bu_bomb("rt_gettree() failed");

    rt_prep_parallel(rti.ptr, 0);
    std::stack<const tree *> stack;

    for (std::size_t i = 0; i < rti.ptr->nregions; ++i)
	stack.push(rti.ptr->Regions[i]->reg_treetop);

    std::pair<btVector3, btVector3> result(btVector3(0.0, 0.0, 0.0),
					   btVector3(0.0, 0.0, 0.0));
    bool found_soltab = false;

    while (!stack.empty()) {
	const tree &current = *stack.top();
	stack.pop();

	switch (current.tr_op) {
	    case OP_SOLID:
		if (!found_soltab) {
		    VMOVE(result.first, current.tr_a.tu_stp->st_min);
		    VMOVE(result.second, current.tr_a.tu_stp->st_max);
		    found_soltab = true;
		} else {
		    VMIN(result.first, current.tr_a.tu_stp->st_min);
		    VMAX(result.second, current.tr_a.tu_stp->st_max);
		}

		break;

	    case OP_UNION:
	    case OP_INTERSECT:
	    case OP_SUBTRACT:
	    case OP_XOR:
		stack.push(current.tr_b.tb_left);
		stack.push(current.tr_b.tb_right);
		break;

	    case OP_NOP:
		break;

	    default:
		bu_bomb("invalid tree operation");
	}
    }

    if (!found_soltab)
	throw simulate::InvalidSimulationError(error_at("no solids found", path));

    result.first /= simulate::world_to_application;
    result.second /= simulate::world_to_application;

    return result;
}


HIDDEN btVector3
get_center_of_mass(db_i &db, const db_full_path &path)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    // TODO: not implemented; return the center of the AABB

    const std::pair<btVector3, btVector3> aabb = get_aabb(db, path);
    return (aabb.first + aabb.second) / 2.0;
}


HIDDEN btVector3
deserialize_vector(const std::string &source)
{
    std::istringstream stream(source);
    btVector3 result(0.0, 0.0, 0.0);

    if ((stream >> std::ws).get() != '<')
	throw simulate::InvalidSimulationError("invalid vector");

    for (std::size_t i = 0; i < 3; ++i) {
	std::string value;
	std::getline(stream, value, i != 2 ? ',' : '>');
	result[i] = simulate::lexical_cast<btScalar>(value, "invalid vector");
    }

    if (stream.unget().get() != '>' || !(stream >> std::ws).eof())
	throw simulate::InvalidSimulationError("invalid vector");

    return result;
}


struct SimulationParameters {
    explicit SimulationParameters() :
	m_gravity(0.0, 0.0, -9.80665),
	m_grid_radius(10)
    {}


    static SimulationParameters get_simulation_parameters(const db_i &db,
	    const db_full_path &path);


    btVector3 m_gravity;
    unsigned m_grid_radius;
};


SimulationParameters
SimulationParameters::get_simulation_parameters(const db_i &db,
	const db_full_path &path)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    bu_attribute_value_set avs;
    BU_AVS_INIT(&avs);
    const simulate::AutoPtr<bu_attribute_value_set, bu_avs_free> avs_autoptr(&avs);

    if (db5_get_attributes(&db, &avs, DB_FULL_PATH_CUR_DIR(&path)))
	bu_bomb("db5_get_attributes() failed");

    SimulationParameters result;

    for (std::size_t i = 0; i < avs.count; ++i)
	if (!bu_strncmp(avs.avp[i].name, attribute_prefix, strlen(attribute_prefix))) {
	    const char * const name = avs.avp[i].name + strlen(attribute_prefix);
	    const char * const value = avs.avp[i].value;

	    if (!bu_strcmp(name, "gravity"))
		result.m_gravity = deserialize_vector(value);
	    else if (!bu_strcmp(name, "grid_radius")) {
		const long temp = simulate::lexical_cast<long>(value,
				  error_at("invalid grid_radius", path));

		if (temp < 0)
		    throw simulate::InvalidSimulationError(error_at("invalid grid_radius", path));

		result.m_grid_radius = temp;
	    } else
		throw simulate::InvalidSimulationError(error_at(std::string() +
						       "invalid scene attribute '" + avs.avp[i].name + "'", path));
	}

    return result;
}


}


namespace simulate
{


class Simulation::Region
{
public:
    ~Region();

    static Region *get_region(db_i &db, const db_full_path &path,
			      btDiscreteDynamicsWorld &world);

    static std::vector<const Region *> get_regions(db_i &db,
	    const db_full_path &path, btDiscreteDynamicsWorld &world);


private:
    explicit Region(db_i &db, const db_full_path &path,
		    btDiscreteDynamicsWorld &world, const std::pair<btVector3, btVector3> &aabb,
		    const btVector3 &center_of_mass, btScalar mass,
		    const btVector3 &linear_velocity, const btVector3 &angular_velocity);

    TemporaryRegionHandle m_region_handle;
    btDiscreteDynamicsWorld &m_world;
    RtMotionState m_motion_state;
    RtCollisionShape m_collision_shape;
    btRigidBody m_rigid_body;
};


Simulation::Region *
Simulation::Region::get_region(db_i &db, const db_full_path &path,
			       btDiscreteDynamicsWorld &world)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    btScalar mass = 1.0;
    btVector3 linear_velocity(0.0, 0.0, 0.0);
    btVector3 angular_velocity(0.0, 0.0, 0.0);
    {
	bu_attribute_value_set avs;
	BU_AVS_INIT(&avs);
	const AutoPtr<bu_attribute_value_set, bu_avs_free> avs_autoptr(&avs);

	if (db5_get_attributes(&db, &avs, DB_FULL_PATH_CUR_DIR(&path)))
	    bu_bomb("db5_get_attributes() failed");

	for (std::size_t i = 0; i < avs.count; ++i)
	    if (!bu_strncmp(avs.avp[i].name, attribute_prefix, strlen(attribute_prefix))) {
		const char * const name = avs.avp[i].name + strlen(attribute_prefix);
		const char * const value = avs.avp[i].value;

		if (!bu_strcmp(name, "type")) {
		    if (bu_strcmp(value, "region"))
			throw InvalidSimulationError(error_at("invalid type", path));
		} else if (!bu_strcmp(name, "mass")) {
		    mass = lexical_cast<btScalar>(value, error_at("invalid mass", path));

		    if (mass < 0.0)
			throw InvalidSimulationError(error_at("invalid mass", path));
		} else if (!bu_strcmp(name, "linear_velocity")) {
		    linear_velocity = deserialize_vector(value);
		} else if (!bu_strcmp(name, "angular_velocity")) {
		    angular_velocity = deserialize_vector(value);
		} else
		    throw InvalidSimulationError(error_at(std::string() + "invalid attribute '" +
							  avs.avp[i].name + "'", path));
	    }
    }

    return new Region(db, path, world, get_aabb(db, path), get_center_of_mass(db,
		      path), mass, linear_velocity, angular_velocity);
}


std::vector<const Simulation::Region *>
Simulation::Region::get_regions(db_i &db, const db_full_path &path,
				btDiscreteDynamicsWorld &world)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    db_update_nref(&db, &rt_uniresource);

    bu_ptbl found = BU_PTBL_INIT_ZERO;
    const AutoPtr<bu_ptbl, db_search_free> autofree_found(&found);

    if (0 > db_search(&found, DB_SEARCH_TREE,
		      (std::string() + "-attr " + attribute_prefix + "type=region -below -attr " +
		       attribute_prefix + "type=region").c_str(), path.fp_len, path.fp_names, &db, NULL))
	bu_bomb("db_search() failed");

    if (BU_PTBL_LEN(&found))
	throw InvalidSimulationError(std::string() + "nested objects with " +
				     attribute_prefix + "type=region");

    if (0 > db_search(&found, DB_SEARCH_TREE,
		      (std::string() + "-depth >0 -attr " + attribute_prefix + "* -not ( -attr " +
		       attribute_prefix + "type=region -or ( -type shape -not -below -attr " +
		       attribute_prefix + "type=region ) )").c_str(), path.fp_len,
		      path.fp_names, &db, NULL))
	bu_bomb("db_search() failed");

    if (BU_PTBL_LEN(&found))
	throw InvalidSimulationError(std::string() +
				     "simulation attributes set on objects that are not " + attribute_prefix +
				     "type=region");

    if (0 > db_search(&found, DB_SEARCH_TREE,
		      (std::string() + "-attr " + attribute_prefix +
		       "type=region -or -type shape -not -below -attr " + attribute_prefix +
		       "type=region").c_str(), path.fp_len, path.fp_names, &db, NULL))
	bu_bomb("db_search() failed");

    if (!BU_PTBL_LEN(&found))
	throw InvalidSimulationError("no regions found");

    std::vector<const Simulation::Region *> result;

    try {
	db_full_path **entry;

	for (BU_PTBL_FOR(entry, (db_full_path **), &found))
	    result.push_back(get_region(db, **entry, world));
    } catch (...) {
	for (std::vector<const Region *>::const_iterator it = result.begin();
	     it != result.end(); ++it)
	    delete *it;

	throw;
    }

    return result;
}


Simulation::Region::Region(db_i &db, const db_full_path &path,
			   btDiscreteDynamicsWorld &world, const std::pair<btVector3, btVector3> &aabb,
			   const btVector3 &center_of_mass, const btScalar mass,
			   const btVector3 &linear_velocity, const btVector3 &angular_velocity) :
    m_region_handle(db, path),
    m_world(world),
    m_motion_state(db, path, center_of_mass),
    m_collision_shape(aabb.second - aabb.first,
		      (aabb.first + aabb.second) / 2.0 - center_of_mass,
		      DB_FULL_PATH_CUR_DIR(&path)->d_namep),
    m_rigid_body(get_rigid_body_construction_info(m_motion_state, m_collision_shape,
		 db, mass))
{
    m_world.addRigidBody(&m_rigid_body);

    m_rigid_body.setLinearVelocity(linear_velocity);
    m_rigid_body.setAngularVelocity(angular_velocity);
}


Simulation::Region::~Region()
{
    m_world.removeRigidBody(&m_rigid_body);
}


Simulation::Simulation(db_i &db, const db_full_path &path) :
    m_debug_draw(db),
    m_broadphase(),
    m_collision_config(),
    m_collision_dispatcher(&m_collision_config),
    m_constraint_solver(),
    m_world(&m_collision_dispatcher, &m_broadphase, &m_constraint_solver,
	    &m_collision_config),
    m_regions(Region::get_regions(db, path, m_world)),
    m_rt_instance(db)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    const SimulationParameters parameters =
	SimulationParameters::get_simulation_parameters(db, path);

    m_world.setDebugDrawer(&m_debug_draw);
    m_world.setGravity(parameters.m_gravity);

    m_collision_dispatcher.registerCollisionCreateFunc(
	RtCollisionShape::RT_COLLISION_SHAPE_TYPE,
	RtCollisionShape::RT_COLLISION_SHAPE_TYPE,
	new RtCollisionAlgorithm::CreateFunc(m_rt_instance, parameters.m_grid_radius,
		*m_world.getDebugDrawer()));
}


Simulation::~Simulation()
{
    for (std::vector<const Region *>::const_iterator it = m_regions.begin();
	 it != m_regions.end(); ++it)
	delete *it;
}


void
Simulation::step(const fastf_t seconds, const DebugMode debug_mode)
{
    const btScalar fixed_time_step = 1.0 / 60.0;
    const int max_substeps = std::numeric_limits<int16_t>::max();

    if (seconds < fixed_time_step)
	throw InvalidSimulationError("duration is less than fixed timestep");
    else if (seconds > max_substeps * fixed_time_step)
	throw InvalidSimulationError("duration is too large for a single step");

    m_world.getDebugDrawer()->setDebugMode(btIDebugDraw::DBG_NoDebug);

    if (debug_mode & debug_aabb)
	m_world.getDebugDrawer()->setDebugMode(m_world.getDebugDrawer()->getDebugMode()
					       | btIDebugDraw::DBG_DrawAabb);

    if (debug_mode & debug_contact)
	m_world.getDebugDrawer()->setDebugMode(m_world.getDebugDrawer()->getDebugMode()
					       | btIDebugDraw::DBG_DrawContactPoints);

    if (debug_mode & debug_ray)
	m_world.getDebugDrawer()->setDebugMode(m_world.getDebugDrawer()->getDebugMode()
					       | btIDebugDraw::DBG_DrawFrames);

    m_world.stepSimulation(seconds, max_substeps, fixed_time_step);
    m_world.debugDrawWorld();
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
