/*                 B R E P L I C A T O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file breplicator.cpp
 *
 * Breplicator is a tool for testing the new boundary representation
 * (BREP) primitive type in librt.  It creates primitives via
 * mk_brep() for testing purposes.
 *
 * Author -
 *   Christopher Sean Morrison
 *   Ed Davisson
 */

#include "common.h"

#include "machine.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "bn.h"
#include "bu.h"


ON_Brep *
generate_brep(int count, ON_3dPoint *points)
{
    ON_Brep *brep = new ON_Brep();
    
    /* make an arb8 */

    // VERTICES

    for (int i=0; i<count; i++) {
	brep->NewVertex(points[i], SMALL_FASTF);
    }

    // LEFT SEGMENTS

    // 0
    ON_Curve* segment01 = new ON_LineCurve(points[0], points[1]);
    segment01->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment01);

    // 1
    ON_Curve* segment12 = new ON_LineCurve(points[1], points[2]);
    segment12->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment12);

    // 2
    ON_Curve* segment23 = new ON_LineCurve(points[2], points[3]);
    segment23->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment23);

    // 3
    ON_Curve* segment30 = new ON_LineCurve(points[3], points[0]);
    segment30->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment30);

    // RIGHT SEGMENTS (these are flipped)

    // 4
    ON_Curve* segment45 = new ON_LineCurve(points[4], points[5]);
    segment45->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment45);

    // 5
    ON_Curve* segment56 = new ON_LineCurve(points[5], points[6]);
    segment56->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment56);

    // 6
    ON_Curve* segment67 = new ON_LineCurve(points[6], points[7]);
    segment67->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment67);

    // 7
    ON_Curve* segment74 = new ON_LineCurve(points[7], points[4]);
    segment74->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment74);

    // HORIZONTAL SEGMENTS

    // 8
    ON_Curve* segment04 = new ON_LineCurve(points[0], points[4]);
    segment04->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment04);

    // 9
    ON_Curve* segment51 = new ON_LineCurve(points[5], points[1]);
    segment51->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment51);

    // 10
    ON_Curve* segment26 = new ON_LineCurve(points[2], points[6]);
    segment26->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment26);

    // 11
    ON_Curve* segment73 = new ON_LineCurve(points[7], points[3]);
    segment73->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment73);

    // SURFACES

    ON_NurbsSurface* surf0123 = new ON_NurbsSurface(3 /*dimension*/, FALSE /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf0123->SetKnot(0, 0, 0.0); surf0123->SetKnot(0, 1, 1.0); surf0123->SetKnot(1, 0, 0.0); surf0123->SetKnot(1, 1, 1.0);
    surf0123->SetCV(0, 0, points[0]);
    surf0123->SetCV(1, 0, points[1]);
    surf0123->SetCV(1, 1, points[2]);
    surf0123->SetCV(0, 1, points[3]);
    brep->m_S.Append(surf0123);
    
    ON_NurbsSurface* surf4765 = new ON_NurbsSurface(3 /*dimension*/, FALSE /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf4765->SetKnot(0, 0, 0.0); surf4765->SetKnot(0, 1, 1.0); surf4765->SetKnot(1, 0, 0.0); surf4765->SetKnot(1, 1, 1.0);
    surf4765->SetCV(0, 0, points[4]);
    surf4765->SetCV(1, 0, points[7]);
    surf4765->SetCV(1, 1, points[6]);
    surf4765->SetCV(0, 1, points[5]);
    brep->m_S.Append(surf4765);
    
    ON_NurbsSurface* surf0451 = new ON_NurbsSurface(3 /*dimension*/, FALSE /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf0451->SetKnot(0, 0, 0.0); surf0451->SetKnot(0, 1, 1.0); surf0451->SetKnot(1, 0, 0.0); surf0451->SetKnot(1, 1, 1.0);
    surf0451->SetCV(0, 0, points[0]);
    surf0451->SetCV(1, 0, points[4]);
    surf0451->SetCV(1, 1, points[5]);
    surf0451->SetCV(0, 1, points[1]);
    brep->m_S.Append(surf0451);
    
    ON_NurbsSurface* surf2673 = new ON_NurbsSurface(3 /*dimension*/, FALSE /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf2673->SetKnot(0, 0, 0.0); surf2673->SetKnot(0, 1, 1.0); surf2673->SetKnot(1, 0, 0.0); surf2673->SetKnot(1, 1, 1.0);
    surf2673->SetCV(0, 0, points[2]);
    surf2673->SetCV(1, 0, points[6]);
    surf2673->SetCV(1, 1, points[7]);
    surf2673->SetCV(0, 1, points[3]);
    brep->m_S.Append(surf2673);
    
    ON_NurbsSurface* surf1562 = new ON_NurbsSurface(3 /*dimension*/, FALSE /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf1562->SetKnot(0, 0, 0.0); surf1562->SetKnot(0, 1, 1.0); surf1562->SetKnot(1, 0, 0.0); surf1562->SetKnot(1, 1, 1.0);
    surf1562->SetCV(0, 0, points[1]);
    surf1562->SetCV(1, 0, points[5]);
    surf1562->SetCV(1, 1, points[6]);
    surf1562->SetCV(0, 1, points[2]);
    brep->m_S.Append(surf1562);
    
    ON_NurbsSurface* surf0374 = new ON_NurbsSurface(3 /*dimension*/, FALSE /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf0374->SetKnot(0, 0, 0.0); surf0374->SetKnot(0, 1, 1.0); surf0374->SetKnot(1, 0, 0.0); surf0374->SetKnot(1, 1, 1.0);
    surf0374->SetCV(0, 0, points[0]);
    surf0374->SetCV(1, 0, points[3]);
    surf0374->SetCV(1, 1, points[7]);
    surf0374->SetCV(0, 1, points[4]);
    brep->m_S.Append(surf0374);

    // TRIM CURVES

    ON_Curve* trim01 = new ON_LineCurve(ON_2dPoint(0,0), ON_2dPoint(1,0));
    trim01->SetDomain(0.0, 1.0);
    brep->m_C2.Append(trim01);

    ON_Curve* trim12 = new ON_LineCurve(ON_2dPoint(1,0), ON_2dPoint(1,1));
    trim12->SetDomain(0.0, 1.0);
    brep->m_C2.Append(trim12);

    ON_Curve* trim23 = new ON_LineCurve(ON_2dPoint(1,1), ON_2dPoint(0,1));
    trim23->SetDomain(0.0, 1.0);
    brep->m_C2.Append(trim23);

    ON_Curve* trim30 = new ON_LineCurve(ON_2dPoint(0,1), ON_2dPoint(0,0));
    trim30->SetDomain(0.0, 1.0);
    brep->m_C2.Append(trim30);

    // EDGES

    brep->NewEdge(brep->m_V[0], brep->m_V[1], 0, NULL, SMALL_FASTF);
    brep->NewEdge(brep->m_V[1], brep->m_V[2], 1, NULL, SMALL_FASTF);
    brep->NewEdge(brep->m_V[2], brep->m_V[3], 2, NULL, SMALL_FASTF);
    brep->NewEdge(brep->m_V[3], brep->m_V[0], 3, NULL, SMALL_FASTF);

    brep->NewEdge(brep->m_V[4], brep->m_V[7], 7, NULL, SMALL_FASTF);
    brep->NewEdge(brep->m_V[7], brep->m_V[6], 6, NULL, SMALL_FASTF);
    brep->NewEdge(brep->m_V[6], brep->m_V[5], 5, NULL, SMALL_FASTF);
    brep->NewEdge(brep->m_V[5], brep->m_V[4], 4, NULL, SMALL_FASTF);

    brep->NewEdge(brep->m_V[5], brep->m_V[1], 9, NULL, SMALL_FASTF);
    brep->NewEdge(brep->m_V[0], brep->m_V[4], 8, NULL, SMALL_FASTF);
    brep->NewEdge(brep->m_V[2], brep->m_V[6], 10, NULL, SMALL_FASTF);
    brep->NewEdge(brep->m_V[7], brep->m_V[3], 11, NULL, SMALL_FASTF);



    return brep;
}


int
main(int argc, char *argv[])
{
    struct rt_wdb *wdbp = NULL;
    const char *name = "brep";
    ON_Brep *brep = NULL;
    int ret;

    bu_log("Breplicating...please wait...\n");

    ON_3dPoint points[8] = {
	/* left */
	ON_3dPoint(0.0, 0.0, 0.0), // 0
	ON_3dPoint(1.0, 0.0, 0.0), // 1
	ON_3dPoint(1.0, 0.0, 2.5), // 2
	ON_3dPoint(0.0, 0.0, 2.5), // 3
	/* right */
	ON_3dPoint(0.0, 1.0, 0.0), // 4
	ON_3dPoint(1.0, 1.0, 0.0), // 5
	ON_3dPoint(1.0, 1.0, 2.5), // 6
	ON_3dPoint(0.0, 1.0, 2.5), // 7
    };

    brep = generate_brep(8, points);
    if (!brep) {
	bu_exit(1, "ERROR: We don't have a valid BREP\n");
    }

    ON_TextLog log(stdout);
    brep->Dump(log);

    wdbp = wdb_fopen("breplicator.g");
    if (!wdbp) {
	delete brep;
	bu_exit(2, "ERROR: Unable to open breplicator.g\n");
    }
    mk_id(wdbp, "Breplicator test geometry");

    bu_log("Creating the BREP as BRL-CAD geometry\n");
    ret = mk_brep( wdbp, name, brep);
    if (ret) {
	delete brep;
	wdb_close(wdbp);
	bu_exit(3, "ERROR: Unable to export %s\n", name);
    }

    bu_log("Done.\n");

    delete brep;
    wdb_close(wdbp);

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// End:
// ex: shiftwidth=4 tabstop=8
