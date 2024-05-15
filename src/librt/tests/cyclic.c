/*                       C Y C L I C . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2024 United States Government as represented by
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

#include "vmath.h"
#include "bu/app.h"
#include "raytrace.h"
#include "../librt_private.h"

int
main(int argc, char *argv[])
{
    struct db_i *dbip;

    bu_setprogname(argv[0]);

    if (argc != 2) {
	bu_exit(1, "Usage: %s file.g", argv[0]);
    }

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    db_update_nref(dbip, &rt_uniresource);

    int tops_cnt_expected = 3;
    int tops_cnt = db_ls(dbip, DB_LS_TOPS, NULL, NULL);

    int cyclic_cnt_expected = 5;
    int cyclic_cnt = db_ls(dbip, DB_LS_CYCLIC, NULL, NULL);

    int ctops_cnt_expected = 8;
    int ctops_cnt = db_ls(dbip, DB_LS_TOPS | DB_LS_CYCLIC, NULL, NULL);

    int ret = 0;
    if (tops_cnt != tops_cnt_expected) {
	bu_log("Expected standard tops count of %d, got %d\n", tops_cnt_expected, tops_cnt);
	ret = 1;
    }

    if (cyclic_cnt != cyclic_cnt_expected) {
	bu_log("Expected standard cyclic count of %d, got %d\n", cyclic_cnt_expected, cyclic_cnt);
	ret = 1;
    }

    if (ctops_cnt != ctops_cnt_expected) {
	bu_log("Expected cyclic tops count of %d, got %d\n", ctops_cnt_expected, ctops_cnt);
	ret = 1;
    }

    return ret;
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
