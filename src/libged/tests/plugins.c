/*                        P L U G I N S . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2020 United States Government as represented by
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

#include <stdio.h>
#include <bu.h>
#include <ged.h>

int
main(int ac, char *av[]) {
    struct ged *gbp;

    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }
    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    gbp = ged_open("db", av[1], 1);

    const char * const *ged_cmds = NULL;
    size_t ged_cmd_cnt = ged_cmd_list(&ged_cmds);

    for (size_t i = 0; i < ged_cmd_cnt; i++) {
	bu_log("%s\n", ged_cmds[i]);
    }

    for (size_t i = 0; i < ged_cmd_cnt; i++) {
	bu_log("ged_execing %s\n", ged_cmds[i]);
	ged_exec(gbp, 1, (const char **)&ged_cmds[i]);
    }

    ged_close(gbp);
    BU_PUT(gbp, struct ged);
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
