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
#include "rt/rt_ecmds.h"

int
main(int argc, char *argv[])
{
    const char *usage = "rt_edit_test_tor file.g testnum";
    long test_num = 0;
    const char *objname = "tor";
    bu_setprogname(argv[0]);
    argc--; argv++;

    if (argc < 2)
	bu_exit(1, "%s", usage);

    struct db_i *dbip = db_open(argv[0], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL)
        bu_exit(1, "ERROR: Unable to read from %s\n", argv[0]);

    if (db_dirbuild(dbip) < 0)
        bu_exit(1, "ERROR: db_dirbuild failed on %s\n", argv[0]);

    db_update_nref(dbip, &rt_uniresource);

    struct directory *dp = db_lookup(dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
        bu_exit(1, "ERROR: Unable to look up object %s\n", objname);

    struct rt_db_internal intern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);

    if (intern.idb_type != ID_TOR)
        bu_exit(1, "ERROR: %s - incorrect object type %d\n", objname, intern.idb_type);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);
    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width = 512;
    v->gv_height = 512;

    // Set up rt_solid_edit container
    struct rt_solid_edit *s = rt_solid_edit_create(&fp, dbip, &tol, v);

    // Get test number
    bu_sscanf(argv[1], "%ld", &test_num);

    switch (test_num) {
	case 1:
	    EDOBJ[intern.idb_type].ft_set_edit_mode(s, ECMD_TOR_R1);
	    s->es_scale = 2.0;
	    rt_solid_edit_process(s);
	    break;
	case 2:
	    break;
	case 3:
	    break;
	case 4:
	    break;
	default:
	    rt_solid_edit_destroy(s);
	    bu_exit(1, "ERROR: unknown test number %ld\n", test_num);
	    break;
    }

    rt_solid_edit_destroy(s);
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

