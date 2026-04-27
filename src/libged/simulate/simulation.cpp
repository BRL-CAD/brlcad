/*                  S I M U L A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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


#include "bv/plot3.h"

#include "simulation.hpp"
#include "rt_collision_algorithm.hpp"
#include "rt_collision_shape.hpp"
#include "rt_roi_collision_shape.hpp"
#include "rt_motion_state.hpp"
#include "utility.hpp"

#include "bu/str.h"
#include "rt/db_attr.h"
#include "rt/db_io.h"
#include "rt/rt_instance.h"
#include "rt/search.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stack>


namespace
{


const char * const attribute_prefix = "simulate::";


static std::string
serialize_vector(const btVector3 &v)
{
    std::ostringstream stream;
    stream.exceptions(std::ostream::failbit | std::ostream::badbit);
    stream << "<" << v.x() << "," << v.y() << "," << v.z() << ">";
    return stream.str();
}


static void
fclose_wrapper(FILE *fp)
{
    /* Ignore return: errors here are non-recoverable in a destructor context. */
    (void)fclose(fp);
}


static void
draw_wireframe_box(FILE *fp, const point_t min_pt, const point_t max_pt)
{
    /* 8 corners of the box */
    point_t c[8];
    VSET(c[0], min_pt[X], min_pt[Y], min_pt[Z]);
    VSET(c[1], max_pt[X], min_pt[Y], min_pt[Z]);
    VSET(c[2], max_pt[X], max_pt[Y], min_pt[Z]);
    VSET(c[3], min_pt[X], max_pt[Y], min_pt[Z]);
    VSET(c[4], min_pt[X], min_pt[Y], max_pt[Z]);
    VSET(c[5], max_pt[X], min_pt[Y], max_pt[Z]);
    VSET(c[6], max_pt[X], max_pt[Y], max_pt[Z]);
    VSET(c[7], min_pt[X], max_pt[Y], max_pt[Z]);

    /* bottom face */
    pdv_3move(fp, c[0]); pdv_3cont(fp, c[1]);
    pdv_3cont(fp, c[2]); pdv_3cont(fp, c[3]);
    pdv_3cont(fp, c[0]);

    /* top face */
    pdv_3move(fp, c[4]); pdv_3cont(fp, c[5]);
    pdv_3cont(fp, c[6]); pdv_3cont(fp, c[7]);
    pdv_3cont(fp, c[4]);

    /* vertical edges */
    pdv_3move(fp, c[0]); pdv_3cont(fp, c[4]);
    pdv_3move(fp, c[1]); pdv_3cont(fp, c[5]);
    pdv_3move(fp, c[2]); pdv_3cont(fp, c[6]);
    pdv_3move(fp, c[3]); pdv_3cont(fp, c[7]);
}


static std::string
error_at(const std::string &message, const db_full_path &path)
{
    RT_CK_FULL_PATH(&path);

    return message + " at '" + DB_FULL_PATH_CUR_DIR(&path)->d_namep + "'";
}



static std::pair<btVector3, btVector3>
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

    rt_iterate_regions(rti.ptr,
        [](struct region *regp, void *udata) -> int {
            auto *s = static_cast<std::stack<const tree *> *>(udata);
            s->push(regp->reg_treetop);
            return 0;
        },
        &stack);

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


static btVector3
get_center_of_mass(db_i &db, const db_full_path &path)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    // TODO: not implemented; return the center of the AABB

    const std::pair<btVector3, btVector3> aabb = get_aabb(db, path);
    return (aabb.first + aabb.second) / 2.0;
}


static btVector3
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


// Define ROI update constants
const btScalar Simulation::ROI_MAX_PREDICTION_TIME = 0.5; // 0.5 seconds
const btScalar Simulation::ROI_PADDING = 0.5; // 0.5 meters
const btScalar Simulation::ROI_MIN_SIZE = 0.2; // 0.2 meters
const btScalar Simulation::ROI_CHANGE_THRESHOLD = 0.01; // 1cm


class Simulation::Region
{
public:
    ~Region();

