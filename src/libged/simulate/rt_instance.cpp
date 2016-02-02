/*                 R T _ I N S T A N C E . C P P
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
/** @file rt_instance.cpp
 *
 * Brief description
 *
 */


#ifdef HAVE_BULLET


#include "common.h"

#include "rt_instance.hpp"

#include <stdexcept>


namespace
{


HIDDEN rt_db_internal
duplicate_comb_internal(const rt_db_internal &source)
{
    if (source.idb_minor_type != ID_COMBINATION)
	throw std::invalid_argument("source is not a combination");

    rt_db_internal dest = source;
    BU_GET(dest.idb_ptr, rt_comb_internal);
    BU_AVS_INIT(&dest.idb_avs);
    bu_avs_merge(&dest.idb_avs, &source.idb_avs);

    {
	const rt_comb_internal &source_comb = *static_cast<const rt_comb_internal *>
					      (source.idb_ptr);
	rt_comb_internal &dest_comb = *static_cast<rt_comb_internal *>(dest.idb_ptr);

	dest_comb = source_comb;
	BU_VLS_INIT(&dest_comb.shader);
	BU_VLS_INIT(&dest_comb.material);
	bu_vls_vlscat(&dest_comb.shader, &source_comb.shader);
	bu_vls_vlscat(&dest_comb.material, &source_comb.material);
	dest_comb.tree = db_dup_subtree(source_comb.tree, &rt_uniresource);
    }

    return dest;
}


HIDDEN void
write_comb_internal(db_i &db_instance, directory &vdirectory,
		    const rt_db_internal &comb_internal)
{

    rt_db_internal temp_internal = duplicate_comb_internal(comb_internal);

    if (rt_db_put_internal(&vdirectory, &db_instance, &temp_internal,
			   &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_put_internal() failed");
}


}


namespace simulate
{


TreeUpdater::TreeUpdater(db_i &db_instance, directory &vdirectory) :
    m_db_instance(db_instance),
    m_directory(vdirectory),
    m_comb_internal(),
    m_is_modified(false),
    m_rt_instance(NULL)
{
    if (rt_db_get_internal(&m_comb_internal, &m_directory, &m_db_instance,
			   bn_mat_identity, &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    if (m_comb_internal.idb_minor_type != ID_COMBINATION)
	throw std::invalid_argument("object is not a combination");
}


TreeUpdater::~TreeUpdater()
{
    if (m_is_modified)
	write_comb_internal(m_db_instance, m_directory, m_comb_internal);

    if (m_rt_instance)
	rt_free_rti(m_rt_instance);

    rt_db_free_internal(&m_comb_internal);
}


void
TreeUpdater::mark_modified()
{
    m_is_modified = true;
}


tree *
TreeUpdater::get_tree()
{
    return static_cast<rt_comb_internal *>(m_comb_internal.idb_ptr)->tree;
}


rt_i &
TreeUpdater::get_rt_instance() const
{
    if (m_is_modified)
	write_comb_internal(m_db_instance, m_directory, m_comb_internal);
    else if (m_rt_instance)
	return *m_rt_instance;

    if (m_rt_instance)
	rt_free_rti(m_rt_instance);

    m_rt_instance = rt_new_rti(&m_db_instance);

    if (!m_rt_instance)
	throw std::runtime_error("rt_new_rti() failed");

    if (rt_gettree(m_rt_instance, m_directory.d_namep) != 0) {
	rt_free_rti(m_rt_instance);
	m_rt_instance = NULL;
	throw std::runtime_error("rt_gettree() failed");
    }

    rt_prep(m_rt_instance);
    m_is_modified = false;
    return *m_rt_instance;
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
