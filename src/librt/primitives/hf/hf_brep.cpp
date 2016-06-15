/*                      H F _ B R E P . C P P
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
/** @file hf_brep.cpp
 *
 * The b-rep conversion of primitive Height Field. It's a deprecated
 * primitive, and dsp is used instead. In this file, hf is first
 * converted to dsp, and then to b-rep.
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"

extern "C" {
    extern int rt_hf_to_dsp(struct rt_db_internal *db_intern);
    extern void rt_dsp_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
}

extern "C" void
rt_hf_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_db_internal tmp_internal;
    RT_CK_DB_INTERNAL(ip);
    tmp_internal = *ip;
    rt_hf_to_dsp(&tmp_internal);

    rt_dsp_brep(b, &tmp_internal, tol);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
