/*                   N U R B _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009 United States Government as represented by
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
/** @file nurb_poly.c
 *
 * Convert old NURBS to new NURBS
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "nurb.h"
#include "brep.h"


extern "C" void
rt_nurb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    int i;
    struct rt_nurb_internal *nip;
    point_t min_pt, max_pt;
    point_t center;

    RT_CK_DB_INTERNAL(ip);
    nip = (struct rt_nurb_internal *)ip->idb_ptr;
    RT_NURB_CK_MAGIC(nip);
    
    for (i = 0; i < nip->nsrf; i++) {
	NMG_CK_SNURB(nip->srfs[i]);
	rt_nurb_s_bound(nip->srfs[i], min_pt, max_pt);
    }

    VADD2SCALE(center, max_pt, min_pt, 0.5);

    /* create a bounding sphere for now */
    ON_Sphere sph(center, DIST_PT_PT(min_pt, max_pt));
    *b = ON_BrepSphere(sph);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
