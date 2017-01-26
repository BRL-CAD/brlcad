/*             R T _ M O T I O N _ S T A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2017 United States Government as represented by
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
/** @file rt_motion_state.cpp
 *
 * Motion state for objects in an rt instance.
 *
 */

#include "common.h"


#ifdef HAVE_BULLET


#include "rt_motion_state.hpp"
#include "utility.hpp"

#include "rt/db_internal.h"
#include "rt/nongeom.h"


namespace
{


HIDDEN void
bt_transform_to_matrix(const btTransform &transform, fastf_t * const matrix)
{
    if (!matrix)
	bu_bomb("missing argument");

    MAT_IDN(matrix);
    matrix[MDX] = transform.getOrigin().getX();
    matrix[MDY] = transform.getOrigin().getY();
    matrix[MDZ] = transform.getOrigin().getZ();
    VMOVE(&matrix[0], transform.getBasis()[0]);
    VMOVE(&matrix[4], transform.getBasis()[1]);
    VMOVE(&matrix[8], transform.getBasis()[2]);
}


HIDDEN void
check_region_path(const db_full_path &full_path)
{
    RT_CK_FULL_PATH(&full_path);

    if (full_path.fp_len < 2)
	throw simulate::InvalidSimulationError("invalid path");

    if (full_path.fp_names[0]->d_nref)
	throw simulate::InvalidSimulationError(std::string() + "root '" +
					       full_path.fp_names[0]->d_namep + "' is referenced elsewhere");

    for (std::size_t i = 1; i < full_path.fp_len - 1; ++i)
	if (full_path.fp_names[i]->d_nref != 1)
	    throw simulate::InvalidSimulationError(std::string() +
						   "multiple references to parent combination '" + full_path.fp_names[i]->d_namep +
						   "'");
}


HIDDEN void
apply_transform(db_i &db, const std::string &path, const btTransform &transform)
{
    db_full_path full_path;
    db_full_path_init(&full_path);
    simulate::AutoPtr<db_full_path, db_free_full_path> autofree_full_path(
	&full_path);

    if (db_string_to_path(&full_path, &db, path.c_str()))
	bu_bomb("db_string_to_path() failed");

    check_region_path(full_path);

    directory &parent_dir = *full_path.fp_names[full_path.fp_len - 2];
    rt_db_internal parent_internal;
    RT_DB_INTERNAL_INIT(&parent_internal);
    simulate::AutoPtr<rt_db_internal, rt_db_free_internal> autofree_internal(
	&parent_internal);

    if (0 > rt_db_get_internal(&parent_internal, &parent_dir, &db, bn_mat_identity,
			       &rt_uniresource))
	bu_bomb("rt_db_get_internal() failed");

    rt_comb_internal &comb = *static_cast<rt_comb_internal *>
			     (parent_internal.idb_ptr);
    RT_CK_COMB(&comb);

    tree * const leaf = db_find_named_leaf(comb.tree,
					   DB_FULL_PATH_CUR_DIR(&full_path)->d_namep);

    if (!leaf)
	bu_bomb("db_find_named_leaf() failed");

    mat_t matrix = MAT_INIT_IDN;
    bt_transform_to_matrix(transform, matrix);

    if (!leaf->tr_l.tl_mat) {
	leaf->tr_l.tl_mat = static_cast<fastf_t *>(bu_malloc(sizeof(mat_t), "tl_mat"));
	MAT_IDN(leaf->tr_l.tl_mat);
    }

    bn_mat_mul2(matrix, leaf->tr_l.tl_mat);

    if (0 > rt_db_put_internal(&parent_dir, &db, &parent_internal, &rt_uniresource))
	bu_bomb("rt_db_put_internal() failed");
}


}


namespace simulate
{


RtMotionState::RtMotionState(db_i &db, const std::string &path,
			     const btVector3 &center_of_mass) :
    m_db(db),
    m_path(path),
    m_transform(btMatrix3x3::getIdentity(), center_of_mass)
{
    RT_CK_DBI(&m_db);

    // Bullet requires that getWorldTransform() returns the center of mass of
    // the rigid body.
    //
    // This is implemented here by keeping a translation to the center of
    // mass in `m_transform`, but applying only the "incremental"
    // transformations to the BRL-CAD matrix in setWorldTransform().
}


const std::string &
RtMotionState::get_path() const
{
    return m_path;
}


void
RtMotionState::getWorldTransform(btTransform &dest) const
{
    dest = m_transform;
}


void
RtMotionState::setWorldTransform(const btTransform &transform)
{
    // FIXME: not properly handling arbitrary rotations applied via
    // parent combinations. This can be worked around by
    // using `xpush` on the scene path before starting the simulation.
    //
    // note: these btTransform objects only specify a rotation about the
    // center of mass followed by a translation.
    apply_transform(m_db, m_path, transform * m_transform.inverse());
    m_transform = transform;
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