    static Region *get_region(db_i &db, const db_full_path &path,
			      btDiscreteDynamicsWorld &world, bool use_saved_state);

    static std::vector<Region *> get_regions(db_i &db,
					    const db_full_path &path, btDiscreteDynamicsWorld &world,
					    bool use_saved_state);

    // Get the rigid body for ROI updates
    btRigidBody *getRigidBody() { return &m_rigid_body; }
    const btRigidBody *getRigidBody() const { return &m_rigid_body; }

    // Get the db_full_path for this region
    const db_full_path &getPath() const { return m_motion_state.get_path(); }

    // Get the physical AABB for this region (global AABB for ROI proxy, shape AABB otherwise)
    void getAabb(btVector3 &aabb_min, btVector3 &aabb_max) const
    {
	if (m_roi_collision_shape) {
	    aabb_min = m_roi_collision_shape->getGlobalMin();
	    aabb_max = m_roi_collision_shape->getGlobalMax();
	} else {
	    m_rigid_body.getCollisionShape()->getAabb(
		m_rigid_body.getWorldTransform(), aabb_min, aabb_max);
	}
    }

    // Check if this region uses ROI proxy
    bool usesRoiProxy() const { return m_roi_collision_shape != NULL; }

    // Get the ROI collision shape (returns NULL if not using ROI)
    RtRoiCollisionShape *getRoiCollisionShape() const {
	return m_roi_collision_shape;
    }


private:
    explicit Region(db_i &db, const db_full_path &path,
		    btDiscreteDynamicsWorld &world, const std::pair<btVector3, btVector3> &aabb,
		    const btVector3 &center_of_mass, btScalar mass,
		    const btVector3 &linear_velocity, const btVector3 &angular_velocity,
		    bool roi_proxy);

    TemporaryRegionHandle m_region_handle;
    btDiscreteDynamicsWorld &m_world;
    RtMotionState m_motion_state;
    // Exactly one of these will be non-NULL, depending on whether ROI proxy is enabled
    RtCollisionShape *m_collision_shape;
    RtRoiCollisionShape *m_roi_collision_shape;
    btRigidBody m_rigid_body;
};


Simulation::Region *
Simulation::Region::get_region(db_i &db, const db_full_path &path,
			       btDiscreteDynamicsWorld &world, bool use_saved_state)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    btScalar mass = 1.0;
    btVector3 linear_velocity(0.0, 0.0, 0.0);
    btVector3 angular_velocity(0.0, 0.0, 0.0);
    bool roi_proxy_specified = false;
    bool roi_proxy = false;
    btVector3 saved_linear_velocity(0.0, 0.0, 0.0);
    btVector3 saved_angular_velocity(0.0, 0.0, 0.0);
    bool has_saved_linear_velocity = false;
    bool has_saved_angular_velocity = false;
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
		} else if (!bu_strcmp(name, "state_linear_velocity")) {
		    /* Saved state from a previous simulation run (set by saveState()). */
		    saved_linear_velocity = deserialize_vector(value);
		    has_saved_linear_velocity = true;
		} else if (!bu_strcmp(name, "state_angular_velocity")) {
		    /* Saved state from a previous simulation run (set by saveState()). */
		    saved_angular_velocity = deserialize_vector(value);
		    has_saved_angular_velocity = true;
		} else if (!bu_strcmp(name, "roi_proxy")) {
		    // Parse roi_proxy attribute: accept 0/1, true/false
		    roi_proxy_specified = true;
		    if (!bu_strcmp(value, "1") || !bu_strcmp(value, "true")) {
			roi_proxy = true;
		    } else if (!bu_strcmp(value, "0") || !bu_strcmp(value, "false")) {
			roi_proxy = false;
		    } else {
			throw InvalidSimulationError(error_at(std::string() + "invalid roi_proxy value \"" + 
							      value + "\" (expected \"0\", \"1\", \"true\", or \"false\")", path));
		    }
		} else
		    throw InvalidSimulationError(error_at(std::string() + "invalid attribute '" +
							  avs.avp[i].name + "'", path));
	    }
    }

    /* When resuming, override initial velocities with the saved state so that
     * the simulation continues from where it left off.  */
    if (use_saved_state) {
	if (has_saved_linear_velocity)
	    linear_velocity = saved_linear_velocity;
	if (has_saved_angular_velocity)
	    angular_velocity = saved_angular_velocity;
    }

    // Default: enable ROI proxy for static bodies (mass near zero) unless explicitly disabled
    if (!roi_proxy_specified && NEAR_ZERO(mass, SMALL_FASTF)) {
	roi_proxy = true;
    }

    return new Region(db, path, world, get_aabb(db, path), get_center_of_mass(db,
									      path), mass, linear_velocity, angular_velocity, roi_proxy);
}


