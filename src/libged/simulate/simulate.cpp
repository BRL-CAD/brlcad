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

#include <sstream>
#include <stdexcept>

#include "ged.h"


namespace
{


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


HIDDEN void
get_tree_objects(db_i &db_instance, tree &vtree,
		 simulate::TreeUpdater &tree_updater,
		 std::vector<simulate::WorldObject *> &objects)
{
    switch (vtree.tr_op) {
	case OP_DB_LEAF: {
	    directory *dir = db_lookup(&db_instance, vtree.tr_l.tl_name, LOOKUP_NOISY);

	    if (!dir) throw std::runtime_error("db_lookup() failed");

	    if (!vtree.tr_l.tl_mat) {
		vtree.tr_l.tl_mat = static_cast<matp_t>(bu_malloc(sizeof(mat_t), "tl_mat"));
		MAT_IDN(vtree.tr_l.tl_mat);
	    }

	    objects.push_back(simulate::WorldObject::create(db_instance, *dir,
			      vtree.tr_l.tl_mat, tree_updater));
	    break;
	}

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
    std::vector<simulate::WorldObject *> objects;
    simulate::PhysicsWorld world;
    simulate::TreeUpdater tree_updater(db_instance, scene_directory);
    tree * const vtree = tree_updater.get_tree();

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
