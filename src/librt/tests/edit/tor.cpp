/*                         T O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file tor.cpp
 *
 * Test editing of TOR primitive parameters.
 */

#include "common.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/avs.h"
#include "bu/env.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "raytrace.h"

int
main(int argc, char *argv[])
{
    const char *usage = "rt_edit_test_tor file.g testnum";
    long test_num = 0;
    const char *objname = "tor.s";
    bu_setprogname(argv[0]);
    argc--; argv++;

    if (argc < 2)
	bu_exit(1, "%s", usage);

    struct db_i *dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL)
        bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    if (db_dirbuild(dbip) < 0)
        bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    db_update_nref(dbip, &rt_uniresource);

    struct directory *dp = db_lookup(dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
        bu_exit(1, "ERROR: Unable to look up object %s\n", objname);

    struct rt_db_internal intern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);

    if (intern.idb_type != ID_TOR)
        bu_exit(1, "ERROR: %s - incorrect object type %d\n", objname, intern.idb_type);


    // Set up rt_solid_edit container


    // Get test number
    bu_sscanf(argv[1], "%ld", &test_num);

    switch (test_num) {
	case 1:
	    break;
	case 2:
	    break;
	case 3:
	    break;
	case 4:
	    break;
	default:
	    bu_exit(1, "ERROR: unknown test number %ld\n", test_num);
	    break;
    }

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

