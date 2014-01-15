/*                    H A L F _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file half_brep.cpp
 *
 * The b-rep conversion of primitive HALF (halfspace) should be
 * ignored. Return a NULL ON_Brep pointer.
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"


/**
 * R T _ H L F _ B R E P
 */
extern "C" void
rt_hlf_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_half_internal *hip;

    RT_CK_DB_INTERNAL(ip);
    hip = (struct rt_half_internal *)ip->idb_ptr;
    RT_HALF_CK_MAGIC(hip);

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
