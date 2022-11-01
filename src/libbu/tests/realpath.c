/*                       R E A L P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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

#include "bu.h"

struct rp_container {
    char dir[MAXPATHLEN];
};
#define RPC_INIT {{0}}

int
main(int ac, char *av[])
{
    int test_num = 0;

    bu_setprogname(av[0]);

    if (ac < 2) {
	bu_exit(1, "Usage: %s {test_number}\n", av[0]);
    }

    sscanf(av[1], "%d", &test_num);

    switch (test_num) {
	case 1:
	    {
		const char *dir = "/tmp/REALPATH_TEST_PATH";
		dir = bu_file_realpath(dir, NULL);
		printf("Test 1 result: %s\n", dir);
		return 0;
	    }
	case 2:
	    {
		struct rp_container RPC = RPC_INIT;
		const char *dir2 = "/tmp/REALPATH_TEST_PATH";
		dir2 = bu_file_realpath(dir2, RPC.dir);
		printf("Test 2 result: %s\n", dir2);
		return 0;
	    }
    }

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
