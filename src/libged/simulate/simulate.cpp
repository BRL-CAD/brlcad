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


#include <map>
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


// for testing; a better attribute-based system will eventually be implemented
static std::map<std::string, std::string>
get_attributes(const db_i &dbi, const std::string &name)
{
    directory *dir = db_lookup(&dbi, name.c_str(), false);

    if (!dir)
	throw std::runtime_error("failed to lookup '" + name + "'");

    bu_attribute_value_set obj_avs;
    bu_avs_init_empty(&obj_avs);

    if (db5_get_attributes(&dbi, &obj_avs, dir) < 0)
	throw std::runtime_error("db5_get_attributes() failed");

    AutoDestroyer<bu_attribute_value_set, bu_avs_free> obj_avs_autodestroy(
	&obj_avs);

    std::map<std::string, std::string> result;

    for (std::size_t i = 0; i < obj_avs.count; ++i)
	if (!result.insert(std::make_pair(obj_avs.avp[i].name,
					  obj_avs.avp[i].value)).second)
	    throw std::runtime_error("duplicate keys");

    return result;
}


static btVector3
get_bounding_box(db_i &dbi, const std::string &name)
{
    directory *dir = db_lookup(&dbi, name.c_str(), false);

    if (!dir)
	throw std::runtime_error("failed to lookup '" + name + "'");

    point_t bb_min, bb_max;

    if (rt_bound_internal(&dbi, dir, bb_min, bb_max))
	throw std::runtime_error("failed to get bounding box");

    point_t lwh; // bb dimensions
    VSUB2(lwh, bb_max, bb_min);
    return btVector3(static_cast<btScalar>(lwh[0]), static_cast<btScalar>(lwh[1]),
		     static_cast<btScalar>(lwh[2])) / 2;
}


static fastf_t
get_volume(db_i &dbi, const std::string &name)
{
    // FIXME: not the true volume
    const btVector3 lwh = get_bounding_box(dbi, name) * 2;
    return lwh.getX() * lwh.getY() * lwh.getZ();
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

	    const btVector3 bounding_box = get_bounding_box(dbi, vtree.tr_l.tl_name);
	    btScalar mass;

	    std::map<std::string, std::string> attributes = get_attributes(dbi,
		    vtree.tr_l.tl_name);

	    if (attributes["simulate::type"] == "static")
		mass = 0;
	    else {
		const btScalar DENSITY = 1;
		mass = DENSITY * static_cast<btScalar>(get_volume(dbi, vtree.tr_l.tl_name));
	    }

	    world.add_object(bounding_box, mass, vtree.tr_l.tl_mat);
	    break;
	}

	case OP_UNION:
	    world_add_tree(world, *vtree.tr_b.tb_left, dbi);
	    world_add_tree(world, *vtree.tr_b.tb_right, dbi);
	    break;

	default:
	    throw std::runtime_error("unsupported operation in scene comb");
    }
}


}


int
ged_simulate(ged *gedp, int argc, const char **argv)
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc != 2) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: USAGE: %s <comb>", argv[0], argv[0]);
	return GED_ERROR;
    }

    directory *dir = db_lookup(gedp->ged_wdbp->dbip, argv[1], false);

    if (!dir || dir->d_minor_type != ID_COMBINATION) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: ERROR: '%s' is not a combination",
		       argv[0], argv[1]);
	return GED_ERROR;
    }

    rt_db_internal internal;
    GED_DB_GET_INTERNAL(gedp, &internal, dir, bn_mat_identity, &rt_uniresource,
			GED_ERROR);
    AutoDestroyer<rt_db_internal, rt_db_free_internal> internal_autodestroy(
	&internal);

    try {
	tree * const vtree = static_cast<rt_comb_internal *>(internal.idb_ptr)->tree;

	if (!vtree) throw std::runtime_error("combination has no members");

	simulate::PhysicsWorld world;
	world_add_tree(world, *vtree, *gedp->ged_wdbp->dbip);
	world.step(5);
    } catch (const std::runtime_error &e) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: ERROR: %s", argv[0], e.what());
	return GED_ERROR;
    }

    if (rt_db_put_internal(dir, gedp->ged_wdbp->dbip, &internal,
			   &rt_uniresource) < 0) {
	bu_vls_sprintf(gedp->ged_result_str, "%s: ERROR: rt_db_put_internal() failed",
		       argv[0]);
	return GED_ERROR;
    }

    bu_vls_sprintf(gedp->ged_result_str, "%s done", argv[0]);

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
		   "%s: ERROR: This command is disabled due to the absence of a physics library",
		   argv[0]);

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
