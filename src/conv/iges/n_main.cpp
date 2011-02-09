/*                      N _ M A I N . C P P
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

#include "common.h"

#include <iostream>

#include "n_iges.hpp"
#include "brlcad_brep.hpp"


using namespace brlcad;

int
main(int argc, char** argv) {
    cout << argc << endl;
    if (argc != 3) {
	bu_exit(0, "iges <iges_filename> <output_filename>\n");
    }

    string file(argv[1]);
    IGES iges(file);

    BRLCADBrepHandler bh;
    iges.readBreps(&bh);
    string out(argv[2]);
    bh.write(out);

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
