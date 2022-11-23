/*                    T E S T _ S E L E C T . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file test_select.c
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
    struct ged *dbp;

    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }
    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    dbp = ged_open("db", av[1], 1);

    const char *s_av[5] = {NULL, NULL, NULL, NULL, NULL};

    s_av[0] = "select";
    s_av[1] = "list";
    ged_exec(dbp, 2, s_av);
    printf("sets: %s\n", bu_vls_addr(dbp->ged_result_str));

    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g/platform.r";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/box.r/box.s";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/cone.r";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/ellipse.r";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/tor.r";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/light.r";
    ged_exec(dbp, 3, s_av);


    s_av[1] = "list";
    s_av[2] = "default";
    ged_exec(dbp, 3, s_av);
    printf("init: %s\n", bu_vls_addr(dbp->ged_result_str));


    s_av[1] = "expand";
    s_av[2] = "default";
    ged_exec(dbp, 3, s_av);

    s_av[1] = "list";
    s_av[2] = "default";
    ged_exec(dbp, 3, s_av);
    printf("expand: %s\n", bu_vls_addr(dbp->ged_result_str));

    s_av[1] = "clear";
    s_av[2] = "default";
    ged_exec(dbp, 3, s_av);

    s_av[1] = "list";
    s_av[2] = "default";
    ged_exec(dbp, 3, s_av);
    printf("clear: %s\n", bu_vls_addr(dbp->ged_result_str));


    s_av[1] = "add";
    s_av[2] = "all.g/platform.r";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/box.r/box.s";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/cone.r";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/ellipse.r";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/tor.r";
    ged_exec(dbp, 3, s_av);
    s_av[2] = "all.g/light.r";
    ged_exec(dbp, 3, s_av);

    s_av[1] = "list";
    s_av[2] = "default";
    ged_exec(dbp, 3, s_av);
    printf("re-init: %s\n", bu_vls_addr(dbp->ged_result_str));

    s_av[1] = "collapse";
    s_av[2] = "default";
    ged_exec(dbp, 3, s_av);
    printf("%s\n", bu_vls_addr(dbp->ged_result_str));

    s_av[1] = "list";
    s_av[2] = "default";
    ged_exec(dbp, 3, s_av);
    printf("collapse: %s\n", bu_vls_addr(dbp->ged_result_str));

    ged_close(dbp);

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
