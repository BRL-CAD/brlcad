/*                    B R E P _ C O B B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file proc-db/brep_cobb.cpp
 *
 * Creates a Cobb NURBS sphere with cube topology, per
 *
 * J. E. Cobb, "Tiling the Sphere with Rational Bezier Patches,"
 * Tech. Report TR UUCS-88-009, University of Utah, 1988.
 */

#include "common.h"

#include "bio.h"
#include "brep.h"
#include "bu/app.h"
#include "wdb.h"


int
main(int argc, char **argv)
{
    const char *id_name = "B-Rep Cobb Sphere";
    const char *geom_name = "cobb.s";
    const char *db_name = "brep_cobb.g";
    bu_setprogname(argv[0]);

    if (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?")) {
	    bu_log("Usage: %s [output.g]\n", argv[0]);
	    return 0;
	}
	db_name = argv[1];
    }
    if (argc > 2) {
	bu_log("Usage: %s [output.g]\n", argv[0]);
	return 1;
    }

    ON::Begin();
    bu_log("Writing a Cobb unit sphere b-rep to %s...\n", db_name);
    struct rt_wdb *outfp = wdb_fopen(db_name);
    if (!outfp) {
	bu_log("ERROR: unable to open %s for writing\n", db_name);
	ON::End();
	return 1;
    }
    mk_id(outfp, id_name);

    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *brep = ON_Brep_CobbSphereUnsewn(1.0, origin);
    if (!brep) {
	bu_log("ERROR: unable to construct the Cobb sphere\n");
	db_close(outfp->dbip);
	ON::End();
	return 1;
    }
    mk_brep(outfp, geom_name, (void *)brep);

    unsigned char rgb[] = {50, 255, 50};
    mk_region1(outfp, "cobb.r", geom_name, "plastic", "", rgb);

    db_close(outfp->dbip);
    delete brep;
    ON::End();
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
