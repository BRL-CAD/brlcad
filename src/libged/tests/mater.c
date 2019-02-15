/*                        M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2019 United States Government as represented by
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
/** @file mater.c
 *
 * Brief description
 *
 */

#include "common.h"

#include <stdio.h>
#include <bu.h>
#include <ged.h>

int
main(int ac, char *av[]) {
    struct ged *gedp;
    const char *gname = "ged_mater_test.g";
    const char *mater_cmd[10] = {"mater", "-d", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

    if (ac != 2) {
	printf("Usage: %s test_name\n", av[0]);
	return 1;
    }

    if (bu_file_exists(gname, NULL)) {
	printf("ERROR: %s already exists, aborting\n", gname);
	return 2;
    }

    gedp = ged_open("db", gname, 0);

    if (ged_mater(gedp, 2, (const char **)mater_cmd) != GED_HELP) {
	bu_log("Basic argv doesn't return GED_HELP\n");
	goto ged_test_fail;
    }

    ged_close(gedp);
    BU_PUT(gedp, struct ged);
    bu_file_delete(gname);

    return 0;

ged_test_fail:
    ged_close(gedp);
    BU_PUT(gedp, struct ged);
    bu_file_delete(gname);
    return 1;
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
