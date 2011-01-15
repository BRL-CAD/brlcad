/*                    T O R _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file tor_brep.cpp
 *
 * Description - Convert torus to brep
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "opennurbs_torus.h"


extern "C" void
rt_tor_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    struct rt_tor_internal *tip;

    RT_CK_DB_INTERNAL(ip);
    tip = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tip);

    ON_3dPoint origin(tip->v);
    ON_3dVector normal(tip->h);
    ON_Plane p(origin, normal);
    ON_Torus tor(p, tip->r_a, tip->r_h);
    *b = ON_BrepTorus(tor);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
