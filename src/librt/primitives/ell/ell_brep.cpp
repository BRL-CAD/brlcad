/*                    E L L _ B R E P . C P P
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
/** @file ell_brep.cpp
 *
 * Convert a Generalized Ellipsoid to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"

/**
 * R T _ E L L _ B R E P
 */
extern "C" void
rt_ell_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_ell_internal *eip;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ell_internal *)ip->idb_ptr;
    RT_ELL_CK_MAGIC(eip);

    point_t origin;
    VSET(origin, 0, 0, 0);

    ON_Sphere sph(origin, MAGNITUDE(eip->a));

    // Get the NURBS form of the surface
    ON_NurbsSurface *ellcurvedsurf = ON_NurbsSurface::New();
    sph.GetNurbForm(*ellcurvedsurf);

    // Scale control points for b and c
    for (int i = 0; i < ellcurvedsurf->CVCount(0); i++) {
	for (int j = 0; j < ellcurvedsurf->CVCount(1); j++) {
	    point_t cvpt;
	    ON_4dPoint ctrlpt;
	    ellcurvedsurf->GetCV(i, j, ctrlpt);
	    VSET(cvpt, ctrlpt.x, ctrlpt.y * MAGNITUDE(eip->b)/MAGNITUDE(eip->a), ctrlpt.z * MAGNITUDE(eip->c)/MAGNITUDE(eip->a));
	    ON_4dPoint newpt = ON_4dPoint(cvpt[0], cvpt[1], cvpt[2], ctrlpt.w);
	    ellcurvedsurf->SetCV(i, j, newpt);
	}
    }

    ellcurvedsurf->SetDomain(0, 0.0, 1.0);
    ellcurvedsurf->SetDomain(1, 0.0, 1.0);


    // Rotate and Translate


    // Make final BREP structure
    (*b)->m_S.Append(ellcurvedsurf);
    int surfindex = (*b)->m_S.Count();
    (*b)->NewFace(surfindex - 1);
    int faceindex = (*b)->m_F.Count();
    (*b)->NewOuterLoop(faceindex-1);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
