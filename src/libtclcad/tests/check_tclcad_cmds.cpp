/*           C H E C K _ T C L C A D _ C M D S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/** @file check_tclcad_cmds.cpp
 *
 * Interrogate the libtclcad to_cmds function table and see if any of the
 * command strings collide with libged command names.
 */

#include "common.h"

#include <cstdio>
#include <set>

#include <bu.h>
#include <ged.h>
#include <tclcad.h>
#include "../tclcad_private.h" // for to_cmdtab and to_cmds

int
main(int UNUSED(argc), const char **argv)
{
    std::set<std::string> skip_names;
    bu_setprogname(argv[0]);

    skip_names.insert(std::string("NULL"));

    std::set<std::string> have_collision;
    std::set<std::string> no_collision;

    struct to_cmdtab *ctp;
    for (ctp = to_cmds; ctp->to_name != (char *)NULL; ctp++) {
	if (skip_names.find(std::string(ctp->to_name)) != skip_names.end())
	    continue;
	if (ged_cmd_valid(ctp->to_name, NULL)) {
	    no_collision.insert(std::string(ctp->to_name));
	} else {
	    have_collision.insert(std::string(ctp->to_name));
	}
    }

    std::set<std::string>::iterator p_it;
    std::cout << "libtclcad commands matching a GED command:\n";
    for (p_it = have_collision.begin(); p_it != have_collision.end(); ++p_it)
	std::cout << "\t" << *p_it << "\n";
    std::cout << "Primitives without a GED command:\n";
    for (p_it = no_collision.begin(); p_it != no_collision.end(); ++p_it)
	std::cout << "\t" << *p_it << "\n";

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
