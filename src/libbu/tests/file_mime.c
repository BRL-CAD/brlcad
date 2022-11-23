/*                 F I L E _ M I M E . C
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
#include <string.h>

#include "bu.h"

int
test_ext(const char *str, bu_mime_context_t context, int expected)
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

    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(av[0]);

    if (ac != 4)
	bu_exit(1, "Usage: %s {extension} {context} {expected}\n", av[0]);

    sscanf(av[2], "%d", &context);

    expected = bu_file_mime_int(av[3]);

    if (context >= BU_MIME_UNKNOWN)
	return -1;

    return test_ext(av[1], (bu_mime_context_t)context, expected);
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
