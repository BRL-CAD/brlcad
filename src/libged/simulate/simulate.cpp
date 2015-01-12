/*                    S I M U L A T E . C P P
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
/** @file simulate.cpp
 *
 * Brief description
 *
 */


#include "common.h"


#ifdef HAVE_BULLET


#include "world_object.hpp"
#include "rt_instance.hpp"

#include <map>
#include <sstream>
#include <stdexcept>

#include "ged.h"


namespace
{


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


HIDDEN std::map<std::string, std::string>
get_attributes(const db_i &db_instance, const std::string &name)
{
    const char * const prefix = "simulate::";
    const std::size_t prefix_len = strlen(prefix);

    directory *dir = db_lookup(&db_instance, name.c_str(), LOOKUP_QUIET);

    if (!dir)
	throw std::invalid_argument("failed to lookup '" + name + "'");

    bu_attribute_value_set obj_avs;
    bu_avs_init_empty(&obj_avs);

    if (db5_get_attributes(&db_instance, &obj_avs, dir) < 0)
	throw std::runtime_error("db5_get_attributes() failed");

    AutoDestroyer<bu_attribute_value_set, bu_avs_free> obj_avs_autodestroy(
	&obj_avs);

    std::map<std::string, std::string> result;

    for (std::size_t i = 0; i < obj_avs.count; ++i)
	if (!bu_strncmp(obj_avs.avp[i].name, prefix, prefix_len))
	    result[obj_avs.avp[i].name + prefix_len] = obj_avs.avp[i].value;

    return result;
}


HIDDEN std::pair<btVector3, btVector3>
get_bounding_box(db_i &db_instance, const std::string &name)
{
    directory *dir = db_lookup(&db_instance, name.c_str(), LOOKUP_QUIET);

    if (!dir)
	throw std::invalid_argument("failed to lookup '" + name + "'");

    btVector3 bounding_box_min, bounding_box_max;
    {
	point_t bb_min, bb_max;

	if (rt_bound_internal(&db_instance, dir, bb_min, bb_max))
	    throw std::runtime_error("failed to get bounding box");

	VMOVE(bounding_box_min, bb_min);
	VMOVE(bounding_box_max, bb_max);
    }

    btVector3 bounding_box_dims = bounding_box_max - bounding_box_min;
    btVector3 bounding_box_pos = bounding_box_dims / 2.0 + bounding_box_min;

    return std::make_pair(bounding_box_dims, bounding_box_pos);
}


HIDDEN btScalar
get_volume(db_i &db_instance, const std::string &name)
{
    directory *dir = db_lookup(&db_instance, name.c_str(), LOOKUP_QUIET);

    if (!dir)
	throw std::invalid_argument("failed to lookup '" + name + "'");

    rt_db_internal internal;

    if (rt_db_get_internal(&internal, dir, &db_instance, bn_mat_identity,
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
    btVector3 bounding_box_dims = get_bounding_box(db_instance, name).first;
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


HIDDEN simulate::WorldObject *
build_world_object(db_i &db_instance, const std::string &name, matp_t &matrix,
		   simulate::TreeUpdater &tree_updater)
{
    if (!matrix) {
	matrix = static_cast<matp_t>(bu_malloc(sizeof(mat_t), "tl_mat"));
	MAT_IDN(matrix);
    }

    const btScalar density = 1.0;
    btScalar mass = density * get_volume(db_instance, name);
    btVector3 linear_velocity(0.0, 0.0, 0.0), angular_velocity(0.0, 0.0, 0.0);

    std::map<std::string, std::string> attributes = get_attributes(db_instance,
	    name);

    for (std::map<std::string, std::string>::const_iterator it = attributes.begin();
	 it != attributes.end(); ++it) {
	if (it->first == "mass") {
	    mass = lexical_cast<btScalar>(it->second, "invalid attribute 'mass'");

	    if (mass < 0.0) throw std::invalid_argument("invalid attribute 'mass'");

	} else if (it->first == "linear_velocity") {
	    linear_velocity = deserialize_vector(it->second);
	} else if (it->first == "angular_velocity") {
	    angular_velocity = deserialize_vector(it->second);
	} else
	    throw std::invalid_argument("invalid attribute '" + it->first + "' on object '"
					+ name  + "'");
    }

    std::pair<btVector3, btVector3> bounding_box = get_bounding_box(db_instance,
	    name);

    // apply matrix scaling
    bounding_box.first[X] *= 1.0 / matrix[MSX];
    bounding_box.first[Y] *= 1.0 / matrix[MSY];
    bounding_box.first[Z] *= 1.0 / matrix[MSZ];
    bounding_box.first *= 1.0 / matrix[MSA];

    return new simulate::WorldObject(matrix, bounding_box.second, tree_updater,
				     mass, bounding_box.first, linear_velocity, angular_velocity);
}


HIDDEN void
get_tree_objects(db_i &db_instance, tree &vtree,
		 simulate::TreeUpdater &tree_updater,
		 std::vector<simulate::WorldObject *> &objects)
{
    switch (vtree.tr_op) {
	case OP_DB_LEAF:
	    objects.push_back(build_world_object(db_instance, vtree.tr_l.tl_name,
						 vtree.tr_l.tl_mat, tree_updater));
	    break;

	case OP_UNION:
	    get_tree_objects(db_instance, *vtree.tr_b.tb_left, tree_updater, objects);
	    get_tree_objects(db_instance, *vtree.tr_b.tb_right, tree_updater, objects);
	    break;

	default:
	    throw std::invalid_argument("unsupported operation in scene comb");
    }
}


HIDDEN void
do_simulate(db_i &db_instance, directory &scene_directory, btScalar seconds)
{
    rt_db_internal internal;

    if (rt_db_get_internal(&internal, &scene_directory, &db_instance,
			   bn_mat_identity, &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    AutoDestroyer<rt_db_internal, rt_db_free_internal> internal_autodestroy(
	&internal);

    simulate::TreeUpdater tree_updater(db_instance, scene_directory, internal);
    {
	simulate::PhysicsWorld world;
	tree * const vtree = static_cast<rt_comb_internal *>(internal.idb_ptr)->tree;
	std::vector<simulate::WorldObject *> objects;

	try {
	    if (vtree) get_tree_objects(db_instance, *vtree, tree_updater, objects);

	    for (std::vector<simulate::WorldObject *>::iterator it = objects.begin();
		 it != objects.end(); ++it)
		(*it)->add_to_world(world);

	    world.step(seconds);
	} catch (...) {
	    for (std::vector<simulate::WorldObject *>::iterator it = objects.begin();
		 it != objects.end(); ++it)
		delete *it;

	    throw;
	}

	for (std::vector<simulate::WorldObject *>::iterator it = objects.begin();
	     it != objects.end(); ++it)
	    delete *it;
    }
}


}


int
ged_simulate(ged *gedp, int argc, const char **argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc != 3) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: USAGE: %s <scene_comb> <seconds>",
		       argv[0], argv[0]);
	return GED_ERROR;
    }

    directory *dir = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET);

    if (!dir || dir->d_minor_type != ID_COMBINATION) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: '%s' is not a combination",
		       argv[0], argv[1]);
	return GED_ERROR;
    }

    try {
	btScalar seconds = lexical_cast<btScalar>(argv[2],
			   "invalid value for 'seconds'");

	if (seconds < 0.0) throw std::runtime_error("invalid value for 'seconds'");

	do_simulate(*gedp->ged_wdbp->dbip, *dir, seconds);
    } catch (const std::logic_error &e) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: %s", argv[0], e.what());
	return GED_ERROR;
    } catch (const std::runtime_error &e) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: %s", argv[0], e.what());
	return GED_ERROR;
    }

    return GED_OK;
}


#else


#include "ged.h"


int
ged_simulate(ged *gedp, int argc, const char **argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    bu_vls_sprintf(gedp->ged_result_str,
		   "%s: This build of BRL-CAD was not compiled with Bullet support", argv[0]);

    return GED_ERROR;
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
