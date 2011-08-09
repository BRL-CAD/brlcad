/*                    V O L _ B R E P . C P P
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
/** @file vol_brep.cpp
 *
 * Convert a vol to an nmg, and thus to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "nmg.h"
#include "brep.h"

extern "C" {
    extern void rt_vol_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);
    extern void rt_nmg_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
}


/**
 * R T _ V O L _ B R E P
 */
extern "C" void
rt_vol_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_db_internal *tmp_internal = (struct rt_db_internal *) bu_malloc(sizeof(struct rt_db_internal), "allocate structure");
    RT_DB_INTERNAL_INIT(tmp_internal);
    struct rt_tess_tol ttmptol;
    ttmptol.abs = 0;
    ttmptol.rel = 0.01;
    ttmptol.norm = 0;
    const struct rt_tess_tol *ttol = &ttmptol;

    struct model *volm = nmg_mm();
    struct nmgregion *volr;
    tmp_internal->idb_ptr = (genptr_t)ip->idb_ptr;
    rt_vol_tess(&volr, volm, tmp_internal, ttol, tol);
    tmp_internal->idb_ptr = (genptr_t)volm;
    rt_nmg_brep(b, tmp_internal, tol);
    FREE_MODEL(volm);
    bu_free(tmp_internal, "free temporary rt_db_internal");
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