std::vector<Simulation::Region *>
Simulation::Region::get_regions(db_i &db, const db_full_path &path,
				btDiscreteDynamicsWorld &world, bool use_saved_state)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    db_update_nref(&db, &rt_uniresource);

    bu_ptbl found = BU_PTBL_INIT_ZERO;
    const AutoPtr<bu_ptbl, db_search_free> autofree_found(&found);

    if (0 > db_search(&found, DB_SEARCH_TREE,
		      (std::string() + "-attr " + attribute_prefix + "type=region -below -attr " +
		       attribute_prefix + "type=region").c_str(), path.fp_len, path.fp_names, &db, NULL, NULL, NULL))
	bu_bomb("db_search() failed");

    if (BU_PTBL_LEN(&found))
	throw InvalidSimulationError(std::string() + "nested objects with " +
				     attribute_prefix + "type=region");

    if (0 > db_search(&found, DB_SEARCH_TREE,
		      (std::string() + "-depth >0 -attr " + attribute_prefix + "* -not ( -attr " +
		       attribute_prefix + "type=region -or ( -type shape -not -below -attr " +
		       attribute_prefix + "type=region ) )").c_str(), path.fp_len,
		      path.fp_names, &db, NULL, NULL, NULL))
	bu_bomb("db_search() failed");

    if (BU_PTBL_LEN(&found))
	throw InvalidSimulationError(std::string() +
				     "simulation attributes set on objects that are not " + attribute_prefix +
				     "type=region");

    if (0 > db_search(&found, DB_SEARCH_TREE,
		      (std::string() + "-attr " + attribute_prefix +
		       "type=region -or -type shape -not -below -attr " + attribute_prefix +
		       "type=region").c_str(), path.fp_len, path.fp_names, &db, NULL, NULL, NULL))
	bu_bomb("db_search() failed");

    if (!BU_PTBL_LEN(&found))
	throw InvalidSimulationError("no regions found");

    std::vector<Simulation::Region *> result;

    try {
	db_full_path **entry;

	for (BU_PTBL_FOR(entry, (db_full_path **), &found))
	    result.push_back(get_region(db, **entry, world, use_saved_state));
    } catch (...) {
	for (Region * const region : result)
	    delete region;

	throw;
    }

    return result;
}


