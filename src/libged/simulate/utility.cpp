/*                     U T I L I T Y . C P P
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
/** @file utility.cpp
 *
 * Helpers used by simulate.
 *
 */


#include "common.h"


#ifdef HAVE_BULLET


#include "utility.hpp"

#include "bu/log.h"
#include "rt/db_attr.h"


namespace
{


HIDDEN void
set_region(db_i &db, directory &dir, const bool is_region)
{
    RT_CK_DBI(&db);
    RT_CK_DIR(&dir);

    if (!(dir.d_flags & RT_DIR_COMB))
	bu_bomb("invalid directory type");

    if (is_region) {
	if (dir.d_flags & RT_DIR_REGION)
	    bu_bomb("already a region");

	if (db5_update_attribute(dir.d_namep, "region", "R", &db))
	    bu_bomb("db5_update_attribute() failed");

	dir.d_flags |= RT_DIR_REGION;
    } else {
	if (!(dir.d_flags & RT_DIR_REGION))
	    bu_bomb("not a region");

	if (db5_update_attribute(dir.d_namep, "region", "", &db))
	    bu_bomb("db5_update_attribute() failed");

	dir.d_flags &= ~RT_DIR_REGION;
    }
}


}


namespace simulate
{


TemporaryRegionHandle::TemporaryRegionHandle(db_i &db,
	const std::string &path) :
    m_db(db),
    m_dir(NULL),
    m_dir_modified(false),
    m_parent_regions()
{
    RT_CK_DBI(&m_db);

    db_full_path full_path;
    db_full_path_init(&full_path);
    AutoPtr<db_full_path, db_free_full_path> autofree_full_path(&full_path);

    if (db_string_to_path(&full_path, &db, path.c_str()))
	bu_bomb("db_string_to_path() failed");

    m_dir = DB_FULL_PATH_CUR_DIR(&full_path);

    if (m_dir->d_flags & RT_DIR_COMB && !(m_dir->d_flags & RT_DIR_REGION)) {
	set_region(m_db, *m_dir, true);
	m_dir_modified = true;
    }

    for (std::size_t i = 0; i < full_path.fp_len - 1; ++i)
	if (full_path.fp_names[i]->d_flags & RT_DIR_REGION) {
	    set_region(m_db, *full_path.fp_names[i], false);
	    m_parent_regions.push_back(full_path.fp_names[i]);
	}
}


TemporaryRegionHandle::~TemporaryRegionHandle()
{
    if (m_dir_modified)
	set_region(m_db, *m_dir, false);

    for (std::vector<directory *>::const_iterator it = m_parent_regions.begin();
	 it != m_parent_regions.end(); ++it)
	set_region(m_db, **it, true);
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
