/*                      T G F - G . C P P
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
/** @file tgf-g.cpp
 *
 * INTAVAL Target Geometry File to BRL-CAD converter:
 * main function
 *
 *  Origin -
 *	TNO (Netherlands)
 *	IABG mbH (Germany)
 */

#include <iostream>
#include <fstream>

#include "regtab.h"
#include "read_dra.h"


int main
(
    int   argc,
    char* argv[]
) {
    int ret = 0;

    if (argc < 4) {
	std::cout << "Usage: " << argv[0] << " <DRA-mat> <DRA-geo> <BRL-g>" << std::endl;
	ret = 1;
    }
    else {
	FILE* in = fopen(argv[1], "r");

	if (in == 0) {
	    std::cout << "Error reading DRA-mat file" << std::endl;
	    ret = 1;
	}
	else {
	    ret = readMaterials(in);
	    fclose(in);

	    if (ret == 0) {
		std::ifstream is(argv[2]);

		if (!is.is_open()) {
		    std::cout << "Error reading DRA-geo file" << std::endl;
		    ret = 1;
		}
		else {
		    struct rt_wdb* wdbp = wdb_fopen(argv[3]); // force create

		    conv(is, wdbp);
		    createRegions(wdbp);
		    wdb_close(wdbp);
		}
	    }
	}
    }

    return ret;
}
