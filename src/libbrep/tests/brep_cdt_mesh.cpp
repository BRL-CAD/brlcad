/*                    B R E P _ C D T _ M E S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2019 United States Government as represented by
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
#include "../cdt_mesh.h"

int
main(int argc, const char **argv)
{

    if (argc != 2) {
	std::cerr << "brep_cdt_mesh <serialization_file>\n";
    }
    cdt_mesh::cdt_mesh_t fmesh;
    fmesh.deserialize(argv[1]);
    fmesh.repair();

    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
