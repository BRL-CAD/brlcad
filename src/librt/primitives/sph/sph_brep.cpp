/*                    S P H _ B R E P . C P P
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
/** @file sph_brep.cpp
 *
 * Convert sph to b-rep
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "bn.h"
#include "bu.h"

#include "opennurbs_sphere.h"

extern "C" void
rt_sph_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_ell_internal *tip;

    RT_CK_DB_INTERNAL(ip);
    tip = (struct rt_ell_internal *)ip->idb_ptr;
    RT_ELL_CK_MAGIC(tip);

    ON_Sphere sph(tip->v, MAGNITUDE(tip->a));
    *b = ON_BrepSphere(sph);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