Simulation::Region::Region(db_i &db, const db_full_path &path,
			   btDiscreteDynamicsWorld &world, const std::pair<btVector3, btVector3> &aabb,
			   const btVector3 &center_of_mass, const btScalar mass,
			   const btVector3 &linear_velocity, const btVector3 &angular_velocity,
			   bool roi_proxy) :
    m_region_handle(db, path),
    m_world(world),
    m_motion_state(db, path, center_of_mass),
    m_collision_shape(NULL),
    m_roi_collision_shape(NULL),
    m_rigid_body(mass, &m_motion_state, NULL, btVector3(0.0, 0.0, 0.0))
{
    btCollisionShape *shape = NULL;

    // Create appropriate collision shape based on mass and roi_proxy settings
    // Exactly one of m_collision_shape or m_roi_collision_shape will be non-NULL
    if (NEAR_ZERO(mass, SMALL_FASTF) && roi_proxy) {
	// Static body with ROI proxy enabled
	m_roi_collision_shape = new RtRoiCollisionShape(aabb.first, aabb.second,
							DB_FULL_PATH_CUR_DIR(&path)->d_namep);
	shape = m_roi_collision_shape;

	// Set initial transform to ROI center (starts at global AABB center)
	btVector3 roi_center = (aabb.first + aabb.second) / 2.0;
	btTransform initial_transform;
	initial_transform.setIdentity();
	initial_transform.setOrigin(roi_center);
	m_rigid_body.setWorldTransform(initial_transform);
    } else {
	// Dynamic body or static body with ROI disabled - use standard collision shape
	m_collision_shape = new RtCollisionShape(aabb.second - aabb.first,
						 (aabb.first + aabb.second) / 2.0 - center_of_mass,
						 DB_FULL_PATH_CUR_DIR(&path)->d_namep);
	shape = m_collision_shape;
    }

    // Set the collision shape and calculate inertia
    m_rigid_body.setCollisionShape(shape);

    btVector3 inertia(0.0, 0.0, 0.0);
    shape->calculateLocalInertia(mass, inertia);
    m_rigid_body.setMassProps(mass, inertia);

    m_world.addRigidBody(&m_rigid_body);

    m_rigid_body.setLinearVelocity(linear_velocity);
    m_rigid_body.setAngularVelocity(angular_velocity);
}


Simulation::Region::~Region()
{
    m_world.removeRigidBody(&m_rigid_body);

    // Clean up collision shapes
    // Only one of these is non-NULL; deleting NULL is safe
    delete m_collision_shape;
    delete m_roi_collision_shape;
}


Simulation::Simulation(db_i &db, const db_full_path &path, bool use_saved_state) :
    m_db(db),
    m_debug_draw(db),
    m_broadphase(),
    m_collision_config(),
    m_collision_dispatcher(&m_collision_config),
    m_constraint_solver(),
    m_world(&m_collision_dispatcher, &m_broadphase, &m_constraint_solver,
	   &m_collision_config),
    m_regions(Region::get_regions(db, path, m_world, use_saved_state)),
    m_rt_instance(db)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    const SimulationParameters parameters =
    SimulationParameters::get_simulation_parameters(db, path);

    m_world.setDebugDrawer(&m_debug_draw);
    m_world.setGravity(parameters.m_gravity);

    // Register collision algorithms for standard RtCollisionShape
    m_collision_dispatcher.registerCollisionCreateFunc(
	RtCollisionShape::RT_COLLISION_SHAPE_TYPE,
	RtCollisionShape::RT_COLLISION_SHAPE_TYPE,
	new RtCollisionAlgorithm::CreateFunc(m_rt_instance, parameters.m_grid_radius,
					    *m_world.getDebugDrawer()));

    // Register collision algorithms for RtRoiCollisionShape
    // ROI shapes need custom collision algorithm with both other ROI shapes and regular shapes
    m_collision_dispatcher.registerCollisionCreateFunc(
	RtRoiCollisionShape::RT_ROI_COLLISION_SHAPE_TYPE,
	RtRoiCollisionShape::RT_ROI_COLLISION_SHAPE_TYPE,
	new RtCollisionAlgorithm::CreateFunc(m_rt_instance, parameters.m_grid_radius,
					    *m_world.getDebugDrawer()));

    m_collision_dispatcher.registerCollisionCreateFunc(
	RtCollisionShape::RT_COLLISION_SHAPE_TYPE,
	RtRoiCollisionShape::RT_ROI_COLLISION_SHAPE_TYPE,
	new RtCollisionAlgorithm::CreateFunc(m_rt_instance, parameters.m_grid_radius,
					    *m_world.getDebugDrawer()));

    m_collision_dispatcher.registerCollisionCreateFunc(
	RtRoiCollisionShape::RT_ROI_COLLISION_SHAPE_TYPE,
	RtCollisionShape::RT_COLLISION_SHAPE_TYPE,
	new RtCollisionAlgorithm::CreateFunc(m_rt_instance, parameters.m_grid_radius,
					    *m_world.getDebugDrawer()));
}


Simulation::~Simulation()
{
    for (Region * const region : m_regions)
	delete region;
}


