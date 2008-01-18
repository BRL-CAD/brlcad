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

    for (int i=0; i<count; i++) {
	bu_log("Adding vertex %d (%.2f, %.2f, %.2f)\n", i, points[i][0], points[i][1], points[i][2]);
	ON_BrepVertex& v = brep->NewVertex(points[i]);
	v.m_tolerance = VDIVIDE_TOL;
    }
    
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
	ON_3dPoint(1.0, 0.0, 2.0), // 2
	ON_3dPoint(0.0, 0.0, 2.0), // 3
	/* right */
	ON_3dPoint(0.0, 1.0, 0.0), // 4
	ON_3dPoint(1.0, 1.0, 0.0), // 5
	ON_3dPoint(1.0, 1.0, 2.0), // 6
	ON_3dPoint(0.0, 1.0, 2.0), // 7
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
