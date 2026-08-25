/*      A S S E R T _ R E G I O N _ M A T E R I A L S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file assert_region_materials.cpp
 *
 * Verify that all regions in a generated .g use the expected GIFT material.
 *
 */

#include "common.h"

#include <cstdlib>
#include <cstring>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "rt/nongeom.h"


int
main(int argc, char *argv[])
{
    if (argc != 4) {
	bu_log("Usage: %s database.g expected_material min_region_count\n", argv[0]);
	return 1;
    }

    bu_setprogname(argv[0]);

    const char *gfile = argv[1];
    int expected_material = atoi(argv[2]);
    int min_region_count = atoi(argv[3]);

    struct db_i *dbip = db_open(gfile, DB_OPEN_READONLY);
    if (!dbip) {
	bu_log("Error: db_open(%s) failed\n", gfile);
	return 1;
    }
    if (db_dirbuild(dbip) < 0) {
	bu_log("Error: db_dirbuild(%s) failed\n", gfile);
	db_close(dbip);
	return 1;
    }

    int region_count = 0;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (!(dp->d_flags & RT_DIR_COMB))
	    continue;

	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	int id = rt_db_get_internal(&intern, dp, dbip, NULL);
	if (id < 0) {
	    bu_log("Error: rt_db_get_internal failed for %s\n", dp->d_namep);
	    db_close(dbip);
	    return 1;
	}
	if (id != ID_COMBINATION) {
	    rt_db_free_internal(&intern);
	    continue;
	}

	struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);
	if (comb->region_flag) {
	    region_count++;
	    if (comb->GIFTmater != expected_material) {
		bu_log("Error: region %s has material %ld, expected %d\n", dp->d_namep, comb->GIFTmater, expected_material);
		rt_db_free_internal(&intern);
		db_close(dbip);
		return 1;
	    }
	}

	 rt_db_free_internal(&intern);
    }
    FOR_ALL_DIRECTORY_END;

    if (region_count < min_region_count) {
	bu_log("Error: found %d regions, expected at least %d\n", region_count, min_region_count);
	db_close(dbip);
	return 1;
    }

    db_close(dbip);
    return 0;
}
