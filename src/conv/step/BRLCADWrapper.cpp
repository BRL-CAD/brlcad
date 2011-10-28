/*                 BRLCADWrapper.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @file step/BRLCADWrapper.cpp
 *
 * C++ wrapper to BRL-CAD database functions.
 *
 */

#include "common.h"

/* interface header */
#include "./BRLCADWrapper.h"

/* system headers */
#include <sstream>
#include <iostream>


int BRLCADWrapper::sol_reg_cnt=0;


BRLCADWrapper::BRLCADWrapper() {
    outfp = NULL;
}


BRLCADWrapper::~BRLCADWrapper() {
    Close();
}


bool
BRLCADWrapper::OpenFile(const char *flnm) {
    //TODO: need to check to make sure we aren't overwriting

    /* open brlcad instance */
    if ((outfp = wdb_fopen(flnm)) == NULL) {
    	bu_log("Cannot open output file (%s)\n", flnm);
    	return false;
    }

    // hold on to output filename
    filename = flnm;

    mk_id(outfp, "Output from STEP converter step-g.");

    return true;
}


bool
BRLCADWrapper::WriteHeader() {
    db5_update_attribute("_GLOBAL", "HEADERINFO", "test header attributes", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERCLASS", "test header classification", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERAPPROVED", "test header approval", outfp->dbip);
    return true;
}


bool
BRLCADWrapper::WriteSphere(double *center, double radius) {
    center[X] = 0.0;
    center[Y] = 0.0;
    center[Z] = 0.0;
    mk_sph(outfp, "s1", center, radius);
    return true;
}


bool
BRLCADWrapper::WriteBrep(std::string name, ON_Brep *brep) {
    std::ostringstream str;
    std::string strcnt;

    if (name.empty()) {
	name = filename;
    }
    //TODO: need to do some name checks here for now static
    //region/solid number increment
    str << sol_reg_cnt++;
    strcnt = str.str();
    std::string sol = name + strcnt + ".s";
    std::string reg = name + strcnt + ".r";

    mk_brep(outfp, sol.c_str(), brep);
    unsigned char rgb[] = {200, 180, 180};
    mk_region1(outfp, reg.c_str(), sol.c_str(), "plastic", "", rgb);

    return true;
}


bool
BRLCADWrapper::Close() {

    if (outfp) {
	wdb_close(outfp);
	outfp = NULL;
    }

    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
