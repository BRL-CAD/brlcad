/*                  S I M U L A T I O N . C P P
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
/** @file simulation.cpp
 *
 * Brief description
 *
 */


#ifdef HAVE_BULLET


#include "common.h"

#include "simulation.hpp"

#include <stdexcept>


namespace simulate
{


Simulation::Simulation(db_i &db_instance, directory &vdirectory) :
    m_db_instance(db_instance),
    m_directory(vdirectory),
    m_tree_updater(db_instance, m_directory),
    m_objects()
{
    if (m_directory.d_minor_type != ID_COMBINATION)
	throw std::runtime_error("object is not a combination");

    tree * const vtree = m_tree_updater.get_tree();

    try {
	if (vtree) get_tree_objects(*vtree);
    } catch (...) {
	for (std::vector<WorldObject *>::iterator it = m_objects.begin();
	     it != m_objects.end(); ++it)
	    delete *it;

	throw;
    }

    m_collision_dispatcher.registerCollisionCreateFunc(
	RtCollisionShape::RT_SHAPE_TYPE, RtCollisionShape::RT_SHAPE_TYPE,
	new RtCollisionAlgorithm::CreateFunc);

    m_world.setGravity(btVector3(0.0, 0.0, -9.8));

}


Simulation::~Simulation()
{
    for (std::vector<WorldObject *>::iterator it = m_objects.begin();
	 it != m_objects.end(); ++it)
	delete *it;
}


void
Simulation::get_tree_objects(tree &vtree)
{
    switch (vtree.tr_op) {
	case OP_DB_LEAF: {
	    directory *dir = db_lookup(&m_db_instance, vtree.tr_l.tl_name, LOOKUP_NOISY);

	    if (!dir) throw std::runtime_error("db_lookup() failed");

	    if (!vtree.tr_l.tl_mat) {
		vtree.tr_l.tl_mat = static_cast<matp_t>(bu_malloc(sizeof(mat_t), "tl_mat"));
		MAT_IDN(vtree.tr_l.tl_mat);
	    }

	    WorldObject * const object = WorldObject::create(m_db_instance, *dir,
					 vtree.tr_l.tl_mat, m_tree_updater);
	    m_objects.push_back(object);
	    object->add_to_world(m_world);
	    break;
	}

	case OP_UNION:
	    get_tree_objects(*vtree.tr_b.tb_left);
	    get_tree_objects(*vtree.tr_b.tb_right);
	    break;

	default:
	    throw std::invalid_argument("unsupported operation in scene combination");
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
