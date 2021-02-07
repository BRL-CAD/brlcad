/*                     C A D _ U S E R . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file cad_user.c
 *
 * Code using BRL-CAD libraries that is intended to simulate an
 * external user of an installed BRL-CAD, without using any compile-time
 * features specific to the BRL-CAD build environment.
 */

#include <common.h>
#include <bu.h>
#include <ged.h>

int
main(int ac, char *av[]) {
    struct ged *gedp;

    /* Need this for bu_dir to work correctly */
    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s g_file\n", av[0]);
	return 1;
    }

    if (!bu_file_exists(av[1], NULL)) {
	printf("Error: %s does not exist\n", av[1]);
	return 1;
    }

    gedp = ged_open("db", av[1], 0);

    if (!gedp) {
	bu_log("Error - could not open database %s\n", av[1]);
	return 1;
    }

    const char *gcmd[3];
    gcmd[0] = "search";
    gcmd[1] = "/";

    if (ged_search(gedp, 2, (const char **)gcmd) != GED_OK) {
	goto user_test_fail;
    }

    ged_close(gedp);
    BU_PUT(gedp, struct ged);
    return 0;

user_test_fail:
    ged_close(gedp);
    BU_PUT(gedp, struct ged);
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