void
Simulation::updateRoiProxies(const fastf_t seconds)
{
    // Clamp prediction time to reasonable bounds
    const btScalar dt_predict = btMin(btScalar(seconds), ROI_MAX_PREDICTION_TIME);

    // Collect dynamic bodies and their swept AABBs
    std::vector<std::pair<btVector3, btVector3> > dynamic_swept_aabbs;

    for (const Region * const region : m_regions) {
	const btRigidBody * const body = region->getRigidBody();

	// Skip static bodies
	if (NEAR_ZERO(body->getMass(), SMALL_FASTF))
	    continue;

	// Get current AABB
	btVector3 current_min, current_max;
	body->getCollisionShape()->getAabb(body->getWorldTransform(), current_min, current_max);

	// Predict future position based on linear velocity
	const btVector3 velocity = body->getLinearVelocity();
	const btVector3 displacement = velocity * dt_predict;

	// Compute swept AABB (union of current and predicted)
	const btVector3 predicted_origin = body->getWorldTransform().getOrigin() + displacement;
	btTransform predicted_transform = body->getWorldTransform();
	predicted_transform.setOrigin(predicted_origin);

	btVector3 predicted_min, predicted_max;
	body->getCollisionShape()->getAabb(predicted_transform, predicted_min, predicted_max);

	// Union of current and predicted AABBs
	btVector3 swept_min, swept_max;
	for (int i = 0; i < 3; ++i) {
	    swept_min[i] = btMin(current_min[i], predicted_min[i]);
	    swept_max[i] = btMax(current_max[i], predicted_max[i]);
	}

	// Add padding for rotation and numerical safety
	btVector3 padding_vec(ROI_PADDING, ROI_PADDING, ROI_PADDING);
	swept_min -= padding_vec;
	swept_max += padding_vec;

	dynamic_swept_aabbs.push_back(std::make_pair(swept_min, swept_max));
    }

    // Update ROI for each static body with ROI proxy enabled
    for (Region * const region : m_regions) {
	if (!region->usesRoiProxy())
	    continue;

	RtRoiCollisionShape *roi_shape = region->getRoiCollisionShape();
	if (!roi_shape)
	    continue;

	btRigidBody *static_body = region->getRigidBody();
	btVector3 global_min = roi_shape->getGlobalMin();
	btVector3 global_max = roi_shape->getGlobalMax();

	// Find nearby dynamic bodies and compute union of their swept AABBs
	bool found_nearby = false;
	btVector3 roi_min = global_max; // Start with invalid bounds
	btVector3 roi_max = global_min;

	for (const std::pair<btVector3, btVector3> &swept_aabb : dynamic_swept_aabbs) {
	    const btVector3 &swept_min = swept_aabb.first;
	    const btVector3 &swept_max = swept_aabb.second;

	    // Check if swept AABB overlaps with global AABB
	    bool overlaps = true;
	    for (int j = 0; j < 3; ++j) {
		if (swept_max[j] < global_min[j] || swept_min[j] > global_max[j]) {
		    overlaps = false;
		    break;
		}
	    }

	    if (overlaps) {
		if (!found_nearby) {
		    roi_min = swept_min;
		    roi_max = swept_max;
		    found_nearby = true;
		} else {
		    // Union with existing ROI
		    for (int j = 0; j < 3; ++j) {
			roi_min[j] = btMin(roi_min[j], swept_min[j]);
			roi_max[j] = btMax(roi_max[j], swept_max[j]);
		    }
		}
	    }
	}

	// If no nearby dynamic bodies, keep previous ROI or use a minimal ROI at center
	if (!found_nearby) {
	    btVector3 current_roi_min = roi_shape->getRoiMin();
	    btVector3 current_roi_max = roi_shape->getRoiMax();
	    btVector3 current_center = (current_roi_min + current_roi_max) / 2.0;

	    btVector3 half_min_size(ROI_MIN_SIZE / 2.0, ROI_MIN_SIZE / 2.0, ROI_MIN_SIZE / 2.0);
	    roi_min = current_center - half_min_size;
	    roi_max = current_center + half_min_size;
	}

	// Store previous ROI for comparison
	btVector3 prev_roi_min = roi_shape->getRoiMin();
	btVector3 prev_roi_max = roi_shape->getRoiMax();

	// Update ROI (setRoiAabb handles clamping and minimum size enforcement)
	roi_shape->setRoiAabb(roi_min, roi_max);

	// Update the rigid body's transform to the new ROI center
	btVector3 new_roi_center = (roi_shape->getRoiMin() + roi_shape->getRoiMax()) / 2.0;
	btTransform new_transform;
	new_transform.setIdentity();
	new_transform.setOrigin(new_roi_center);
	static_body->setWorldTransform(new_transform);

	// Check if ROI changed significantly (to avoid unnecessary updates)
	bool roi_changed = false;
	for (int i = 0; i < 3; ++i) {
	    if (btFabs(prev_roi_min[i] - roi_shape->getRoiMin()[i]) > ROI_CHANGE_THRESHOLD ||
		btFabs(prev_roi_max[i] - roi_shape->getRoiMax()[i]) > ROI_CHANGE_THRESHOLD) {
		roi_changed = true;
		break;
	    }
	}

	// Update broadphase AABB if ROI changed
	if (roi_changed) {
	    m_world.updateSingleAabb(static_body);
	}
    }
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

    // Update ROI proxies for static bodies before stepping simulation
    updateRoiProxies(seconds);

    m_world.stepSimulation(seconds, max_substeps, fixed_time_step);
    m_world.debugDrawWorld();
}


