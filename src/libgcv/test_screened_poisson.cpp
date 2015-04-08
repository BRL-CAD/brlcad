/*          T E S T _ S C R E E N E D _ P O I S S O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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

#include <map>
#include <set>
#include <queue>
#include <list>
#include <vector>
#include <iostream>
#include <fstream>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "gcv_private.h"

int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct directory *dp;
    struct rt_wdb *wdbp;
    struct rt_db_internal intern;
    struct bu_vls bot_name = BU_VLS_INIT_ZERO;
    int tol;

    if (argc != 4) {
	bu_exit(1, "Usage: %s file.g object tol(inter number of mm)", argv[0]);
    }

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
    }

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    if (wdbp == RT_WDB_NULL) {
	bu_exit(1, "ERROR: wdb open failed\n");
    }

    tol = atoi(argv[3]);

    RT_DB_INTERNAL_INIT(&intern)
    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	bu_exit(1, "ERROR: Unable to get internal representation of %s\n", argv[2]);
    }

    int *faces;
    point_t *points;
    int num_faces, num_pnts;
    gcv_generate_mesh(&faces, &num_faces, &points, &num_pnts, dbip, dp->d_namep, tol);

    bu_vls_printf(&bot_name, "%s.bot", dp->d_namep);
    if (mk_bot(wdbp, bu_vls_addr(&bot_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, num_pnts, num_faces, (fastf_t *)points, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL)) {
	bu_log("mk_bot failed\n");
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
