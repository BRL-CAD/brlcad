/*                        P A R S E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file parse.c
 *
 * Unit tests for libmcpcad command string parsing.
 *
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "mcpcad.h"

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    if (argc > 1) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    /* canary: NULL input must be rejected, and freeing NULL is a no-op */
    struct mcpcad_cmd *c = mcpcad_cmd_parse(NULL);
    if (c) {
	fprintf(stderr, "FAIL: mcpcad_cmd_parse(NULL) returned non-NULL\n");
	return 1;
    }
    mcpcad_cmd_free(NULL);

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
