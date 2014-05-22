/*                T E S T _ D I R B U I L D . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "raytrace.h"

int
main(int argc, char **argv)
{
    struct db_i *left_dbip = DBI_NULL;
    struct db_i *center_dbip = DBI_NULL;
    struct db_i *right_dbip = DBI_NULL;

    struct directory *left_dp = RT_DIR_NULL;
    struct directory *right_dp = RT_DIR_NULL;

    if (argc != 4) {
	bu_log("Error - please specify three .g files\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[0]);
    }

    if (!bu_file_exists(argv[2], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if (!bu_file_exists(argv[3], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[2]);
    }

    /* diff3 case */
    if ((left_dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }
    RT_CK_DBI(left_dbip);
    if (db_dirbuild(left_dbip) < 0) {
	db_close(left_dbip);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    if ((center_dbip = db_open(argv[2], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[2]);
    }
    RT_CK_DBI(center_dbip);
    if (db_dirbuild(center_dbip) < 0) {
	db_close(left_dbip);
	db_close(center_dbip);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[2]);
    }

    if ((right_dbip = db_open(argv[3], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[3]);
    }
    RT_CK_DBI(right_dbip);
    if (db_dirbuild(right_dbip) < 0) {
	db_close(center_dbip);
	db_close(left_dbip);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[3]);
    }

    FOR_ALL_DIRECTORY_START(left_dp, left_dbip) {
	right_dp = db_lookup(right_dbip, left_dp->d_namep, 0);

	if (left_dp == RT_DIR_NULL || right_dp == RT_DIR_NULL) {
	    bu_exit(1, "db_lookup failed\n");
	}
	if (!BU_STR_EQUAL(left_dp->d_namep, right_dp->d_namep)) {
	    bu_exit(1, "db_lookup failed\n");
	}

    } FOR_ALL_DIRECTORY_END;

    db_close(left_dbip);
    db_close(right_dbip);
    db_close(center_dbip);
    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
