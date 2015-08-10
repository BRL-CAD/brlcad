/*                    T W I S T E D C U B E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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
/** @file twistedcube.cpp
 *
 * Implementation of TwistedCube functions used in brep_simple.cpp,
 * brep_cube.cpp, and surfaceintersect.cpp
 *
 */

#include "twistedcube.h"

ON_Curve*
TwistedCubeEdgeCurve(const ON_3dPoint& from, const ON_3dPoint& to)
{
    // creates a 3d line segment to be used as a 3d curve in an ON_Brep
    ON_Curve* c3d = new ON_LineCurve(from, to);
    c3d->SetDomain(0.0, 1.0); // XXX is this UV bounds?
    return c3d;
}

void
MakeTwistedCubeEdge(ON_Brep& brep, int from, int to, int curve)
{
    ON_BrepVertex& v0 = brep.m_V[from];
    ON_BrepVertex& v1 = brep.m_V[to];
    ON_BrepEdge& edge = brep.NewEdge(v0, v1, curve);
    edge.m_tolerance = 0.0; // exact!
}

ON_Surface*
TwistedCubeSideSurface(const ON_3dPoint& SW, const ON_3dPoint& SE, const ON_3dPoint& NE, const ON_3dPoint& NW)
{
    ON_NurbsSurface* pNurbsSurface = new ON_NurbsSurface(3, // dimension
							 0, // not rational
							 2, // u order
							 2, // v order,
							 2, // number of control vertices in u
							 2 // number of control verts in v
	);
    pNurbsSurface->SetCV(0, 0, SW);
    pNurbsSurface->SetCV(1, 0, SE);
    pNurbsSurface->SetCV(1, 1, NE);
    pNurbsSurface->SetCV(0, 1, NW);
    // u knots
    pNurbsSurface->SetKnot(0, 0, 0.0);
    pNurbsSurface->SetKnot(0, 1, 1.0);
    // v knots
    pNurbsSurface->SetKnot(1, 0, 0.0);
    pNurbsSurface->SetKnot(1, 1, 1.0);

    return pNurbsSurface;
}


ON_Curve*
TwistedCubeTrimmingCurve(const ON_Surface& s,
			 int side // 0 = SW to SE, 1 = SE to NE, 2 = NE to NW, 3 = NW, SW
    )
{
    // a trimming curve is a 2d curve whose image lies in the surface's
    // domain. The "active" portion of the surface is to the left of the
    // trimming curve (looking down the orientation of the curve). An
    // outer trimming loop consists of a simple closed curve running
    // counter-clockwise around the region it trims

    ON_2dPoint from, to;
    double u0, u1, v0, v1;
    s.GetDomain(0, &u0, &u1);
    s.GetDomain(1, &v0, &v1);

    switch (side) {
	case 0:
	    from.x = u0; from.y = v0;
	    to.x   = u1; to.y   = v0;
	    break;
	case 1:
	    from.x = u1; from.y = v0;
	    to.x   = u1; to.y   = v1;
	    break;
	case 2:
	    from.x = u1; from.y = v1;
	    to.x   = u0; to.y   = v1;
	    break;
	case 3:
	    from.x = u0; from.y = v1;
	    to.x   = u0; to.y   = v0;
	    break;
	default:
	    return NULL;
    }
    ON_Curve* c2d = new ON_LineCurve(from, to);
    c2d->SetDomain(0.0, 1.0);
    return c2d;
}


int // return value not used?
MakeTwistedCubeTrimmingLoop(ON_Brep& brep,
			    ON_BrepFace& face,
			    int UNUSED(v0), int UNUSED(v1), int UNUSED(v2), int UNUSED(v3), // indices of corner vertices
			    int e0, int eo0, // edge index + orientation w.r.t surface trim
			    int e1, int eo1,
			    int e2, int eo2,
			    int e3, int eo3)
{
    // get a reference to the surface
    const ON_Surface& srf = *brep.m_S[face.m_si];

    ON_BrepLoop& loop = brep.NewLoop(ON_BrepLoop::outer, face);

    // create the trimming curves running counter-clockwise around the
    // surface's domain, start at the south side
    ON_Curve* c2;
    int c2i, ei = 0, bRev3d = 0;
    ON_2dPoint q;

    // flags for isoparametric curves
    ON_Surface::ISO iso = ON_Surface::not_iso;

    for (int side = 0; side < 4; side++) {
	// side: 0=south, 1=east, 2=north, 3=west
	c2 = TwistedCubeTrimmingCurve(srf, side);
	c2i = brep.m_C2.Count();
	brep.m_C2.Append(c2);

	switch (side) {
	    case 0:
		ei = e0;
		bRev3d = (eo0 == -1);
		iso = ON_Surface::S_iso;
		break;
	    case 1:
		ei = e1;
		bRev3d = (eo1 == -1);
		iso = ON_Surface::E_iso;
		break;
	    case 2:
		ei = e2;
		bRev3d = (eo2 == -1);
		iso = ON_Surface::N_iso;
		break;
	    case 3:
		ei = e3;
		bRev3d = (eo3 == -1);
		iso = ON_Surface::W_iso;
		break;
	}

	ON_BrepTrim& trim = brep.NewTrim(brep.m_E[ei], bRev3d, loop, c2i);
	trim.m_iso = iso;

	// the type gives metadata on the trim type in this case, "mated"
	// means the trim is connected to an edge, is part of an
	// outer/inner/slit loop, no other trim from the same edge is
	// connected to the edge, and at least one trim from a different
	// loop is connected to the edge
	trim.m_type = ON_BrepTrim::mated; // i.e. this b-rep is closed, so
	// all trims have mates

	// not convinced these shouldn't be set with a member function
	trim.m_tolerance[0] = 0.0; // exact
	trim.m_tolerance[1] = 0.0; //
    }
    return loop.m_loop_index;
}

void
MakeTwistedCubeFace(ON_Brep& brep,
		    int surf,
		    int orientation,
		    int v0, int v1, int v2, int v3, // the indices of corner vertices
		    int e0, int eo0, // edge index + orientation
		    int e1, int eo1,
		    int e2, int eo2,
		    int e3, int eo3)
{
    ON_BrepFace& face = brep.NewFace(surf);
    MakeTwistedCubeTrimmingLoop(brep,
				face,
				v0, v1, v2, v3,
				e0, eo0,
				e1, eo1,
				e2, eo2,
				e3, eo3);
    // should the normal be reversed?
    face.m_bRev = (orientation == -1);
}


void
printPoints(struct rt_brep_internal* bi)
{
    ON_TextLog tl(stdout);
    ON_Brep* brep = bi->brep;

    if (brep) {
	const int count = brep->m_V.Count();
	for (int i = 0; i < count; i++) {
	    ON_BrepVertex& bv = brep->m_V[i];
	    bv.Dump(tl);
	}
    } else {
	bu_log("brep was NULL!\n");
    }
}
