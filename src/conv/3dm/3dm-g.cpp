/*                           3 D M - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file 3dm-g.cpp
 *
 * Program to convert a Rhino model (in a .3dm file) to a BRL-CAD .g
 * file.
 *
 */


#include "common.h"

#include "stdio.h" /* for sscanf */

#ifdef OBJ_BREP

#include "conv3dm-g.hpp"
#include "bu/getopt.h"
#include <iostream>


static const char * const USAGE = "USAGE: 3dm-g [-v vmode] [-r] [-u] -o output_file.g input_file.3dm\n";


int
main(int argc, char** argv)
{
    int verbose_mode = 0;
    bool random_colors = false;
    bool use_uuidnames = false;
    char* outputFileName = NULL;
    const char* inputFileName;

    int c;
    while ((c = bu_getopt(argc, argv, "o:dv:t:s:ruh?")) != -1) {
	switch (c) {
	    case 's':	/* scale factor */
		break;
	    case 'o':	/* specify output file name */
		outputFileName = bu_optarg;
		break;
	    case 'd':	/* debug */
		break;
	    case 't':	/* tolerance */
		break;
	    case 'v':	/* verbose */
		sscanf(bu_optarg, "%d", &verbose_mode);
		break;
	    case 'r':  /* randomize colors */
		random_colors = true;
		break;
	    case 'u':
		use_uuidnames = true;
		break;
	    default:
		std::cerr << USAGE;
		return 1;
	}
    }

    argc -= bu_optind;
    argv += bu_optind;
    inputFileName  = argv[0];
    if (outputFileName == NULL) {
	std::cerr << USAGE;
	return 1;
    }

    conv3dm::RhinoConverter converter(outputFileName);
    converter.write_model(inputFileName, use_uuidnames, random_colors);

    return 0;
}


#else


#include <iostream>


int
main()
{
    std::cerr << "ERROR: Boundary Representation object support is not available with\n"
	      "       this compilation of BRL-CAD.\n";

    return 1;
}




#endif //!OBJ_BREP




/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
