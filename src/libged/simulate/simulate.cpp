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


#include "physics_world.hpp"
#include "rt_instance.hpp"


#include <map>
#include <sstream>
#include <stdexcept>


#include "ged.h"


namespace
{


template <typename T, void(*Destructor)(T *)>
struct AutoDestroyer {
    AutoDestroyer(T *vptr) : ptr(vptr) {}


    ~AutoDestroyer()
    {
	Destructor(ptr);
    }


    T * const ptr;


private:
    AutoDestroyer(const AutoDestroyer &);
    AutoDestroyer &operator=(const AutoDestroyer &);
};


template<typename Target, typename Source>
Target lexical_cast(Source arg)
{
    std::stringstream interpreter;
    Target result;

    if (!(interpreter << arg) ||
	!(interpreter >> result) ||
	!(interpreter >> std::ws).eof())
	throw std::invalid_argument("bad lexical_cast");

    return result;
}


static std::map<std::string, std::string>
get_attributes(const db_i &dbi, const std::string &name)
{
    const char * const prefix = "simulate::";
    const std::size_t prefix_len = strlen(prefix);

    directory *dir = db_lookup(&dbi, name.c_str(), false);

    if (!dir)
	throw std::invalid_argument("failed to lookup '" + name + "'");

    bu_attribute_value_set obj_avs;
    bu_avs_init_empty(&obj_avs);

    if (db5_get_attributes(&dbi, &obj_avs, dir) < 0)
	throw std::runtime_error("db5_get_attributes() failed");

    AutoDestroyer<bu_attribute_value_set, bu_avs_free> obj_avs_autodestroy(
	&obj_avs);

    std::map<std::string, std::string> result;

    for (std::size_t i = 0; i < obj_avs.count; ++i)
	if (!bu_strncmp(obj_avs.avp[i].name, prefix, prefix_len))
	    result[obj_avs.avp[i].name + prefix_len] = obj_avs.avp[i].value;

    return result;
}


static void
get_bounding_box_dimensions(db_i &dbi, const std::string &name, vect_t &dest)
{
    directory *dir = db_lookup(&dbi, name.c_str(), false);

    if (!dir)
	throw std::invalid_argument("failed to lookup '" + name + "'");

    point_t bb_min, bb_max;

    if (rt_bound_internal(&dbi, dir, bb_min, bb_max))
	throw std::runtime_error("failed to get bounding box");

    VSUB2(dest, bb_max, bb_min);
}


static fastf_t
get_volume(db_i &dbi, const std::string &name)
{
    // FIXME: not the true volume
    vect_t lwh;
    get_bounding_box_dimensions(dbi, name, lwh);
    return lwh[X] * lwh[Y] * lwh[Z];
}


static void
world_add_tree(simulate::PhysicsWorld &world, tree &vtree, db_i &dbi)
{
    switch (vtree.tr_op) {
	case OP_DB_LEAF: {
	    if (!vtree.tr_l.tl_mat) {
		vtree.tr_l.tl_mat = static_cast<fastf_t *>(bu_malloc(sizeof(mat_t), "tl_mat"));
		MAT_IDN(vtree.tr_l.tl_mat);
	    }

	    vect_t bounding_box_dimensions;
	    get_bounding_box_dimensions(dbi, vtree.tr_l.tl_name, bounding_box_dimensions);
	    const fastf_t density = 1.0;
	    fastf_t mass = density * get_volume(dbi, vtree.tr_l.tl_name);

	    std::map<std::string, std::string> attributes = get_attributes(dbi,
		    vtree.tr_l.tl_name);

	    for (std::map<std::string, std::string>::const_iterator it = attributes.begin();
		 it != attributes.end(); ++it) {
		if (it->first == "mass") {
		    mass = lexical_cast<fastf_t>(it->second);

		    if (mass < 0.0) throw std::invalid_argument(
			    std::string("invalid attribute 'mass' on object '")
			    + vtree.tr_l.tl_name + "'");
		} else throw std::invalid_argument("invalid attribute '" + it->first +
						       "' on object '" + vtree.tr_l.tl_name  + "'");
	    }

	    world.add_object(bounding_box_dimensions, mass, vtree.tr_l.tl_mat);
	    break;
	}

	case OP_UNION:
	    world_add_tree(world, *vtree.tr_b.tb_left, dbi);
	    world_add_tree(world, *vtree.tr_b.tb_right, dbi);
	    break;

	default:
	    throw std::invalid_argument("unsupported operation in scene comb");
    }
}


static void
do_simulate(db_i &dbi, directory &scene_directory, fastf_t seconds)
{
    rt_db_internal internal;

    if (rt_db_get_internal(&internal, &scene_directory, &dbi, bn_mat_identity,
			   &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    AutoDestroyer<rt_db_internal, rt_db_free_internal> internal_autodestroy(
	&internal);

    {
	AutoDestroyer<rt_i, rt_free_rti> rt_instance(rt_new_rti(&dbi));

	if (!rt_instance.ptr) throw std::runtime_error("rt_new_rti() failed");

	if (rt_gettree(rt_instance.ptr, scene_directory.d_namep) < 0)
	    throw std::runtime_error("rt_gettree() failed");

	rt_prep(rt_instance.ptr);
	rt_instance_data::rt_instance = rt_instance.ptr;

	simulate::PhysicsWorld world;
	tree * const vtree = static_cast<rt_comb_internal *>(internal.idb_ptr)->tree;

	if (vtree) world_add_tree(world, *vtree, dbi);

	world.step(seconds);
    }

    if (rt_db_put_internal(&scene_directory, &dbi, &internal, &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_put_internal() failed");
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

    directory *dir = db_lookup(gedp->ged_wdbp->dbip, argv[1], false);

    if (!dir || dir->d_minor_type != ID_COMBINATION) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: '%s' is not a combination",
		       argv[0], argv[1]);
	return GED_ERROR;
    }

    try {
	fastf_t seconds = lexical_cast<fastf_t>(argv[2]);

	if (seconds < 0) throw std::runtime_error("invalid value for 'seconds'");

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
