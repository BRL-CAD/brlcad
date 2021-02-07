/*                     T E S T _ L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
    struct ged g;
    const char *list[2] = {"l", NULL};
    int seconds;
    int64_t start;

    bu_setprogname(av[0]);

    if (ac < 1 || ac > 2) {
	printf("Usage: %s [seconds]\n", av[0]);
	return 1;
    }

    if (ac > 1) {
	size_t invocations = 0;
	seconds = atoi(av[1]);
	start = bu_gettime();

	while ((double)(bu_gettime() - start) < (seconds * 1000000.0)) {
	    struct bu_process *info;
	    const char *cmd[2];
	    cmd[0] = av[0];
	    cmd[1] = NULL;
	    bu_process_exec(&info, av[0], 1, cmd, 0, 0);
#if 0
	    {
		char buffer[1024] = {0};
		int count = 1024;
		bu_process_read(buffer, &count, info, 1, 1024);
		bu_log("read %d: [%s]\n", count, buffer);
	    }
#endif
	    bu_process_wait(NULL, info, 0);
	    invocations++;
	    /* bu_snooze(BU_SEC2USEC(1)); */
	}
	bu_log("Invoked %zu times (%.1lf per sec)\n", invocations, invocations / ((bu_gettime() - start) / 1000000.0));
    } else {
	ged_init(&g);
	ged_list(&g, 1, list);
	/* intentionally sinking the ged result string
	*/
	printf("%s\n", bu_vls_addr(g.ged_result_str));
	ged_free(&g);
    }

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
