/*                    T O R _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
 *
 * Incorporates modified versions of openNURBS torus functions:
 *
 * Copyright (c) 1993-2012 Robert McNeel & Associates. All rights reserved.
 * OpenNURBS, Rhinoceros, and Rhino3D are registered trademarks of Robert
 * McNeel & Associates.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
 * MERCHANTABILITY ARE HEREBY DISCLAIMED.
 *
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


HIDDEN ON_RevSurface*
Torus_RevSurfaceForm(const ON_Torus& torus )
{
    ON_Circle circle = torus.MinorCircleRadians(0.0);
    ON_ArcCurve* circle_crv = new ON_ArcCurve(circle);
    ON_RevSurface* pRevSurface = new ON_RevSurface();
    pRevSurface->m_angle.Set(0.0,2.0*ON_PI);
    pRevSurface->m_t = pRevSurface->m_angle;
    pRevSurface->m_curve = circle_crv;
    pRevSurface->m_axis.from = torus.plane.origin;
    pRevSurface->m_axis.to = torus.plane.origin + torus.plane.zaxis;
    pRevSurface->m_bTransposed = false;
    double r[2], h[2];
    h[0] = fabs(torus.minor_radius);
    h[1] = -h[0];
    r[0] = fabs(torus.major_radius) + h[0];
    r[1] = -r[0];
    int i, j, k, n=0;
    ON_3dPoint pt[8];
    for (i=0;i<2;i++)
    {
	for (j=0;j<2;j++)
	{
	    for (k=0;k<2;k++)
	    {
		pt[n++] = torus.plane.PointAt( r[i], r[j], h[k] );
	    }
	}
    }
    pRevSurface->m_bbox.Set( 3, 0, 8, 3, &pt[0].x );
    return pRevSurface;
}

ON_Brep*
Torus_Brep( const ON_Torus& torus)
{
    ON_BOOL32 bArcLengthParameterization = true;
    ON_Brep* brep = NULL;
    ON_RevSurface* pRevSurface = Torus_RevSurfaceForm(torus);
    if ( pRevSurface )
    {
	if ( bArcLengthParameterization )
	{
	    double r = fabs(torus.major_radius);
	    if ( r <= ON_SQRT_EPSILON )
		r = 1.0;
	    r *= ON_PI;
	    pRevSurface->SetDomain(0,0.0,2.0*r);
	    r = fabs(torus.minor_radius);
	    if ( r <= ON_SQRT_EPSILON )
		r = 1.0;
	    r *= ON_PI;
	    pRevSurface->SetDomain(1,0.0,2.0*r);
	}
	brep = ON_BrepRevSurface( pRevSurface, false, false, NULL );
	if ( !brep )
	    delete pRevSurface;
    }
    return brep;
}


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
    *b = Torus_Brep(tor);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
