/*                    H A L F _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2026 United States Government as represented by
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
 * An infinite halfspace has no finite closed B-Rep representation.  Direct
 * conversion therefore returns NULL.  Combination conversion supplies finite
 * operand bounds and constructs a bounded proxy before evaluating a halfspace
 * in context.
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"


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
