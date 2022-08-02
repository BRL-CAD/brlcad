/*                 R T _ I N S T A N C E . H P P
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
/** @file rt_instance.hpp
 *
 * Manage rt instances.
 *
 */


#ifndef SIMULATE_RT_INSTANCE_H
#define SIMULATE_RT_INSTANCE_H


#include "common.h"

#include "rt/db_instance.h"
#include "rt/xray.h"

#include <btBulletDynamicsCommon.h>

#include <vector>


namespace simulate
{


class RtInstance
{
public:
    explicit RtInstance(db_i &db);

    std::vector<std::pair<btVector3, btVector3> > get_overlaps(
	const db_full_path &path_a, const db_full_path &path_b,
	const xrays &rays) const;


private:
    db_i &m_db;
};


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
