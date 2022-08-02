/*             R T _ M O T I O N _ S T A T E . C P P
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
    MAT_DELTAS_VEC(matrix, transform.getOrigin() * simulate::world_to_application);
    VMOVE(&matrix[0], transform.getBasis()[0]);
    VMOVE(&matrix[4], transform.getBasis()[1]);
    VMOVE(&matrix[8], transform.getBasis()[2]);
}


HIDDEN void
path_to_matrix(db_i &db, const db_full_path &path, fastf_t * const result)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    if (!result)
	bu_bomb("missing argument");

    MAT_IDN(result);

    db_full_path temp;
    const simulate::AutoPtr<db_full_path, db_free_full_path> autofree_path(&temp);
    db_full_path_init(&temp);
    db_dup_full_path(&temp, &path);

    if (1 != db_path_to_mat(&db, &temp, result, 0, &rt_uniresource))
	throw std::runtime_error("db_path_to_mat() failed");
}


HIDDEN void
check_region_path(const db_full_path &path)
{
    RT_CK_FULL_PATH(&path);

    if (path.fp_len < 2)
	throw simulate::InvalidSimulationError("invalid path");

    if (path.fp_names[0]->d_nref)
	throw simulate::InvalidSimulationError(std::string() + "root '" +
					       path.fp_names[0]->d_namep + "' is referenced elsewhere");

    for (std::size_t i = 1; i < path.fp_len - 1; ++i)
	if (path.fp_names[i]->d_nref != 1)
	    throw simulate::InvalidSimulationError(std::string() +
						   "multiple references to parent combination '" + path.fp_names[i]->d_namep +
						   "'");
}


HIDDEN void
apply_tree_matrix(db_i &db, const db_full_path &path,
		  const fastf_t * const matrix)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    if (!matrix)
	bu_bomb("missing argument");

    check_region_path(path);

    directory &parent_dir = *path.fp_names[path.fp_len - 2];
    rt_db_internal parent_internal;
    const simulate::AutoPtr<rt_db_internal, rt_db_free_internal> autofree_internal(
	&parent_internal);
    RT_DB_INTERNAL_INIT(&parent_internal);

    if (0 > rt_db_get_internal(&parent_internal, &parent_dir, &db, bn_mat_identity,
			      &rt_uniresource))
	bu_bomb("rt_db_get_internal() failed");

    rt_comb_internal &comb = *static_cast<rt_comb_internal *>
    (parent_internal.idb_ptr);
    RT_CK_COMB(&comb);

    tree * const leaf = db_find_named_leaf(comb.tree,
					   DB_FULL_PATH_CUR_DIR(&path)->d_namep);

    if (!leaf)
	bu_bomb("db_find_named_leaf() failed");

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


RtMotionState::RtMotionState(db_i &db, const db_full_path &path,
			     const btVector3 &center_of_mass) :
    m_db(db),
    m_path(),
    m_autofree_m_path(&m_path),
    m_transform(btMatrix3x3::getIdentity(), center_of_mass)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    db_full_path_init(&m_path);
    db_dup_full_path(&m_path, &path);

    check_region_path(m_path);
}


const db_full_path &
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
    const btTransform incremental_transform = transform * m_transform.inverse();
    m_transform = transform;

    mat_t transform_matrix = MAT_INIT_IDN;
    bt_transform_to_matrix(incremental_transform, transform_matrix);

    mat_t parent_path_matrix = MAT_INIT_IDN;
    mat_t parent_path_matrix_inverse = MAT_INIT_IDN;
    {
	db_full_path parent_path = get_path();
	DB_FULL_PATH_POP(&parent_path);
	path_to_matrix(m_db, parent_path, parent_path_matrix);
	bn_mat_inv(parent_path_matrix_inverse, parent_path_matrix);
    }

    // leaf = path_inverse * transform * path * leaf_prev
    // so that when calculating the final transformation of the leaf node:
    // path * leaf == transform * path * leaf_prev
    mat_t matrix = MAT_INIT_IDN;
    MAT_COPY(matrix, parent_path_matrix);
    bn_mat_mul2(transform_matrix, matrix);
    bn_mat_mul2(parent_path_matrix_inverse, matrix);
    apply_tree_matrix(m_db, get_path(), matrix);
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
