/*                    J O I N T _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2022 United States Government as represented by
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
/** @file joint_brep.cpp
 *
 * The b-rep conversion of the "joint" pseudo solid. It's not a candidate
 * for b-rep conversion. Return a NULL ON_Brep pointer.
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"


extern "C" void
rt_joint_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_joint_internal *jip;

    RT_CK_DB_INTERNAL(ip);
    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);

    *b = NULL;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
