/*                 BRLCADWrapper.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file BRLCADWrapper.cpp
 *
 * C++ wrapper to BRL-CAD database functions.
 *
 */

#include "BRLCADWrapper.h"

/* brlcad headers */
#include <bu.h>
#include <wdb.h>


BRLCADWrapper::BRLCADWrapper() {
	// TODO Auto-generated constructor stub

}

BRLCADWrapper::~BRLCADWrapper() {
	// TODO Auto-generated destructor stub
}

bool
BRLCADWrapper::OpenFile( const char *flnm ) {

	/* open brlcad instance */
    if ((outfp = wdb_fopen(flnm)) == NULL) {
    	bu_log("Cannot open output file (%s)\n", flnm);
    	return false;
    }

    /* testing */
    mk_id(outfp, "Output from STEP converter step-g.");
    bu_log("Output from STEP converter step-g.\n");
}
// testing attribute write
bool
BRLCADWrapper::WriteHeader() {
    db5_update_attribute("_GLOBAL", "HEADERINFO", "test header attributes", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERCLASS", "test header classification", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERAPPROVED", "test header approval", outfp->dbip);
}
// testing of solid write
bool
BRLCADWrapper::WriteSphere(double *center, double radius) {
    center[X] = 0.0;
    center[Y] = 0.0;
    center[Z] = 0.0;
    mk_sph(outfp, "s1", center, 10.0);
}
bool
BRLCADWrapper::Close() {

    wdb_close(outfp);

    return true;
}
