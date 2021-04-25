/*                 F I L E _ M I M E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
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
#include <string.h>

#include "bu.h"

int
test_ext(const char *str, long context, int expected)
{
    int status = 0;
    int type = bu_file_mime(str, context);

    if (type == expected) {
	bu_log("%s -> %d [PASS]\n", str, type);
    } else {
	bu_log("%s -> %d [FAIL]  (should be: {%d})\n", str, type, expected);
	status = 1;
    }

    return status;
}


int
main(int ac, char *av[])
{
    int context = 0;
    int expected = 0;

    bu_setprogname(av[0]);

    if (ac != 4)
	bu_exit(1, "Usage: %s {extension} {context} {expected}\n", av[0]);

    sscanf(av[2], "%d", &context);
    sscanf(av[3], "%d", &expected);

    if (context >= BU_MIME_UNKNOWN)
	return -1;

    return test_ext(av[1], (long)context, expected);
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
