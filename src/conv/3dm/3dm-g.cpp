/*                       3 D M - G . C P P
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

#ifdef OBJ_BREP

#include "common.h"

#include "conv3dm-g.hpp"

#include <iostream>
#include <stdexcept>

#include "bu/getopt.h"


int
main(int argc, char** argv)
{
    static const char * const usage =
	"Usage: 3dm-g [-v] [-r] [-u] -o output_file.g input_file.3dm\n";

    bool verbose_mode = false;
    bool random_colors = false;
    bool use_uuidnames = false;
    const char *output_path = NULL;
    const char *input_path;

    int c;

    while ((c = bu_getopt(argc, argv, "o:dvt:s:ruh?")) != -1) {
	switch (c) {
	    case 's':	/* scale factor */
		break;

	    case 'o':	/* specify output file name */
		output_path = bu_optarg;
		break;

	    case 'd':	/* debug */
		break;

	    case 't':	/* tolerance */
		break;

	    case 'v':	/* verbose */
		verbose_mode = true;
		break;

	    case 'r':  /* randomize colors */
		random_colors = true;
		break;

	    case 'u':
		use_uuidnames = true;
		break;

	    default:
		std::cerr << usage;
		return 1;
	}
    }

    argv += bu_optind;
    input_path = argv[0];

    if (output_path == NULL) {
	std::cerr << usage;
	return 1;
    }

    conv3dm::RhinoConverter converter(output_path, verbose_mode);

    try {
	converter.write_model(input_path, use_uuidnames, random_colors);
    } catch (const std::logic_error &e) {
	std::cerr << "Conversion failed: " << e.what() << '\n';
	return 1;
    } catch (const std::runtime_error &e) {
	std::cerr << "Conversion failed: " << e.what() << '\n';
	return 1;
    }

    return 0;
}


#else //!OBJ_BREP


#include "common.h"

#include <iostream>


int
main()
{
    std::cerr <<
	      "ERROR: Boundary Representation object support is not available with\n"
	      "       this compilation of BRL-CAD.\n";

    return 1;
}


#endif //!OBJ_BREP

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