void
Simulation::saveState()
{
    RT_CK_DBI(&m_db);

    for (const Region * const region : m_regions) {
	const btRigidBody * const body = region->getRigidBody();

	/* Static bodies (mass == 0) never move, so their velocity state is
	 * always zero and does not need to be persisted.  */
	if (NEAR_ZERO(body->getMass(), SMALL_FASTF))
	    continue;

	const db_full_path &path = region->getPath();
	const char * const name = DB_FULL_PATH_CUR_DIR(&path)->d_namep;

	const std::string linear_vel_str = serialize_vector(body->getLinearVelocity());
	const std::string angular_vel_str = serialize_vector(body->getAngularVelocity());

	if (db5_update_attribute(name, "simulate::state_linear_velocity",
				 linear_vel_str.c_str(), &m_db))
	    bu_bomb("db5_update_attribute() failed");

	if (db5_update_attribute(name, "simulate::state_angular_velocity",
				 angular_vel_str.c_str(), &m_db))
	    bu_bomb("db5_update_attribute() failed");
    }
}


void
Simulation::writePlotFile(const std::string &filename) const
{
    FILE *fp = fopen(filename.c_str(), "wb");

    if (!fp)
	throw InvalidSimulationError("failed to open plot file '" + filename + "': "
				     + std::string(strerror(errno)));

    const AutoPtr<FILE, fclose_wrapper> autofree_fp(fp);

    for (const Region * const region : m_regions) {
	const btRigidBody * const body = region->getRigidBody();

	btVector3 aabb_min, aabb_max;
	region->getAabb(aabb_min, aabb_max);

	/* Convert from Bullet units (m) to application units (mm).  */
	const btVector3 aabb_min_app = aabb_min * world_to_application;
	const btVector3 aabb_max_app = aabb_max * world_to_application;

	point_t min_pt, max_pt;
	VSET(min_pt, static_cast<fastf_t>(aabb_min_app.x()),
	     static_cast<fastf_t>(aabb_min_app.y()),
	     static_cast<fastf_t>(aabb_min_app.z()));
	VSET(max_pt, static_cast<fastf_t>(aabb_max_app.x()),
	     static_cast<fastf_t>(aabb_max_app.y()),
	     static_cast<fastf_t>(aabb_max_app.z()));

	/* Green for dynamic bodies, red for static bodies.  */
	if (NEAR_ZERO(body->getMass(), SMALL_FASTF))
	    pl_color(fp, 255, 0, 0);
	else
	    pl_color(fp, 0, 200, 0);

	draw_wireframe_box(fp, min_pt, max_pt);
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
