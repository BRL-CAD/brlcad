/*              C H E C K _ P R I M _ C M D S . C P P
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
/** @file check_prim_cmds.cpp
 *
 * Interrogate the librt function table for ft_label entries, and for
 * any non-null entries check to see if we have a corresponding libged
 * command.
 */

#include "common.h"

#include <cstdio>
#include <set>

#include <bu.h>
#include <raytrace.h>
#include <ged.h>

int
main(int UNUSED(argc), const char **argv)
{
    std::set<std::string> skip_names;
    bu_setprogname(argv[0]);

    skip_names.insert(std::string("NULL"));
    skip_names.insert(std::string("UNUSED1"));
    skip_names.insert(std::string("UNUSED2"));

    std::set<std::string> have_cmds;
    std::set<std::string> no_cmds;

    const struct rt_functab *ftp;
    for (ftp = OBJ; ftp->magic != 0; ftp++) {
	if (!ftp || !strlen(ftp->ft_label))
	    continue;
	if (skip_names.find(std::string(ftp->ft_label)) != skip_names.end())
	    continue;
	if (ged_cmd_exists(ftp->ft_label)) {
	    have_cmds.insert(std::string(ftp->ft_label));
	} else {
	    no_cmds.insert(std::string(ftp->ft_label));
	}
    }

    std::set<std::string>::iterator p_it;
    std::cout << "Primitives with a GED command:\n";
    for (p_it = have_cmds.begin(); p_it != have_cmds.end(); ++p_it)
	std::cout << "\t" << *p_it << "\n";
    std::cout << "Primitives without a GED command:\n";
    for (p_it = no_cmds.begin(); p_it != no_cmds.end(); ++p_it)
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
